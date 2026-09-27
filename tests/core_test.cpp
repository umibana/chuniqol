#include "core.h"
#include <cstdio>
#include <stdexcept>
#include <fstream>

using namespace chuni;
#define CHECK(x) do { if (!(x)) throw std::runtime_error("FAIL: " #x); } while (0)

static void scan(ChartFilter& filter) {
    for (unsigned i = 0; i < 120; ++i)
        CHECK(!filter.accept({L"C:\\music\\" + std::to_wstring(i) + L"_00.c2s", i * 10, 1}));
}

int main() {
    try {
        ChartFilter filter;
        scan(filter);
        CHECK(filter.accept({L"C:\\music\\2946_02.c2s", 7000, 1}));
        CHECK(!filter.accept({L"c:\\MUSIC\\2946_02.C2S", 7001, 1}));
        CHECK(filter.accept({L"C:\\music\\2946_02.c2s", 70000, 1}));
        CHECK(filter.accept({L"C:\\music\\2946_01.c2s", 130000, 2}));
        CHECK(filter.accept({L"C:\\music\\2946_01.c2s", 130001, 3}));
        ChartFilter no_scan;
        CHECK(!no_scan.accept({L"C:\\music\\2946_02.c2s", 100, 2}));
        CHECK(!no_scan.accept({L"C:\\music\\2946_02.c2s", 10000, 2}));
        filter.reset();
        CHECK(!filter.accept({L"C:\\music\\2946_02.c2s", 160000, 1}));
        CHECK(timeout_ms(164838) == 180000);
        CHECK(timeout_ms(205000) == 220000);
        CHECK(timeout_ms(177884) == 192884);
        CHECK(timeout_ms(0) == 180000);
        CHECK(timeout_ms(999999999) == 180000);
        Lifetime life;
        Song song{"2946", "Chasing destiny", "EXPERT", "10+", 164838};
        life.load(song, 1000, 12345);
        CHECK(life.active(180999));
        CHECK(!life.active(181000));
        life.load(song, 200000, 12500);
        life.load(song, 250000, 12550);
        CHECK(life.active(380000));
        CHECK(!life.active(430000));
        life.clear();
        CHECK(!life.active(0));

        const std::string xml = u8R"(<MusicData><name><id>2946</id><str>日本 &amp; \"chart\"</str></name><fumens><MusicFumenData><type><data>EXPERT</data></type><file><path>2946_02.c2s</path></file><level>10</level><levelDecimal>50</levelDecimal></MusicFumenData></fumens></MusicData>)";
        auto parsed = parse_metadata(xml, "2946_02.C2S", "T_FINAL_MSEC\t164838\n");
        CHECK(parsed.has_value());
        CHECK(parsed->title == u8"日本 & \\\"chart\\\"");
        CHECK(parsed->difficulty == "EXPERT" && parsed->level == "10+");
        CHECK(parsed->final_note_ms == 164838);
        CHECK(!parse_metadata(xml, "2946_03.c2s", "").has_value());
        CHECK(!parse_metadata("<broken>", "2946_02.c2s", "").has_value());
        life.load(*parsed, 0, 12345);
        auto command = activity_command(life, 1, 55, "test", "");
        CHECK(command["cmd"] == "SET_ACTIVITY");
        CHECK(command["args"]["pid"] == 55);
        CHECK(command["args"]["activity"]["details"] == parsed->title);
        CHECK(command["args"]["activity"]["timestamps"]["start"] == 12345);
        CHECK(activity_command(life, 180000, 55, "clear", "")["args"]["activity"].is_null());
        CHECK(valid_app_id("1552492287634837536"));
        CHECK(!valid_app_id("not-a-token"));
        puts("Native core tests: PASS");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr, "%s\n", error.what()); return 1; }
}
