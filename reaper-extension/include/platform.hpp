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
#include <fstream>
#include <functional>
#include <istream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace rmcp::platform {
// Result of inspecting a path without following it. `readable` is false when the
// attributes could not be read at all; `symlink` marks the links and reparse
// points that directory walks must refuse to descend into.
struct FileKind {
    bool readable = false;
    bool symlink = false;
};

// Where a path's bytes would live. Only `local` is acceptable for rendering:
// `remote` is a recognised network filesystem and `unknown` is anything that
// could not be positively identified, which is rejected rather than trusted.
enum class Storage { local, remote, unknown };

// Why acquire_lock failed. `busy` is the expected conflict with another
// instance or installer; `unsafe` is a lock path that is not a plain file.
enum class LockFailure { none, busy, unsafe, io };

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
inline lock_t acquire_lock(const std::filesystem::path& path, LockFailure* why = nullptr) {
    auto h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (why) {
        auto error = h == invalid_lock ? GetLastError() : ERROR_SUCCESS;
        *why = error == ERROR_SUCCESS ? LockFailure::none
             : error == ERROR_SHARING_VIOLATION ? LockFailure::busy : LockFailure::io;
    }
    return h;
}
inline void release_lock(lock_t& h) {
    if (h != invalid_lock) CloseHandle(h);
    h = invalid_lock;
}

