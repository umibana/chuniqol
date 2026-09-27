param([string]$ZigPath=$env:ZIG_EXE)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if(!$ZigPath){$command=Get-Command zig -ErrorAction SilentlyContinue;if($command){$ZigPath=$command.Source}}
if(!$ZigPath -or !(Test-Path -LiteralPath $ZigPath)){throw 'Pass -ZigPath with Zig 0.13.0.'}
$zig=(Resolve-Path -LiteralPath $ZigPath).Path
if((& $zig version).Trim() -ne '0.13.0'){throw 'Requires Zig 0.13.0.'}
$build=Join-Path $root 'build';New-Item -ItemType Directory -Force $build | Out-Null
$src=Join-Path $root 'src/tracks'
$flags=@('c++','-target','x86-windows-gnu','-std=c++17','-O2','-Wall','-Wextra','-Werror','-static','-fno-exceptions','-fno-rtti')
& $zig @flags (Join-Path $src 'patch.cpp') (Join-Path $src 'patch_test.cpp') -o (Join-Path $build 'tracks-test.exe')
if($LASTEXITCODE){throw 'Track patch test build failed.'}
& (Join-Path $build 'tracks-test.exe')
if($LASTEXITCODE){throw 'Track patch test failed.'}
& $zig @flags (Join-Path $src 'patch.cpp') (Join-Path $src 'dllmain.cpp') -shared -lbcrypt -o (Join-Path $build 'chuni-tracks.dll')
if($LASTEXITCODE){throw 'Tracks DLL build failed.'}
& $zig @flags (Join-Path $src 'load_test.cpp') -o (Join-Path $build 'tracks-load-test.exe')
if($LASTEXITCODE){throw 'Tracks DLL load test build failed.'}
# Tests use build artifacts only; the game is never launched or changed.
$ini=Join-Path $build 'chuni-tracks.ini'
'[tracks]', 'enabled=0' | Set-Content -LiteralPath $ini -Encoding ascii
& (Join-Path $build 'tracks-load-test.exe') (Join-Path $build 'chuni-tracks.dll') '2'
if($LASTEXITCODE){throw 'Tracks disabled-config test failed.'}
Copy-Item -LiteralPath (Join-Path $src 'chuni-tracks.ini') -Destination $ini -Force
& (Join-Path $build 'tracks-load-test.exe') (Join-Path $build 'chuni-tracks.dll') '-1'
if($LASTEXITCODE){throw 'Tracks unsupported-host test failed.'}
Write-Output "Built x86 chuni-tracks.dll + enabled INI in $build. Not installed."
