# make_icons.ps1 — Crisp simge ureticisi.
#
# Uretilenler:
#   res/icons/app.ico          uygulama simgesi (accent plaka + beyaz kose ayraclari)
#   res/icons/tray_dark.ico    koyu gorev cubugu icin beyaz ayraclar (seffaf zemin)
#   res/icons/tray_light.ico   acik gorev cubugu icin koyu ayraclar
#   packaging/Assets/*.png     MSIX gorselleri, ayni tasarim
#
# MOTIF: dort kose ayraci — bir ekran alintisi aracinin evrensel isareti.
# Cerceve degil AYRAC cizilir; 16 pikselde tam cerceve bulanik bir kareye
# donusurken ayraclar okunakli kalir.
#
# Cikti dizeleri BILEREK ASCII: bu dosya BOM'suz UTF-8 ve PowerShell 5.1
# BOM'suz dosyalari ANSI sanip Turkce karakterleri bozar.
#
# Kullanim (proje kokunden):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/make_icons.ps1

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# --- Yollar ------------------------------------------------------------------
# Betik cwd'ye degil kendi konumuna gore calisir: CMake yapilandirma sirasinda
# cagirdiginda calisma dizini farkli olabilir.
$root     = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$iconDir  = Join-Path $root 'res\icons'
$assetDir = Join-Path $root 'packaging\Assets'
New-Item -ItemType Directory -Force -Path $iconDir  | Out-Null
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null

# --- Sabitler ----------------------------------------------------------------
$Box      = 72.0                 # tasarim kutusu
$IcoSizes = @(16, 20, 24, 32, 48, 256)

$Accent   = [Drawing.Color]::FromArgb(255,  10, 132, 255)   # #0A84FF
$White    = [Drawing.Color]::FromArgb(255, 255, 255, 255)
$Ink      = [Drawing.Color]::FromArgb(255,  26,  26,  26)   # #1A1A1A

$report = @()
function Add-Report {
    param([string]$Path)
    $script:report += [PSCustomObject]@{
        File  = $Path.Substring($root.Length + 1)
        Bytes = (Get-Item -LiteralPath $Path).Length
    }
}

# ---------------------------------------------------------------------------
# Geometri
# ---------------------------------------------------------------------------

function Add-RoundRect {
    param(
        [Drawing.Drawing2D.GraphicsPath]$Path,
        [double]$X, [double]$Y, [double]$W, [double]$H, [double]$R
    )
    $d = $R * 2.0
    $Path.StartFigure()
    $Path.AddArc([float]$X,             [float]$Y,             [float]$d, [float]$d, 180, 90)
    $Path.AddArc([float]($X + $W - $d), [float]$Y,             [float]$d, [float]$d, 270, 90)
    $Path.AddArc([float]($X + $W - $d), [float]($Y + $H - $d), [float]$d, [float]$d,   0, 90)
    $Path.AddArc([float]$X,             [float]($Y + $H - $d), [float]$d, [float]$d,  90, 90)
    $Path.CloseFigure()
}

# Tek bir kose ayraci: L seklinde iki cizgi. $DirX/$DirY kollarin hangi yone
# uzayacagini soyler (+1 saga/asagi, -1 sola/yukari).
function Add-Bracket {
    param(
        [Drawing.Drawing2D.GraphicsPath]$Path,
        [double]$X, [double]$Y, [double]$Arm,
        [int]$DirX, [int]$DirY
    )
    $Path.StartFigure()
    $Path.AddLine([float]($X + $Arm * $DirX), [float]$Y, [float]$X, [float]$Y)
    $Path.AddLine([float]$X, [float]$Y, [float]$X, [float]($Y + $Arm * $DirY))
}

# Dort ayracin tamamini iceren yol. Inset kenardan uzaklik, Arm kol uzunlugu.
function New-BracketPath {
    param([double]$Inset, [double]$Arm)

    $path = New-Object Drawing.Drawing2D.GraphicsPath
    $lo = $Inset
    $hi = $Box - $Inset

    Add-Bracket -Path $path -X $lo -Y $lo -Arm $Arm -DirX  1 -DirY  1   # sol ust
    Add-Bracket -Path $path -X $hi -Y $lo -Arm $Arm -DirX -1 -DirY  1   # sag ust
    Add-Bracket -Path $path -X $lo -Y $hi -Arm $Arm -DirX  1 -DirY -1   # sol alt
    Add-Bracket -Path $path -X $hi -Y $hi -Arm $Arm -DirX -1 -DirY -1   # sag alt
    return $path
}

# ---------------------------------------------------------------------------
# Cizim
# ---------------------------------------------------------------------------

