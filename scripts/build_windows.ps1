$ErrorActionPreference = 'Stop'
python -m pip install -c packaging/constraints.txt 'pyinstaller==6.22.3'
if ($LASTEXITCODE) { throw 'PyInstaller installation failed' }
python scripts/collect_licenses.py build/licenses
if ($LASTEXITCODE) { throw 'License collection failed' }
$BundleNative = (Resolve-Path dist/native).Path
$BundleLicense = (Resolve-Path LICENSE).Path
$BundleNotices = (Resolve-Path build/licenses).Path
python -m PyInstaller --noconfirm --clean --onedir --console --noupx --name reaper-mcp --specpath build --paths src --collect-submodules mcp.server --collect-submodules mcp.types --collect-submodules reaper_mcp --recursive-copy-metadata mcp --add-data "${BundleNative}:native" --add-data "${BundleLicense}:." --add-data "${BundleNotices}:licenses" src/reaper_mcp/cli.py
if ($LASTEXITCODE) { throw 'Executable build failed' }
Copy-Item README.md dist/reaper-mcp/README.md
Copy-Item LICENSE dist/reaper-mcp/LICENSE
python scripts/smoke_bundle.py dist/reaper-mcp/reaper-mcp.exe
if ($LASTEXITCODE) { throw 'Frozen executable smoke failed' }
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" packaging/windows.iss
if ($LASTEXITCODE) { throw 'Installer build failed' }
python scripts/smoke_bundle.py dist/reaper-mcp/reaper-mcp.exe dist/release/reaper-mcp-0.1.0-alpha.1-windows-x64-setup.exe
if ($LASTEXITCODE) { throw 'Setup install/uninstall smoke failed' }
Compress-Archive -Path dist/reaper-mcp -DestinationPath dist/release/reaper-mcp-0.1.0-alpha.1-windows-x64.zip -Force
Copy-Item dist/native/reaper_mcp.dll dist/release/reaper_mcp.dll
python -m build
if ($LASTEXITCODE) { throw 'Python distribution build failed' }
Copy-Item dist/*.whl dist/release/
Copy-Item dist/*.tar.gz dist/release/
python -m pip freeze | Set-Content dist/release/build-dependencies.txt
Get-ChildItem dist/release -File | ForEach-Object { "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name } | Set-Content dist/release/SHA256SUMS.txt
