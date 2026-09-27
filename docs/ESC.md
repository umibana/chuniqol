# ESC

`chuni-escape.dll` (x86) triggers Track Skip when ESC is pressed.

- Needs `chuni-premium.dll` enabled.

```powershell
.\build-premium.ps1 -ZigPath C:\tools\zig\zig.exe
.\build-escape.ps1  -ZigPath C:\tools\zig\zig.exe
.\install-extras.ps1 -GameBin C:\path\App\bin -Escape
```

`chuni-escape.ini` (no ini = off):

```ini
[escape]
enabled=1
```

Log: `chuni-escape.log`.

More: [esc](../research/notes/esc.md), [results](../research/notes/results.md).
