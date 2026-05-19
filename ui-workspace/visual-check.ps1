$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root "build\ui-workspace-check"
$indexHtml = (Resolve-Path (Join-Path $PSScriptRoot "index.html")).Path.Replace("\", "/")
$indexUnlimitedHtml = (Resolve-Path (Join-Path $PSScriptRoot "index-unlimited.html")).Path.Replace("\", "/")
$wheelLegHtml = (Resolve-Path (Join-Path $PSScriptRoot "wheel-leg.html")).Path.Replace("\", "/")
$capacitorHtml = (Resolve-Path (Join-Path $PSScriptRoot "capacitor-voltage.html")).Path.Replace("\", "/")
$teamLogoHtml = (Resolve-Path (Join-Path $PSScriptRoot "team-logo-arcs.html")).Path.Replace("\", "/")
$schoolEmblemHtml = (Resolve-Path (Join-Path $PSScriptRoot "school-emblem.html")).Path.Replace("\", "/")
$autoAimIconsHtml = (Resolve-Path (Join-Path $PSScriptRoot "auto-aim-icons.html")).Path.Replace("\", "/")
$movementSpeedHtml = (Resolve-Path (Join-Path $PSScriptRoot "movement-speed.html")).Path.Replace("\", "/")
$helmetHtml = (Resolve-Path (Join-Path $PSScriptRoot "holographic-helmet.html")).Path.Replace("\", "/")
$bottomDashboardHtml = (Resolve-Path (Join-Path $PSScriptRoot "bottom-holographic-dashboard.html")).Path.Replace("\", "/")
$rmJuly2024Html = (Resolve-Path (Join-Path $PSScriptRoot "rm-july-2024.html")).Path.Replace("\", "/")
$indexUrl = "file:///$indexHtml"
$indexUnlimitedUrl = "file:///$indexUnlimitedHtml"
$wheelLegUrl = "file:///$wheelLegHtml"
$capacitorUrl = "file:///$capacitorHtml"
$teamLogoUrl = "file:///$teamLogoHtml"
$schoolEmblemUrl = "file:///$schoolEmblemHtml"
$autoAimIconsUrl = "file:///$autoAimIconsHtml"
$movementSpeedUrl = "file:///$movementSpeedHtml"
$helmetUrl = "file:///$helmetHtml"
$bottomDashboardUrl = "file:///$bottomDashboardHtml"
$rmJuly2024Url = "file:///$rmJuly2024Html"
$chrome = Join-Path $env:LOCALAPPDATA "ms-playwright\chromium_headless_shell-1217\chrome-headless-shell-win64\chrome-headless-shell.exe"

if (-not (Test-Path -LiteralPath $chrome)) {
    throw "Chromium headless shell not found: $chrome"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\index-desktop.png" --virtual-time-budget=1200 $indexUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\index-unlimited.png" --virtual-time-budget=1200 $indexUnlimitedUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\wheel-leg-l0.png" --virtual-time-budget=1200 "${wheelLegUrl}?state=0"
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\wheel-leg-l1.png" --virtual-time-budget=1200 "${wheelLegUrl}?state=1"
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\wheel-leg-l2.png" --virtual-time-budget=1200 "${wheelLegUrl}?state=2"
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\wheel-leg-mobile.png" --virtual-time-budget=1200 $wheelLegUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\capacitor-low.png" --virtual-time-budget=1200 "${capacitorUrl}?voltage=6.0"
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\capacitor-good.png" --virtual-time-budget=1200 "${capacitorUrl}?voltage=22.5"
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\capacitor-full.png" --virtual-time-budget=1200 "${capacitorUrl}?voltage=26.0"
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\capacitor-mobile.png" --virtual-time-budget=1200 $capacitorUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\team-logo-arcs.png" --virtual-time-budget=1200 $teamLogoUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\team-logo-arcs-only.png" --virtual-time-budget=1200 "${teamLogoUrl}?reference=0"
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\team-logo-arcs-mobile.png" --virtual-time-budget=1200 $teamLogoUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\school-emblem.png" --virtual-time-budget=1200 $schoolEmblemUrl
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\school-emblem-mobile.png" --virtual-time-budget=1200 $schoolEmblemUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\auto-aim-icons-vehicle.png" --virtual-time-budget=1200 "${autoAimIconsUrl}?mode=0"
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\auto-aim-icons-energy-a.png" --virtual-time-budget=1200 "${autoAimIconsUrl}?mode=1"
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\auto-aim-icons-energy-b.png" --virtual-time-budget=1200 "${autoAimIconsUrl}?mode=2"
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\auto-aim-icons-outpost.png" --virtual-time-budget=1200 "${autoAimIconsUrl}?mode=3"
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\auto-aim-icons-mobile.png" --virtual-time-budget=1200 $autoAimIconsUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\movement-speed.png" --virtual-time-budget=1200 $movementSpeedUrl
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\movement-speed-mobile.png" --virtual-time-budget=1200 $movementSpeedUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\holographic-helmet.png" --virtual-time-budget=1200 $helmetUrl
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\holographic-helmet-mobile.png" --virtual-time-budget=1200 $helmetUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\bottom-holographic-dashboard.png" --virtual-time-budget=1200 $bottomDashboardUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\bottom-holographic-dashboard-combat.png" --virtual-time-budget=1200 "${bottomDashboardUrl}?preset=combat"
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\bottom-holographic-dashboard-mobile.png" --virtual-time-budget=1200 $bottomDashboardUrl
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\rm-july-2024.png" --virtual-time-budget=1200 $rmJuly2024Url
& $chrome --headless --disable-gpu --window-size=1440,900 "--screenshot=$outDir\rm-july-2024-big.png" --virtual-time-budget=1200 "${rmJuly2024Url}?mode=3"
& $chrome --headless --disable-gpu --window-size=390,844 "--screenshot=$outDir\rm-july-2024-mobile.png" --virtual-time-budget=1200 $rmJuly2024Url

Get-ChildItem -LiteralPath $outDir | Select-Object Name, Length, FullName