function New-Surface {
    param([int]$Width, [int]$Height)

    $bmp = New-Object Drawing.Bitmap($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode   = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([Drawing.Color]::Transparent)
    return @{ Bitmap = $bmp; Graphics = $g }
}

# Ayraclari verilen renkte cizer. $Scale, tasarim kutusundan hedef piksele
# olceklendirme carpani.
function Draw-Brackets {
    param(
        [Drawing.Graphics]$Graphics,
        [double]$Scale,
        [Drawing.Color]$Color,
        [double]$Inset,
        [double]$Arm,
        [double]$Thickness
    )

    $path = New-BracketPath -Inset $Inset -Arm $Arm
    try {
        $matrix = New-Object Drawing.Drawing2D.Matrix
        $matrix.Scale([float]$Scale, [float]$Scale)
        $path.Transform($matrix)
        $matrix.Dispose()

        $pen = New-Object Drawing.Pen($Color, [float]($Thickness * $Scale))
        try {
            $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
            $pen.EndCap   = [Drawing.Drawing2D.LineCap]::Round
            $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
            $Graphics.DrawPath($pen, $path)
        }
        finally { $pen.Dispose() }
    }
    finally { $path.Dispose() }
}

# Uygulama simgesi karesi: accent plaka + beyaz ayraclar.
function New-LogoBitmap {
    param([int]$Width, [int]$Height, [double]$Fill = 0.88)

    $surface = New-Surface -Width $Width -Height $Height
    $g = $surface.Graphics
    try {
        # Kare olmayan hedeflerde (geniş kutucuk) logo ortalanir.
        $side  = [Math]::Min($Width, $Height) * $Fill
        $scale = $side / $Box
        $offX  = ($Width  - $side) / 2.0
        $offY  = ($Height - $side) / 2.0

        $state = $g.Save()
        $g.TranslateTransform([float]$offX, [float]$offY)

        $plate = New-Object Drawing.Drawing2D.GraphicsPath
        try {
            Add-RoundRect -Path $plate -X 0 -Y 0 -W $Box -H $Box -R 16
            $matrix = New-Object Drawing.Drawing2D.Matrix
            $matrix.Scale([float]$scale, [float]$scale)
            $plate.Transform($matrix)
            $matrix.Dispose()

            $brush = New-Object Drawing.SolidBrush($Accent)
            try { $g.FillPath($brush, $plate) } finally { $brush.Dispose() }
        }
        finally { $plate.Dispose() }

        Draw-Brackets -Graphics $g -Scale $scale -Color $White `
                      -Inset 20 -Arm 13 -Thickness 7

        $g.Restore($state)
    }
    finally { $g.Dispose() }

    return $surface.Bitmap
}

# Tepsi simgesi: plaka YOK, yalnizca ayraclar. Gorev cubugu kendi zeminini
# saglar; plaka eklemek simgeyi komsularindan kalin gosterirdi.
function New-TrayBitmap {
    param([int]$Size, [Drawing.Color]$Color)

    $surface = New-Surface -Width $Size -Height $Size
    $g = $surface.Graphics
    try {
        # Tepside plaka olmadigi icin ayraclar daha genise yayilir ve kalinlasir:
        # 16 pikselde ince cizgiler gri bir bulasiga donusur.
        Draw-Brackets -Graphics $g -Scale ($Size / $Box) -Color $Color `
                      -Inset 9 -Arm 19 -Thickness 9
    }
    finally { $g.Dispose() }

    return $surface.Bitmap
}

# ---------------------------------------------------------------------------
# ICO yazimi
# ---------------------------------------------------------------------------
# Her kare PNG olarak gomulur. Sikistirilmamis DIB kareler dosyayi bes kat
# buyutur ve Windows Vista'dan beri PNG kareler her boyutta desteklenir.

# DIKKAT: bas virgul SART. PowerShell fonksiyon donusunde dizileri tek tek
# ogelere cozer; `return $bytes` cagirana byte[] degil, yuzlerce ayri Byte
# nesnesi verir ve BinaryWriter.Write o koleksiyonu yazamaz. `,$bytes` fazladan
# bir sarmal ekler, cozulen o sarmal olur ve byte[] saglam kalir.
function Get-PngBytes {
    param([Drawing.Bitmap]$Bitmap)

    $stream = New-Object IO.MemoryStream
    try {
        $Bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
        return ,$stream.ToArray()
    }
    finally { $stream.Dispose() }
}

function Write-Ico {
    param(
        [string]$Path,
        [scriptblock]$MakeBitmap   # tek parametre: kenar uzunlugu (piksel)
    )

    # ArrayList: `+=` ile buyuyen bir dizi her adimda kopyalanir ve ic ice
    # dizilerde tip bilgisini kaybeder.
    $frames = New-Object System.Collections.ArrayList
    foreach ($size in $IcoSizes) {
        $bmp = & $MakeBitmap $size
        try {
            [byte[]]$png = Get-PngBytes -Bitmap $bmp
            if ($png.Length -eq 0) { throw "PNG kodlamasi bos dondu ($size px)." }
            [void]$frames.Add($png)
        }
        finally { $bmp.Dispose() }
    }

    $stream = [IO.File]::Create($Path)
    $writer = New-Object IO.BinaryWriter($stream)
    try {
        # ICONDIR
        $writer.Write([UInt16]0)                  # reserved
        $writer.Write([UInt16]1)                  # type: 1 = ikon
        $writer.Write([UInt16]$IcoSizes.Count)

        # ICONDIRENTRY basina 16 bayt; piksel verisi girdilerden sonra baslar.
        $offset = 6 + 16 * $IcoSizes.Count
        for ($i = 0; $i -lt $IcoSizes.Count; $i++) {
            $size = $IcoSizes[$i]
            # 256 piksel, ICO basliginda 0 olarak kodlanir.
            if ($size -ge 256) { $encoded = 0 } else { $encoded = $size }
            $writer.Write([Byte]$encoded)         # genislik
            $writer.Write([Byte]$encoded)         # yukseklik
            $writer.Write([Byte]0)                # palet rengi yok
            $writer.Write([Byte]0)                # reserved
            $writer.Write([UInt16]1)              # duzlem
            $writer.Write([UInt16]32)             # bit/piksel
            $writer.Write([UInt32]$frames[$i].Length)
            $writer.Write([UInt32]$offset)
            $offset += $frames[$i].Length
        }

        foreach ($frame in $frames) { $writer.Write([byte[]]$frame, 0, $frame.Length) }
    }
    finally {
        $writer.Dispose()
        $stream.Dispose()
    }

    Add-Report -Path $Path
}

function Write-LogoPng {
    param([string]$Path, [int]$Width, [int]$Height)

    $bmp = New-LogoBitmap -Width $Width -Height $Height
    try { [IO.File]::WriteAllBytes($Path, (Get-PngBytes -Bitmap $bmp)) } finally { $bmp.Dispose() }
    Add-Report -Path $Path
}

# ---------------------------------------------------------------------------
# Uretim
# ---------------------------------------------------------------------------

Write-Host 'Crisp simge uretimi...'

Write-Ico -Path (Join-Path $iconDir 'app.ico') -MakeBitmap {
    param($size) New-LogoBitmap -Width $size -Height $size
}

Write-Ico -Path (Join-Path $iconDir 'tray_dark.ico') -MakeBitmap {
    param($size) New-TrayBitmap -Size $size -Color $White
}

Write-Ico -Path (Join-Path $iconDir 'tray_light.ico') -MakeBitmap {
    param($size) New-TrayBitmap -Size $size -Color $Ink
}

# --- MSIX gorselleri ---------------------------------------------------------
# Taban dosya + olcek cesitleri. Windows kutucugu 125/150/200/400 DPI'da
# ".scale-N" dosyasini secer; yoksa olcek-100'u buyutur ve kenarlar bulaniklasir.
$AssetBases = @(
    @{ Name = 'Square44x44Logo';   W =  44; H =  44 },
    @{ Name = 'Square71x71Logo';   W =  71; H =  71 },
    @{ Name = 'Square150x150Logo'; W = 150; H = 150 },
    @{ Name = 'Square310x310Logo'; W = 310; H = 310 },
    @{ Name = 'Wide310x150Logo';   W = 310; H = 150 },
    @{ Name = 'StoreLogo';         W =  50; H =  50 },
    @{ Name = 'SplashScreen';      W = 620; H = 300 }
)
$AssetScales = @(125, 150, 200, 400)
$TargetSizes = @(16, 24, 32, 48, 256)

# .NET'in varsayilan yuvarlamasi bankaci yuvarlamasidir (62,5 -> 62). Windows'un
# bekledigi degerler yukari yuvarlamayla gelir: 50 -> 63, 71 -> 89, 150 -> 188.
function Get-ScaledPx {
    param([int]$Base, [int]$Scale)
    return [int][math]::Round($Base * $Scale / 100.0, 0, [MidpointRounding]::AwayFromZero)
}

foreach ($a in $AssetBases) {
    Write-LogoPng -Path (Join-Path $assetDir "$($a.Name).png") -Width $a.W -Height $a.H
    foreach ($s in $AssetScales) {
        Write-LogoPng -Path (Join-Path $assetDir "$($a.Name).scale-$s.png") `
                      -Width  (Get-ScaledPx -Base $a.W -Scale $s) `
                      -Height (Get-ScaledPx -Base $a.H -Scale $s)
    }
}

foreach ($t in $TargetSizes) {
    Write-LogoPng -Path (Join-Path $assetDir "Square44x44Logo.targetsize-$t.png") -Width $t -Height $t
    Write-LogoPng -Path (Join-Path $assetDir "Square44x44Logo.targetsize-${t}_altform-unplated.png") -Width $t -Height $t
}

# --- Rapor -------------------------------------------------------------------
foreach ($r in $report) {
    Write-Host ('  {0,-62} {1,9:N0} bayt' -f $r.File, $r.Bytes)
}
$zero = @($report | Where-Object { $_.Bytes -eq 0 })
if ($zero.Count -gt 0) {
    throw ("Bos dosya uretildi: " + ($zero.File -join ', '))
}
Write-Host ('Tamam: {0} dosya.' -f $report.Count)
