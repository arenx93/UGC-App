# Downloads the official Codex CLI for Windows x64 and saves it as $Destination (codex.exe),
# bundled next to Framecraft.exe so users only need to sign in with ChatGPT.
# Source: github.com/openai/codex releases, with the @openai/codex npm package as a fallback.
param([Parameter(Mandatory = $true)][string]$Destination)
$ErrorActionPreference = "Stop"
$work = Join-Path $env:RUNNER_TEMP "codex-download"
if (-not $env:RUNNER_TEMP) { $work = Join-Path $env:TEMP "codex-download" }
Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $work | Out-Null

function From-Release {
    $headers = @{ "User-Agent" = "framecraft-ci" }
    if ($env:GITHUB_TOKEN) { $headers["Authorization"] = "Bearer $env:GITHUB_TOKEN" }
    $release = Invoke-RestMethod -Uri "https://api.github.com/repos/openai/codex/releases/latest" -Headers $headers
    $asset = $release.assets | Where-Object { $_.name -match "x86_64-pc-windows-msvc" -and $_.name -match "\.(zip|exe)$" -and $_.name -notmatch "responses|proxy|sandbox|setup" } | Select-Object -First 1
    if (-not $asset) { return $false }
    $file = Join-Path $work $asset.name
    Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $file -Headers @{ "User-Agent" = "framecraft-ci" }
    if ($file -like "*.zip") {
        Expand-Archive $file -DestinationPath (Join-Path $work "release") -Force
        $exe = Get-ChildItem (Join-Path $work "release") -Recurse -Filter "codex*.exe" | Select-Object -First 1
    } else {
        $exe = Get-Item $file
    }
    if (-not $exe) { return $false }
    Copy-Item $exe.FullName $Destination -Force
    return $true
}

function From-Npm {
    Push-Location $work
    npm pack "@openai/codex@latest" | Out-Null
    $tgz = Get-ChildItem -Filter "openai-codex-*.tgz" | Select-Object -First 1
    tar -xzf $tgz.FullName
    Pop-Location
    $exe = Get-ChildItem $work -Recurse -Filter "codex.exe" | Where-Object { $_.FullName -match "x86_64-pc-windows-msvc" } | Select-Object -First 1
    if (-not $exe) { return $false }
    Copy-Item $exe.FullName $Destination -Force
    return $true
}

$ok = $false
try { $ok = From-Release } catch { Write-Warning "Release download failed: $_" }
if (-not $ok) { $ok = From-Npm }
if (-not $ok) { throw "No se pudo descargar Codex para Windows" }
& $Destination --version
Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
