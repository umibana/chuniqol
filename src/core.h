#pragma once
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include "json.hpp"

namespace chuni {
struct ChartEvent { std::wstring path; uint64_t tick; uint32_t thread; };
class ChartFilter {
public:
    bool accept(const ChartEvent& event);
    void reset();
    bool armed() const { return armed_; }
private:
    std::set<std::wstring> initial_;
    bool armed_ = false, seen_ = false;
    uint64_t last_ = 0, group_start_ = 0;
    uint32_t group_thread_ = 0;
    std::wstring group_path_;
};
struct Song {
    std::string id, title, difficulty, level;
    uint64_t final_note_ms = 0;
};
uint64_t timeout_ms(uint64_t final_note_ms);
class Lifetime {
public:
    void load(const Song& song, uint64_t tick, int64_t unix_seconds);
    void clear();
    bool active(uint64_t now) const;
    const Song& song() const { return song_; }
    int64_t start() const { return start_; }
    uint64_t deadline() const { return deadline_; }
private:
    Song song_;
    bool loaded_ = false;
    uint64_t deadline_ = 0;
    int64_t start_ = 0;
};
std::optional<Song> parse_metadata(const std::string& xml, const std::string& filename, const std::string& chart);
std::optional<Song> read_metadata(const std::wstring& chart_path);
std::string utf8(const std::wstring& text);
std::wstring lower_path(std::wstring path);
bool valid_app_id(const std::string& value);
nlohmann::json activity_command(const Lifetime& life, uint64_t now, uint32_t pid,
                                 const std::string& nonce, const std::string& image);
}
