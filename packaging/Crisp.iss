; Crisp.iss — Kurulum programı (Inno Setup 6).
;
; NEDEN KURULUM VAR: taşınabilir ZIP ilk seçenek olmayı sürdürüyor ve öyle
; kalmalı — tek dosya, kaydı kirletmez, USB'de çalışır. Ama bir kısayol, bir
; "Uygulamalar ve özellikler" girdisi ve Windows ile başlatma seçeneği isteyen
; kullanıcının bunları elle kurması gerekiyordu.
;
; İKİSİ AYNI .exe'yi TAŞIR. Kurulum ayrı bir yapı üretmiyor; build\Release
; içindeki aynı dosyayı paketliyor. Ayrı bir yapı, "taşınabilir sürümde çalışan
; şey kurulumda çalışmıyor" cümlesinin başlangıcı olurdu.
;
; TAŞINABİLİR KİP KAZAYLA AÇILMAZ: Crisp, yanında bir Crisp.ini görürse ayarları
; oraya yazıyor. Kurulum dizinine böyle bir dosya KONMUYOR, dolayısıyla kurulmuş
; sürüm ayarlarını her zaman kayıt defterine yazar.

#define AppName        "Crisp"
#define AppPublisher   "ShadesOfDeath"
#define AppUrl         "https://github.com/shadesofdeath/Crisp"
#define AppExe         "Crisp.exe"

; Sürüm ve dosya yolları dışarıdan verilir; varsayılanları yalnızca elle
; derlerken işe yarar.
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\Release"
#endif
#ifndef WizardDir
  #define WizardDir "..\build\wizard"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\Release"
#endif

[Setup]
; KİMLİK BİR GUID: uygulamanın adı ya da yayıncısı değişse bile Windows aynı
; kurulumu tanımaya devam etsin, ve 0.7.0 üzerine 0.8.0 kurulduğunda ikinci bir
; girdi değil, bir güncelleme olsun.
AppId={{7C6B4C1E-9E52-4E4A-B0E7-2B1B4F0D9A31}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} kurulumu

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
; Bileşen yok, dolayısıyla sayfası da yok: tek bir .exe kuruluyor.
DisableProgramGroupPage=yes
DisableDirPage=no
AllowNoIcons=yes
LicenseFile=..\LICENSE

; KARŞILAMA SAYFASI AÇIK. `modern` sihirbaz kipi onu varsayılan olarak
; kapatıyor ve kurulum doğrudan lisans metniyle başlıyordu. Soldaki tam boy
; görsel yalnızca o sayfada görünür; kapalıyken üretilmesinin de bir anlamı
; kalmıyor.
DisableWelcomePage=no
DisableReadyPage=no

; YÖNETİCİ İSTEMİYOR. Crisp kayıt defterinde yalnızca HKCU'yu kullanıyor,
; sürücü kurmuyor, hizmet kaydetmiyor. `lowest` + `auto` dizinleri, kullanıcı
; yöneticiyse Program Files'a, değilse kendi AppData'sına kurar — ve yükseltme
; istemi hiç çıkmaz.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

; ÇALIŞAN SÜRÜMÜ KAPAT. Crisp bir tepsi uygulaması ve çoğu zaman açıktır;
; üstüne yazmaya çalışmak "dosya kullanımda" hatası verirdi.
;
; AppMutex KULLANILMIYOR, Restart Manager kullanılıyor. Mutex, çalışan bir
; uygulamayı yalnızca TESPİT eder ve kuruluma "devam etme" der; sessiz kurulumda
; ise hiçbir şey söylemeden çıkar. Sınandı: Crisp açıkken `unins000.exe
; /VERYSILENT` hiçbir şey yapmadan başarıyla dönüyordu — dosyalar, kısayollar ve
; başlangıç girdisi olduğu gibi kalıyordu ve kullanıcının elinde "kaldırdım"
; sanısı oluyordu. Restart Manager ise uygulamayı KAPATIR ve iş devam eder.
CloseApplications=yes
CloseApplicationsFilter=*.exe
RestartApplications=no

Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763

