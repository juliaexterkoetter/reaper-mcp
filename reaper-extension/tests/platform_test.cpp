#include "platform.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#endif

namespace {
using namespace rmcp;

void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

// A peer that closes mid-conversation is ordinary, and on POSIX the resulting
// write raises SIGPIPE, whose default action terminates the host process. This
// exercises that exact sequence: without the suppression this test does not
// fail, it kills its own process, which is what it would do to REAPER.
void surviving_a_closed_peer() {
    check(platform::startup(), "socket startup failed");
    auto listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(listener != platform::invalid_socket, "listener socket failed");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    check(::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0, "bind failed");
    check(::listen(listener, 1) == 0, "listen failed");
    platform::socklen_arg length = sizeof(addr);
    check(::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &length) == 0, "getsockname failed");

    auto client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(client != platform::invalid_socket, "client socket failed");
    check(::connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0, "connect failed");
    auto served = ::accept(listener, nullptr, nullptr);
    check(served != platform::invalid_socket, "accept failed");
    platform::suppress_sigpipe(served);
    platform::close_socket(client);

    // The first write usually lands in the socket buffer and only the next one
    // meets the peer's reset, so keep writing until the failure appears.
    const std::string payload(4096, 'x');
    bool reported_failure = false;
    for (int attempt = 0; attempt < 64 && !reported_failure; ++attempt) {
        auto sent = ::send(served, payload.data(),
                           static_cast<platform::iolen_t>(payload.size()), platform::send_flags);
        if (sent < 0) reported_failure = true;
    }
    check(reported_failure, "writing to a closed peer never reported an error");

    platform::close_socket(served);
    platform::close_socket(listener);
    platform::cleanup();
}

void inspecting_paths(const std::filesystem::path& dir) {
    auto file = dir / "regular.txt";
    std::ofstream(file) << "content";
    auto regular = platform::inspect(file);
    check(regular.readable, "regular file reported unreadable");
    check(!regular.symlink, "regular file reported as a link");

    auto missing = platform::inspect(dir / "does-not-exist");
    check(!missing.readable, "missing path reported readable");
    check(!missing.symlink, "missing path reported as a link");

    // Symlink creation needs privileges or developer mode on Windows, so a
    // refusal there is reported rather than silently passing.
    auto link = dir / "link.txt";
    std::error_code ec;
    std::filesystem::create_symlink(file, link, ec);
    if (ec) {
        std::cout << "  symlink detection not exercised: " << ec.message() << '\n';
        return;
    }
    auto linked = platform::inspect(link);
    check(linked.readable, "symlink reported unreadable");
    check(linked.symlink, "symlink not detected, directory walks would follow it");
}

const char* name(platform::Storage storage) {
    switch (storage) {
    case platform::Storage::local: return "local";
    case platform::Storage::remote: return "remote";
    default: return "unknown";
    }
}
void expect(platform::Storage actual, platform::Storage expected, const std::string& what) {
    if (actual != expected)
        throw std::runtime_error(what + ": expected " + name(expected) + ", got " + name(actual));
}

#ifndef _WIN32
// A fake filesystem for the classifier: magic numbers per existing path, an
// errno for paths that fail, a mount table, and a record of what was probed.
struct FakeMounts {
    std::map<std::string, unsigned long> types;
    std::map<std::string, int> errors;
    std::string mountinfo;
    bool mountinfo_readable = true, resolvable = true;
    std::vector<std::string> probed;

    platform::StorageProbe probe() {
        return {
            [this](const std::filesystem::path& p, unsigned long& type) {
                probed.push_back(p.generic_string());
                auto error = errors.find(p.generic_string());
                if (error != errors.end()) return error->second;
                auto found = types.find(p.generic_string());
                if (found == types.end()) return ENOENT;
                type = found->second;
                return 0;
            },
            [this](const std::filesystem::path& p, std::filesystem::path& out) {
                out = p;
                return resolvable;
            },
            [this](std::string& out) {
                out = mountinfo;
                return mountinfo_readable;
            },
        };
    }
};

constexpr unsigned long ext4 = 0xEF53, nfs = 0x6969, cifs = 0xFF534D42, fuse = 0x65735546;

std::string mount_line(const std::string& point, const std::string& fstype, const std::string& source) {
    return "36 25 0:44 / " + point + " rw,relatime shared:1 - " + fstype + " " + source + " rw\n";
}

