$ErrorActionPreference = 'Stop'
python -m pip install 'pyinstaller==6.22.3'
if ($LASTEXITCODE) { throw 'PyInstaller installation failed' }
python -m PyInstaller --noconfirm --clean --onedir --console --noupx --name reaper-mcp --specpath build --paths src --collect-all mcp --recursive-copy-metadata mcp --add-data 'dist/native:native' --add-data 'LICENSE:.' src/reaper_mcp/cli.py
if ($LASTEXITCODE) { throw 'Executable build failed' }
Copy-Item README.md dist/reaper-mcp/README.md
Copy-Item LICENSE dist/reaper-mcp/LICENSE
python scripts/smoke_bundle.py dist/reaper-mcp/reaper-mcp.exe
if ($LASTEXITCODE) { throw 'Frozen executable smoke failed' }
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" packaging/windows.iss
if ($LASTEXITCODE) { throw 'Installer build failed' }
Compress-Archive -Path dist/reaper-mcp -DestinationPath dist/release/reaper-mcp-0.1.0-alpha.1-windows-x64.zip -Force
Copy-Item dist/native/reaper_mcp.dll dist/release/reaper_mcp.dll
Get-ChildItem dist/release -File | ForEach-Object { "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name } | Set-Content dist/release/SHA256SUMS.txt
