$ErrorActionPreference = 'Stop'

$Version = if ($env:REAPER_MCP_VERSION) {
    $env:REAPER_MCP_VERSION
} elseif ($env:GITHUB_REF_NAME -and $env:GITHUB_REF_NAME.StartsWith('v')) {
    $env:GITHUB_REF_NAME.Substring(1)
} else {
    '0.1.0-alpha.1'
}

$ReleasePrefix = "reaper-mcp-$Version-windows-x64"

python -m pip install -c packaging/constraints.txt 'pyinstaller==6.22.3'
if ($LASTEXITCODE) { throw 'PyInstaller installation failed' }

npm install -g @anthropic-ai/mcpb@2.1.2
if ($LASTEXITCODE) { throw 'MCPB CLI installation failed' }

python scripts/collect_licenses.py build/licenses
if ($LASTEXITCODE) { throw 'License collection failed' }

$BundleNative = (Resolve-Path dist/native).Path
$BundleLicense = (Resolve-Path LICENSE).Path
$BundleNotices = (Resolve-Path build/licenses).Path

python -m PyInstaller --noconfirm --clean --onedir --console --noupx --name reaper-mcp --specpath build --paths src --collect-submodules mcp.server --collect-submodules mcp.types --collect-submodules reaper_mcp --recursive-copy-metadata mcp --add-data "${BundleNative}:native" --add-data "${BundleLicense}:." --add-data "${BundleNotices}:licenses" src/reaper_mcp/cli.py
if ($LASTEXITCODE) { throw 'Executable build failed' }

Get-ChildItem -Path *.md | Copy-Item -Destination dist/reaper-mcp/
Copy-Item docs dist/reaper-mcp/docs -Recurse
Copy-Item LICENSE dist/reaper-mcp/LICENSE

python scripts/smoke_bundle.py dist/reaper-mcp/reaper-mcp.exe
if ($LASTEXITCODE) { throw 'Frozen executable smoke failed' }

& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" "/DAppVersion=$Version" packaging/windows.iss
if ($LASTEXITCODE) { throw 'Installer build failed' }

$Setup = "dist/release/$ReleasePrefix-setup.exe"

python scripts/smoke_bundle.py dist/reaper-mcp/reaper-mcp.exe $Setup
if ($LASTEXITCODE) { throw 'Setup install/uninstall smoke failed' }

$Zip = "dist/release/$ReleasePrefix.zip"

Compress-Archive -Path dist/reaper-mcp -DestinationPath $Zip -Force

$Mcpb = "dist/release/$ReleasePrefix.mcpb"

python scripts/build_mcpb.py --bundle dist/reaper-mcp --manifest packaging/mcpb/manifest.json --output $Mcpb --version $Version
if ($LASTEXITCODE) { throw 'MCPB build failed' }

Copy-Item dist/native/reaper_mcp.dll dist/release/reaper_mcp.dll

Get-Content dist/native/licenses/REAPER-SDK.txt, dist/native/licenses/nlohmann-json.txt |
    Set-Content dist/release/NATIVE-LICENSES.txt

python -m build
if ($LASTEXITCODE) { throw 'Python distribution build failed' }

Copy-Item dist/*.whl dist/release/
Copy-Item dist/*.tar.gz dist/release/

python -m pip freeze |
    Set-Content dist/release/build-dependencies.txt

Get-ChildItem dist/release -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' } |
    ForEach-Object {
        "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
    } |
    Set-Content dist/release/SHA256SUMS.txt
