param(
    [Parameter(Mandatory=$true)][string]$QtKit,
    [Parameter(Mandatory=$true)][string]$WasmQtKit,
    [Parameter(Mandatory=$true)][string]$Emsdk,
    [string]$OutputDirectory = 'build/web-dist',
    # Both or neither. Only a publishable key may be published with the page.
    [string]$SupabaseUrl = '',
    [string]$SupabasePublishableKey = '',
    # For hosts that cannot serve pre-encoded files, such as Amplify Hosting.
    [switch]$SkipCompression
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
$utf8 = New-Object Text.UTF8Encoding($false)
Push-Location -LiteralPath $sourceRoot
try {
    & cmake --preset wasm-release
    if ($LASTEXITCODE -ne 0) { throw 'WebAssembly configure failed' }
    & cmake --build --preset wasm-release
    if ($LASTEXITCODE -ne 0) { throw 'WebAssembly build failed' }
    $buildRoot = Join-Path $sourceRoot 'build/wasm-release'
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path

    # A release is named after its content, so the same build always gets the same name and any
    # change gets a new one. Its files live under r/<release>/ and are never overwritten.
    $versioned = @(
        @{ Name = 'mnaka_client.js'; From = $buildRoot }, @{ Name = 'mnaka_client.wasm'; From = $buildRoot },
        @{ Name = 'qtloader.js'; From = $buildRoot }, @{ Name = 'mnaka-boot.js'; From = $PSScriptRoot },
        @{ Name = 'mnaka-shell.css'; From = $PSScriptRoot })
    $sha = [Security.Cryptography.SHA256]::Create()
    $digest = New-Object IO.MemoryStream
    foreach ($entry in $versioned) {
        $hash = $sha.ComputeHash([IO.File]::ReadAllBytes((Join-Path $entry.From $entry.Name)))
        $digest.Write($hash, 0, $hash.Length)
    }
    $release = (-join ($sha.ComputeHash($digest.ToArray()) | ForEach-Object { $_.ToString('x2') })).Substring(0, 12)

    # Earlier releases and the files of the old flat layout are removed; hosts publish this folder as-is.
    $releasesRoot = Join-Path $outputRoot 'r'
    if (Test-Path -LiteralPath $releasesRoot) { Remove-Item -LiteralPath $releasesRoot -Recurse -Force }
    foreach ($stale in 'mnaka_client.js', 'mnaka_client.wasm', 'qtloader.js', 'mnaka-boot.js', 'mnaka-shell.css', 'mnaka_client.js.br', 'mnaka_client.wasm.br') {
        $path = Join-Path $outputRoot $stale
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    $releaseRoot = Join-Path $releasesRoot $release
    New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
    foreach ($entry in $versioned) { Copy-Item -LiteralPath (Join-Path $entry.From $entry.Name) -Destination $releaseRoot }
    $wasmBytes = (Get-Item -LiteralPath (Join-Path $releaseRoot 'mnaka_client.wasm')).Length

    foreach ($name in 'manifest.json', 'icon-192.png', 'icon-512.png', 'icon-maskable-512.png') {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $outputRoot
    }
    Copy-Item -LiteralPath (Join-Path $sourceRoot 'apps/client/assets/logo.png') -Destination (Join-Path $outputRoot 'mnaka-logo.png')
    $page = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'index.html')).Replace('__RELEASE__', $release).Replace('__WASM_BYTES__', "$wasmBytes")
    [IO.File]::WriteAllText((Join-Path $outputRoot 'index.html'), $page, $utf8)
    # The running page compares itself with this to notice that it is out of date.
    [IO.File]::WriteAllText((Join-Path $outputRoot 'release.json'), (@{ id = $release; wasmBytes = $wasmBytes } | ConvertTo-Json -Compress), $utf8)

    $configPath = Join-Path $outputRoot 'mnaka-config.json'
    if ($SupabaseUrl) {
        $config = @{ supabaseUrl = $SupabaseUrl; supabasePublishableKey = $SupabasePublishableKey } | ConvertTo-Json
        [IO.File]::WriteAllText($configPath, $config, $utf8)
    } elseif (Test-Path -LiteralPath $configPath) { Remove-Item -LiteralPath $configPath }
    if (-not $SkipCompression) {
        & $node.FullName (Join-Path $PSScriptRoot 'compress.mjs') (Join-Path $releaseRoot 'mnaka_client.wasm') (Join-Path $releaseRoot 'mnaka_client.js')
        if ($LASTEXITCODE -ne 0) { throw 'Brotli compression failed' }
    }
} finally { Pop-Location }
if ($SupabaseUrl) { Write-Output "Browser client release $release staged in $outputRoot for account service $SupabaseUrl (publishable key only)." }
else { Write-Output "Browser client release $release staged in $outputRoot. It contains no credentials or endpoints." }
