# Rich presence research

SDHD 2.50, `chusanApp.exe` x86, SHA256
`71f8fe2dc8dcf287e5eaa77add22af1137aa5aa7e721049deefd79890d3fae6e`, base `0x400000`.
Paths like `App/` refer to the private game install, not this repo.

## Status

| Feature | Status |
| --- | --- |
| Song, difficulty, level on Discord | Works |
| Clear by timeout | Heuristic, doesn't detect results |
| Menu / song select / results | States found, not validated in game |
| Current / total track | Accessors found, not validated |

Next step: capture real states with the probe (`research/state`) and match
them to what's on screen.

## Song detection

`NtCreateFile` hook → `.c2s` opens → queue (4096) → worker → `Music.xml` in the
same folder → Discord IPC (`discord-ipc-0..9`).

- Real path via `GetFinalPathNameByHandleW`.
- Metadata from the `fumens/MusicFumenData` entry whose `file/path` matches: title,
  difficulty (`type/data`), `level`, `+` if `levelDecimal ≥ 50`. Don't guess
  difficulty from the file suffix.
- Startup filter: ~20k opens at boot. Waits for ≥100 distinct paths and a ≥5 s
  pause; groups same path+thread within 250 ms.
- Timeout: `max(180000, T_FINAL_MSEC + 15000)` ms; every accepted open resets it.
- Lost observations invalidate the activity until restart.
- Hooks set up outside `DllMain`; dll pinned; `GetLastError` preserved.

`SET_ACTIVITY`: title in `details`, `EXPERT | Lv. 10+` in `state`; clear with
`activity: null`.

## State machines

`Game` (ctor `0xdcff90`, vtable `0x1924b2c`): 0 `Advertise`, 1 `Entry`, 2 `Play`, 3 `CheckContinue`.

`Play` (ctor `0xdf40a0`, vtable `0x1929e48`):

| # | State | # | State |
| --- | --- | --- | --- |
| 0 | PlayModeSelect | 9 | PlayLinkedVerse |
| 1 | PlayContinueBonus | 10 | PlayNetBattle |
| 2 | PlayTutorial | 11 | PlayMusic |
| 3 | PlayCourseSelect | 12 | PlayMusicResult |
| 4 | PlayMapSelect | 13 | PlayTotalResult |
| 5 | PlayTicket | 14 | PlayUserBox |
| 6 | PlaySelect | 15 | PlayMateGarden |
| 7 | PlayCourseReady | 16 | PlayGameOver |
| 8 | PlayLinkedVerseSelect | | |

Unvalidated guess: `Advertise`→start menu, `PlaySelect`→song select,
`PlayMusic`→song, `PlayMusicResult`→results.

### Transition callback

| What | VA |
| --- | --- |
| Thunk | `0x4118ab` → `0x5fb360` (RVA `0x1fb360`), cdecl (name, state, phase) |
| Seen calls | `0xdf460b` (enter), `0xe11d77` (exit) |
| Generic setter | `0xa4e770` (writes `this+8`) |
| Base ctor | `0xa4b290` (`+0x18` = state count) |
| Debug globals | `0x1cae538` name, `0x1c1a33c` state, `0x1cae550` phase — not a scene API |

Results: `0xe11750` reads `this+0x70`; not validated as a signal.

## Current and total track

- Current: thunk `0x4519dd` → `0xb101d0` = `*(*(this+4)+0x20)`, `this` from global
  `0x1cb969c`. Unknown if it starts at 0 or 1.
- Total: thunk `0x440c73` → `0xb11520` → `0xb113d0`. Includes mode and bonuses;
  don't fake `1/3→2/3→3/3`, observe the native return.

## State probe

`research/state` writes `tick_ms thread machine state phase track_raw total_observed`.
Markers: `# READY`, `# REFUSED` (hash/bytes), `# LOST`, `# LIMIT` (100k lines).

Check: starting track index, total vs screen, results also on early fail, no
leftover activity after going back to song select.

## Code

| Path | What |
| --- | --- |
| `src/core.cpp` | Filter, metadata, timeout, json |
| `src/dllmain.cpp` | Hook, queue, worker |
| `src/discord_ipc.cpp` | Discord IPC |
| `tests/` | Tests |
| `research/state/` | Probe and PE inspector |

New exe = find and validate addresses again. Don't remove the version check.
