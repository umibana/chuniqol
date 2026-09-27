param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
$path=(Resolve-Path -LiteralPath $Executable).Path
$hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
if($hash -ne '71f8fe2dc8dcf287e5eaa77add22af1137aa5aa7e721049deefd79890d3fae6e'){throw 'Unsupported game SHA256. No modification performed.'}
$bytes=[IO.File]::ReadAllBytes($path)
$pe=[BitConverter]::ToInt32($bytes,60)
if([BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x14c){throw 'Expected x86 image.'}
$checks=@(
 @{Offset=0x3f1460;Expected='b803000000c3'},
 @{Offset=0x7171f0;Expected='53568b710432db578bcee83d4095ff'},
 @{Offset=0x7108c8;Expected='b8090000003bf05f0f47f0'}
)
foreach($check in $checks){
 $actual=[BitConverter]::ToString($bytes,$check.Offset,$check.Expected.Length/2).Replace('-','').ToLowerInvariant()
 if($actual -ne $check.Expected){throw ('Expected original bytes missing at file offset 0x{0:x}.' -f $check.Offset)}
}
Write-Output 'PASS: supported x86 SHA256, base 3 getter, native end-credit predicate and existing cap 9. Read-only.'
