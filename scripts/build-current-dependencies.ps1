param([string]$VisualStudioPath = '', [string]$VcpkgRoot = '', [switch]$Refresh)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'BuildEnvironment.ps1')
$target = Get-EmberBuildTarget $repo
$tools = Get-EmberToolPaths $repo $VisualStudioPath $VcpkgRoot
$dependencyRoot = Join-Path (Join-Path $repo $target.buildDirectory) 'dependencies'
$triplet = 'x86-windows-wchar-filenames'
$installed = Join-Path $dependencyRoot $triplet
$receiptPath = Join-Path (Join-Path $repo $target.buildDirectory) 'vcpkg-dependencies.json'
$vcpkgExe = Join-Path $tools.VcpkgRoot 'vcpkg.exe'
$vcpkgVersion = (& $vcpkgExe version | Out-String).Trim()
if ($LASTEXITCODE) { throw 'Could not read the vcpkg version.' }

# Pin the reusable installed tree to every input that affects manifest resolution
# or overlay ABIs. A changed manifest/overlay/tool version forces vcpkg install.
$inputFiles = @((Join-Path $repo 'vcpkg.json'), (Join-Path $repo 'vcpkg-configuration.json'))
$inputFiles += @(Get-ChildItem -LiteralPath (Join-Path $repo 'vcpkg-overlays') -File -Recurse | ForEach-Object FullName)
$inputLines = @("vcpkgRoot=$($tools.VcpkgRoot)", "vcpkgVersion=$vcpkgVersion", "triplet=$triplet")
foreach ($path in $inputFiles | Sort-Object -Unique) {
    $relative = [IO.Path]::GetRelativePath($repo, $path).Replace('\','/')
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $inputLines += "$relative $hash"
}
$sha = [Security.Cryptography.SHA256]::Create()
try { $inputHash = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($inputLines -join "`n")))).Replace('-','').ToLowerInvariant() }
finally { $sha.Dispose() }

$requiredShares = @('cli11','detours','fmt','ggpo','imgui','nlohmann_json','sdl3','spdlog','wil','zlib')
$requiredFiles = @('bin/GGPO.dll','lib/GGPO.lib','lib/detours.lib','lib/fmt.lib','lib/SDL3-static.lib','lib/spdlog.lib','lib/zlib.lib')
$treeComplete = (Test-Path -LiteralPath (Join-Path $installed 'share/ggpo/vcpkg_abi_info.txt'))
foreach ($name in $requiredShares) {
    if (!(Test-Path -LiteralPath (Join-Path $installed "share/$name"))) { $treeComplete = $false }
}
foreach ($relative in $requiredFiles) {
    if (!(Test-Path -LiteralPath (Join-Path $installed $relative) -PathType Leaf)) { $treeComplete = $false }
}

$receiptMatches = $false
if (!$Refresh -and $treeComplete) {
    if (Test-Path -LiteralPath $receiptPath) {
        try {
            $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
            $receiptMatches = ($receipt.inputHash -eq $inputHash -and $receipt.triplet -eq $triplet)
        } catch { $receiptMatches = $false }
    } else {
        # Adopt the completed tree from a successful earlier vcpkg install.
        # Verify the custom GGPO port ABI before recording its dependency inputs.
        $receiptMatches = $true
    }
    if ($receiptMatches) {
        . (Join-Path $PSScriptRoot 'BuildProvenance.ps1')
        try { $null = Get-DependencyReceipt $repo (Join-Path $repo $target.buildDirectory) }
        catch { $receiptMatches = $false }
    }
}

if ($receiptMatches) {
    if (!(Test-Path -LiteralPath $receiptPath)) {
        [ordered]@{inputHash=$inputHash;triplet=$triplet;vcpkgVersion=$vcpkgVersion;installedRoot=$installed;verifiedUtc=[DateTime]::UtcNow.ToString('o')} |
            ConvertTo-Json | Set-Content -LiteralPath $receiptPath
    }
    Write-Output "Reusing verified vcpkg dependencies: $installed"
} else {
    Enter-EmberVcEnvironment $tools.VisualStudioPath
    $env:VCPKG_ROOT = $tools.VcpkgRoot
    & $vcpkgExe install "--triplet=$triplet" "--host-triplet=$triplet" "--x-manifest-root=$repo" "--x-install-root=$dependencyRoot"
    if ($LASTEXITCODE) { throw 'Designated dependency build failed' }
    if (!(Test-Path -LiteralPath (Join-Path $installed 'share/ggpo/vcpkg_abi_info.txt'))) { throw 'vcpkg completed without the expected GGPO ABI record.' }
    . (Join-Path $PSScriptRoot 'BuildProvenance.ps1')
    $null = Get-DependencyReceipt $repo (Join-Path $repo $target.buildDirectory)
    [ordered]@{inputHash=$inputHash;triplet=$triplet;vcpkgVersion=$vcpkgVersion;installedRoot=$installed;verifiedUtc=[DateTime]::UtcNow.ToString('o')} |
        ConvertTo-Json | Set-Content -LiteralPath $receiptPath
    Write-Output "Verified dependency installation: $installed"
}
