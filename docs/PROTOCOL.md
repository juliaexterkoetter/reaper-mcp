# Internal protocol v1

Separate from the MCP wire protocol handled by the official SDK.
One request/response per TCP connection, UTF-8 JSON terminated by LF, maximum
1 MiB including LF. IPv4 127.0.0.1 only; ephemeral port from bridge.json.
Discovery schema: `{"protocol_version":1,"port":12345,"pid":123}`.
Private config.json contains a 256-bit random hexadecimal `token` and `policy`.

Request: `{"jsonrpc":"2.0","id":"uuid","method":"tracks.list","params":{},"auth":"TOKEN"}`.
Response: `{"jsonrpc":"2.0","id":"uuid","result":[]}`.
Error: `{"jsonrpc":"2.0","id":"uuid","error":{"code":-32000,"message":"Track missing","data":{"code":"TRACK_NOT_FOUND"}}}`.
JSON-RPC error codes are integers; application codes live in error.data.code.
Notifications and batches are unsupported. IDs must be strings. Non-finite
numbers are forbidden. Unknown methods fail closed. No raw REAPER action tool.

`bridge.info` negotiates protocol_version, extension_version, reaper_version
and explicit method capabilities. Incompatible versions fail before mutation.
Client timeout: 5 seconds; no automatic retries. After timeout, outcome may be
unknown: inspect state before editing again. Server closes idle clients after
2 seconds. A client disconnect does not roll back an executed edit.

Track/item/FX selectors use GUIDs, tracks may also use exact names; duplicate
names return AMBIGUOUS_TRACK. Indices are only used for API-specific parameters
and validated against fresh counts. Operations target the current project;
clients must inspect again after tab changes. Mutations form native Undo blocks.
