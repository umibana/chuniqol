param([string]$ZigPath = $env:ZIG_EXE)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$repo = Split-Path (Split-Path $root)
if (!$ZigPath) {
    $command = Get-Command zig -ErrorAction SilentlyContinue
    if ($command) { $ZigPath = $command.Source }
}
if (!$ZigPath -or !(Test-Path -LiteralPath $ZigPath)) { throw 'Pass -ZigPath pointing to Zig 0.13.0.' }
$zig = (Resolve-Path -LiteralPath $ZigPath).Path
$version = & $zig version
if ($LASTEXITCODE -or $version.Trim() -ne '0.13.0') { throw 'This build requires Zig 0.13.0.' }
$mh = Join-Path $repo 'vendor/minhook'
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$objects = @()
foreach ($source in @('buffer', 'hook', 'trampoline', 'hde/hde32')) {
    $object = Join-Path $build (($source -replace '/', '-') + '.o')
    & $zig cc -target x86-windows-gnu -O2 -I (Join-Path $mh 'include') -c (Join-Path $mh "src/$source.c") -o $object
    if ($LASTEXITCODE) { throw "MinHook compilation failed: $source" }
    $objects += $object
}
& $zig c++ -target x86-windows-gnu -std=c++17 -O2 -Wall -Wextra -static -shared `
    -I (Join-Path $mh 'include') (Join-Path $root 'state_probe.cpp') @objects -lbcrypt `
    -o (Join-Path $build 'chuni-state-diagnostic.dll')
if ($LASTEXITCODE) { throw 'State diagnostic compilation failed' }
Write-Output 'State diagnostic DLL compiled (not runtime-verified).'
