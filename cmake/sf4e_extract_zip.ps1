param(
    [Parameter(Mandatory = $true)][string]$Archive,
    [Parameter(Mandatory = $true)][string]$Destination
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = [IO.Path]::GetFullPath($Destination).TrimEnd('\') + '\'
New-Item -ItemType Directory -Force -Path $root | Out-Null
$zip = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Archive))
try {
    foreach ($entry in $zip.Entries) {
        $relative = $entry.FullName.Replace('/', '\')
        if (!$relative) { continue }
        $target = [IO.Path]::GetFullPath([IO.Path]::Combine($root, $relative))
        if (!$target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Archive entry escapes destination: $($entry.FullName)"
        }
        if ($entry.FullName.EndsWith('/')) {
            New-Item -ItemType Directory -Force -Path $target | Out-Null
            continue
        }
        $parent = Split-Path -Parent $target
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        $inputStream = $entry.Open()
        $outputStream = [IO.File]::Open($target, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $inputStream.CopyTo($outputStream) }
        finally {
            $outputStream.Dispose()
            $inputStream.Dispose()
        }
    }
}
finally { $zip.Dispose() }
