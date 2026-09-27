param(
    [string]$ApplicationId = '1552492287634837536',
    [string]$ZigPath = $env:ZIG_EXE
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (!$ZigPath) {
    $command = Get-Command zig -ErrorAction SilentlyContinue
    if ($command) { $ZigPath = $command.Source }
}
if (!$ZigPath -or !(Test-Path -LiteralPath $ZigPath)) {
    throw 'Install Zig 0.13.0 and pass -ZigPath C:\tools\zig\zig.exe (or set ZIG_EXE / PATH).'
}
$zig = (Resolve-Path -LiteralPath $ZigPath).Path
$version = & $zig version
if ($LASTEXITCODE -or $version.Trim() -ne '0.13.0') { throw 'This build requires Zig 0.13.0.' }
if ($ApplicationId -notmatch '^[1-9][0-9]{16,19}$') { throw 'Invalid public Discord Application ID' }
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force $build | Out-Null
$common = @('c++', '-target', 'x86-windows-gnu', '-std=c++17', '-O2', '-Wall', '-Wextra', '-static',
    '-I', (Join-Path $root 'src'), '-I', (Join-Path $root 'vendor/tinyxml2'), '-I', (Join-Path $root 'vendor/json'))
$core = @((Join-Path $root 'src/core.cpp'), (Join-Path $root 'vendor/tinyxml2/tinyxml2.cpp'))
& $zig @common @core (Join-Path $root 'tests/core_test.cpp') '-o' (Join-Path $build 'core-test.exe')
if ($LASTEXITCODE) { throw 'Native core build failed' }
Write-Output "Built native core tests: $build"
& $zig @common @core (Join-Path $root 'src/discord_ipc.cpp') (Join-Path $root 'tests/ipc_probe.cpp') '-o' (Join-Path $build 'ipc-probe.exe')
if ($LASTEXITCODE) { throw 'IPC test build failed' }
$mh = Join-Path $root 'vendor/minhook'
$objects = @()
foreach ($source in @('buffer', 'hook', 'trampoline', 'hde/hde32')) {
    $object = Join-Path $build (($source -replace '/', '-') + '.o')
    & $zig cc -target x86-windows-gnu -O2 -I (Join-Path $mh 'include') -c (Join-Path $mh "src/$source.c") -o $object
    if ($LASTEXITCODE) { throw "MinHook compilation failed: $source" }
    $objects += $object
}
& $zig @common @core (Join-Path $root 'src/discord_ipc.cpp') (Join-Path $root 'src/dllmain.cpp') @objects `
    -I (Join-Path $mh 'include') "-DCHUNI_APPLICATION_ID=`"$ApplicationId`"" -shared -o (Join-Path $build 'chuni-rpc.dll')
if ($LASTEXITCODE) { throw 'Rich Presence DLL build failed' }
& $zig @common (Join-Path $root 'tests/hook_smoke.cpp') -o (Join-Path $build 'hook-smoke.exe')
if ($LASTEXITCODE) { throw 'Hook smoke test build failed' }
& $zig @common @core (Join-Path $root 'tests/capture_replay.cpp') -o (Join-Path $build 'capture-replay.exe')
if ($LASTEXITCODE) { throw 'Capture replay test build failed' }
Write-Output "Built native Rich Presence DLL (compile only): $build"
