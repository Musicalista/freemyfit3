# Renders an 8x12 monochrome bitmap font (ASCII + Latin-1 accents used in Portuguese) to JSON: {codepoint: [12 row bytes]}
Add-Type -AssemblyName System.Drawing
$out = Join-Path $PSScriptRoot 'font8x12.json'
$chars = New-Object System.Collections.Generic.List[int]
32..126 | ForEach-Object { $chars.Add($_) }
foreach ($cp in @(225,224,226,227,228,233,232,234,235,237,236,238,239,243,242,244,245,246,250,249,251,252,231,241,193,192,194,195,196,201,200,202,205,204,211,210,212,213,214,218,217,219,220,199,209,191,161,186,170,176,8364,163)) { $chars.Add($cp) }
$font = New-Object System.Drawing.Font('Consolas', 10.0, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$res = [ordered]@{}
foreach ($cp in $chars) {
    $bmp = New-Object System.Drawing.Bitmap 8, 12
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
    $g.DrawString([string][char]$cp, $font, [System.Drawing.Brushes]::White, -1.0, -1.0)
    $rows = @()
    for ($y = 0; $y -lt 12; $y++) {
        $b = 0
        for ($x = 0; $x -lt 8; $x++) { if ($bmp.GetPixel($x, $y).R -gt 100) { $b = $b -bor (0x80 -shr $x) } }
        $rows += $b
    }
    $res["$cp"] = $rows
    $g.Dispose(); $bmp.Dispose()
}
$res | ConvertTo-Json -Compress | Set-Content -Path $out -Encoding ascii
"wrote $out ($($chars.Count) glyphs)"
