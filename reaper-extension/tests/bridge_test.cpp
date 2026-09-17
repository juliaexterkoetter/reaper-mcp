#include "bridge.hpp"
#include <iostream>

int main() {
    using namespace rmcp;
    auto dir = std::filesystem::temp_directory_path() / ("rmcp-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(dir);
    try {
        std::ofstream(dir / "config.json") << json{{"token",std::string(64,'a')},{"policy","read-only"}};
        Bridge bridge;
        bridge.dispatch = [](const std::string& method, const json&) -> json {
            if (method == "ping") return {{"ok",true}};
            throw Error("METHOD_NOT_FOUND", "Unknown method");
        };
        std::ofstream(dir / "bridge.json") << "stale discovery";
        bridge.start(dir);
        { Bridge other;
          try { other.start(dir); throw std::runtime_error("second instance accepted"); }
          catch(const Error& e) { if(e.code!="INSTANCE_CONFLICT")throw; }
        }
        if(!std::filesystem::exists(dir / "bridge.json"))throw std::runtime_error("other instance removed discovery");
        auto request = json{{"jsonrpc","2.0"},{"id","1"},{"method","ping"},
            {"params",json::object()},{"auth",std::string(64,'a')}};
        auto result = json::parse(bridge.respond(request.dump()));
        if (!result.at("result").at("ok")) throw std::runtime_error("ping failed");
        request["auth"] = "wrong";
        result = json::parse(bridge.respond(request.dump()));
        if (result.at("error").at("data").at("code") != "PERMISSION_DENIED")
            throw std::runtime_error("auth bypass");
        if (!json::parse(bridge.respond("[]")).contains("error")) throw std::runtime_error("batch accepted");
        if (!json::parse(bridge.respond("garbage")).contains("error")) throw std::runtime_error("bad JSON accepted");
        std::string deep(1000,'[');deep+=std::string(1000,']');
        if(!json::parse(bridge.respond(deep)).contains("error"))throw std::runtime_error("deep JSON accepted");
        bridge.stop();
        if (std::filesystem::exists(dir / "bridge.json")) throw std::runtime_error("discovery leaked");
        std::filesystem::remove_all(dir);
        std::cout << "Native bridge checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(dir);
        return 1;
    }
}
