# Builds Framecraft for Windows locally, the same way GitHub Actions does.
#
#   powershell -ExecutionPolicy Bypass -File windows\scripts\build.ps1            # build + run core tests
#   powershell -ExecutionPolicy Bypass -File windows\scripts\build.ps1 -Package   # also zip + installer
#   powershell -ExecutionPolicy Bypass -File windows\scripts\build.ps1 -Run       # build and open the app
#
# Needs Visual Studio 2022 or later with "Desktop development with C++" (MSVC, Windows SDK, CMake).
# NuGet and Codex CLI are downloaded automatically. Output: windows\build\Release\Framecraft.exe
[CmdletBinding()]
param(
    [switch]$Package,          # zip + Inno Setup installer into windows\dist
    [switch]$Run,              # open the app when the build finishes
    [switch]$SkipTests,        # skip the portable core tests
    [switch]$SkipCodex,        # don't download Codex CLI (the ChatGPT engine won't work)
    [string]$Version = "1.0.1"
)

$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repo

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Magenta }
function Fail($text) { Write-Host "ERROR: $text" -ForegroundColor Red; exit 1 }

# --- Visual Studio (MSBuild, CMake, VC runtime) ------------------------------------------------
Step "Buscando Visual Studio"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Fail "No se encontró Visual Studio. Instalá Visual Studio 2022+ con 'Desarrollo de escritorio con C++'." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { Fail "Visual Studio no tiene el workload de C++. Agregá 'Desarrollo de escritorio con C++' desde el Visual Studio Installer." }
$msbuild = Join-Path $vs "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) { Fail "MSBuild no encontrado en $vs" }
Write-Host "Visual Studio: $vs"

# CMake: the one bundled with Visual Studio, or one on PATH.
$cmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source }
$ctest = if ($cmake) { Join-Path (Split-Path $cmake) "ctest.exe" } else { $null }

# --- Core tests -------------------------------------------------------------------------------
if (-not $SkipTests) {
    if (-not $cmake) {
        Write-Warning "CMake no encontrado: salteo los tests del núcleo (instalá el componente 'C++ CMake tools')."
    } else {
        Step "Tests del núcleo"
        & $cmake -S windows/core -B build-core | Out-Host
        if ($LASTEXITCODE -ne 0) { Fail "cmake configure falló" }
        & $cmake --build build-core --config Release | Out-Host
        if ($LASTEXITCODE -ne 0) { Fail "Compilación del núcleo falló" }
        & $ctest --test-dir build-core -C Release --output-on-failure | Out-Host
        if ($LASTEXITCODE -ne 0) { Fail "Tests del núcleo fallaron" }
    }
}

# --- NuGet restore ----------------------------------------------------------------------------
Step "Paquetes NuGet (Windows App SDK, C++/WinRT, WIL)"
$nuget = (Get-Command nuget -ErrorAction SilentlyContinue).Source
if (-not $nuget) {
    $nuget = Join-Path $repo "windows\.tools\nuget.exe"
    if (-not (Test-Path $nuget)) {
        New-Item -ItemType Directory -Force (Split-Path $nuget) | Out-Null
        Invoke-WebRequest "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" -OutFile $nuget
    }
}
& $nuget restore windows\app\packages.config -PackagesDirectory windows\packages -NonInteractive | Out-Host
if ($LASTEXITCODE -ne 0) { Fail "nuget restore falló" }

# --- App --------------------------------------------------------------------------------------
Step "Compilando Framecraft.exe"
& $msbuild windows\app\Framecraft.vcxproj /p:Configuration=Release /p:Platform=x64 /m /v:m /nologo
if ($LASTEXITCODE -ne 0) { Fail "La compilación de la app falló (mirá los errores de arriba)" }
$out = Resolve-Path windows\build\Release

# --- Codex CLI + C++ runtime next to the exe ----------------------------------------------------
if (-not $SkipCodex -and -not (Test-Path (Join-Path $out "codex\bin\codex.exe"))) {
    Step "Descargando Codex CLI (motor ChatGPT)"
    & (Join-Path $PSScriptRoot "fetch-codex.ps1") -Destination (Join-Path $out "codex")
}
Step "Runtime de Visual C++ (app-local)"
$crt = Get-ChildItem "$vs\VC\Redist\MSVC" -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
    ForEach-Object { Get-ChildItem (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue } |
    Select-Object -First 1
if ($crt) { Copy-Item (Join-Path $crt.FullName "*.dll") $out -Force }
else { Write-Warning "No encontré el runtime de VC++ para copiar; la app igual abre en esta PC." }

Write-Host "`nListo: $out\Framecraft.exe" -ForegroundColor Green

# --- Package ----------------------------------------------------------------------------------
if ($Package) {
    Step "Empaquetando $Version"
    New-Item -ItemType Directory -Force windows\dist | Out-Null
    Get-ChildItem $out -Include *.pdb,*.lib,*.exp,*.ilk -Recurse | Remove-Item -Force
    Compress-Archive -Path "$out\*" -DestinationPath "windows\dist\Framecraft-$Version-Windows-x64.zip" -Force
    $iscc = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe")
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (Test-Path $iscc) {
        & $iscc /Qp "/DAppVersion=$Version" "/DSourceDir=..\build\Release" windows\installer\Framecraft.iss
        if ($LASTEXITCODE -ne 0) { Fail "Inno Setup falló" }
    } else {
        Write-Warning "Inno Setup 6 no está instalado: solo generé el .zip (instalalo con 'winget install JRSoftware.InnoSetup')."
    }
    Get-ChildItem windows\dist | Select-Object Name, Length | Format-Table -AutoSize
}

if ($Run) { Start-Process (Join-Path $out "Framecraft.exe") }
