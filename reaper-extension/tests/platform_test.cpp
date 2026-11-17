#include "platform.hpp"
#include <fstream>
#include <iostream>
#include <string>

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

// A real network mount cannot be assumed in CI, so this pins the half that can
// be: local storage is accepted, and a path that does not exist yet is judged
// by the mount it would be created on rather than defaulting to "unknown".
void classifying_local_storage(const std::filesystem::path& dir) {
    check(!platform::is_network_path(dir), "temporary directory classified as remote");
    check(!platform::is_network_path(dir / "not-created-yet" / "deeper"),
          "missing path under local storage classified as remote");
}

void reporting_identity() {
    check(platform::process_id() > 0, "process id not positive");
    auto first = platform::tick_count();
    auto second = platform::tick_count();
    check(second >= first, "tick count went backwards");
}
}  // namespace

int main() {
    auto dir = std::filesystem::temp_directory_path() /
               ("rmcp-platform-test-" + std::to_string(rmcp::platform::process_id()));
    std::filesystem::create_directories(dir);
    try {
        surviving_a_closed_peer();
        inspecting_paths(dir);
        classifying_local_storage(dir);
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
