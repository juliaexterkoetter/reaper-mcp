from reaper_mcp.tools.common import Params, Spec

SPECS = [
    Spec(
        "reaper_get_environment",
        "environment.get",
        Params,
        "read",
        "Read REAPER versions, current project, device sample rate and resource directories.",
    ),
    Spec(
        "reaper_list_resource_presets",
        "environment.presets",
        Params,
        "read",
        "Discover FX chain and track template files without loading or overwriting them.",
    ),
]
