$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$dll = Join-Path $root 'build/chuni-rpc.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Build chuni-rpc.dll first.' }
$destination = Join-Path $root ('dist/chuni-rpc-dev-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination $destination
Get-ChildItem -LiteralPath (Join-Path $root 'package') -File | Copy-Item -Destination $destination
$licenses = Join-Path $destination 'licenses'
New-Item -ItemType Directory -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'vendor/minhook/LICENSE.txt') -Destination (Join-Path $licenses 'MinHook.txt')
Copy-Item -LiteralPath (Join-Path $root 'vendor/tinyxml2/LICENSE.txt') -Destination (Join-Path $licenses 'TinyXML2.txt')
Copy-Item -LiteralPath (Join-Path $root 'vendor/json/LICENSE.MIT') -Destination (Join-Path $licenses 'nlohmann-json.txt')
$manifest = Get-ChildItem -LiteralPath $destination -Recurse -File | ForEach-Object {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash, $_.FullName.Substring($destination.Length + 1)
}
$manifest | Set-Content -LiteralPath (Join-Path $destination 'SHA256SUMS.txt') -Encoding Ascii
Compress-Archive -Path (Join-Path $destination '*') -DestinationPath "$destination.zip"
Write-Output "UNVERIFIED development package: $destination.zip"
