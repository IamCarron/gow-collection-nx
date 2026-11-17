# Gera os arquivos do launcher a partir das SUAS imagens/música (não versionados):
#   launcher/romfs/bg.bgra      capa 1280x720 em BGRA (qualquer imagem 16:9; é redimensionada)
#   launcher/romfs/music.mp3    música de fundo (opcional, tocada em loop)
#   res/icon_launcher.jpg       ícone 256x256 do hbmenu (recorte quadrado da capa; -IconX escolhe a coluna)
# Uso: powershell -File tools/make_launcher_assets.ps1 -Image capa.jpg [-Music musica.mp3] [-IconX 570]
param([Parameter(Mandatory)][string]$Image, [string]$Music, [int]$IconX = -1)
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$romfs = Join-Path $root 'launcher\romfs'; $res = Join-Path $root 'res'
New-Item -ItemType Directory -Force $romfs, $res | Out-Null

$src = [System.Drawing.Image]::FromFile((Resolve-Path $Image))
$bmp = New-Object System.Drawing.Bitmap 1280, 720, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, 1280, 720)); $g.Dispose()
$r = New-Object System.Drawing.Rectangle 0, 0, 1280, 720
$d = $bmp.LockBits($r, 'ReadOnly', [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$buf = New-Object byte[] ($d.Stride * 720)
[System.Runtime.InteropServices.Marshal]::Copy($d.Scan0, $buf, 0, $buf.Length); $bmp.UnlockBits($d); $bmp.Dispose()
[System.IO.File]::WriteAllBytes((Join-Path $romfs 'bg.bgra'), $buf)

$side = [Math]::Min($src.Width, $src.Height)
if ($IconX -lt 0) { $IconX = [int](($src.Width - $side) / 2) }
$ico = New-Object System.Drawing.Bitmap 256, 256
$g = [System.Drawing.Graphics]::FromImage($ico); $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, 256, 256), (New-Object System.Drawing.Rectangle $IconX, 0, $side, $side), [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $ico.Save((Join-Path $res 'icon_launcher.jpg'), [System.Drawing.Imaging.ImageFormat]::Jpeg); $ico.Dispose(); $src.Dispose()

if ($Music) { Copy-Item $Music (Join-Path $romfs 'music.mp3') -Force }
"ok: $romfs, $res"
