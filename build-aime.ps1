param([string]$ZigPath=$env:ZIG_EXE, [string]$PythonPath)
$ErrorActionPreference='Stop'
if (!$ZigPath) { $taskCommand=Get-Command zig -ErrorAction SilentlyContinue; if ($taskCommand) {$ZigPath=$taskCommand.Source} }
if (!$ZigPath -or !(Test-Path -LiteralPath $ZigPath)) { throw 'Pass -ZigPath with Zig 0.13.0.' }
$zig=(Resolve-Path -LiteralPath $ZigPath).Path
if ((& $zig version).Trim() -ne '0.13.0') {throw 'Requires Zig 0.13.0.'}
$build=Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $build | Out-Null
& $zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -Wall -Wextra -Werror -static -fno-exceptions -fno-rtti `
    (Join-Path $PSScriptRoot 'src/aime/dllmain.cpp') -shared -lshlwapi -o (Join-Path $build 'chuni-aime.dll')
if ($LASTEXITCODE) {throw 'AimeIO DLL build failed.'}
if (!$PythonPath) { $taskCommand=Get-Command python -ErrorAction SilentlyContinue; if ($taskCommand) {$PythonPath=$taskCommand.Source} }
if (!$PythonPath -or !(Test-Path -LiteralPath $PythonPath)) {throw 'Pass -PythonPath to run the required DLL integration tests.'}
& $PythonPath (Join-Path $PSScriptRoot 'tests/aime-driver-test.py')
if ($LASTEXITCODE) {throw 'AimeIO DLL integration tests failed.'}
Write-Output 'Built x64 chuni-aime.dll; integration tests passed. In-game behavior remains unverified.'
