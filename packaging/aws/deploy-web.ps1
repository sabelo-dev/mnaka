param(
    [Parameter(Mandatory=$true)][string]$Bucket,
    [Parameter(Mandatory=$true)][string]$DistributionId,
    [string]$SourceDirectory = 'build/web-dist'
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
$releaseInfo = Join-Path $sourceRoot 'release.json'
if (-not (Test-Path -LiteralPath $releaseInfo)) { throw 'Missing release.json; run packaging/web/stage-web.ps1 first' }
$release = (Get-Content -LiteralPath $releaseInfo -Raw | ConvertFrom-Json).id
# Release files never change once published, so they are cached for good. The page and the small
# files beside it name the release and are revalidated on every load.
$immutable = 'public,max-age=31536000,immutable'
$revalidate = 'public,max-age=0,must-revalidate'
$prefix = "r/$release"
# The page goes last so it never points at files that are not there yet. Earlier releases are
# left in the bucket: a page that is still open keeps working until it reloads.
$files = @(
    @{ Key = "$prefix/mnaka_client.wasm"; File = "$prefix/mnaka_client.wasm.br"; Type = 'application/wasm'; Encoding = 'br'; Cache = $immutable },
    @{ Key = "$prefix/mnaka_client.js"; File = "$prefix/mnaka_client.js.br"; Type = 'text/javascript; charset=utf-8'; Encoding = 'br'; Cache = $immutable },
    @{ Key = "$prefix/qtloader.js"; File = "$prefix/qtloader.js"; Type = 'text/javascript; charset=utf-8'; Cache = $immutable },
    @{ Key = "$prefix/mnaka-boot.js"; File = "$prefix/mnaka-boot.js"; Type = 'text/javascript; charset=utf-8'; Cache = $immutable },
    @{ Key = "$prefix/mnaka-shell.css"; File = "$prefix/mnaka-shell.css"; Type = 'text/css; charset=utf-8'; Cache = $immutable },
    @{ Key = 'mnaka-logo.png'; File = 'mnaka-logo.png'; Type = 'image/png'; Cache = $revalidate },
    @{ Key = 'icon-192.png'; File = 'icon-192.png'; Type = 'image/png'; Cache = $revalidate },
    @{ Key = 'icon-512.png'; File = 'icon-512.png'; Type = 'image/png'; Cache = $revalidate },
    @{ Key = 'icon-maskable-512.png'; File = 'icon-maskable-512.png'; Type = 'image/png'; Cache = $revalidate },
    @{ Key = 'manifest.json'; File = 'manifest.json'; Type = 'application/manifest+json'; Cache = $revalidate },
    @{ Key = 'mnaka-config.json'; File = 'mnaka-config.json'; Type = 'application/json'; Cache = $revalidate; Optional = $true },
    @{ Key = 'release.json'; File = 'release.json'; Type = 'application/json'; Cache = $revalidate },
    @{ Key = 'index.html'; File = 'index.html'; Type = 'text/html; charset=utf-8'; Cache = $revalidate }
)
# Present only when the bundle was staged with an account service.
$files = @($files | Where-Object { -not $_.Optional -or (Test-Path -LiteralPath (Join-Path $sourceRoot $_.File)) })
foreach ($entry in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $entry.File))) {
        throw "Missing $($entry.File); run packaging/web/stage-web.ps1 without -SkipCompression first"
    }
}
foreach ($entry in $files) {
    $arguments = @('s3', 'cp', (Join-Path $sourceRoot $entry.File), "s3://$Bucket/$($entry.Key)",
        '--content-type', $entry.Type, '--cache-control', $entry.Cache, '--only-show-errors')
    if ($entry.Encoding) { $arguments += @('--content-encoding', $entry.Encoding) }
    & aws @arguments
    if ($LASTEXITCODE -ne 0) { throw "Upload of $($entry.Key) failed" }
}
# Only the unversioned files can be stale at the edge.
& aws cloudfront create-invalidation --distribution-id $DistributionId --paths '/index.html' '/' '/release.json' '/mnaka-config.json' '/manifest.json' --query 'Invalidation.Id' --output text
if ($LASTEXITCODE -ne 0) { throw 'CloudFront invalidation failed' }
Write-Output "Published release $release to s3://$Bucket and invalidated distribution $DistributionId."
