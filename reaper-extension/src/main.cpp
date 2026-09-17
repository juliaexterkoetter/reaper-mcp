#include "bridge.hpp"
#define REAPERAPI_IMPLEMENT
#include "api.hpp"

namespace {
reaper_plugin_info_t* host = nullptr;
rmcp::Bridge bridge;
rmcp::json dispatch(const std::string& method, const rmcp::json&) {
    if (method == "bridge.info") return {{"protocol_version",1},
        {"extension_version","0.1.0a1"},{"reaper_version",GetAppVersion()},
        {"capabilities",rmcp::json::array()}};
    throw rmcp::Error("METHOD_NOT_FOUND", "Unknown method");
}
void tick() noexcept {
    try { bridge.tick(); } catch (...) { bridge.stop(); }
}
}
extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
    REAPER_PLUGIN_HINSTANCE, reaper_plugin_info_t* info) {
    if (!info) {
        if (host) host->Register("-timer", reinterpret_cast<void*>(tick));
        bridge.stop(); host = nullptr; return 0;
    }
    if (info->caller_version != REAPER_PLUGIN_VERSION || !info->GetFunc ||
        !info->Register || REAPERAPI_LoadAPI(info->GetFunc)) return 0;
    try {
        bridge.dispatch = dispatch;
        bridge.start(std::filesystem::u8path(GetResourcePath()) / "ReaperMCP");
        host = info;
        if (!host->Register("timer", reinterpret_cast<void*>(tick))) {
            bridge.stop(); host = nullptr; return 0;
        }
        return 1;
    } catch (...) { bridge.stop(); return 0; }
}
