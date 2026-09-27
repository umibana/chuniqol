param([string]$ZigPath = $env:ZIG_EXE)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if (!$ZigPath) {
    $command=Get-Command zig -ErrorAction SilentlyContinue
    if ($command) { $ZigPath=$command.Source }
}
if (!$ZigPath -or !(Test-Path -LiteralPath $ZigPath)) {
    throw 'Install Zig 0.13.0 and pass -ZigPath C:\tools\zig\zig.exe (or set ZIG_EXE / PATH). '
}
$zig=(Resolve-Path -LiteralPath $ZigPath).Path
if ((& $zig version).Trim() -ne '0.13.0') {throw 'Requires Zig 0.13.0'}
$build=Join-Path $root 'build'
$objdir=Join-Path $build 'escape-objects'
New-Item -ItemType Directory -Force $objdir | Out-Null
$common=@('c++','-target','x86-windows-gnu','-std=c++17','-O2','-Wall','-Wextra','-static','-I',(Join-Path $root 'src'))
& $zig @common (Join-Path $root 'tests/escape-policy-test.cpp') -o (Join-Path $build 'escape-policy-test.exe')
if($LASTEXITCODE){throw 'Escape policy test build failed'}
& (Join-Path $build 'escape-policy-test.exe')
if($LASTEXITCODE){throw 'Escape policy tests failed'}
$mh=Join-Path $root 'vendor/minhook'
$objects=@()
foreach($source in @('buffer','hook','trampoline','hde/hde32')){
 $object=Join-Path $objdir (($source -replace '/','-')+'.o')
 & $zig cc -target x86-windows-gnu -O2 -I (Join-Path $mh 'include') -c (Join-Path $mh "src/$source.c") -o $object
 if($LASTEXITCODE){throw "MinHook build failed: $source"}
 $objects+=$object
}
& $zig @common (Join-Path $root 'src/escape/dllmain.cpp') @objects -I (Join-Path $mh 'include') -shared -lbcrypt -o (Join-Path $build 'chuni-escape.dll')
if($LASTEXITCODE){throw 'Escape DLL build failed'}
Write-Output 'Built chuni-escape.dll; policy tests passed. Runtime remains unverified.'
