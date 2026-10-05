<#
.SYNOPSIS
    Generate the native startup image and vector logo from reproducible architectural linework.
#>
[CmdletBinding()]
param()
. "$PSScriptRoot/Common.ps1"
Add-Type -AssemblyName System.Drawing
$brandFolder = Join-Path $RepoRoot 'Content/Splash'
$brandVectorFolder = Join-Path $RepoRoot 'Resources/Branding'
New-Item -ItemType Directory -Force -Path $brandFolder, $brandVectorFolder | Out-Null
$brandBitmap = [Drawing.Bitmap]::new(720, 420)
$brandGraphics = [Drawing.Graphics]::FromImage($brandBitmap)
$brandGraphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$brandGraphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$brandGraphics.Clear([Drawing.ColorTranslator]::FromHtml('#1E2328'))
$brandPen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#D6AD78'), 4)
$brandPen.StartCap = $brandPen.EndCap = [Drawing.Drawing2D.LineCap]::Square
$brandLines = @(
    @(2,2,94,2), @(94,2,94,94), @(94,94,2,94), @(2,94,2,2),
    @(20,70,20,26), @(20,26,32,18), @(32,18,32,70),
    @(44,70,44,26), @(44,26,64,26), @(64,26,64,70), @(64,70,44,70),
    @(76,70,76,26), @(76,26,84,20)
)
$brandSvgLines = foreach ($line in $brandLines) {
    $brandGraphics.DrawLine($brandPen, [float](312+$line[0]), [float](76+$line[1]), [float](312+$line[2]), [float](76+$line[3]))
    '<line x1="{0}" y1="{1}" x2="{2}" y2="{3}"/>' -f $line
}
$brandSvg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96"><g fill="none" stroke="#D6AD78" stroke-width="4" stroke-linecap="square">' + ($brandSvgLines -join '') + '</g></svg>'
[IO.File]::WriteAllText((Join-Path $brandVectorFolder 'logo.svg'), $brandSvg + "`n", [Text.UTF8Encoding]::new($false))
$brandCentered = [Drawing.StringFormat]::new()
$brandCentered.Alignment = [Drawing.StringAlignment]::Center
$brandWhite = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#EEF0EC'))
$brandMuted = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#B3BAB9'))
$brandTrack = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#3C454B'))
$brandTitle = [Drawing.Font]::new('Segoe UI', 34, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$brandSubtitle = [Drawing.Font]::new('Segoe UI', 14, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
try {
    $brandGraphics.DrawString('101st FLOOR', $brandTitle, $brandWhite, [Drawing.RectangleF]::new(0,205,720,50), $brandCentered)
    $brandGraphics.DrawString('ESTUDIO ARQUITECTÓNICO', $brandSubtitle, $brandMuted, [Drawing.RectangleF]::new(0,266,720,24), $brandCentered)
    $brandGraphics.DrawString('Preparando tu espacio de diseño…', $brandSubtitle, $brandMuted, [Drawing.RectangleF]::new(0,334,720,25), $brandCentered)
    $brandGraphics.FillRectangle($brandTrack, 140, 374, 440, 4)
    $brandBitmap.Save((Join-Path $brandFolder 'Splash.png'), [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $brandGraphics.Dispose(); $brandBitmap.Dispose(); $brandPen.Dispose(); $brandCentered.Dispose()
    $brandWhite.Dispose(); $brandMuted.Dispose(); $brandTrack.Dispose()
    $brandTitle.Dispose(); $brandSubtitle.Dispose()
}
Write-Output (Join-Path $brandFolder 'Splash.png')

# ICO uses ordinary BGRA DIB frames, including the AND masks required by the resource compiler.
$brandIconFolder = Join-Path $RepoRoot 'Build/Windows'
New-Item -ItemType Directory -Force -Path $brandIconFolder | Out-Null
$brandFrames = foreach ($brandSize in 16, 32, 48, 256) {
    $brandIconBitmap = [Drawing.Bitmap]::new($brandSize, $brandSize)
    $brandIconGraphics = [Drawing.Graphics]::FromImage($brandIconBitmap)
    $brandIconGraphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $brandIconGraphics.Clear([Drawing.ColorTranslator]::FromHtml('#1E2328'))
    $brandIconPen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#D6AD78'), [float](4*$brandSize/112))
    $brandIconPen.StartCap = $brandIconPen.EndCap = [Drawing.Drawing2D.LineCap]::Square
    foreach ($line in $brandLines) {
        $brandIconGraphics.DrawLine($brandIconPen, [float](($line[0]+8)*$brandSize/112), [float](($line[1]+8)*$brandSize/112),
            [float](($line[2]+8)*$brandSize/112), [float](($line[3]+8)*$brandSize/112))
    }
    if ($brandSize -eq 256) { $brandIconBitmap.Save((Join-Path $brandIconFolder 'InstallerLogo.bmp'), [Drawing.Imaging.ImageFormat]::Bmp) }
    $brandFrameStream = [IO.MemoryStream]::new()
    $brandFrameWriter = [IO.BinaryWriter]::new($brandFrameStream)
    $brandMaskBytes = [int]([Math]::Ceiling($brandSize/32.0)*4*$brandSize)
    $brandFrameWriter.Write([uint32]40); $brandFrameWriter.Write([int32]$brandSize); $brandFrameWriter.Write([int32]($brandSize*2))
    $brandFrameWriter.Write([uint16]1); $brandFrameWriter.Write([uint16]32); $brandFrameWriter.Write([uint32]0)
    $brandFrameWriter.Write([uint32]($brandSize*$brandSize*4+$brandMaskBytes))
    1..4 | ForEach-Object { $brandFrameWriter.Write([uint32]0) }
    for ($brandY = $brandSize-1; $brandY -ge 0; $brandY--) {
        for ($brandX = 0; $brandX -lt $brandSize; $brandX++) {
            $brandPixel = $brandIconBitmap.GetPixel($brandX, $brandY)
            $brandFrameWriter.Write([byte]$brandPixel.B); $brandFrameWriter.Write([byte]$brandPixel.G)
            $brandFrameWriter.Write([byte]$brandPixel.R); $brandFrameWriter.Write([byte]$brandPixel.A)
        }
    }
    $brandFrameWriter.Write([byte[]]::new($brandMaskBytes))
    [pscustomobject]@{ Size = $brandSize; Data = $brandFrameStream.ToArray() }
    $brandFrameWriter.Dispose(); $brandFrameStream.Dispose(); $brandIconGraphics.Dispose(); $brandIconBitmap.Dispose(); $brandIconPen.Dispose()
}
$brandIconStream = [IO.File]::Create((Join-Path $brandIconFolder 'Application.ico'))
$brandIconWriter = [IO.BinaryWriter]::new($brandIconStream)
try {
    $brandIconWriter.Write([uint16]0); $brandIconWriter.Write([uint16]1); $brandIconWriter.Write([uint16]$brandFrames.Count)
    $brandOffset = 6+16*$brandFrames.Count
    foreach ($brandFrame in $brandFrames) {
        $brandSizeByte = [byte]($brandFrame.Size % 256)
        $brandIconWriter.Write($brandSizeByte); $brandIconWriter.Write($brandSizeByte)
        $brandIconWriter.Write([byte]0); $brandIconWriter.Write([byte]0)
        $brandIconWriter.Write([uint16]1); $brandIconWriter.Write([uint16]32)
        $brandIconWriter.Write([uint32]$brandFrame.Data.Length); $brandIconWriter.Write([uint32]$brandOffset)
        $brandOffset += $brandFrame.Data.Length
    }
    foreach ($brandFrame in $brandFrames) { $brandIconWriter.Write([byte[]]$brandFrame.Data) }
}
finally { $brandIconWriter.Dispose(); $brandIconStream.Dispose() }
Write-Output (Join-Path $brandIconFolder 'Application.ico')
