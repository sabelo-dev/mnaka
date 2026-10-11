param(
    [Parameter(Mandatory=$true)][string]$QtKit,          # desktop host kit, e.g. C:/Qt/6.12.0/mingw_64
    [Parameter(Mandatory=$true)][string]$AndroidQtKit,   # e.g. C:/Qt/6.12.0/android_arm64_v8a
    [Parameter(Mandatory=$true)][string]$AndroidSdk,
    [Parameter(Mandatory=$true)][string]$AndroidNdk,
    [Parameter(Mandatory=$true)][string]$Jdk,
    [string]$OutputDirectory = 'build/android-apk',
    # Both or neither. Android has no environment to configure, so these are compiled in.
    [string]$SupabaseUrl = '',
    [string]$SupabasePublishableKey = ''
)
$ErrorActionPreference = 'Stop'
if ([bool]$SupabaseUrl -ne [bool]$SupabasePublishableKey) { throw 'Pass -SupabaseUrl and -SupabasePublishableKey together' }
if ($SupabasePublishableKey -and -not $SupabasePublishableKey.StartsWith('sb_publishable_')) {
    throw 'Refusing to compile in a key that is not a Supabase publishable key'
}
$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$env:MNAKA_QT_KIT = (Resolve-Path -LiteralPath $QtKit).Path -replace '\\', '/'
$env:MNAKA_ANDROID_QT_KIT = (Resolve-Path -LiteralPath $AndroidQtKit).Path -replace '\\', '/'
$env:ANDROID_SDK_ROOT = (Resolve-Path -LiteralPath $AndroidSdk).Path -replace '\\', '/'
$env:ANDROID_NDK_ROOT = (Resolve-Path -LiteralPath $AndroidNdk).Path -replace '\\', '/'
$env:JAVA_HOME = (Resolve-Path -LiteralPath $Jdk).Path
$env:PATH = (Join-Path $env:JAVA_HOME 'bin') + ';' + $env:PATH
Push-Location -LiteralPath $sourceRoot
try {
    & cmake --preset android-debug "-DMNAKA_SUPABASE_URL=$SupabaseUrl" "-DMNAKA_SUPABASE_PUBLISHABLE_KEY=$SupabasePublishableKey"
    if ($LASTEXITCODE -ne 0) { throw 'Android configure failed' }
    # Builds the native library, then packages and signs it with the local debug key.
    & cmake --build --preset android-debug --target apk
    if ($LASTEXITCODE -ne 0) { throw 'Android build failed' }
    $apk = Get-ChildItem -Path (Join-Path $sourceRoot 'build/android-debug') -Filter '*.apk' -Recurse |
        Where-Object { $_.FullName -notmatch 'intermediates' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $apk) { throw 'No APK was produced' }
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $target = Join-Path (Resolve-Path -LiteralPath $OutputDirectory).Path 'mnaka.apk'
    Copy-Item -LiteralPath $apk.FullName -Destination $target -Force
} finally { Pop-Location }
Write-Output "APK: $target ($([math]::Round((Get-Item $target).Length / 1MB, 1)) MB). Debug-signed: fine for installing yourself, not for a store."
