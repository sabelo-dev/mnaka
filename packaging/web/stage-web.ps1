param(
    [Parameter(Mandatory=$true)][string]$QtKit,
    [Parameter(Mandatory=$true)][string]$WasmQtKit,
    [Parameter(Mandatory=$true)][string]$Emsdk,
    [string]$OutputDirectory = 'build/web-dist',
    # Both or neither. Only a publishable key may be published with the page.
    [string]$SupabaseUrl = '',
    [string]$SupabasePublishableKey = ''
)
$ErrorActionPreference = 'Stop'
if ([bool]$SupabaseUrl -ne [bool]$SupabasePublishableKey) { throw 'Pass -SupabaseUrl and -SupabasePublishableKey together' }
if ($SupabasePublishableKey -and -not $SupabasePublishableKey.StartsWith('sb_publishable_')) {
    throw 'Refusing to publish a key that is not a Supabase publishable key'
}
$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$emsdkRoot = (Resolve-Path -LiteralPath $Emsdk).Path
$node = Get-ChildItem -Path (Join-Path $emsdkRoot 'node') -Filter node.exe -Recurse | Select-Object -First 1
$python = Get-ChildItem -Path (Join-Path $emsdkRoot 'python') -Filter python.exe -Recurse | Select-Object -First 1
if (-not $node -or -not $python) { throw 'Install and activate an Emscripten SDK first; its node and python were not found' }
# The Qt toolchain file locates Emscripten through EMSDK; emcc needs the rest.
$env:EMSDK = $emsdkRoot -replace '\\', '/'
$env:EM_CONFIG = "$($env:EMSDK)/.emscripten"
$env:EMSDK_NODE = $node.FullName
$env:EMSDK_PYTHON = $python.FullName
$env:MNAKA_QT_KIT = (Resolve-Path -LiteralPath $QtKit).Path -replace '\\', '/'
$env:MNAKA_WASM_QT_KIT = (Resolve-Path -LiteralPath $WasmQtKit).Path -replace '\\', '/'
Push-Location -LiteralPath $sourceRoot
try {
    & cmake --preset wasm-release
    if ($LASTEXITCODE -ne 0) { throw 'WebAssembly configure failed' }
    & cmake --build --preset wasm-release
    if ($LASTEXITCODE -ne 0) { throw 'WebAssembly build failed' }
    $buildRoot = Join-Path $sourceRoot 'build/wasm-release'
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
    # Only these files are published; Qt's generated sample page and logo are left behind.
    foreach ($name in 'mnaka_client.js', 'mnaka_client.wasm', 'qtloader.js') {
        Copy-Item -LiteralPath (Join-Path $buildRoot $name) -Destination $outputRoot
    }
    foreach ($name in 'index.html', 'mnaka-shell.css', 'mnaka-boot.js') {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $outputRoot
    }
    $configPath = Join-Path $outputRoot 'mnaka-config.json'
    if ($SupabaseUrl) {
        $config = @{ supabaseUrl = $SupabaseUrl; supabasePublishableKey = $SupabasePublishableKey } | ConvertTo-Json
        [IO.File]::WriteAllText($configPath, $config, (New-Object Text.UTF8Encoding($false)))
    } elseif (Test-Path -LiteralPath $configPath) { Remove-Item -LiteralPath $configPath }
    & $node.FullName (Join-Path $PSScriptRoot 'compress.mjs') (Join-Path $outputRoot 'mnaka_client.wasm') (Join-Path $outputRoot 'mnaka_client.js')
    if ($LASTEXITCODE -ne 0) { throw 'Brotli compression failed' }
} finally { Pop-Location }
if ($SupabaseUrl) { Write-Output "Browser client staged in $outputRoot for account service $SupabaseUrl (publishable key only)." }
else { Write-Output "Browser client staged in $outputRoot. It contains no credentials or endpoints." }
