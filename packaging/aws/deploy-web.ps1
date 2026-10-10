param(
    [Parameter(Mandatory=$true)][string]$Bucket,
    [Parameter(Mandatory=$true)][string]$DistributionId,
    [string]$SourceDirectory = 'build/web-dist'
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
# File names are not content-hashed: browsers revalidate every load and the edge is invalidated below.
$cacheControl = 'public,max-age=0,s-maxage=86400,must-revalidate'
# index.html goes last so it never points at files that are not there yet.
$files = @(
    @{ Key = 'mnaka_client.wasm'; File = 'mnaka_client.wasm.br'; Type = 'application/wasm'; Encoding = 'br' },
    @{ Key = 'mnaka_client.js'; File = 'mnaka_client.js.br'; Type = 'text/javascript; charset=utf-8'; Encoding = 'br' },
    @{ Key = 'qtloader.js'; File = 'qtloader.js'; Type = 'text/javascript; charset=utf-8' },
    @{ Key = 'mnaka-boot.js'; File = 'mnaka-boot.js'; Type = 'text/javascript; charset=utf-8' },
    @{ Key = 'mnaka-shell.css'; File = 'mnaka-shell.css'; Type = 'text/css; charset=utf-8' },
    @{ Key = 'index.html'; File = 'index.html'; Type = 'text/html; charset=utf-8' }
)
foreach ($entry in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $entry.File))) {
        throw "Missing $($entry.File); run packaging/web/stage-web.ps1 first"
    }
}
foreach ($entry in $files) {
    $arguments = @('s3', 'cp', (Join-Path $sourceRoot $entry.File), "s3://$Bucket/$($entry.Key)",
        '--content-type', $entry.Type, '--cache-control', $cacheControl, '--only-show-errors')
    if ($entry.Encoding) { $arguments += @('--content-encoding', $entry.Encoding) }
    & aws @arguments
    if ($LASTEXITCODE -ne 0) { throw "Upload of $($entry.Key) failed" }
}
& aws cloudfront create-invalidation --distribution-id $DistributionId --paths '/*' --query 'Invalidation.Id' --output text
if ($LASTEXITCODE -ne 0) { throw 'CloudFront invalidation failed' }
Write-Output "Published to s3://$Bucket and invalidated distribution $DistributionId."
