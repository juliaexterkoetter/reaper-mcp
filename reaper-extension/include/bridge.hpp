#pragma once
#include "platform.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace rmcp {
using json = nlohmann::json;
struct Error : std::runtime_error {
    std::string code;
    Error(std::string c, std::string m) : std::runtime_error(m), code(std::move(c)) {}
};
constexpr size_t max_frame = 1024 * 1024;
class Bridge {
    struct Peer {
        platform::socket_t socket = platform::invalid_socket;
        std::string input, output;
        size_t sent = 0;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    };
    platform::socket_t listener = platform::invalid_socket;
    platform::lock_t instance_lock = platform::invalid_lock;
    std::vector<Peer> peers;
    std::filesystem::path discovery, lock_path;
    std::string token;
    bool networking = false;
public:
    std::string policy;
    std::function<json(const std::string&, const json&)> dispatch;
    ~Bridge() { stop(); }
    void stop() noexcept {
        for (auto& p : peers) platform::close_socket(p.socket);
        peers.clear();
        if (listener != platform::invalid_socket) platform::close_socket(listener);
        listener = platform::invalid_socket;
        if (!discovery.empty()) {
            std::error_code ec;
            std::filesystem::remove(discovery, ec);
            discovery.clear();
        }
        platform::release_lock(instance_lock, lock_path);
        lock_path.clear();
        if (networking) platform::cleanup();
        networking = false;
    }
    void start(const std::filesystem::path& root) {
        std::ifstream file(root / "config.json");
        auto config = json::parse(file);
        token = config.at("token").get<std::string>();
        policy = config.at("policy").get<std::string>();
        if (token.size() != 64 || token.find_first_not_of("0123456789abcdef") != std::string::npos ||
            (policy != "read-only" && policy != "confirm-destructive" && policy != "full-control"))
            throw Error("INVALID_CONFIGURATION", "Invalid configuration");
        // A single exclusive lock serializes instances and installers. The OS
        // releases it after crashes, so stale discovery can safely be replaced.
        lock_path = root / "instance.lock";
        instance_lock = platform::acquire_lock(lock_path);
        if (instance_lock == platform::invalid_lock)
            throw Error("INSTANCE_CONFLICT", "Another REAPER instance or installer owns this resource directory");
        std::error_code ec;
        std::filesystem::remove(root / "bridge.json", ec);
        if(ec) throw Error("DISCOVERY_ERROR", "Cannot remove stale discovery");
        if (!platform::startup()) throw Error("NETWORK_ERROR", "Socket startup failed");
        networking = true;
        listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == platform::invalid_socket) throw Error("NETWORK_ERROR", "socket failed");
        if (!platform::set_nonblocking(listener)) throw Error("NETWORK_ERROR", "nonblocking failed");
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) || ::listen(listener, 8))
            throw Error("NETWORK_ERROR", "bind failed");
        platform::socklen_arg size = sizeof(addr);
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &size))
            throw Error("NETWORK_ERROR", "getsockname failed");
        discovery = root / "bridge.json";
        auto temp = root / "bridge.tmp";
        std::ofstream out(temp, std::ios::binary);
        out << json{{"protocol_version",1},{"pid",platform::process_id()},
                    {"port",ntohs(addr.sin_port)}}.dump();
        out.close();
        if (!out) throw Error("DISCOVERY_ERROR", "Cannot write discovery");
        std::filesystem::rename(temp, discovery);
    }
    std::string respond(const std::string& input) {
        json id = nullptr;
        try {
            if(input.size()>max_frame) throw Error("REQUEST_TOO_LARGE", "Request exceeds size limit");
            auto req = json::parse(input, [](int depth, json::parse_event_t, json&) {
                if(depth>32) throw Error("INVALID_REQUEST", "JSON nesting exceeds 32 levels");
                return true;
            });
            if (!req.is_object() || req.value("jsonrpc", "") != "2.0" ||
                !req.contains("id") || !req["id"].is_string() ||
                !req.contains("params") || !req["params"].is_object())
                throw Error("INVALID_REQUEST", "Expected JSON-RPC request with object params and string id");
            id = req["id"];
            if (req.value("auth", "") != token) throw Error("PERMISSION_DENIED", "Authentication failed");
            const auto method = req.at("method").get<std::string>();
            auto result = dispatch(method, req["params"]);
            auto response = json{{"jsonrpc","2.0"},{"id",id},{"result",result}}.dump() + "\n";
            if (response.size() > max_frame) throw Error("RESPONSE_TOO_LARGE", "Narrow the query");
            return response;
        } catch (const Error& e) {
            return json{{"jsonrpc","2.0"},{"id",id},{"error",{{"code",-32000},
                {"message",e.what()},{"data",{{"code",e.code}}}}}}.dump()+"\n";
        } catch (...) {
            return json{{"jsonrpc","2.0"},{"id",id},{"error",{{"code",-32602},
                {"message","Invalid request or parameters"},{"data",{{"code","INVALID_PARAMETER"}}}}}}.dump()+"\n";
        }
    }
    void tick() {
        if (listener == platform::invalid_socket) return;
        // Work bounded to 8 connections and 64 KiB per connection per timer tick.
        if (peers.size() < 8) {
            platform::socket_t s = ::accept(listener, nullptr, nullptr);
            if (s != platform::invalid_socket) {
                if (!platform::set_nonblocking(s)) platform::close_socket(s);
                else { platform::suppress_sigpipe(s); peers.emplace_back().socket = s; }
            }
        }
        for (auto it = peers.begin(); it != peers.end();) {
            bool close = std::chrono::steady_clock::now()-it->start > std::chrono::seconds(2);
            if (!close && it->output.empty()) {
                char buf[65536];
                auto n = ::recv(it->socket, buf, static_cast<platform::iolen_t>(sizeof(buf)), 0);
                if (n > 0) {
                    it->input.append(buf, static_cast<size_t>(n));
                    if (it->input.size() > max_frame) close = true;
                    else if (it->input.find('\n') != std::string::npos)
                        it->output = respond(it->input.substr(0,it->input.find('\n')));
                } else if (!n || !platform::would_block()) close = true;
            }
            if (!close && !it->output.empty()) {
                auto n = ::send(it->socket, it->output.data()+it->sent,
                    static_cast<platform::iolen_t>(std::min(size_t(65536),it->output.size()-it->sent)),
                    platform::send_flags);
                if (n > 0) it->sent += static_cast<size_t>(n);
                else if (!n || !platform::would_block()) close = true;
                if (it->sent == it->output.size()) close = true;
            }
            if (close) { platform::close_socket(it->socket); it = peers.erase(it); }
            else ++it;
        }
    }
};
}
