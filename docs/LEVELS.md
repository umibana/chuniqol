# LEVELS

`chuni-levels.dll` (x86) adds internal level to the song title on song
select, e.g. `Song [13.2]`.

```powershell
.\build-levels.ps1 -ZigPath C:\tools\zig\zig.exe   # -CompileOnly: skip running tests
```

`chuni-levels.ini` (no ini = off):

```ini
[levels]
enabled=1
```

Log: `chuni-levels.log` (activation + up to 128 applied entries, `1320` = 13.2).