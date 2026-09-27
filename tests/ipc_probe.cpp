#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core.h"
#include "discord_ipc.h"
#include <cstdio>

int main(int argc, char** argv) {
    std::wstring pipe;
    if (argc > 1) {
        for (const char* at = argv[1]; *at; ++at) pipe += (wchar_t)(unsigned char)*at;
    }
    chuni::DiscordIpc ipc("1552492287634837536", [](const std::string& text) { puts(text.c_str()); fflush(stdout); }, pipe);
    chuni::Lifetime life;
    chuni::Song song{"test", "SDHD Rich Presence connection test", "EXPERT", "10+", 0};
    const uint64_t start = GetTickCount64();
    const int64_t epoch = 1790213353;
    life.load(song, start, epoch);
    ipc.desired(chuni::activity_command(life, start, GetCurrentProcessId(), "", ""));
    bool clearing = false;
    while (GetTickCount64() - start < 20000) {
        const auto now = GetTickCount64();
        ipc.poll(now);
        if (!clearing && ipc.acknowledgements() >= 1 && now - start >= 3000) {
            life.clear();
            ipc.desired(chuni::activity_command(life, now, GetCurrentProcessId(), "", ""));
            clearing = true;
        }
        if (clearing && ipc.synchronized()) { puts("IPC set/clear: PASS"); return 0; }
        Sleep(20);
    }
    fprintf(stderr, "IPC test timed out\n");
    return 1;
}
