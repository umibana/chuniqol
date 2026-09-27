# TRACKS

`chuni-tracks.dll` 3 Tracks to 4. Patches one byte in memory.

```powershell
.\build-tracks.ps1 -ZigPath C:\tools\zig\zig.exe
.\src\tracks\verify-target.ps1 -Executable C:\path\App\bin\chusanApp.exe
```

`chuni-tracks.ini` (no ini or `enabled≠1` = off):

```ini
[tracks]
enabled=1
```

Log: `chuni-tracks.log` (`DISABLED`, `REFUSED`, `ERROR` or `READY`).
Disable: `enabled=0` and restart.



Callers: `0xb113f0` (total calc, `0xb113d0`) and `0xcf9ccd`. End-of-credit check
`0xb17df0` compares the counter at `data+0x20` with the total; the mod checks
it's untouched and refuses to load if something else patched it.
