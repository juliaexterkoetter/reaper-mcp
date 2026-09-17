#include "bridge.hpp"
#define REAPERAPI_IMPLEMENT
#include "api.hpp"
#include "adapter.hpp"
#include "tracks.hpp"
#include "items.hpp"
#include "takes.hpp"
#include "transport.hpp"
#include "fx.hpp"
#include "markers.hpp"

namespace {
reaper_plugin_info_t* host = nullptr;
rmcp::Bridge bridge;
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
        rmcp::add_project_operations();
        rmcp::add_track_operations();
        rmcp::add_item_operations();
        rmcp::add_take_operations();
        rmcp::add_transport_operations();
        rmcp::add_fx_operations();
        rmcp::add_marker_operations();
        bridge.dispatch = [](const std::string& m,const rmcp::json& p) { return rmcp::dispatch(m,p,bridge.policy); };
        bridge.start(std::filesystem::u8path(GetResourcePath()) / "ReaperMCP");
        host = info;
        if (!host->Register("timer", reinterpret_cast<void*>(tick))) {
            bridge.stop(); host = nullptr; return 0;
        }
        return 1;
    } catch (...) { bridge.stop(); return 0; }
}
