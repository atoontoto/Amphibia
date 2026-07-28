; Amphibia Windows x64 installer.
; Build only through scripts/package-windows.ps1 so all inputs are validated.

#ifndef MyAppVersion
  #error MyAppVersion must be supplied by the packaging script
#endif
#ifndef StageRoot
  #error StageRoot must be supplied by the packaging script
#endif
#ifndef ReleaseOutputDir
  #error ReleaseOutputDir must be supplied by the packaging script
#endif

#define MyAppName "Amphibia"
#define MyPublisher "Amphibia Project"
#define MyAppId "802D7F30-6B3A-44C3-B809-93DA217821CA"
#define MyUninstallKey "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{" + MyAppId + "}_is1"
#define MyArtifactName "Amphibia-" + MyAppVersion + "-Windows-Setup"

[Setup]
AppId={{{#MyAppId}}
AppName=Amphibia
AppVerName={#MyAppName} {#MyAppVersion}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
AppCopyright=Copyright (C) 2022 Steven Atkinson; Amphibia modifications 2026 Amphibia contributors
AppContact=
AppPublisherURL=
AppSupportURL=
AppUpdatesURL=
DefaultDirName={autopf}\Amphibia
DefaultGroupName=Amphibia
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
UsePreviousGroup=yes
UsePreviousSetupType=yes
UsePreviousTasks=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.10240
ChangesAssociations=no
ChangesEnvironment=no
CloseApplications=yes
CloseApplicationsFilter=Amphibia.exe,Amphibia.vst3
RestartApplications=no
RestartIfNeededByRun=no
AllowNetworkDrive=no
AllowUNCPath=no
AllowRootDirectory=no
Compression=lzma2
SolidCompression=yes
ShowComponentSizes=yes
SetupLogging=yes
UninstallLogging=yes
UninstallDisplayName={#MyAppName} {#MyAppVersion}
OutputDir={#ReleaseOutputDir}
OutputBaseFilename={#MyArtifactName}
LicenseFile={#StageRoot}\LICENSE.txt
InfoBeforeFile={#StageRoot}\PRIVACY.txt
VersionInfoVersion={#MyAppVersion}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoTextVersion={#MyAppVersion}
VersionInfoProductTextVersion={#MyAppVersion}
VersionInfoCompany={#MyPublisher}
VersionInfoDescription=Amphibia Setup
VersionInfoProductName={#MyAppName}
VersionInfoOriginalFileName={#MyArtifactName}.exe
VersionInfoCopyright=Copyright (C) 2022 Steven Atkinson; Amphibia modifications 2026 Amphibia contributors
WizardStyle=modern
SignedUninstaller=no

[Types]
Name: "full"; Description: "Standalone application and VST3 plug-in"
Name: "standalone"; Description: "Standalone application"
Name: "vst3"; Description: "VST3 plug-in"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Amphibia standalone application"; Types: full standalone custom
Name: "vst3"; Description: "Amphibia VST3 plug-in"; Types: full vst3 custom
Name: "documentation"; Description: "Documentation, license, privacy, attribution, and notices"; Types: full standalone vst3 custom

[Tasks]
Name: "startmenu"; Description: "Create Start Menu shortcuts"; Components: standalone; Flags: checkedonce
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: standalone; Flags: unchecked

[Files]
Source: "{#StageRoot}\Standalone\Amphibia.exe"; DestDir: "{app}"; Components: standalone; Flags: replacesameversion restartreplace
Source: "{#StageRoot}\VST3\Amphibia.vst3\Contents\x86_64-win\Amphibia.vst3"; DestDir: "{commoncf64}\VST3\Amphibia.vst3\Contents\x86_64-win"; Components: vst3; Flags: replacesameversion restartreplace
Source: "{#StageRoot}\VST3\Amphibia.vst3\Contents\Resources\THIRD_PARTY_NOTICES.txt"; DestDir: "{commoncf64}\VST3\Amphibia.vst3\Contents\Resources"; Components: vst3; Flags: ignoreversion
Source: "{#StageRoot}\LICENSE.txt"; DestDir: "{app}"; Components: standalone or vst3; Flags: ignoreversion
Source: "{#StageRoot}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Components: standalone or vst3; Flags: ignoreversion
Source: "{#StageRoot}\ATTRIBUTION.txt"; DestDir: "{app}"; Components: standalone or vst3; Flags: ignoreversion
Source: "{#StageRoot}\PRIVACY.txt"; DestDir: "{app}"; Components: standalone or vst3; Flags: ignoreversion
Source: "{#StageRoot}\README.txt"; DestDir: "{app}"; Components: documentation; Flags: ignoreversion
Source: "{#StageRoot}\CHANGELOG.txt"; DestDir: "{app}"; Components: documentation; Flags: ignoreversion
Source: "{#StageRoot}\Documentation\*"; DestDir: "{app}\Documentation"; Components: documentation; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Amphibia"; Filename: "{app}\Amphibia.exe"; WorkingDir: "{app}"; Components: standalone; Tasks: startmenu
Name: "{group}\Amphibia Setup"; Filename: "{app}\Amphibia.exe"; Parameters: "--setup"; WorkingDir: "{app}"; Components: standalone; Tasks: startmenu
Name: "{group}\Amphibia Audio Settings"; Filename: "{app}\Amphibia.exe"; Parameters: "--audio-settings"; WorkingDir: "{app}"; Components: standalone; Tasks: startmenu
Name: "{group}\Amphibia Documentation"; Filename: "{sys}\explorer.exe"; Parameters: """{app}\Documentation"""; Components: standalone and documentation; Tasks: startmenu
Name: "{group}\Uninstall Amphibia"; Filename: "{uninstallexe}"; Components: standalone; Tasks: startmenu
Name: "{autodesktop}\Amphibia"; Filename: "{app}\Amphibia.exe"; WorkingDir: "{app}"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\Amphibia.exe"; Parameters: "--first-run"; Description: "Launch Amphibia setup"; WorkingDir: "{app}"; Components: standalone; Flags: postinstall nowait skipifsilent runasoriginaluser; Check: ShouldOfferFirstRun

[InstallDelete]
Type: files; Name: "{app}\Amphibia.exe"; Check: not IsStandaloneSelected
Type: filesandordirs; Name: "{commoncf64}\VST3\Amphibia.vst3"; Check: not IsVst3Selected
Type: filesandordirs; Name: "{app}\Documentation"; Check: not IsDocumentationSelected
Type: files; Name: "{autodesktop}\Amphibia.lnk"; Check: not IsDesktopTaskSelected
Type: files; Name: "{group}\Amphibia.lnk"; Check: not IsStartMenuTaskSelected
Type: files; Name: "{group}\Amphibia Setup.lnk"; Check: not IsStartMenuTaskSelected
Type: files; Name: "{group}\Amphibia Audio Settings.lnk"; Check: not IsStartMenuTaskSelected
Type: files; Name: "{group}\Amphibia Documentation.lnk"; Check: not IsStartMenuTaskSelected
Type: files; Name: "{group}\Uninstall Amphibia.lnk"; Check: not IsStartMenuTaskSelected

[Code]
const
  InstalledVersionKey = '{#MyUninstallKey}';
  CredentialTarget = 'org.amphibiaaudio.Amphibia.TONE3000/oauth-v1';
  CredentialTypeGeneric = 1;

var
  FreshInstall: Boolean;
  RemoveUserData: Boolean;

function CredDelete(TargetName: String; TypeValue, Flags: Cardinal): Boolean;
  external 'CredDeleteW@advapi32.dll stdcall';

function IsStandaloneSelected: Boolean;
begin
  Result := WizardIsComponentSelected('standalone');
end;

function IsVst3Selected: Boolean;
begin
  Result := WizardIsComponentSelected('vst3');
end;

function IsDocumentationSelected: Boolean;
begin
  Result := WizardIsComponentSelected('documentation');
end;

function IsDesktopTaskSelected: Boolean;
begin
  Result := WizardIsTaskSelected('desktopicon') and IsStandaloneSelected;
end;

function IsStartMenuTaskSelected: Boolean;
begin
  Result := WizardIsTaskSelected('startmenu') and IsStandaloneSelected;
end;

function AtLeastOneExecutableSelected: Boolean;
begin
  Result := IsStandaloneSelected or IsVst3Selected;
end;

function ReadInstalledVersion(var VersionText: String): Boolean;
begin
  Result := RegQueryStringValue(HKLM64, InstalledVersionKey, 'DisplayVersion', VersionText);
end;

function InitializeSetup: Boolean;
var
  InstalledVersion: String;
  InstalledPacked, PackagePacked: Int64;
begin
  FreshInstall := not ReadInstalledVersion(InstalledVersion);
  Result := True;
  if not FreshInstall then
  begin
    if StrToVersion(InstalledVersion, InstalledPacked) and
       StrToVersion('{#MyAppVersion}', PackagePacked) and
       (ComparePackedVersion(PackagePacked, InstalledPacked) < 0) then
    begin
      SuppressibleMsgBox(
        'A newer Amphibia version (' + InstalledVersion + ') is already installed.' + #13#10 +
        'Uninstall it before installing this older package. Settings and managed content are preserved by default.',
        mbError, MB_OK, IDOK);
      Result := False;
    end;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectComponents) and not AtLeastOneExecutableSelected then
  begin
    SuppressibleMsgBox(
      'Select at least one executable component: the standalone application or the VST3 plug-in.',
      mbError, MB_OK, IDOK);
    Result := False;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  if not AtLeastOneExecutableSelected then
    Result := 'At least one executable Amphibia component must be selected.'
  else
    Result := '';
end;

function ShouldOfferFirstRun: Boolean;
begin
  Result := FreshInstall and IsStandaloneSelected;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo,
  MemoTypeInfo, MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result :=
    MemoTypeInfo + NewLine + NewLine +
    MemoComponentsInfo + NewLine + NewLine +
    'Destinations:' + NewLine;
  if IsStandaloneSelected then
    Result := Result + Space + ExpandConstant('{autopf}\Amphibia') + NewLine;
  if IsVst3Selected then
    Result := Result + Space + ExpandConstant('{commoncf64}\VST3\Amphibia.vst3') + NewLine;
  if MemoTasksInfo <> '' then
    Result := Result + NewLine + MemoTasksInfo;
end;

function IsSafeAmphibiaRoot(const Candidate: String): Boolean;
var
  Expected, ParentPath, Expanded: String;
  FindRec: TFindRec;
begin
  Result := False;
  try
    ParentPath := RemoveBackslashUnlessRoot(ExpandFileName(ExpandConstant('{localappdata}')));
    Expected := PathCombine(ParentPath, 'Amphibia');
    Expanded := RemoveBackslashUnlessRoot(ExpandFileName(Candidate));
    if not PathSame(Expanded, Expected) then
      exit;
    if PathSame(Expanded, ExtractFileDrive(Expanded) + '\') or PathSame(Expanded, ParentPath) then
      exit;
    if FindFirst(Expanded, FindRec) then
    begin
      try
        if (FindRec.Attributes and FILE_ATTRIBUTE_REPARSE_POINT) <> 0 then
          exit;
      finally
        FindClose(FindRec);
      end;
    end;
    Result := True;
  except
    Log('Amphibia user-data root validation failed; data was preserved.');
  end;
end;

function SafeDeleteOwnedTree(const Path, Root: String): Boolean;
var
  Child: String;
  FindRec: TFindRec;
begin
  Result := False;
  if not (PathSame(Path, Root) or PathStartsWith(Path, AddBackslash(Root), True)) then
    exit;
  if not DirExists(Path) then
  begin
    Result := True;
    exit;
  end;

  if FindFirst(PathCombine(Path, '*'), FindRec) then
  begin
    try
      repeat
        if (FindRec.Name <> '.') and (FindRec.Name <> '..') then
        begin
          Child := PathCombine(Path, FindRec.Name);
          if not PathStartsWith(Child, AddBackslash(Root), True) then
            exit;
          if (FindRec.Attributes and FILE_ATTRIBUTE_REPARSE_POINT) <> 0 then
          begin
            if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
            begin
              if not RemoveDir(Child) then
                exit;
            end
            else if not DeleteFile(Child) then
              exit;
          end
          else if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
          begin
            if not SafeDeleteOwnedTree(Child, Root) then
              exit;
          end
          else if not DeleteFile(Child) then
            exit;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
  Result := RemoveDir(Path);
end;

function InitializeUninstall: Boolean;
var
  Choice: Integer;
begin
  RemoveUserData := False;
  Result := True;
  if UninstallSilent then
    exit;

  Choice := SuppressibleMsgBox(
    'Optional user-data removal' + #13#10 + #13#10 +
    'Choose Yes only if you also want to remove Amphibia''s managed downloads, imported managed copies, settings, cache, logs, and stored TONE3000 credentials.' + #13#10 + #13#10 +
    'This does not remove your original NAM folders, original IR folders, or original ZIP archives.' + #13#10 + #13#10 +
    'The default (No) preserves all user data for reinstall or upgrade.',
    mbConfirmation, MB_YESNO or MB_DEFBUTTON2, IDNO);
  RemoveUserData := Choice = IDYES;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  UserDataRoot: String;
begin
  if (CurUninstallStep = usPostUninstall) and RemoveUserData then
  begin
    UserDataRoot := ExpandConstant('{localappdata}\Amphibia');
    if IsSafeAmphibiaRoot(UserDataRoot) then
    begin
      if SafeDeleteOwnedTree(UserDataRoot, RemoveBackslashUnlessRoot(ExpandFileName(UserDataRoot))) then
        Log('Removed the fixed Amphibia user-data root.')
      else
        Log('Could not completely remove the fixed Amphibia user-data root; remaining data was preserved.');
    end
    else
      Log('Refused to remove Amphibia user data because the fixed root failed safety validation.');

    if CredDelete(CredentialTarget, CredentialTypeGeneric, 0) then
      Log('Removed the exact Amphibia TONE3000 credential.')
    else
      Log('The exact Amphibia TONE3000 credential was absent or could not be removed.');
  end;
end;