inline FileKind inspect(const std::filesystem::path& path) {
    auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return {};
    return {true, (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0};
}
// UNC prefixes and drives the OS reports as remote are network storage. Only
// drive types that are positively local are accepted; DRIVE_UNKNOWN and
// DRIVE_NO_ROOT_DIR are unresolved and therefore not trusted.
inline Storage classify_storage(const std::filesystem::path& path) {
    if (!path.is_absolute()) return Storage::unknown;
    if (path.u8string().rfind("\\\\", 0) == 0) return Storage::remote;
    switch (GetDriveTypeW(path.root_path().c_str())) {
    case DRIVE_REMOTE: return Storage::remote;
    case DRIVE_FIXED: case DRIVE_REMOVABLE: case DRIVE_RAMDISK: case DRIVE_CDROM: return Storage::local;
    default: return Storage::unknown;
    }
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
//
// The lock file is never unlinked. flock belongs to an inode, not a pathname:
// removing the file while a contender already holds a descriptor to it lets that
// contender lock the orphaned inode while a newcomer creates and locks a fresh
// one, and both then believe they own the directory. A permanent file keeps one
// inode for everybody. O_NOFOLLOW and the regular-file check refuse links, FIFOs
// and devices planted at the path; the final identity check refuses a file that
// was replaced between open and flock.
inline lock_t acquire_lock(const std::filesystem::path& path, LockFailure* why = nullptr) {
    auto fail = [why](LockFailure reason) {
        if (why) *why = reason;
        return invalid_lock;
    };
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
                    S_IRUSR | S_IWUSR);
    if (fd == -1) return fail(errno == ELOOP ? LockFailure::unsafe : LockFailure::io);
    struct stat opened{};
    if (::fstat(fd, &opened) != 0) { ::close(fd); return fail(LockFailure::io); }
    if (!S_ISREG(opened.st_mode)) { ::close(fd); return fail(LockFailure::unsafe); }
    if (::flock(fd, LOCK_EX | LOCK_NB) == -1) {
        auto reason = errno == EWOULDBLOCK ? LockFailure::busy : LockFailure::io;
        ::close(fd);
        return fail(reason);
    }
    struct stat named{};
    if (::lstat(path.c_str(), &named) != 0 || named.st_dev != opened.st_dev ||
        named.st_ino != opened.st_ino) {
        ::close(fd);
        return fail(LockFailure::unsafe);
    }
    if (why) *why = LockFailure::none;
    return fd;
}
// Closing the descriptor is what releases the lock; the file itself stays.
inline void release_lock(lock_t& fd) {
    if (fd != invalid_lock) ::close(fd);
    fd = invalid_lock;
}

// lstat rather than stat: the caller needs to know the entry *is* a link, not
// what it points at.
inline FileKind inspect(const std::filesystem::path& path) {
    struct stat info{};
    if (::lstat(path.c_str(), &info) != 0) return {};
    return {true, S_ISLNK(info.st_mode)};
}

// POSIX has no UNC paths or drive letters, so locality is a property of the
// mounted filesystem. statfs gives its magic number; for FUSE, which covers both
// local and network mounts, the subtype comes from /proc/self/mountinfo.
namespace detail {
// statfs magic numbers, spelled out because <linux/magic.h> lacks several of
// them on older kernels' headers.
inline constexpr unsigned long local_filesystems[] = {
    0xEF53,      // ext2/3/4
    0x58465342,  // xfs
    0x9123683E,  // btrfs
    0x2FC12FC1,  // zfs
    0xF2F52010,  // f2fs
    0xCA451A4E,  // bcachefs
    0x3153464A,  // jfs
    0x52654973,  // reiserfs
    0x01021994,  // tmpfs
    0x794C7630,  // overlayfs (its upper layer cannot be a network filesystem)
    0x4D44,      // vfat/msdos
    0x2011BAB0,  // exfat
    0x7366746E,  // ntfs3
    0x482B,      // hfsplus
};
inline constexpr unsigned long network_filesystems[] = {
    0x6969,      // nfs
    0x517B,      // smb
    0xFF534D42,  // cifs
    0xFE534D42,  // smb2
    0x5346414F,  // afs
    0x6B414653,  // kafs
    0x73757245,  // coda
    0x00C36400,  // ceph
    0x7461636F,  // ocfs2 (shared cluster storage)
    0x01161970,  // gfs2 (shared cluster storage)
    0x01021997,  // 9p
    0x0BD00BD0,  // lustre
};
inline constexpr unsigned long fuse_filesystem = 0x65735546;
// FUSE subtypes whose data lives on another machine. A subtype absent from this
// list is not assumed local: stacking filesystems such as bindfs or gocryptfs
// inherit the locality of whatever they sit on, which mountinfo does not say.
inline constexpr const char* network_fuse_subtypes[] = {
    "sshfs", "rclone", "s3fs", "gcsfuse", "goofys", "curlftpfs", "davfs", "davfs2",
    "smbnetfs", "glusterfs", "ceph-fuse", "cephfs", "juicefs", "lizardfs", "mfs",
    "moosefs", "onedriver", "google-drive-ocamlfuse", "gvfsd-fuse", "httpfs2",
};

struct Mount {
    std::string point, fstype, source;
};

// mountinfo escapes space, tab, newline and backslash as three octal digits.
inline std::string unescape_mount_field(const std::string& field) {
    std::string out;
    for (std::size_t i = 0; i < field.size(); ++i) {
        if (field[i] == '\\' && field.size() - i >= 4 && field[i + 1] >= '0' && field[i + 1] <= '3' &&
            field[i + 2] >= '0' && field[i + 2] <= '7' && field[i + 3] >= '0' && field[i + 3] <= '7') {
            out += static_cast<char>((field[i + 1] - '0') * 64 + (field[i + 2] - '0') * 8 + (field[i + 3] - '0'));
            i += 3;
        } else {
            out += field[i];
        }
    }
    return out;
}

// Format: id parent major:minor root mount-point options [optional...] - fstype source super-options
inline std::vector<Mount> parse_mountinfo(std::istream& in) {
    std::vector<Mount> mounts;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string id, parent, device, root, point, options, field;
        if (!(fields >> id >> parent >> device >> root >> point >> options)) continue;
        while (fields >> field && field != "-") {}
        Mount mount;
        if (field != "-" || !(fields >> mount.fstype >> mount.source)) continue;
        mount.point = unescape_mount_field(point);
        mount.source = unescape_mount_field(mount.source);
        mounts.push_back(std::move(mount));
    }
    return mounts;
}

// The mount that owns a resolved path: the longest mount point containing it,
// with later entries shadowing earlier ones mounted at the same point.
inline std::optional<Mount> owning_mount(const std::vector<Mount>& mounts,
                                         const std::filesystem::path& resolved) {
    std::optional<Mount> best;
    std::size_t best_length = 0;
    auto target = resolved.lexically_normal();
    for (const auto& mount : mounts) {
        auto point = std::filesystem::path(mount.point).lexically_normal();
        auto relative = target.lexically_relative(point);
        if (relative.empty() || *relative.begin() == "..") continue;
        auto length = point.native().size();
        if (!best || length >= best_length) {
            best = mount;
            best_length = length;
        }
    }
    return best;
}

inline Storage classify_fuse(const std::optional<Mount>& mount) {
    if (!mount) return Storage::unknown;
    // fuseblk always sits on a block device, which is the one FUSE form whose
    // locality is known from mountinfo alone (ntfs-3g, exfat-fuse).
    if (mount->fstype == "fuseblk") {
        return mount->source.rfind("/dev/", 0) == 0 ? Storage::local : Storage::unknown;
    }
    constexpr const char prefix[] = "fuse.";
    if (mount->fstype.rfind(prefix, 0) != 0) return Storage::unknown;
    auto subtype = mount->fstype.substr(sizeof(prefix) - 1);
    for (const auto* remote : network_fuse_subtypes)
        if (subtype == remote) return Storage::remote;
    return Storage::unknown;
}
}  // namespace detail

