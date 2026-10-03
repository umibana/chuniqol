# chuniqol

different variety of mods for sdhd 2.50

> [!WARNING]
> ⚠️ This code has been fully vibe-coded using GPT Astra 6, only the results were tested by a human. ⚠️


| Name                | Function                                          | Doc                        |
| ------------------- | ------------------------------------------------- | -------------------------- |
| `chuni-rpc.dll`     | Discord Rich presence (song + difficulty)         | [RPC](docs/RPC.md)         |
| `chuni-credits.dll` | Start with 99 credits                             | [CREDITS](docs/CREDITS.md) |
| `chuni-premium.dll` | Automatically select PREMIUM TICKET               | [PREMIUM](docs/PREMIUM.md) |
| `chuni-escape.dll`  | Track skip with ESC                               | [ESC](docs/ESC.md)         |
| `chuni-tracks.dll`  | 4 songs instead of 3.                             | [TRACKS](docs/TRACKS.md)   |
| `chuni-levels.dll`  | Show internal difficulty level next to song title | [LEVELS](docs/LEVELS.md)   |




## Build

Windows 10/11, PowerShell, [Zig 0.13.0](https://ziglang.org/download/0.13.0/).

Deps go in `vendor/` (not included, see [THIRD_PARTY.md](THIRD_PARTY.md)):

```powershell
git clone https://github.com/TsudaKageyu/minhook vendor/minhook
git -C vendor/minhook checkout c3fcafd
git clone --depth 1 -b 10.0.0 https://github.com/leethomason/tinyxml2 vendor/tinyxml2
mkdir vendor/json
iwr https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -OutFile vendor/json/json.hpp
iwr https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT -OutFile vendor/json/LICENSE.MIT
```

The rpc needs all three, the other mods only need MinHook.

```powershell
./build.ps1 -ZigPath 'C:\tools\zig\zig.exe'   # or ZIG_EXE / zig in PATH
./build/core-test.exe
```

Output: `build/chuni-rpc.dll`. `./package.ps1` makes a zip in `dist/`.

## Install RPC

Copy dll next to `chusanApp.exe` and add `-k chuni-rpc.dll` to `launch.bat`

```bat
inject_x86 -d -k chusanhook_x86.dll -k chuni-rpc.dll chusanApp.exe
```

Works with the default Discord app. To show your own name/icon, see
[RPC](docs/RPC.md#discord-app).

## Install the rest

```powershell
.\build-credits.ps1 -ZigPath C:\tools\zig\zig.exe
.\build-premium.ps1 -ZigPath C:\tools\zig\zig.exe
.\build-escape.ps1  -ZigPath C:\tools\zig\zig.exe
.\build-tracks.ps1  -ZigPath C:\tools\zig\zig.exe
.\build-levels.ps1  -ZigPath C:\tools\zig\zig.exe
# game and amdaemon closed:
.\install-extras.ps1 -GameBin C:\path\SDHD\App\bin -Escape -Tracks -Levels
```

Installer backs up launcher/config, sets `max_credit: 99`, keeps existing
modules and ini files. Each mod needs `enabled=1` in its ini.

### Manual install

Back up `launch.bat` and `config_common.json`.

1. Copy the dlls from `build/` next to `chusanApp.exe` (`App\bin`).
2. In `config_common.json` set `"max_credit": 99`.
3. In `launch.bat` add `-k chuni-credits.dll` to the **inject_x64** (amdaemon)
  line, and the rest to the **inject_x86** (chusanApp) line, keeping what's
   already there:

```bat
inject_x64 ... -k chuni-credits.dll amdaemon.exe ...
inject_x86 -d -k chusanhook_x86.dll -k chuni-rpc.dll -k chuni-premium.dll -k chuni-escape.dll -k chuni-tracks.dll -k chuni-levels.dll chusanApp.exe
```

1. Create an ini next to each dll (credits doesn't need one):

```ini
; chuni-premium.ini
[premium]
enabled=1
ticketId=2070
[display]
restoreCredits=1

; chuni-escape.ini
[escape]
enabled=1

; chuni-tracks.ini
[tracks]
enabled=1

; chuni-levels.ini
[levels]
enabled=1
```

Skip any mod you don't want; ESC needs premium.

## maimai DX — SDEZ 1.66

Mods for SDEZ 1.66 are kept separately from the SDHD mods above.
See [NativeTitleLevel](sdez-1.66/NativeTitleLevel/README.md) for internal difficulty in the native song title, build instructions and installation.
