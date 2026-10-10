param(
    [Parameter(Mandatory=$true)][string]$QtKit,
    [string]$BuildDirectory = 'build/foundation',
    [switch]$IncludeTests
)
$ErrorActionPreference = 'Stop'
$kitRoot = (Resolve-Path -LiteralPath $QtKit).Path
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$deployTool = Join-Path $kitRoot 'bin/windeployqt.exe'
$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
& $deployTool --qmldir (Join-Path $sourceRoot 'apps/client/qml') --no-translations (Join-Path $buildRoot 'mnaka_client.exe')
if ($LASTEXITCODE -ne 0) { throw 'Qt UI deployment failed' }
if (Test-Path -LiteralPath (Join-Path $buildRoot 'mnaka_migrate.exe')) {
    & $deployTool --no-translations --skip-plugin-types sqldrivers (Join-Path $buildRoot 'mnaka_migrate.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt migration utility deployment failed' }
    # Deploy the permitted database driver explicitly; no alternate SQL drivers.
    $driverDir = Join-Path $buildRoot 'sqldrivers'
    New-Item -ItemType Directory -Force -Path $driverDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $kitRoot 'plugins/sqldrivers/qsqlpsql.dll') -Destination $driverDir
}
if ($IncludeTests) {
    Copy-Item -LiteralPath (Join-Path $kitRoot 'bin/Qt6Test.dll') -Destination $buildRoot
    Copy-Item -LiteralPath (Join-Path $kitRoot 'plugins/platforms/qoffscreen.dll') -Destination (Join-Path $buildRoot 'platforms')
}
Write-Output 'Qt assets deployed. libpq and its compatible dependencies require separate PostgreSQL qualification.'