// The queries classify_storage needs, separated so tests can supply filesystems
// that a CI machine does not have.
struct StorageProbe {
    // statfs(2): returns 0 and sets the magic number, or returns an errno value.
    std::function<int(const std::filesystem::path&, unsigned long&)> filesystem_type;
    // realpath(3) of an existing path; false when it cannot be resolved.
    std::function<bool(const std::filesystem::path&, std::filesystem::path&)> resolve;
    // Contents of /proc/self/mountinfo; false when unavailable.
    std::function<bool(std::string&)> mountinfo;
};

inline StorageProbe system_storage_probe() {
    return {
        [](const std::filesystem::path& path, unsigned long& type) {
            struct statfs info{};
            if (::statfs(path.c_str(), &info) != 0) return errno;
            // f_type is a signed word; the magic numbers are 32-bit patterns.
            type = static_cast<unsigned long>(info.f_type) & 0xFFFFFFFFUL;
            return 0;
        },
        [](const std::filesystem::path& path, std::filesystem::path& resolved) {
            std::error_code ec;
            resolved = std::filesystem::canonical(path, ec);
            return !ec;
        },
        [](std::string& out) {
            std::ifstream in("/proc/self/mountinfo");
            if (!in) return false;
            std::ostringstream text;
            text << in.rdbuf();
            out = text.str();
            return true;
        },
    };
}

// A directory that does not exist yet is judged by the mount it would be
// created on: missing components are walked up past, and only a missing
// component (ENOENT) is. Every other lookup failure, an unrecognised
// filesystem, or a FUSE mount whose locality cannot be established is unknown.
inline Storage classify_storage(const std::filesystem::path& path, const StorageProbe& probe) {
    if (!path.is_absolute()) return Storage::unknown;
    auto existing = path;
    unsigned long type = 0;
    for (;;) {
        int error = probe.filesystem_type(existing, type);
        if (error == 0) break;
        if (error != ENOENT || !existing.has_relative_path()) return Storage::unknown;
        existing = existing.parent_path();
    }
    for (auto magic : detail::network_filesystems)
        if (type == magic) return Storage::remote;
    for (auto magic : detail::local_filesystems)
        if (type == magic) return Storage::local;
    if (type != detail::fuse_filesystem) return Storage::unknown;
    std::filesystem::path resolved;
    std::string table;
    if (!probe.resolve(existing, resolved) || !probe.mountinfo(table)) return Storage::unknown;
    std::istringstream in(table);
    return detail::classify_fuse(detail::owning_mount(detail::parse_mountinfo(in), resolved));
}

inline Storage classify_storage(const std::filesystem::path& path) {
    return classify_storage(path, system_storage_probe());
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
