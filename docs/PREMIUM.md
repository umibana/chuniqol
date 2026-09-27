# PREMIUM

`chuni-premium.dll` picks PREMIUM TICKET automatically.

```ini
[premium]
enabled=1
ticketId=2070

[display]
# too lazy to find og .dll i patched over it
restoreCredits=1
```

```powershell
.\build-premium.ps1 -ZigPath C:\tools\zig\zig.exe
```

## Details

Ticket screen opens a 20 s window. Selects with the native setter and only
sends input at `D6D811` (event 4), `D6DAD8` and `D6DE56` (event 21). Real user
input stops it; going back to selection doesn't retry.

If `chuni-escape.dll` is loaded, the input hook calls its
`ChuniEscapeInputFilter` export so there's only one detour on that function.
ESC needs PREMIUM enabled.

More: [research notes](../research/notes/premium.md).
