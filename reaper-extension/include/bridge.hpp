#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
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
        SOCKET socket;
        std::string input, output;
        size_t sent = 0;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    };
    SOCKET listener = INVALID_SOCKET;
    HANDLE instance_lock = INVALID_HANDLE_VALUE;
    std::vector<Peer> peers;
    std::filesystem::path discovery;
    std::string token;
    bool winsock = false;
public:
    std::string policy;
    std::function<json(const std::string&, const json&)> dispatch;
    ~Bridge() { stop(); }
    void stop() noexcept {
        for (auto& p : peers) closesocket(p.socket);
        peers.clear();
        if (listener != INVALID_SOCKET) closesocket(listener);
        listener = INVALID_SOCKET;
        if (!discovery.empty()) {
            std::error_code ec;
            std::filesystem::remove(discovery, ec);
            discovery.clear();
        }
        if (instance_lock != INVALID_HANDLE_VALUE) CloseHandle(instance_lock);
        instance_lock = INVALID_HANDLE_VALUE;
        if (winsock) WSACleanup();
        winsock = false;
    }
    void start(const std::filesystem::path& root) {
        std::ifstream file(root / "config.json");
        auto config = json::parse(file);
        token = config.at("token").get<std::string>();
        policy = config.at("policy").get<std::string>();
        if (token.size() != 64 || token.find_first_not_of("0123456789abcdef") != std::string::npos ||
            (policy != "read-only" && policy != "confirm-destructive" && policy != "full-control"))
            throw Error("INVALID_CONFIGURATION", "Invalid configuration");
        // Share mode zero serializes instances and installers. The OS releases this
        // handle after crashes, so stale discovery can safely be replaced.
        instance_lock = CreateFileW((root / "instance.lock").c_str(), GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (instance_lock == INVALID_HANDLE_VALUE)
            throw Error("INSTANCE_CONFLICT", "Another REAPER instance or installer owns this resource directory");
        std::error_code ec;
        std::filesystem::remove(root / "bridge.json", ec);
        if(ec) throw Error("DISCOVERY_ERROR", "Cannot remove stale discovery");
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2,2), &data)) throw Error("NETWORK_ERROR", "WSAStartup failed");
        winsock = true;
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET) throw Error("NETWORK_ERROR", "socket failed");
        u_long nonblocking = 1;
        if (ioctlsocket(listener, FIONBIO, &nonblocking)) throw Error("NETWORK_ERROR", "nonblocking failed");
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) || listen(listener, 8))
            throw Error("NETWORK_ERROR", "bind failed");
        int size = sizeof(addr);
        if (getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &size))
            throw Error("NETWORK_ERROR", "getsockname failed");
        discovery = root / "bridge.json";
        auto temp = root / "bridge.tmp";
        std::ofstream out(temp, std::ios::binary);
        out << json{{"protocol_version",1},{"pid",GetCurrentProcessId()},
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
        if (listener == INVALID_SOCKET) return;
        // Work bounded to 8 connections and 64 KiB per connection per timer tick.
        if (peers.size() < 8) {
            SOCKET s = accept(listener, nullptr, nullptr);
            if (s != INVALID_SOCKET) {
                u_long mode = 1;
                if (ioctlsocket(s, FIONBIO, &mode)) closesocket(s);
                else peers.push_back(Peer{s});
            }
        }
        for (auto it = peers.begin(); it != peers.end();) {
            bool close = std::chrono::steady_clock::now()-it->start > std::chrono::seconds(2);
            if (!close && it->output.empty()) {
                char buf[65536];
                int n = recv(it->socket, buf, sizeof(buf), 0);
                if (n > 0) {
                    it->input.append(buf, n);
                    if (it->input.size() > max_frame) close = true;
                    else if (it->input.find('\n') != std::string::npos)
                        it->output = respond(it->input.substr(0,it->input.find('\n')));
                } else if (!n || WSAGetLastError() != WSAEWOULDBLOCK) close = true;
            }
            if (!close && !it->output.empty()) {
                int n = send(it->socket, it->output.data()+it->sent,
                    static_cast<int>(std::min(size_t(65536),it->output.size()-it->sent)), 0);
                if (n > 0) it->sent += n;
                else if (!n || WSAGetLastError() != WSAEWOULDBLOCK) close = true;
                if (it->sent == it->output.size()) close = true;
            }
            if (close) { closesocket(it->socket); it = peers.erase(it); }
            else ++it;
        }
    }
};
}
