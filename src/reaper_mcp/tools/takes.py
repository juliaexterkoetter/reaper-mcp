from reaper_mcp.tools.common import Spec
from reaper_mcp.tools.items import Item

SPECS = [
    Spec("reaper_list_takes", "takes.list", Item, "read", "List all takes in an item."),
    Spec(
        "reaper_get_active_take",
        "takes.active",
        Item,
        "read",
        "Get the active take, or null for an empty item.",
    ),
]
