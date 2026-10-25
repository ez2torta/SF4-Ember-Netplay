param(
    [Parameter(Position = 0)][string]$Target = 'help',
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Rest = @(),
    [string]$ToolsRoot = 'E:\tools',
    [string]$VisualStudioPath = $env:SF4E_VISUAL_STUDIO_PATH
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'BuildEnvironment.ps1')
$buildTarget = Get-EmberBuildTarget $repo
$build = Join-Path $repo $buildTarget.buildDirectory
$stage = Join-Path $repo $buildTarget.installDirectory
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { Join-Path $ToolsRoot 'vcpkg' }

function Show-Help {
    Write-Host @'
Uso: dev <target>

  build   Compila sin ejecutar tests (bucle de desarrollo).
  full    Compila y ejecuta toda la suite; deja un recibo empaquetable.
  go      Compila sin tests y lanza el juego.
  run     Lanza el juego ya compilado.
  test    Ejecuta CTest sobre la build actual, sin recompilar.
          'dev test Gamepad Slots' limita la ejecucion a los tests que casan.

Opciones: -ToolsRoot <ruta> (por defecto E:\tools), -VisualStudioPath <ruta>.
Los argumentos extra se pasan a scripts/setup-build.ps1.
'@
}

function Invoke-Build([switch]$Tests) {
    $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        (Join-Path $PSScriptRoot 'setup-build.ps1'), '-Build', '-ToolsRoot', $ToolsRoot)
    if ($VisualStudioPath) { $arguments += @('-VisualStudioPath', $VisualStudioPath) }
    if (!$Tests) { $arguments += '-SkipTests' }
    $arguments += $Rest
    & powershell.exe @arguments
    if ($LASTEXITCODE) { throw "La compilacion fallo (codigo $LASTEXITCODE)." }
}

function Start-Game {
    $launcher = Join-Path $stage 'Launcher.exe'
    if (!(Test-Path -LiteralPath $launcher)) {
        throw "No hay Launcher.exe en $stage. Ejecuta 'dev build' primero."
    }
    if (Get-Process -Name SSFIV -ErrorAction SilentlyContinue) {
        Write-Host 'SSFIV.exe ya esta en ejecucion; cierralo antes de volver a lanzar.'
    }
    Start-Process -FilePath $launcher -WorkingDirectory $stage
}

switch ($Target.ToLowerInvariant()) {
    'build' { Invoke-Build }
    'full' { Invoke-Build -Tests }
    'go' { Invoke-Build; Start-Game }
    'run' { Start-Game }
    'test' {
        $tools = Get-EmberToolPaths $repo $VisualStudioPath $vcpkgRoot
        $ctest = Join-Path $tools.VisualStudioPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
        if (!(Test-Path -LiteralPath $ctest)) { throw "No encontre ctest en $ctest." }
        # HelperRust, EmberRust and BridgeSdk need rustc, which this PC does not
        # have; they build nothing else.
        $arguments = @('--test-dir', $build, '--output-on-failure',
            '-E', 'Iroh(Room|Game|Authorized|Recovery)|CustomRoom(FourTables|Spectators)|DiscordSmoke|HelperRust|EmberRust|BridgeSdk')
        # Optional test names narrow the run, as in 'dev test Gamepad Slots'.
        if ($Rest.Count -gt 0) { $arguments += @('-R', ($Rest -join '|')) }
        & $ctest @arguments
        if ($LASTEXITCODE) { throw "Los tests fallaron (codigo $LASTEXITCODE)." }
    }
    'help' { Show-Help }
    default {
        Write-Host "Target desconocido: $Target"
        Show-Help
        exit 1
    }
}
