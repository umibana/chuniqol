param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$expectedHash = '014992a5af1b62b5fd42d2cb0c19a38aba4b5437a3ef82d304de2c93065ed70d'
$path = (Resolve-Path -LiteralPath $Executable).Path
if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'Unsupported executable SHA256. No file has been modified.'
}
$bytes = [IO.File]::ReadAllBytes($path)
$pe = [BitConverter]::ToInt32($bytes,60)
if ([BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664) { throw 'Expected AMD64 image.' }
$sectionCount = [BitConverter]::ToUInt16($bytes,$pe+6)
$sectionTable = $pe+24+[BitConverter]::ToUInt16($bytes,$pe+20)
function Convert-Rva([int]$rva) {
    for ($i=0;$i -lt $sectionCount;$i++) {
        $section = $sectionTable+$i*40
        $start = [BitConverter]::ToUInt32($bytes,$section+12)
        $size = [BitConverter]::ToUInt32($bytes,$section+16)
        $raw = [BitConverter]::ToUInt32($bytes,$section+20)
        if ($rva -ge $start -and $rva -lt ($start+$size)) { return [int]($raw+$rva-$start) }
    }
    throw "Unmapped RVA $rva"
}
$checks = @(
    @{Rva=0x2deb30;Bytes='4c8bc14885c97506b803000081c3'},
    @{Rva=0x2de4c0;Bytes='48895c2408574883ec200fb6fa8bd9e88cf8ffff83f802'},
    @{Rva=0x2de4f7;Bytes='40287c4a28'},
    @{Rva=0x2deb03;Bytes='ff15d7282d00488bcbe81f000000'}
)
foreach ($check in $checks) {
    $offset = Convert-Rva $check.Rva
    $length = $check.Bytes.Length/2
    $actual = [BitConverter]::ToString($bytes,$offset,$length).Replace('-','').ToLowerInvariant()
    if ($actual -ne $check.Bytes) { throw ('Expected-byte mismatch at RVA 0x{0:x}' -f $check.Rva) }
}
Write-Output 'PASS: supported x64 SHA256, getter/debit prologues, native subtraction and locked getter call. Read-only verification.'