// CI has no sshfs, NFS or ntfs-3g mount, so the classification is pinned
// against injected statfs and mountinfo results covering each case.
void classifying_injected_storage() {
    using platform::Storage;
    FakeMounts fs;
    fs.types = {{"/", ext4}, {"/srv/nfs", nfs}, {"/srv/smb", cifs}, {"/mnt/ssh", fuse},
                {"/mnt/ssh/disk", fuse}, {"/mnt/rclone", fuse}, {"/mnt/bind", fuse},
                {"/mnt/my drive", fuse}, {"/mnt/orphan", fuse}, {"/mnt/odd", 0x12345678}};
    fs.errors = {{"/denied", EACCES}, {"/file/child", ENOTDIR}, {"/loop", ELOOP}};
    fs.mountinfo = mount_line("/", "ext4", "/dev/sda2") +
                   mount_line("/mnt/ssh", "fuse.sshfs", "user@host:/srv") +
                   mount_line("/mnt/ssh/disk", "fuseblk", "/dev/sdb1") +
                   mount_line("/mnt/rclone", "fuse.rclone", "drive:") +
                   mount_line("/mnt/bind", "fuse.bindfs", "/srv/nfs") +
                   mount_line("/mnt/my\\040drive", "fuseblk", "/dev/sdc1");
    auto classify = [&](const char* path) { return platform::classify_storage(path, fs.probe()); };

    expect(classify("/home/user/renders"), Storage::local, "ext4");
    expect(classify("/srv/nfs/renders"), Storage::remote, "nfs");
    expect(classify("/srv/smb"), Storage::remote, "cifs");
    expect(classify("/mnt/ssh/renders"), Storage::remote, "sshfs");
    expect(classify("/mnt/ssh/disk/renders"), Storage::local, "fuseblk nested under sshfs");
    expect(classify("/mnt/rclone"), Storage::remote, "rclone");
    // A stacking FUSE filesystem inherits the locality of what it sits on, which
    // mountinfo does not reveal; here it is in fact backed by NFS.
    expect(classify("/mnt/bind/renders"), Storage::unknown, "bindfs");
    expect(classify("/mnt/my drive/renders"), Storage::local, "escaped mount point");
    expect(classify("/mnt/orphan"), Storage::unknown, "FUSE mount missing from mountinfo");
    expect(classify("/mnt/odd"), Storage::unknown, "unrecognised filesystem");
    expect(classify("/denied/renders"), Storage::unknown, "EACCES");
    expect(classify("/file/child"), Storage::unknown, "ENOTDIR");
    expect(classify("/loop"), Storage::unknown, "ELOOP");
    expect(platform::classify_storage("relative/renders", fs.probe()), Storage::unknown, "relative path");

    // Missing descendants are judged by the nearest existing ancestor, reached
    // only through ENOENT.
    fs.probed.clear();
    expect(classify("/srv/nfs/new/deeper"), Storage::remote, "missing descendants of nfs");
    check(fs.probed == std::vector<std::string>{"/srv/nfs/new/deeper", "/srv/nfs/new", "/srv/nfs"},
          "ancestor walk did not stop at the first existing directory");
    expect(classify("/mnt/ssh/new/deeper"), Storage::remote, "missing descendants of sshfs");

    fs.types.erase("/");
    expect(classify("/nothing/exists"), Storage::unknown, "no existing ancestor");
    fs.types["/"] = ext4;

    fs.mountinfo_readable = false;
    expect(classify("/mnt/ssh/disk"), Storage::unknown, "mountinfo unavailable");
    fs.mountinfo_readable = true;
    fs.resolvable = false;
    expect(classify("/mnt/ssh/disk"), Storage::unknown, "unresolvable FUSE path");
    fs.resolvable = true;

    // A later mount over the same point shadows the earlier one.
    fs.mountinfo += mount_line("/mnt/ssh/disk", "fuse.sshfs", "user@host:/disk");
    expect(classify("/mnt/ssh/disk"), Storage::remote, "shadowed mount");
}
#endif

