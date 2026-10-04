# Downloads the official Codex CLI for Windows x64 into $Destination (a "codex" folder next to
# Framecraft.exe: bin\codex.exe plus the sandbox helpers), so users only need to sign in with ChatGPT.
# Source: github.com/openai/codex releases, with the @openai/codex npm package as a fallback.
param([Parameter(Mandatory = $true)][string]$Destination)
$ErrorActionPreference = "Stop"
$workRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } elseif ($env:TEMP) { $env:TEMP } else { [System.IO.Path]::GetTempPath() }
$work = Join-Path $workRoot "codex-download"
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
    New-Item -ItemType Directory -Force (Join-Path $Destination "bin") | Out-Null
    Copy-Item $exe.FullName (Join-Path $Destination "bin\codex.exe") -Force
    return $true
}

function From-Npm {
    # The binaries live in a platform build of the package: @openai/codex@<version>-win32-x64.
    $version = (npm view "@openai/codex" version).Trim()
    Push-Location $work
    npm pack "@openai/codex@$version-win32-x64" | Out-Null
    $tgz = Get-ChildItem -Filter "*.tgz" | Select-Object -First 1
    tar -xzf $tgz.FullName
    Pop-Location
    $vendor = Get-ChildItem $work -Recurse -Directory -Filter "x86_64-pc-windows-msvc" | Select-Object -First 1
    if (-not $vendor) { return $false }
    New-Item -ItemType Directory -Force (Join-Path $Destination "bin") | Out-Null
    Copy-Item (Join-Path $vendor.FullName "bin\codex.exe") (Join-Path $Destination "bin\codex.exe") -Force
    foreach ($folder in @("codex-path", "codex-resources")) {
        $source = Join-Path $vendor.FullName $folder
        if (Test-Path $source) { Copy-Item $source (Join-Path $Destination $folder) -Recurse -Force }
    }
    # Voice support is not used by Framecraft.
    Remove-Item (Join-Path $Destination "codex-resources\voice") -Recurse -Force -ErrorAction SilentlyContinue
    return $true
}

$ok = $false
try { $ok = From-Npm } catch { Write-Warning "npm download failed: $_" }
if (-not $ok) { $ok = From-Release }
if (-not $ok) { throw "No se pudo descargar Codex para Windows" }
& (Join-Path $Destination "bin\codex.exe") --version
Get-ChildItem $Destination -Recurse -File | Select-Object @{n="File";e={$_.FullName.Substring($Destination.Length)}}, Length
Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
