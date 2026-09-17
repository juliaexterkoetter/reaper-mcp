#define REAPERAPI_IMPLEMENT
#include "api.hpp"

namespace {
reaper_plugin_info_t* host = nullptr;
void tick() noexcept {}
}

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(
    REAPER_PLUGIN_HINSTANCE, reaper_plugin_info_t* info) {
    if (!info) {
        if (host) host->Register("-timer", reinterpret_cast<void*>(tick));
        host = nullptr;
        return 0;
    }
    if (info->caller_version != REAPER_PLUGIN_VERSION || !info->GetFunc ||
        !info->Register || REAPERAPI_LoadAPI(info->GetFunc)) return 0;
    host = info;
    if (!host->Register("timer", reinterpret_cast<void*>(tick))) {
        host = nullptr;
        return 0;
    }
    return 1;
}
