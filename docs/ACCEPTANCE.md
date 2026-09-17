# Real Windows acceptance test

This test remains necessary: CI does not run REAPER or third-party plugins.
Use a disposable copy of a saved project, with no recording armed. The supplied
`tests/fixtures/acceptance.rpp` has two empty tracks and no media dependencies.
For item/FX/automation/render tests, add a short local audio file and explicitly
choose a plugin and existing envelope available in your installation.

1. Close REAPER, run the release Setup, then open REAPER 7.80+ x64.
2. Open and save a copy of the test project.
3. Open a new terminal and run `reaper-mcp doctor`. All three checks should be OK.
4. Restart Codex. Ask: “Liste todas as tracks do projeto aberto.” Verify actual names.
5. Ask to lower only `Voice` by 3 dB and pan it 10% left. Confirm the readback.
6. Ask to mute/unmute and solo/unsolo `Voice`. Verify the final flags are off.
7. Ask to list its FX. An empty list is correct for this fixture.
8. Ask for Undo after a single volume edit. Approve the destructive Undo request;
   verify the previous volume returns. Do not edit manually between mutation/Undo.
9. Ask to save the existing project, approve, close/reopen it and check values.
10. Close REAPER and run doctor: it should fail with an actionable bridge error.
    Reopen REAPER; doctor should recover without editing configuration.
11. Close REAPER, rerun Setup (idempotent), then uninstall. Verify unrelated files
    and project/media remain. Reinstall for normal use.

Extended acceptance: split/trim an audio item and Undo; inspect takes; add a
plugin discovered by the FX listing and change one normalized parameter; create
and delete a marker/region; edit an existing volume-envelope point and reject a
stale state version. Use returned GUIDs after edits.

Rendering is experimental. Configure a simple single master output in REAPER,
approve one render, wait for it to finish, then query render status and compare its job ID. Check the new
output directory, audition the file and confirm original settings were restored.
A nonempty file is not evidence that rendering completed without cancellation.

Optional developer read-only E2E:

```powershell
$env:REAPER_MCP_E2E = "1"
pytest tests/e2e -q
```

Report the release version, doctor output, and which steps passed. No passwords,
tokens, private audio or complete project files are needed for initial diagnosis.
