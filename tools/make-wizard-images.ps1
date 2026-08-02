# Inno Setup'ın sihirbaz görselleri. BMP olmak ZORUNDA — Inno PNG kabul etmiyor.
#
# ÜRETİLİYOR, ÇİZİLMİYOR: iki görselin de tek içeriği uygulama simgesi ve iki
# renk. Elle çizilmiş bir BMP'yi depoda tutmak, simge değiştiğinde sessizce
# eskiyen bir dosya tutmak olurdu.
[CmdletBinding()]
param(
    # .ico DEĞİL, .png. app.ico'nun kareleri PNG sıkıştırmalı ve GDI+'ın Icon
    # sınıfı onları çözemiyor — `ToBitmap` doğrudan hata veriyor. Aynı simgenin
    # paket varlıklarındaki PNG hâli sorunsuz açılıyor.
    [string] $LogoPath = (Join-Path $PSScriptRoot '..\packaging\Assets\Square150x150Logo.scale-200.png'),
    [string] $OutDir   = (Join-Path $PSScriptRoot '..\build\wizard')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$LogoPath = (Resolve-Path -LiteralPath $LogoPath).Path
$logo = [System.Drawing.Image]::FromFile($LogoPath)
if (-not (Test-Path -LiteralPath $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path

# Uygulamanın kendi renkleri: kaplamanın vurgusu ve panel zemini.
$accent = [System.Drawing.Color]::FromArgb(10, 132, 255)
$back   = [System.Drawing.Color]::FromArgb(24, 24, 27)

function Save-Bmp {
    param([System.Drawing.Bitmap] $Bitmap, [string] $Name)
    $path = Join-Path $OutDir $Name
    $Bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $Bitmap.Dispose()
    Write-Host "  $Name  $($Bitmap.Width)x$($Bitmap.Height)"
    $path
}

function New-Panel {
    param([int] $Width, [int] $Height, [int] $IconSide)

    $bmp = New-Object System.Drawing.Bitmap $Width, $Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

    # Köşegen degrade: düz zemin bir arka plan gibi durur, degrade tasarlanmış.
    $rect = New-Object System.Drawing.Rectangle 0, 0, $Width, $Height
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect, $back, [System.Drawing.Color]::FromArgb(8, 60, 120), 55.0)
    $g.FillRectangle($brush, $rect)
    $brush.Dispose()

    # Seçim dikdörtgeni: uygulamanın yaptığı işin tek çizgilik anlatımı.
    $pen = New-Object System.Drawing.Pen $accent, 2
    $inset = [int]($Width * 0.14)
    $g.DrawRectangle($pen, $inset, [int]($Height * 0.30), ($Width - 2 * $inset),
                     [int]($Height * 0.34))
    $pen.Dispose()

    $target = New-Object System.Drawing.Rectangle(
        [int](($Width - $IconSide) / 2), [int](($Height - $IconSide) / 2),
        $IconSide, $IconSide)
    $g.DrawImage($logo, $target)

    $g.Dispose()
    $bmp
}

Write-Host 'Sihirbaz görselleri:'
Save-Bmp (New-Panel 164 314 64)  'WizardImage.bmp'          | Out-Null
Save-Bmp (New-Panel 328 628 128) 'WizardImage@2x.bmp'       | Out-Null
Save-Bmp (New-Panel 138 140 64)  'WizardSmallImage.bmp'     | Out-Null
Save-Bmp (New-Panel 276 280 128) 'WizardSmallImage@2x.bmp'  | Out-Null
$logo.Dispose()
Write-Host "Çıktı: $OutDir"
