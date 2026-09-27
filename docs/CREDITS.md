# CREDITS

`chuni-credits.dll` sets 99 credits.

- `max_credit` only accepts 1–99
- Load with `-k chuni-credits.dll` on the **inject_x64** line (amdaemon).
- Disable: remove the `-k` and restart. Saved credits stay.
- Log: `chuni-credits.log` (`Hooks ready`, `Actual stock=99`).

```powershell
.\build-credits.ps1 -ZigPath C:\tools\zig\zig.exe
.\src\credits\verify-target.ps1 -Executable C:\path\App\bin\amdaemon.exe
```

## Details

amdaemon RVAs:

| What | RVA |
|---|---:|
| Manager initialized (DWORD) | `0x96a050` |
| Credits/fraction per node | `0x96a078 + node*2` |
| Dirty flag | `0x96a148` |
| Max / node count | `0x96a40c` / `0x96a40e` |
| Native credit copy | `0x2deb30` |
| Native spend | `0x2de4c0` |
| Critical section | `0xabb940` |

Hooks the copy (called from `0x2deb0c`) and the spend (`0x2de42c`, `0x2de494`),
all inside the `0x2dead0` lock. Doesn't use the SUB→OR patch (it can raise
credits on some amounts). More: [research notes](../research/notes/credits.md).
