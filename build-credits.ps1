param([string]$ZigPath = $env:ZIG_EXE)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (!$ZigPath) {
    $command = Get-Command zig -ErrorAction SilentlyContinue
    if ($command) { $ZigPath = $command.Source }
}
if (!$ZigPath -or !(Test-Path -LiteralPath $ZigPath)) {
    throw 'Pass -ZigPath with the path to Zig 0.13.0.'
}
$zig = (Resolve-Path -LiteralPath $ZigPath).Path
$version = & $zig version
if ($LASTEXITCODE -or $version.Trim() -ne '0.13.0') { throw 'This build requires Zig 0.13.0.' }
$build = Join-Path $root 'build'
$objectsDir = Join-Path $build 'credits'
New-Item -ItemType Directory -Force $objectsDir | Out-Null
$source = Join-Path $root 'src/credits'
$common = @('c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-static', '-fno-exceptions', '-fno-rtti')
& $zig @common (Join-Path $source 'credit_core.cpp') (Join-Path $source 'credit_test.cpp') -o (Join-Path $build 'credits-test.exe')
if ($LASTEXITCODE) { throw 'Credits core tests build failed.' }
& (Join-Path $build 'credits-test.exe')
if ($LASTEXITCODE) { throw 'Credits core tests failed.' }
$mh = Join-Path $root 'vendor/minhook'
$objects = @()
foreach ($item in @('buffer', 'hook', 'trampoline', 'hde/hde64')) {
    $object = Join-Path $objectsDir (($item -replace '/', '-') + '.o')
    & $zig cc -target x86_64-windows-gnu -O2 -I (Join-Path $mh 'include') -c (Join-Path $mh "src/$item.c") -o $object
    if ($LASTEXITCODE) { throw "MinHook x64 compilation failed: $item" }
    $objects += $object
}
& $zig @common (Join-Path $source 'hook_test.cpp') @objects -I (Join-Path $mh 'include') -o (Join-Path $build 'credits-hook-test.exe')
if ($LASTEXITCODE) { throw 'Credits hook test build failed.' }
& (Join-Path $build 'credits-hook-test.exe')
if ($LASTEXITCODE) { throw 'Credits hook test failed.' }
& $zig @common (Join-Path $source 'credit_core.cpp') (Join-Path $source 'dllmain.cpp') @objects `
    -I (Join-Path $mh 'include') -lbcrypt -shared -o (Join-Path $build 'chuni-credits.dll')
if ($LASTEXITCODE) { throw 'Credits DLL build failed.' }
& $zig @common (Join-Path $source 'load_test.cpp') -o (Join-Path $build 'credits-load-test.exe')
if ($LASTEXITCODE) { throw 'Credits load test build failed.' }
& (Join-Path $build 'credits-load-test.exe') (Join-Path $build 'chuni-credits.dll')
if ($LASTEXITCODE) { throw 'Credits DLL fail-closed test failed.' }
Write-Output "Built and tested x64 credits DLL (not installed): $(Join-Path $build 'chuni-credits.dll')"