OutputDir={#OutputDir}
OutputBaseFilename={#AppName}-{#AppVersion}-setup-x64
SetupIconFile=..\res\icons\app.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} {#AppVersion}

WizardStyle=modern
WizardSizePercent=100
WizardImageFile={#WizardDir}\WizardImage.bmp,{#WizardDir}\WizardImage@2x.bmp
WizardSmallImageFile={#WizardDir}\WizardSmallImage.bmp,{#WizardDir}\WizardSmallImage@2x.bmp
WizardImageStretch=yes

; DİL SORULUYOR, SEZİLMİYOR. `auto` sistem diliyle eşleşen bir çeviri
; bulduğunda soruyu hiç sormaz — kurulum Türkçe bir Windows'ta doğrudan Türkçe
; açılır ve on beş dili desteklediği hiçbir yerde görünmez. Uygulamanın kendi
; arayüzü de aynı dili seçiyor; buradaki soru, ikisini birbirinden ayırmak
; isteyen kullanıcı için.
ShowLanguageDialog=yes
UsePreviousLanguage=yes

[Languages]
; Crisp on altı dile çevrilmiş; Inno'nun kendi çevirileri bunlardan on beşini
; karşılıyor. Çince için resmî bir .isl yok, o kurulumda İngilizce görünür —
; uygulamanın kendi arayüzü yine Çince açılır.
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "pt"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "pl"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "nl"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "cs"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "sv"; MessagesFile: "compiler:Languages\Swedish.isl"
Name: "uk"; MessagesFile: "compiler:Languages\Ukrainian.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"

[CustomMessages]
en.StartWithWindows=Start {#AppName} when I sign in
tr.StartWithWindows=Oturum açtığımda {#AppName} başlasın
de.StartWithWindows={#AppName} bei der Anmeldung starten
fr.StartWithWindows=Démarrer {#AppName} à l'ouverture de session
es.StartWithWindows=Iniciar {#AppName} al iniciar sesión
it.StartWithWindows=Avvia {#AppName} all'accesso
pt.StartWithWindows=Iniciar o {#AppName} ao entrar
ru.StartWithWindows=Запускать {#AppName} при входе в систему
pl.StartWithWindows=Uruchamiaj {#AppName} przy logowaniu
nl.StartWithWindows={#AppName} starten bij aanmelden
cs.StartWithWindows=Spustit {#AppName} při přihlášení
sv.StartWithWindows=Starta {#AppName} vid inloggning
uk.StartWithWindows=Запускати {#AppName} під час входу
ja.StartWithWindows=サインイン時に {#AppName} を起動する
ko.StartWithWindows=로그인할 때 {#AppName} 시작

en.OpenProjectPage=Open the project page on GitHub
tr.OpenProjectPage=Projenin GitHub sayfasını aç
de.OpenProjectPage=Projektseite auf GitHub öffnen
fr.OpenProjectPage=Ouvrir la page du projet sur GitHub
es.OpenProjectPage=Abrir la página del proyecto en GitHub
it.OpenProjectPage=Apri la pagina del progetto su GitHub
pt.OpenProjectPage=Abrir a página do projeto no GitHub
ru.OpenProjectPage=Открыть страницу проекта на GitHub
pl.OpenProjectPage=Otwórz stronę projektu na GitHubie
nl.OpenProjectPage=Projectpagina op GitHub openen
cs.OpenProjectPage=Otevřít stránku projektu na GitHubu
sv.OpenProjectPage=Öppna projektsidan på GitHub
uk.OpenProjectPage=Відкрити сторінку проєкту на GitHub
ja.OpenProjectPage=GitHub のプロジェクトページを開く
ko.OpenProjectPage=GitHub 프로젝트 페이지 열기

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"
; İŞARETLİ GELİYOR, ÇÜNKÜ ÇALIŞMAYAN BİR KISAYOL ARACI HİÇBİR ŞEY YAPMAZ.
; Crisp global kısayolları dinleyen bir tepsi uygulaması; kapalıyken PrtScn'e
; basmak bir işe yaramaz. Kutu görünür yerde duruyor ve kaldırma sırasında
; kayıt defteri girdisi de siliniyor.
Name: "startup"; Description: "{cm:StartWithWindows}"

[Files]
Source: "{#SourceDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; DestName: "README.md"; Flags: ignoreversion
Source: "..\CHANGELOG.md"; DestDir: "{app}"; DestName: "CHANGELOG.md"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; HKCU: kurulum yönetici istemiyor ve başlangıç girdisi zaten kullanıcıya ait.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExe}"""; \
    Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; \
    Flags: nowait postinstall skipifsilent
; Kurulum bitince proje sayfası. `postinstall` olduğu için son sayfada bir
; kutu olarak görünür: açılması kullanıcının onayına bağlı, sessiz kurulumda
; hiç açılmaz.
Filename: "{#AppUrl}"; Description: "{cm:OpenProjectPage}"; \
    Flags: shellexec nowait postinstall skipifsilent

[UninstallDelete]
; Kaldırma AYARLARI SİLMEZ. Kullanıcının kısayolları, kaydetme klasörü ve API
; anahtarı HKCU'da duruyor; yeniden kurulduğunda hepsi yerinde olsun. Silinen
; tek şey, kurulumun kendi koyduğu dosyalar.
Type: dirifempty; Name: "{app}"

[Code]
// ÇALIŞAN CRISP'İ KAPATIR.
//
// Restart Manager BUNU YAPAMIYOR ve sınandı: Crisp açıkken kaldırma çalışınca
// kısayollar ve başlangıç girdisi siliniyor, Crisp.exe ise yerinde kalıyor ve
// uygulama çalışmayı sürdürüyordu. Sebep, Crisp'in görünür bir penceresi
// olmaması — bir tepsi uygulamasının tek penceresi gizli. Restart Manager
// kapatma isteğini pencerelere gönderir; gönderecek pencere bulamayınca da
// dosyayı kullanımda bırakır.
//
// Pencere gizli ama SINIFI belli, ve WM_CLOSE onu düzgünce kapatıyor:
// uygulama WM_CLOSE'u işlemiyor, DefWindowProc pencereyi yok ediyor, WM_DESTROY
// da tepsi simgesini kaldırıp kısayolları serbest bırakarak çıkıyor. Yani
// süreci öldürmüyoruz, kapanmasını istiyoruz.
const
  WM_CLOSE = $0010;
  CrispWindowClass = 'CrispMessageWindow';

function StopCrisp(): Boolean;
var
  Window: HWND;
  Waited: Integer;
begin
  Waited := 0;
  Window := FindWindowByClassName(CrispWindowClass);
  while (Window <> 0) and (Waited < 5000) do
  begin
    PostMessage(Window, WM_CLOSE, 0, 0);
    Sleep(200);
    Waited := Waited + 200;
    Window := FindWindowByClassName(CrispWindowClass);
  end;
  // Kapanmadıysa da devam ediyoruz: Restart Manager ikinci bir şans, ve o da
  // olmazsa Windows dosyayı yeniden başlatmada siler. Kullanıcıyı "önce
  // uygulamayı kapatın" diyerek geri çevirmek, kapatılacak görünür bir pencere
  // olmadığı için işe yaramaz bir öğüt olurdu.
  Result := True;
end;

function InitializeSetup(): Boolean;
begin
  Result := StopCrisp();
end;

function InitializeUninstall(): Boolean;
begin
  Result := StopCrisp();
end;
