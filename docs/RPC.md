# RPC

`chuni-rpc.dll` (x86) shows song, difficulty and level on Discord, e.g.
`Chasing destiny` / `EXPERT | Lv. 10+`.

How it works: hooks `NtCreateFile`, catches `.c2s` chart opens, looks up the
song in the `Music.xml` next to the chart and sends it over Discord IPC.
Startup scan (~20k opens) and duplicate opens are filtered.

## Discord app

Discord shows "Playing <app name>", where the name comes from a Discord
application. Default is `1552492287634837536`. To use your own name/icon:

1. Create an app at https://discord.com/developers/applications. Its name is
   what Discord shows.
2. Optional: upload an image under Rich Presence → Art Assets.
3. Copy the Application ID and put it in `chuni-rpc.ini` next to the dll, or
   bake it in with `./build.ps1 -ApplicationId <id>`.

## Config

`chuni-rpc.ini` is optional, all keys have defaults:

```ini
[presence]
enabled=1
application_id=1552492287634837536
large_image=
logging=1
```

- `large_image`: asset key from step 2, empty = no image.
- `logging`: writes `chuni-rpc.log` next to the dll.

## Notes

- Activity clears after `max(180 s, last note + 15 s)`.
- Discord desktop must be open and activity sharing on.
- Uninstall: remove the `-k chuni-rpc.dll` arg and the dll.

Menu/results/track counter are still research, see
[notes](../research/notes/rich-presence.md).
