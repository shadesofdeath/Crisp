# check_icons.ps1 — Simge varliklarini DOGRULAR, uretmez.
#
# Simgeler elle tasarlanmis varliklardir ve depoda tutulur. Bu betik onlarin
# yerinde, dogru boyutta ve dogru bicimde oldugunu kontrol eder; eksik ya da
# bozuk bir varlik derlemeyi SESSIZCE degil, aciklamayla durdurur.
#
# Bir uretici olsaydi, CMake her yapilandirmada elle cizilmis dosyalarin
# uzerine yazardi.
#
# Cikti dizeleri BILEREK ASCII: bu dosya BOM'suz UTF-8 ve PowerShell 5.1
# BOM'suz dosyalari ANSI sanip Turkce karakterleri bozar.
#
# Kullanim (proje kokunden):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/check_icons.ps1

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root     = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$iconDir  = Join-Path $root 'res\icons'
$assetDir = Join-Path $root 'packaging\Assets'

$problems = New-Object System.Collections.ArrayList
function Add-Problem {
    param([string]$Text)
    [void]$script:problems.Add($Text)
}

# --- ICO dogrulamasi ---------------------------------------------------------
# ICO basligi elle okunur: System.Drawing.Icon yalnizca tek bir kareyi acar ve
# eksik bir boyutu fark etmez. Asil risk 16 px karesinin unutulmasidir; o
# durumda Windows 256 px kareyi kucultur ve tepsi simgesi bulanik cikar.
$IcoFrames = @(16, 20, 24, 32, 48, 256)

function Test-Ico {
    param([string]$Name)

    $path = Join-Path $iconDir $Name
    if (-not (Test-Path -LiteralPath $path)) {
        Add-Problem "$Name : dosya yok"
        return
    }

    $b = [IO.File]::ReadAllBytes($path)
    if ($b.Length -lt 22) { Add-Problem "$Name : dosya cok kucuk"; return }

    $type = [BitConverter]::ToUInt16($b, 2)
    if ($type -ne 1) { Add-Problem "$Name : ICO degil (tur=$type)"; return }

    $count = [BitConverter]::ToUInt16($b, 4)
    $found = @()
    for ($i = 0; $i -lt $count; $i++) {
        $o = 6 + $i * 16
        if ($o + 16 -gt $b.Length) { Add-Problem "$Name : girdi tablosu kesik"; return }

        $w = if ($b[$o] -eq 0) { 256 } else { [int]$b[$o] }
        $bits = [BitConverter]::ToUInt16($b, $o + 6)
        $size = [BitConverter]::ToUInt32($b, $o + 8)
        $off  = [BitConverter]::ToUInt32($b, $o + 12)

        if ($off + $size -gt $b.Length) { Add-Problem "$Name : ${w}px karesi dosya disina tasiyor"; continue }
        if ($bits -ne 32) { Add-Problem "$Name : ${w}px karesi $bits bpp, 32 olmali" }
        if (-not ($b[$off] -eq 0x89 -and $b[$off+1] -eq 0x50)) {
            Add-Problem "$Name : ${w}px karesi PNG sikistirmali degil"
        }
        $found += $w
    }

    foreach ($need in $IcoFrames) {
        if ($found -notcontains $need) { Add-Problem "$Name : ${need}px karesi eksik" }
    }
}

# --- PNG dogrulamasi ---------------------------------------------------------
# .NET'in varsayilan yuvarlamasi bankaci yuvarlamasidir (62,5 -> 62). Windows'un
# bekledigi degerler yukari yuvarlamayla gelir: 50 -> 63, 71 -> 89, 150 -> 188.
function Get-ScaledPx {
    param([int]$Base, [int]$Scale)
    return [int][math]::Round($Base * $Scale / 100.0, 0, [MidpointRounding]::AwayFromZero)
}

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

function Test-Png {
    param([string]$Name, [int]$Width, [int]$Height)

    $path = Join-Path $assetDir $Name
    if (-not (Test-Path -LiteralPath $path)) {
        Add-Problem "$Name : dosya yok"
        return
    }

    $img = [Drawing.Image]::FromFile($path)
    try {
        if ($img.Width -ne $Width -or $img.Height -ne $Height) {
            Add-Problem ("{0} : {1}x{2}, beklenen {3}x{4}" -f $Name, $img.Width, $img.Height, $Width, $Height)
        }
        # Alfa kanali sart: MSIX kutucugu simgeyi kendi zemininin uzerine
        # bindirir, opak bir PNG kutucukta beyaz kare olarak gorunur.
        if ("$($img.PixelFormat)" -notmatch 'Argb') {
            Add-Problem ("{0} : {1}, alfa kanali yok" -f $Name, $img.PixelFormat)
        }
    }
    finally { $img.Dispose() }
}

# ---------------------------------------------------------------------------
# Kosum
# ---------------------------------------------------------------------------

Write-Host 'Crisp simge dogrulamasi...'

Test-Ico -Name 'app.ico'
Test-Ico -Name 'tray_dark.ico'
Test-Ico -Name 'tray_light.ico'

$pngCount = 0
foreach ($a in $AssetBases) {
    Test-Png -Name "$($a.Name).png" -Width $a.W -Height $a.H
    $pngCount++
    foreach ($s in $AssetScales) {
        Test-Png -Name "$($a.Name).scale-$s.png" `
                 -Width  (Get-ScaledPx -Base $a.W -Scale $s) `
                 -Height (Get-ScaledPx -Base $a.H -Scale $s)
        $pngCount++
    }
}
foreach ($t in $TargetSizes) {
    Test-Png -Name "Square44x44Logo.targetsize-$t.png" -Width $t -Height $t
    Test-Png -Name "Square44x44Logo.targetsize-${t}_altform-unplated.png" -Width $t -Height $t
    $pngCount += 2
}

if ($problems.Count -gt 0) {
    Write-Host ''
    Write-Host ('{0} sorun bulundu:' -f $problems.Count) -ForegroundColor Red
    foreach ($p in $problems) { Write-Host "  $p" }
    Write-Host ''
    throw 'Simge dogrulamasi basarisiz.'
}

Write-Host ('  3 ICO ({0} kare) ve {1} PNG dogrulandi.' -f ($IcoFrames.Count * 3), $pngCount)
