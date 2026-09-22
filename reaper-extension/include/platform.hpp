#pragma once
// Socket and single-instance primitives differ between Win32 and POSIX. The
// bridge uses this thin layer so its logic reads the same on every host.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <linux/magic.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif
#include <chrono>
#include <cstddef>
#include <filesystem>

namespace rmcp::platform {
// Result of inspecting a path without following it. `readable` is false when the
// attributes could not be read at all; `symlink` marks the links and reparse
// points that directory walks must refuse to descend into.
struct FileKind {
    bool readable = false;
    bool symlink = false;
};

#ifdef _WIN32
using socket_t = SOCKET;
using socklen_arg = int;
using iolen_t = int;  // Win32 send/recv take an int length, POSIX takes size_t.
using lock_t = HANDLE;
inline constexpr socket_t invalid_socket = INVALID_SOCKET;
inline const lock_t invalid_lock = INVALID_HANDLE_VALUE;

inline bool startup() { WSADATA d{}; return WSAStartup(MAKEWORD(2, 2), &d) == 0; }
inline void cleanup() { WSACleanup(); }
inline void close_socket(socket_t s) { closesocket(s); }
inline bool set_nonblocking(socket_t s) { u_long m = 1; return ioctlsocket(s, FIONBIO, &m) == 0; }
// True when the last operation merely found no data on a non-blocking socket.
inline bool would_block() { return WSAGetLastError() == WSAEWOULDBLOCK; }
inline int process_id() { return static_cast<int>(GetCurrentProcessId()); }
inline unsigned long long tick_count() { return GetTickCount64(); }

// Share mode zero serializes instances and installers. The OS releases this
// handle after crashes, so stale discovery can safely be replaced.
inline lock_t acquire_lock(const std::filesystem::path& path) {
    return CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
}
inline void release_lock(lock_t& h, const std::filesystem::path&) {
    if (h != invalid_lock) CloseHandle(h);
    h = invalid_lock;
}

inline FileKind inspect(const std::filesystem::path& path) {
    auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return {};
    return {true, (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0};
}
// UNC prefixes and drives the OS reports as remote both mean the target is not
// a local filesystem.
inline bool is_network_path(const std::filesystem::path& path) {
    if (path.u8string().rfind("\\\\", 0) == 0) return true;
    return GetDriveTypeW(path.root_path().c_str()) == DRIVE_REMOTE;
}
#else
using socket_t = int;
using socklen_arg = socklen_t;
using iolen_t = std::size_t;
using lock_t = int;
inline constexpr socket_t invalid_socket = -1;
inline constexpr lock_t invalid_lock = -1;

inline bool startup() { return true; }
inline void cleanup() {}
inline void close_socket(socket_t s) { ::close(s); }
inline bool set_nonblocking(socket_t s) {
    int flags = ::fcntl(s, F_GETFL, 0);
    return flags != -1 && ::fcntl(s, F_SETFL, flags | O_NONBLOCK) != -1;
}
// EINTR is included deliberately: a signal interrupting the call is not a peer
// failure, and treating it as one would drop a healthy connection.
inline bool would_block() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
inline int process_id() { return static_cast<int>(::getpid()); }
// Matches GetTickCount64: milliseconds from an unspecified monotonic origin,
// used only to make generated names unique, never to compute wall-clock time.
inline unsigned long long tick_count() {
    return static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// flock is advisory but sufficient here: every participant is this project's
// own code, and the kernel releases the lock when the fd closes or the process
// dies, matching the crash behaviour the Win32 path relies on.
inline lock_t acquire_lock(const std::filesystem::path& path) {
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (fd == -1) return invalid_lock;
    if (::flock(fd, LOCK_EX | LOCK_NB) == -1) {
        ::close(fd);
        return invalid_lock;
    }
    return fd;
}
// FILE_FLAG_DELETE_ON_CLOSE has no POSIX equivalent, so the file is unlinked
// explicitly; the lock itself is released by closing the descriptor.
inline void release_lock(lock_t& fd, const std::filesystem::path& path) {
    if (fd != invalid_lock) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        ::close(fd);
    }
    fd = invalid_lock;
}

// lstat rather than stat: the caller needs to know the entry *is* a link, not
// what it points at.
inline FileKind inspect(const std::filesystem::path& path) {
    struct stat info{};
    if (::lstat(path.c_str(), &info) != 0) return {};
    return {true, S_ISLNK(info.st_mode)};
}

// POSIX has no UNC paths or drive letters, so remoteness is a property of the
// mounted filesystem. These are the magic numbers of the network filesystems a
// render directory could plausibly land on; anything unrecognised is treated as
// local, matching how GetDriveTypeW reports an unknown volume.
// FUSE is deliberately absent: sshfs is remote but so are many purely local
// FUSE mounts, and rejecting every one of them would block ordinary renders.
inline bool is_network_path(const std::filesystem::path& path) {
    constexpr long network_filesystems[] = {
        NFS_SUPER_MAGIC,  SMB_SUPER_MAGIC,  CIFS_SUPER_MAGIC, SMB2_SUPER_MAGIC,
        AFS_SUPER_MAGIC,  AFS_FS_MAGIC,     CODA_SUPER_MAGIC, CEPH_SUPER_MAGIC,
        OCFS2_SUPER_MAGIC, V9FS_MAGIC,
    };
    // The directory may not exist yet, so fall back to the nearest existing
    // ancestor: a path is remote exactly when the mount it will live on is.
    struct statfs info{};
    auto probe = path;
    while (::statfs(probe.c_str(), &info) != 0) {
        if (errno != ENOENT || !probe.has_parent_path() || probe.parent_path() == probe) return false;
        probe = probe.parent_path();
    }
    for (auto magic : network_filesystems)
        if (static_cast<long>(info.f_type) == magic) return true;
    return false;
}
#endif

// Win32 never signals the process on a write to a closed peer, but POSIX raises
// SIGPIPE, whose default action would terminate REAPER. MSG_NOSIGNAL turns that
// into an ordinary EPIPE return; macOS lacks the flag and uses SO_NOSIGPIPE.
#if defined(MSG_NOSIGNAL)
inline constexpr int send_flags = MSG_NOSIGNAL;
#else
inline constexpr int send_flags = 0;
#endif
inline void suppress_sigpipe(socket_t s) {
#if !defined(_WIN32) && !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    int on = 1;
    ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
    (void)s;
#endif
}
}  // namespace rmcp::platform
