param([string]$ZigPath = $env:ZIG_EXE, [switch]$CompileOnly)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if (!$ZigPath) { $command=Get-Command zig -ErrorAction SilentlyContinue; if($command){$ZigPath=$command.Source} }
if (!$ZigPath -or !(Test-Path -LiteralPath $ZigPath)) { throw 'Install Zig 0.13.0 and pass -ZigPath C:\tools\zig\zig.exe (or set ZIG_EXE / PATH).' }
$zig=(Resolve-Path -LiteralPath $ZigPath).Path
if ((& $zig version).Trim() -ne '0.13.0') {throw 'Requires Zig 0.13.0'}
$build=Join-Path $root 'build'
$objdir=Join-Path $build 'levels-objects'
$hostdir=Join-Path $build 'levels-unsupported-host'
New-Item -ItemType Directory -Force $objdir,$hostdir | Out-Null
$common=@('c++','-target','x86-windows-gnu','-std=c++17','-O2','-Wall','-Wextra','-static','-I',(Join-Path $root 'src'))
& $zig @common (Join-Path $root 'tests/levels-format-test.cpp') -o (Join-Path $build 'levels-format-test.exe')
if($LASTEXITCODE){throw 'Levels compile-time formatting assertions failed'}
& $zig @common (Join-Path $root 'tests/levels-unsupported-host.cpp') -municode -o (Join-Path $hostdir 'levels-unsupported-host.exe')
if($LASTEXITCODE){throw 'Levels unsupported-host test build failed'}
$mh=Join-Path $root 'vendor/minhook'
$objects=@()
foreach($source in @('buffer','hook','trampoline','hde/hde32')){
 $object=Join-Path $objdir (($source -replace '/','-')+'.o')
 & $zig cc -target x86-windows-gnu -O2 -I (Join-Path $mh 'include') -c (Join-Path $mh "src/$source.c") -o $object
 if($LASTEXITCODE){throw "MinHook build failed: $source"}
 $objects+=$object
}
& $zig @common (Join-Path $root 'src/levels/dllmain.cpp') @objects -I (Join-Path $mh 'include') -shared -lbcrypt -o (Join-Path $build 'chuni-levels.dll')
if($LASTEXITCODE){throw 'Levels DLL build failed'}
if($CompileOnly){Write-Output 'Built chuni-levels.dll; compile-time formatting assertions passed. Native tests skipped by -CompileOnly; runtime remains unverified.';return}
& (Join-Path $build 'levels-format-test.exe')
if($LASTEXITCODE){throw 'Levels formatting executable failed'}
Copy-Item -LiteralPath (Join-Path $build 'chuni-levels.dll') -Destination (Join-Path $hostdir 'chuni-levels.dll') -Force
Set-Content -LiteralPath (Join-Path $hostdir 'chuni-levels.ini') -Value "[levels]`nenabled=1" -Encoding ascii
& (Join-Path $hostdir 'levels-unsupported-host.exe') (Join-Path $hostdir 'chuni-levels.dll')
if($LASTEXITCODE){throw 'Levels unsupported host was not refused'}
Write-Output 'Built chuni-levels.dll; formatting and unsupported-host tests passed. In-game rendering remains unverified.'