// The real system half: the temporary directory, and a path that does not
// exist yet beneath it, are positively local.
void classifying_local_storage(const std::filesystem::path& dir) {
    expect(platform::classify_storage(dir), platform::Storage::local, "temporary directory");
    expect(platform::classify_storage(dir / "not-created-yet" / "deeper"), platform::Storage::local,
           "missing path under the temporary directory");
}

void locking_within_a_process(const std::filesystem::path& dir) {
    auto path = dir / "process.lock";
    platform::LockFailure why{};
    auto first = platform::acquire_lock(path, &why);
    check(first != platform::invalid_lock && why == platform::LockFailure::none, "first lock failed");
    auto second = platform::acquire_lock(path, &why);
    check(second == platform::invalid_lock && why == platform::LockFailure::busy,
          "second lock was not reported busy");
    platform::release_lock(first);
    auto third = platform::acquire_lock(path, &why);
    check(third != platform::invalid_lock, "lock not available after release");
    platform::release_lock(third);
}

#ifndef _WIN32
// Child processes report over a pipe and wait for the parent's go-ahead, so each
// step of a scenario happens in a fixed order across real processes.
struct Child {
    pid_t pid = -1;
    int to_child = -1, from_child = -1;
};
void send_byte(int fd, char c) { check(::write(fd, &c, 1) == 1, "pipe write failed"); }
char read_byte(int fd) {
    char c = 0;
    check(::read(fd, &c, 1) == 1, "pipe read failed");
    return c;
}
template <class Body> Child spawn(Body body) {
    int down[2], up[2];
    check(::pipe(down) == 0 && ::pipe(up) == 0, "pipe failed");
    Child child;
    child.pid = ::fork();
    check(child.pid != -1, "fork failed");
    if (child.pid == 0) {
        ::close(down[1]);
        ::close(up[0]);
        int code = 1;
        try { code = body(down[0], up[1]); } catch (...) {}
        ::_exit(code);
    }
    ::close(down[0]);
    ::close(up[1]);
    child.to_child = down[1];
    child.from_child = up[0];
    return child;
}
int reap(Child& child) {
    ::close(child.to_child);
    ::close(child.from_child);
    int status = 0;
    ::waitpid(child.pid, &status, 0);
    return status;
}

// The reviewer's reproduction: B opens the existing lock file before A releases
// it, then locks its descriptor once A is gone. C, arriving while B holds the
// lock, must be refused; when release unlinked the file, C created and locked a
// fresh inode and both owned the directory.
void excluding_a_preopened_contender(const std::filesystem::path& dir) {
    auto path = dir / "preopened.lock";
    auto a = platform::acquire_lock(path);
    check(a != platform::invalid_lock, "A could not lock");
    auto b = spawn([&](int in, int out) {
        // fork duplicated A's descriptor; a flock belongs to the open file
        // description, so B must drop its copy to be a separate contender.
        ::close(a);
        int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (fd == -1) return 2;
        send_byte(out, 'o');          // opened, not yet locked
        read_byte(in);                // wait for A's release
        if (::flock(fd, LOCK_EX | LOCK_NB) != 0) return 3;
        send_byte(out, 'l');          // B holds the lock
        read_byte(in);                // hold until C has tried
        ::close(fd);
        return 0;
    });
    check(read_byte(b.from_child) == 'o', "B did not open");
    platform::release_lock(a);
    send_byte(b.to_child, 'r');
    check(read_byte(b.from_child) == 'l', "B could not lock after A released");

    auto c = spawn([&](int, int out) {
        platform::LockFailure why{};
        auto fd = platform::acquire_lock(path, &why);
        send_byte(out, fd == platform::invalid_lock && why == platform::LockFailure::busy ? 'b' : 'X');
        return 0;
    });
    check(read_byte(c.from_child) == 'b', "C acquired the lock while B held it: exclusion broken");
    reap(c);
    send_byte(b.to_child, 'q');
    check(WIFEXITED(reap(b)), "B did not exit cleanly");
    check(std::filesystem::is_regular_file(path), "lock file was removed");
}

