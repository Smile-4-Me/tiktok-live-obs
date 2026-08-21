; TikTok Live OBS Bridge - Windows installer
; Build with Inno Setup 6 or later:
;   ISCC.exe installer\TikTokLiveObsBridge.iss

#define AppName "TikTok Live OBS Bridge"
#define AppVersion "1.0.0"
#define AppPublisher "TikTok Live OBS Bridge Contributors"
#define PluginModule "tiktok-live-obs-bridge"
#define SourceRoot ".."

[Setup]
AppId={{3DB1D387-4015-4843-B0DB-4195958B6208}-{code:InstallationId}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\obs-studio
AppendDefaultDirName=no
DirExistsWarning=no
DisableDirPage=no
UsePreviousAppDir=no
UsePreviousLanguage=no
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=TikTok-Live-OBS-Bridge-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
LanguageDetectionMethod=uilanguage
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
CloseApplications=yes
RestartApplications=no
UninstallDisplayName={#AppName} — {code:InstallationDisplayName}
UninstallFilesDir={commonappdata}\TikTok Live OBS Bridge\uninstall\{code:InstallationId}
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription=Installer for {#AppName}
LicenseFile={#SourceRoot}\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Messages]
english.SelectDirLabel3=Choose your OBS Studio installation folder.%n%nThe installer will add the plugin to this OBS installation.
german.SelectDirLabel3=Wählen Sie Ihren OBS-Studio-Installationsordner aus.%n%nDer Installer fügt das Plugin dieser OBS-Installation hinzu.

[CustomMessages]
english.InvalidObsFolder=Please select a valid OBS Studio installation folder.
german.InvalidObsFolder=Bitte wählen Sie einen gültigen OBS-Studio-Installationsordner aus.
english.FinishedInstruction=Installation completed.%n%nRestart OBS Studio so the plugin appears in the Docks menu.
german.FinishedInstruction=Die Installation wurde abgeschlossen.%n%nStarten Sie OBS Studio neu, damit das Plugin im Menü „Docks“ erscheint.
english.UninstallDataTitle=Keep plugin configuration
german.UninstallDataTitle=Plugin-Konfiguration behalten
english.UninstallDataDescription=Choose whether your profiles, account connections, and preferences should remain available for a future installation of this OBS instance.
german.UninstallDataDescription=Wählen Sie, ob Ihre Profile, Konto-Verknüpfungen und Einstellungen für eine spätere Installation dieser OBS-Instanz erhalten bleiben sollen.
english.KeepConfiguration=Keep plugin configuration
german.KeepConfiguration=Plugin-Konfiguration behalten
english.ContinueUninstall=Continue uninstall
german.ContinueUninstall=Deinstallation fortsetzen

[Files]
Source: "{#SourceRoot}\dist\{#PluginModule}.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "{#SourceRoot}\data\locale\*.ini"; DestDir: "{app}\data\obs-plugins\{#PluginModule}\locale"; Flags: ignoreversion
Source: "{#SourceRoot}\LICENSE"; DestDir: "{app}\data\obs-plugins\{#PluginModule}"; Flags: ignoreversion

[UninstallDelete]
Type: files; Name: "{app}\obs-plugins\64bit\{#PluginModule}.dll"
Type: filesandordirs; Name: "{app}\data\obs-plugins\{#PluginModule}\locale"
Type: files; Name: "{app}\data\obs-plugins\{#PluginModule}\LICENSE"

[Code]
var
  WizardIsInitialized: Boolean;
  KeepPluginConfiguration: Boolean;

function InstallationId(Param: String): String;
var
  SelectedFolder: String;
begin
  { AppId is queried before the wizard exists. A temporary value is sufficient
    at that point; Inno Setup queries the final value again before installing. }
  if not WizardIsInitialized then begin
    Result := 'pending';
    Exit;
  end;

  SelectedFolder := WizardDirValue;
  if SelectedFolder = '' then begin
    Result := 'pending';
    Exit;
  end;

  { A separate identifier keeps updates and uninstall records independent for
    each OBS installation, including multiple portable installations. }
  Result := Copy(GetSHA1OfString(AnsiString(Lowercase(SelectedFolder))), 1, 16);
end;

function InstallationDisplayName(Param: String): String;
begin
  if not WizardIsInitialized then begin
    Result := 'OBS Studio';
    Exit;
  end;

  Result := ExtractFileName(WizardDirValue);
  if Result = '' then
    Result := 'OBS Studio';
end;

function PluginStorageId(): String;
begin
  Result := Copy(GetSHA256OfString(AnsiString(Lowercase(ExpandConstant('{app}')))), 1, 16);
end;

function PluginConfigurationDirectory(): String;
begin
  Result := ExpandConstant('{localappdata}\obs64');
end;

function PluginProfilesPath(): String;
begin
  Result := AddBackslash(PluginConfigurationDirectory()) +
    'tiktok-live-obs-bridge-profiles-' + PluginStorageId() + '.ini';
end;

function PluginSettingsPath(): String;
begin
  Result := AddBackslash(PluginConfigurationDirectory()) +
    'tiktok-live-obs-bridge-' + PluginStorageId() + '.ini';
end;

procedure RemoveLegacyGlobalPlugin();
begin
  { Older development installers used OBS' machine-wide ProgramData plugin
    location. OBS scans it in addition to the selected installation, which can
    create a duplicate module load. The current installer always uses the
    selected OBS installation folder. }
  DelTree(ExpandConstant('{commonappdata}\obs-studio\plugins\{#PluginModule}'),
    True, True, True);
end;

procedure DeleteStoredCredential(const TargetName: String);
var
  ExitCode: Integer;
begin
  Exec(ExpandConstant('{sys}\cmdkey.exe'), '/delete:"' + TargetName + '"', '',
    SW_HIDE, ewWaitUntilTerminated, ExitCode);
end;

procedure DeletePluginConfiguration();
var
  ProfilesPath: String;
  ProfileCount: Integer;
  Index: Integer;
  ProfileId: String;
  StorageId: String;
begin
  ProfilesPath := PluginProfilesPath();
  StorageId := PluginStorageId();
  ProfileCount := StrToIntDef(GetIniString('profiles', 'size', '0', ProfilesPath), 0);
  for Index := 1 to ProfileCount do begin
    ProfileId := GetIniString('profiles', IntToStr(Index) + '\\id', '', ProfilesPath);
    if ProfileId <> '' then begin
      DeleteStoredCredential('TikTokLiveObsBridge/' + StorageId + '/Streamlabs/' + ProfileId);
      DeleteStoredCredential('TikTokLiveObsBridge/' + StorageId + '/LiveCredentials/' + ProfileId);
    end;
  end;

  DeleteFile(ProfilesPath);
  DeleteFile(PluginSettingsPath());
end;

function AskToKeepPluginConfiguration(): Boolean;
var
  Form: TSetupForm;
  Description: TNewStaticText;
  KeepCheckBox: TNewCheckBox;
  ContinueButton: TNewButton;
  CancelButton: TNewButton;
begin
  Result := True;
  KeepPluginConfiguration := True;
  if UninstallSilent then
    Exit;

  Form := CreateCustomForm(430, 150, False, False);
  try
    Form.Caption := CustomMessage('UninstallDataTitle');
    Form.Position := poScreenCenter;

    Description := TNewStaticText.Create(Form);
    Description.Parent := Form;
    Description.AutoSize := False;
    Description.WordWrap := True;
    Description.SetBounds(16, 16, 398, 50);
    Description.Caption := CustomMessage('UninstallDataDescription');

    KeepCheckBox := TNewCheckBox.Create(Form);
    KeepCheckBox.Parent := Form;
    KeepCheckBox.SetBounds(16, 78, 390, 22);
    KeepCheckBox.Caption := CustomMessage('KeepConfiguration');
    KeepCheckBox.Checked := True;

    ContinueButton := TNewButton.Create(Form);
    ContinueButton.Parent := Form;
    ContinueButton.SetBounds(206, 112, 125, 28);
    ContinueButton.Caption := CustomMessage('ContinueUninstall');
    ContinueButton.Default := True;
    ContinueButton.ModalResult := mrOk;

    CancelButton := TNewButton.Create(Form);
    CancelButton.Parent := Form;
    CancelButton.SetBounds(339, 112, 75, 28);
    CancelButton.Caption := SetupMessage(msgButtonCancel);
    CancelButton.Cancel := True;
    CancelButton.ModalResult := mrCancel;

    if Form.ShowModal() <> mrOk then begin
      Result := False;
      Exit;
    end;

    KeepPluginConfiguration := KeepCheckBox.Checked;
  finally
    Form.Free();
  end;
end;

function IsValidObsFolder(const Folder: String): Boolean;
begin
  Result := FileExists(AddBackslash(Folder) + 'bin\64bit\obs64.exe');
end;

function FindInstalledObsFolder(var Folder: String): Boolean;
begin
  Result := RegQueryStringValue(HKLM64,
    'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio',
    'InstallLocation', Folder);

  if Result and IsValidObsFolder(Folder) then
    Exit;

  Folder := ExpandConstant('{autopf}\obs-studio');
  Result := IsValidObsFolder(Folder);
end;

procedure InitializeWizard;
var
  DetectedObsFolder: String;
begin
  { This only pre-fills the folder selection page. Nothing is installed until
    the user continues through the wizard and confirms the installation. }
  WizardIsInitialized := True;
  if FindInstalledObsFolder(DetectedObsFolder) then
    WizardForm.DirEdit.Text := DetectedObsFolder;
end;

procedure RegisterExtraCloseApplicationsResources;
begin
  { Also check OBS itself. This covers first-time plugin installations where
    no existing plugin DLL is available for Restart Manager to detect yet. }
  if WizardIsInitialized and IsValidObsFolder(WizardDirValue) then
    RegisterExtraCloseApplicationsResource(
      False, AddBackslash(WizardDirValue) + 'bin\64bit\obs64.exe');
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.FinishedLabel.Caption := CustomMessage('FinishedInstruction');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    RemoveLegacyGlobalPlugin();
end;

function InitializeUninstall(): Boolean;
begin
  Result := AskToKeepPluginConfiguration();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveLegacyGlobalPlugin();

  if (CurUninstallStep = usPostUninstall) and not KeepPluginConfiguration then
    DeletePluginConfiguration();
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and not IsValidObsFolder(WizardDirValue) then begin
    MsgBox(ExpandConstant('{cm:InvalidObsFolder}'), mbError, MB_OK);
    Result := False;
  end;
end;
