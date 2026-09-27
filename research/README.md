# research

Not needed for the mods. Addresses only apply to SDHD 2.50
(chusanApp SHA256 `71f8fe2dc8dcf287e5eaa77add22af1137aa5aa7e721049deefd79890d3fae6e`).

## Notes

| File | About |
| --- | --- |
| [rich-presence](notes/rich-presence.md) | Song detection, state machines, track counter |
| [credits](notes/credits.md) | amdaemon credit storage and spend |
| [premium](notes/premium.md) | Ticket select view and purchase flow |
| [esc](notes/esc.md) | Song abort / native Track Skip |
| [results](notes/results.md) | Result screen progression after a skip |

## State probe

`state/state_probe.cpp`: hooks the state callback and track total. Not
validated in game, don't ship it.

```powershell
./research/state/build.ps1 -ZigPath 'C:\tools\zig\zig.exe'
```

Copy `research/state/build/chuni-state-diagnostic.dll` into the game, set
`CHUNI_STATE_LOG` to an absolute path of a new tsv outside the repo, and add
`-k chuni-state-diagnostic.dll` in a separate launcher.

## Static inspection

```powershell
python -m pip install capstone==5.0.6
python research/state/inspect_game.py --exe 'D:\game\App\bin\chusanApp.exe' --va 0x4118ab --size 320
```