// The kernel drops a flock when its holder exits or is killed, and the file
// keeps its inode, so a restarted REAPER locks the same object as everyone else.
void restarting_after_exit_and_crash(const std::filesystem::path& dir) {
    auto path = dir / "restart.lock";
    struct stat before{}, after{};
    for (bool crash : {false, true}) {
        auto holder = spawn([&](int in, int out) {
            auto fd = platform::acquire_lock(path);
            send_byte(out, fd == platform::invalid_lock ? 'X' : 'h');
            read_byte(in);
            if (crash) ::raise(SIGKILL);
            ::_exit(0);  // exits without release_lock, as a crashed host would
            return 0;
        });
        check(read_byte(holder.from_child) == 'h', "holder could not lock");
        check(::lstat(path.c_str(), &before) == 0, "lock file missing while held");
        platform::LockFailure why{};
        check(platform::acquire_lock(path, &why) == platform::invalid_lock &&
                  why == platform::LockFailure::busy, "lock not exclusive while held by another process");
        send_byte(holder.to_child, 'x');
        auto status = reap(holder);
        check(crash ? WIFSIGNALED(status) : WIFEXITED(status), "holder did not end as intended");
        auto lock = platform::acquire_lock(path);
        check(lock != platform::invalid_lock,
              crash ? "lock not recovered after a crash" : "lock not recovered after exit");
        check(::lstat(path.c_str(), &after) == 0 && after.st_ino == before.st_ino,
              "lock file inode changed across restart");
        platform::release_lock(lock);
    }
}

// Same-user files planted at the lock path are refused rather than followed or
// opened; a FIFO must not block the REAPER main thread.
void refusing_unsafe_lock_files(const std::filesystem::path& dir) {
    auto target = dir / "victim.txt";
    std::ofstream(target) << "keep";
    auto link = dir / "link.lock";
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    check(!ec, "symlink creation failed");
    platform::LockFailure why{};
    check(platform::acquire_lock(link, &why) == platform::invalid_lock &&
              why == platform::LockFailure::unsafe, "symlinked lock accepted");
    int probe = ::open(target.c_str(), O_RDONLY);
    check(probe != -1 && ::flock(probe, LOCK_EX | LOCK_NB) == 0, "symlink target was locked");
    ::close(probe);

    auto dangling = dir / "dangling.lock";
    std::filesystem::create_symlink(dir / "absent", dangling, ec);
    check(!ec, "dangling symlink creation failed");
    check(platform::acquire_lock(dangling, &why) == platform::invalid_lock &&
              why == platform::LockFailure::unsafe, "dangling symlinked lock accepted");
    check(!std::filesystem::exists(dir / "absent"), "dangling symlink target was created");

    auto fifo = dir / "fifo.lock";
    check(::mkfifo(fifo.c_str(), 0600) == 0, "mkfifo failed");
    check(platform::acquire_lock(fifo, &why) == platform::invalid_lock &&
              why == platform::LockFailure::unsafe, "FIFO lock accepted");

    auto directory = dir / "directory.lock";
    std::filesystem::create_directory(directory);
    check(platform::acquire_lock(directory, &why) == platform::invalid_lock, "directory lock accepted");
}
#endif
void reporting_identity() {
    check(platform::process_id() > 0, "process id not positive");
    auto first = platform::tick_count();
    auto second = platform::tick_count();
    check(second >= first, "tick count went backwards");
}
}  // namespace

// Helper for cross-language tests: holds the native lock on a path until stdin
// closes, so the Python installer can be checked against the real native code.
int hold_lock(const char* path) {
    rmcp::platform::LockFailure why{};
    auto lock = rmcp::platform::acquire_lock(path, &why);
    const char* states[] = {"held", "busy", "unsafe", "io"};
    std::cout << states[static_cast<int>(why)] << std::endl;
    std::string line;
    std::getline(std::cin, line);
    rmcp::platform::release_lock(lock);
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "hold-lock") return hold_lock(argv[2]);
    auto dir = std::filesystem::temp_directory_path() /
               ("rmcp-platform-test-" + std::to_string(rmcp::platform::process_id()));
    std::filesystem::create_directories(dir);
    try {
        surviving_a_closed_peer();
        inspecting_paths(dir);
#ifndef _WIN32
        classifying_injected_storage();
#endif
        classifying_local_storage(dir);
        locking_within_a_process(dir);
#ifndef _WIN32
        excluding_a_preopened_contender(dir);
        restarting_after_exit_and_crash(dir);
        refusing_unsafe_lock_files(dir);
#endif
        reporting_identity();
        std::filesystem::remove_all(dir);
        std::cout << "Platform primitive checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(dir);
        return 1;
    }
}
