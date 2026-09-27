// Development-only replay of real captures. Does not inject into the game or
// connect to Discord. Execute only where locally built programs are permitted.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <stdexcept>

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static uint64_t milliseconds(const std::string& utc) {
    unsigned year, month, day, hour, minute, second, ms;
    require(sscanf(utc.c_str(), "%u-%u-%uT%u:%u:%u.%uZ", &year, &month, &day,
                   &hour, &minute, &second, &ms) == 7, "Invalid capture timestamp");
    SYSTEMTIME time{};
    time.wYear = (WORD)year; time.wMonth = (WORD)month; time.wDay = (WORD)day;
    time.wHour = (WORD)hour; time.wMinute = (WORD)minute;
    time.wSecond = (WORD)second; time.wMilliseconds = (WORD)ms;
    FILETIME file;
    require(SystemTimeToFileTime(&time, &file) != 0, "Invalid UTC date");
    ULARGE_INTEGER ticks;
    ticks.LowPart = file.dwLowDateTime; ticks.HighPart = file.dwHighDateTime;
    return ticks.QuadPart / 10000;
}

static std::wstring wide(const std::string& value) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), (int)value.size(), nullptr, 0);
    require(count > 0, "Invalid UTF-8 path");
    std::wstring result(count, L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        (int)value.size(), result.data(), count) == count, "UTF-8 conversion failed");
    // Captures contain NT absolute paths; metadata uses the Win32 equivalent.
    if (result.rfind(L"\\??\\", 0) == 0) result.replace(0, 4, L"\\\\?\\");
    return result;
}

struct Expected { const char* filename; const char* difficulty; const char* level; };

static void replay(const char* capture, const std::vector<Expected>& expected) {
    std::ifstream input(capture);
    require(input.good(), "Cannot open capture");
    chuni::ChartFilter filter;
    chuni::Lifetime life;
    unsigned accepted = 0, observations = 0;
    uint64_t prior_start = 0;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line.rfind("utc\t", 0) == 0) continue;
        std::vector<std::string> fields;
        std::istringstream row(line);
        std::string field;
        while (std::getline(row, field, '\t')) fields.push_back(field);
        require(fields.size() == 10, "Unexpected capture column count");
        if (fields[4] != "1" || fields[8] != "0") continue;
        const auto tick = milliseconds(fields[0]);
        const auto path = wide(fields[9]);
        ++observations;
        if (!filter.accept({path, tick, (uint32_t)std::stoul(fields[2])})) continue;
        require(accepted < expected.size(), "Unexpected extra song (startup/pair filter failure)");
        const auto& want = expected[accepted];
        require(chuni::utf8(path.substr(path.find_last_of(L"\\/") + 1)) == want.filename,
                "Incorrect chart selected");
        const auto song = chuni::read_metadata(path);
        require(song.has_value(), "Cannot read real chart metadata");
        require(song->title == "Chasing destiny", "Incorrect song title");
        require(song->difficulty == want.difficulty && song->level == want.level,
                "Incorrect difficulty or level");
        life.load(*song, tick, (int64_t)(tick / 1000) - 11644473600LL);
        require(tick > prior_start, "Replay did not advance time");
        require(life.deadline() >= tick + 180000, "Replay did not reset lifetime");
        auto activity = chuni::activity_command(life, tick, 123, "replay", "");
        require(activity["args"]["activity"]["details"] == "Chasing destiny", "Incorrect activity title");
        require(activity["args"]["activity"]["state"] ==
                    std::string(want.difficulty) + " | Lv. " + want.level, "Incorrect activity difficulty");
        require(chuni::activity_command(life, life.deadline(), 123, "clear", "")["args"]["activity"].is_null(),
                "Timeout failed to clear activity");
        prior_start = tick;
        ++accepted;
    }
    require(observations == 20472, "Capture does not match the reviewed complete session");
    require(accepted == expected.size(), "Missing a played song");
    printf("Capture replay: %u observations -> %u expected plays: PASS\n", observations, accepted);
}

int main(int argc, char** argv) {
    if (argc != 3) { puts("Usage: capture-replay.exe first-charts.tsv second-charts.tsv"); return 2; }
    try {
        replay(argv[1], {{"2946_02.c2s", "EXPERT", "10+"}, {"2946_03.c2s", "MASTER", "14"}, {"2946_00.c2s", "BASIC", "3"}});
        replay(argv[2], {{"2946_02.c2s", "EXPERT", "10+"}, {"2946_02.c2s", "EXPERT", "10+"}, {"2946_01.c2s", "ADVANCED", "6"}});
        return 0;
    } catch (const std::exception& error) { fprintf(stderr, "%s\n", error.what()); return 1; }
}
