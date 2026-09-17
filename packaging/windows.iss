#define AppVersion "0.1.0-alpha.1"
[Setup]
AppId={{7C563B9B-458B-47FB-B778-838B125CBE13}
AppName=REAPER MCP
AppVersion={#AppVersion}
AppPublisher=Julia Exterkoetter
AppPublisherURL=https://github.com/juliaexterkoetter/reaper-mcp
DefaultDirName={localappdata}\Programs\ReaperMCP
DefaultGroupName=REAPER MCP
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist\release
OutputBaseFilename=reaper-mcp-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE
CloseApplications=no
ChangesEnvironment=yes
UninstallDisplayIcon={app}\reaper-mcp.exe

[Files]
Source: "..\dist\reaper-mcp\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\REAPER MCP diagnostic"; Filename: "{cmd}"; Parameters: "/K """"{app}\reaper-mcp.exe"" doctor"""
Name: "{group}\REAPER MCP documentation"; Filename: "https://github.com/juliaexterkoetter/reaper-mcp"

[Code]
var Paths: TInputQueryWizardPage;

procedure InitializeWizard;
begin
  Paths := CreateInputQueryPage(wpSelectDir, 'REAPER integration',
    'Close REAPER before continuing.',
    'Leave blank for automatic detection. For portable REAPER, select its folder path. Codex CLI must be on PATH.');
  Paths.Add('REAPER installation folder (optional):', False);
  Paths.Add('REAPER resource folder (optional):', False);
  Paths.Values[0] := ExpandConstant('{param:REAPERPATH|}');
  Paths.Values[1] := ExpandConstant('{param:RESOURCEDIR|}');
end;

procedure AddCommandPath;
var Value, AppPath: String;
begin
  AppPath := ExpandConstant('{app}');
  RegQueryStringValue(HKCU, 'Environment', 'Path', Value);
  if Pos(';' + Uppercase(AppPath) + ';', ';' + Uppercase(Value) + ';') = 0 then begin
    if Value <> '' then Value := Value + ';';
    if not RegWriteExpandStringValue(HKCU, 'Environment', 'Path', Value + AppPath) then
      RaiseException('Cannot add the command to the user PATH.');
    RegWriteStringValue(HKCU, 'Software\ReaperMCP', 'OwnedPath', AppPath);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var Value, Owned: String; Index: Integer;
begin
  if CurUninstallStep = usPostUninstall then begin
    if RegQueryStringValue(HKCU, 'Software\ReaperMCP', 'OwnedPath', Owned) then begin
      RegQueryStringValue(HKCU, 'Environment', 'Path', Value);
      Value := ';' + Value + ';';
      Index := Pos(';' + Uppercase(Owned) + ';', Uppercase(Value));
      if Index > 0 then Delete(Value, Index, Length(Owned) + 1);
      if (Length(Value) > 0) and (Value[1] = ';') then Delete(Value, 1, 1);
      if (Length(Value) > 0) and (Value[Length(Value)] = ';') then Delete(Value, Length(Value), 1);
      RegWriteExpandStringValue(HKCU, 'Environment', 'Path', Value);
      RegDeleteValue(HKCU, 'Software\ReaperMCP', 'OwnedPath');
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var Args: String; Code: Integer;
begin
  if CurStep = ssPostInstall then begin
    Args := 'install';
    if Paths.Values[0] <> '' then Args := Args + ' --reaper-path "' + Paths.Values[0] + '"';
    if Paths.Values[1] <> '' then Args := Args + ' --resource-dir "' + Paths.Values[1] + '"';
    if ExpandConstant('{param:SKIPCODEX|0}') = '1' then Args := Args + ' --skip-codex';
    if not Exec(ExpandConstant('{app}\reaper-mcp.exe'), Args, '', SW_SHOW, ewWaitUntilTerminated, Code) then
      RaiseException('Cannot start REAPER MCP installer.');
    if Code <> 0 then
      RaiseException('REAPER integration failed. Application files remain available for repair. Run reaper-mcp install in the installed folder to see the cause.');
    AddCommandPath;
  end;
end;

function InitializeUninstall(): Boolean;
var Code: Integer;
begin
  Result := Exec(ExpandConstant('{app}\reaper-mcp.exe'), 'uninstall', '', SW_SHOW, ewWaitUntilTerminated, Code);
  if Result then Result := Code = 0;
  if not Result then MsgBox('Integration removal failed. Close REAPER and run reaper-mcp uninstall to see the cause. Application files were preserved.', mbError, MB_OK);
end;
