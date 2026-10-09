# ESC

`chuni-escape.dll` (x86) triggers Track Skip with ESC or a TASOLLER PLUS FN1 tap when launched through RythmFunLauncher. FN1 + FN2 remains reserved for closing the game.

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
tasoller_fn1=1
```

Log: `chuni-escape.log`.

More: [esc](../research/notes/esc.md), [results](../research/notes/results.md).
