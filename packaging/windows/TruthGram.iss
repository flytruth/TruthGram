; =============================================================================
; TruthGram Windows installer (Inno Setup)
; Invoke via:  ISCC /DTRUTHGRAM_VERSION=x.y.z /DTRUTHGRAM_STAGING=path TruthGram.iss
; =============================================================================
#ifndef TRUTHGRAM_VERSION
  #error TRUTHGRAM_VERSION must be defined (e.g. /DTRUTHGRAM_VERSION=1.0.0)
#endif
#ifndef TRUTHGRAM_STAGING
  #error TRUTHGRAM_STAGING must point to the build staging directory
#endif

#define TruthGramApp "TruthGram"
#define TruthGramExe "TruthGram.exe"
#define TruthGramPublisher "TruthGram Contributors"
#define TruthGramURL "https://github.com/truthgram/truthgram-desktop"
#define TruthGramStaging GetEnv("TRUTHGRAM_STAGING")

[Setup]
AppName={#TruthGramApp}
AppVersion={#TRUTHGRAM_VERSION}
AppPublisher={#TruthGramPublisher}
AppPublisherURL={#TruthGramURL}
AppSupportURL={#TruthGramURL}
AppUpdatesURL={#TruthGramURL}
DefaultDirName={autopf}\{#TruthGramApp}
DefaultGroupName={#TruthGramApp}
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=TruthGram-{#TRUTHGRAM_VERSION}-setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=arm64 x64
ArchitecturesInstallIn64BitMode=arm64 x64
WizardStyle=modern
LicenseFile={#TruthGramStaging}\LICENSE
UninstallDisplayIcon={app}\{#TruthGramExe}
; Tell Windows to exclude the window from Game Bar / screen capture by default
; for the installer UI shell; the runtime WDA_EXCLUDEFROMCAPTURE is set in code.
CloseApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "{#TruthGramStaging}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#TruthGramApp}"; Filename: "{app}\{#TruthGramExe}"
Name: "{group}\Uninstall {#TruthGramApp}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#TruthGramApp}"; Filename: "{app}\{#TruthGramExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#TruthGramExe}"; Description: "Launch {#TruthGramApp}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{%LOCALAPPDATA}\TruthGram"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
