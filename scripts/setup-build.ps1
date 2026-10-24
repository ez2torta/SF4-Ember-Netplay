param(
    [string]$ToolsRoot = 'E:\tools',
    [string]$DiscordSdkArchive = 'E:\tools\DiscordSocialSdk-1.10.19337.zip',
    [string]$VisualStudioPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools',
    [ValidatePattern('^[D-Z]$')][string]$RustStorageDrive = 'D',
    [switch]$RefreshDependencies,
    [switch]$SkipTests,
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$vcpkgRoot = Join-Path $ToolsRoot 'vcpkg'
$rustStorageRoot = Join-Path "$RustStorageDrive`:\" 'tools\sf4e-rust'
$rustDriveInfo = New-Object IO.DriveInfo("$RustStorageDrive`:\")
if (!$rustDriveInfo.IsReady -or $rustDriveInfo.DriveFormat -ne 'NTFS') { throw "$RustStorageDrive`: must be available and formatted as NTFS for Rust and Cargo." }
if ($rustDriveInfo.AvailableFreeSpace -lt 8GB) { throw "$RustStorageDrive`: needs at least 8 GB free for Rust and Cargo." }
if ($rustDriveInfo.DriveFormat -ne 'NTFS') { throw 'R: no está formateado como NTFS. No iniciaré Cargo allí.' }
$rustupHome = Join-Path $rustStorageRoot 'rustup'
$cargoHome = Join-Path $rustStorageRoot 'cargo'
$rustShimHome = Join-Path $rustStorageRoot 'rust-shims'
$vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
$expectedSdkHash = 'd784097504685953849cc2842561d8a8013326fe0b8df65a1de63ddefde62045'

function Download-File([string]$Uri, [string]$Destination) {
    New-Item -ItemType Directory -Force -Path (Split-Path $Destination -Parent) | Out-Null
    Write-Host "Descargando $Uri"
    Invoke-WebRequest -Uri $Uri -OutFile $Destination
}

New-Item -ItemType Directory -Force -Path $ToolsRoot, $rustupHome, $cargoHome | Out-Null
$drive = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($ToolsRoot))
$freeBytes = (Get-PSDrive -Name $drive.TrimEnd('\').TrimEnd(':') -PSProvider FileSystem).Free
Write-Host ("Espacio libre en {0}: {1:N1} GB" -f $drive, ($freeBytes / 1GB))
if ($freeBytes -lt 8GB) { throw 'Se recomiendan al menos 8 GB libres para dependencias y compilación. Libera espacio e inténtalo de nuevo.' }

# Keep Rust downloads, toolchains and packages on the selected HDD.
$env:RUSTUP_HOME = $rustupHome
$env:VCPKG_ROOT = $vcpkgRoot

if (!(Test-Path -LiteralPath (Join-Path $vcpkgRoot '.git'))) {
    $git = Get-Command git -ErrorAction SilentlyContinue
    if (!$git) { throw 'Git for Windows es necesario para clonar vcpkg con su historial.' }
    if (Test-Path -LiteralPath $vcpkgRoot) {
        $backup = Join-Path $ToolsRoot ('vcpkg-zip-bootstrap-' + [guid]::NewGuid().ToString('N'))
        Move-Item -LiteralPath $vcpkgRoot -Destination $backup
        Write-Host "Guardé la carpeta anterior de vcpkg en: $backup"
    }
    & $git.Source clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo clonar vcpkg con Git.' }
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo preparar vcpkg.' }
}

if (!(Test-Path -LiteralPath $vcpkgExe) -and (Test-Path -LiteralPath (Join-Path $vcpkgRoot '.git'))) {
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo preparar vcpkg.' }
}

if (!(Test-Path -LiteralPath $vcpkgExe)) {
    $archive = Join-Path $ToolsRoot 'vcpkg-bootstrap.zip'
    $extract = Join-Path $ToolsRoot ('vcpkg-bootstrap-' + [guid]::NewGuid().ToString('N'))
    if (Test-Path -LiteralPath $vcpkgRoot) { throw "$vcpkgRoot ya existe, pero no contiene vcpkg.exe. Renómbrala o elige otra carpeta con -ToolsRoot." }
    Download-File 'https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip' $archive
    Expand-Archive -LiteralPath $archive -DestinationPath $extract -Force
    $source = Get-ChildItem -LiteralPath $extract -Directory | Select-Object -First 1
    Move-Item -LiteralPath $source.FullName -Destination $vcpkgRoot
    Remove-Item -LiteralPath $extract -Recurse -Force
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo preparar vcpkg.' }
}

$rustupInit = Join-Path $ToolsRoot 'rustup-init.exe'
New-Item -ItemType Directory -Force -Path (Join-Path $rustShimHome 'bin'), (Join-Path $cargoHome 'bin') | Out-Null
$rustShimExe = Join-Path $rustShimHome 'bin\rustup.exe'
$existingRustup = Join-Path $cargoHome 'bin\rustup.exe'
if (!(Test-Path -LiteralPath $rustShimExe) -and (Test-Path -LiteralPath $existingRustup)) {
    Copy-Item -LiteralPath $existingRustup -Destination $rustShimExe
}
if (!(Test-Path -LiteralPath $rustShimExe) -or !(Test-Path -LiteralPath (Join-Path $rustupHome 'settings.toml'))) {
    if (!(Test-Path -LiteralPath $rustupInit)) {
    Download-File 'https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe' $rustupInit
    }
    $env:CARGO_HOME = $rustShimHome
    & $rustupInit -y --no-modify-path --default-host x86_64-pc-windows-msvc --default-toolchain none
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo preparar Rustup en la carpeta de enlaces de Windows.' }
}
$env:CARGO_HOME = $rustShimHome
$env:PATH = (Join-Path $rustShimHome 'bin') + ';' + $env:PATH
$toolchainText = Get-Content (Join-Path $repo 'rust/sf4-net/rust-toolchain.toml') -Raw
$toolchain = [regex]::Match($toolchainText, '(?m)^channel\s*=\s*"([^"]+)"').Groups[1].Value
if (!$toolchain) { throw 'No pude leer la versión de Rust de rust-toolchain.toml.' }
 $toolchainName = "$toolchain-x86_64-pc-windows-msvc"
$toolchainDir = Join-Path (Join-Path $rustupHome 'toolchains') $toolchainName
$toolchainBin = Join-Path $toolchainDir 'bin'
$rustcExe = Join-Path $toolchainBin 'rustc.exe'
$cargoExe = Join-Path $toolchainBin 'cargo.exe'
$rustReady = $false
if ((Test-Path -LiteralPath $rustcExe) -and (Test-Path -LiteralPath $cargoExe)) {
    $rustcVersion = (& $rustcExe --version 2>$null | Out-String).Trim()
    $rustReady = ($LASTEXITCODE -eq 0 -and $rustcVersion -match "^rustc $([regex]::Escape($toolchain))\s")
}
if (!$rustReady) {
    & $rustShimExe toolchain install $toolchain --profile minimal --component clippy,rustfmt --target x86_64-pc-windows-msvc --force
    $rustInstallExit = $LASTEXITCODE
    if ((Test-Path -LiteralPath $rustcExe) -and (Test-Path -LiteralPath $cargoExe)) {
        $rustcVersion = (& $rustcExe --version 2>$null | Out-String).Trim()
        $rustReady = ($LASTEXITCODE -eq 0 -and $rustcVersion -match "^rustc $([regex]::Escape($toolchain))\s")
    }
    if (!$rustReady) { throw "No se pudo instalar o verificar Rust $toolchain (código de instalación $rustInstallExit)." }
    if ($rustInstallExit -ne 0) { Write-Warning 'Rustup no pudo registrar la toolchain en E:, pero rustc funciona. El build usará directamente los ejecutables ya descargados.' }
}

# Use the verified compiler directly on E: volumes that do not support rustup's
# hard links. Cargo registry and crate cache remain under ToolsRoot as well.
Copy-Item -LiteralPath $cargoExe -Destination (Join-Path (Join-Path $cargoHome 'bin') 'cargo.exe') -Force
$env:CARGO_HOME = $cargoHome
$env:PATH = $toolchainBin + ';' + (Join-Path $cargoHome 'bin') + ';' + $env:PATH

if (!(Test-Path -LiteralPath $DiscordSdkArchive)) {
    Write-Warning "Falta el SDK de Discord. Descarga DiscordSocialSdk-1.10.19337.zip desde el portal de desarrolladores y guárdalo en: $DiscordSdkArchive"
} else {
    $actualHash = (Get-FileHash -LiteralPath $DiscordSdkArchive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $expectedSdkHash) { throw "El SHA-256 del SDK no coincide. Esperado $expectedSdkHash; obtenido $actualHash." }
    Write-Host 'SDK de Discord verificado.'
}

$vsBuild = Join-Path $VisualStudioPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (!(Test-Path -LiteralPath $vsBuild)) {
    Write-Warning "Falta Visual Studio C++ Build Tools. Instálalo con los componentes x86/x64, CMake, Ninja y Windows SDK; luego indica su carpeta con -VisualStudioPath. Guía: docs/development/BUILDING.md"
}
$pwshCommand = Get-Command pwsh -ErrorAction SilentlyContinue
if ($pwshCommand) {
    $pwshExe = $pwshCommand.Source
} else {
    $pwshRoot = Join-Path $ToolsRoot 'PowerShell'
    $pwshExe = Join-Path $pwshRoot 'pwsh.exe'
    if (!(Test-Path -LiteralPath $pwshExe)) {
        $pwshZip = Join-Path $ToolsRoot 'PowerShell-win-x64.zip'
        Download-File 'https://github.com/PowerShell/PowerShell/releases/download/v7.6.6/PowerShell-7.6.6-win-x64.zip' $pwshZip
        New-Item -ItemType Directory -Force -Path $pwshRoot | Out-Null
        Expand-Archive -LiteralPath $pwshZip -DestinationPath $pwshRoot -Force
    }
    if (!(Test-Path -LiteralPath $pwshExe)) { throw 'No pude preparar PowerShell 7 en E:\tools\PowerShell.' }
    Write-Host "PowerShell 7 portable: $pwshExe"
}

Write-Host "vcpkg: $vcpkgRoot"
Write-Host "Rustup/Cargo: $rustupHome / $cargoHome"
Write-Host "Dependencias del proyecto: $repo\build\current\dependencies"

if ($Build) {
    if (!(Test-Path -LiteralPath $vsBuild)) { throw 'Instala Visual Studio C++ Build Tools antes de compilar.' }
    if (!(Test-Path -LiteralPath $DiscordSdkArchive)) { throw 'Coloca primero el SDK de Discord en la ruta indicada.' }
    $buildScript = Join-Path $repo 'scripts\build-current.ps1'
    # E: does not record file ownership, so Git's dubious-ownership check rejects
    # this checkout. Trust only this exact path for child Git processes in this run.
    $gitConfigCount = 0
    if ($env:GIT_CONFIG_COUNT -match '^\d+$') { $gitConfigCount = [int]$env:GIT_CONFIG_COUNT }
    [Environment]::SetEnvironmentVariable("GIT_CONFIG_KEY_$gitConfigCount", 'safe.directory', 'Process')
    [Environment]::SetEnvironmentVariable("GIT_CONFIG_VALUE_$gitConfigCount", ([IO.Path]::GetFullPath($repo).Replace('\','/')), 'Process')
    $env:GIT_CONFIG_COUNT = [string]($gitConfigCount + 1)
    & $pwshExe -NoProfile -File $buildScript -VisualStudioPath $VisualStudioPath -VcpkgRoot $vcpkgRoot -DiscordSdkArchive $DiscordSdkArchive -RefreshDependencies:$RefreshDependencies -SkipTests:$SkipTests
    if ($LASTEXITCODE -ne 0) { throw 'La compilación no terminó correctamente.' }
}
