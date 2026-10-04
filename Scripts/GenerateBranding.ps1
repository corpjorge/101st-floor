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
