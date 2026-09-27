#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core.h"
#include "tinyxml2.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <limits>

namespace chuni {
std::wstring lower_path(std::wstring path) {
    std::transform(path.begin(), path.end(), path.begin(), [](wchar_t c) { return c == L'/' ? L'\\' : std::towlower(c); });
    return path;
}

bool ChartFilter::accept(const ChartEvent& event) {
    const auto path = lower_path(event.path);
    if (!armed_) {
        const bool gap = seen_ && event.tick >= last_ && event.tick - last_ >= 5000;
        if (gap && initial_.size() >= 100) {
            armed_ = true;
            initial_.clear();
        } else {
            if (gap) initial_.clear();
            if (initial_.size() < 100) initial_.insert(path);
            last_ = event.tick;
            seen_ = true;
            return false;
        }
    }
    if (path == group_path_ && event.thread == group_thread_ && event.tick >= group_start_ && event.tick - group_start_ <= 250)
        return false;
    group_path_ = path;
    group_thread_ = event.thread;
    group_start_ = event.tick;
    return true;
}
void ChartFilter::reset() { *this = ChartFilter{}; }
uint64_t timeout_ms(uint64_t duration) {
    return duration > 0 && duration <= 3600000 ? std::max<uint64_t>(180000, duration + 15000) : 180000;
}
void Lifetime::load(const Song& song, uint64_t tick, int64_t unix_seconds) {
    song_ = song;
    loaded_ = true;
    deadline_ = tick + timeout_ms(song.final_note_ms);
    start_ = unix_seconds;
}
void Lifetime::clear() { loaded_ = false; }
bool Lifetime::active(uint64_t now) const { return loaded_ && now < deadline_; }

std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), (int)text.size(), result.data(), size, nullptr, nullptr);
    return result;
}
static const char* text(const tinyxml2::XMLElement* node, const char* child) {
    if (!node) return "";
    auto element = node->FirstChildElement(child);
    return element && element->GetText() ? element->GetText() : "";
}
static std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}
static std::string filename_only(const std::string& value) {
    auto separator = value.find_last_of("\\/");
    return separator == std::string::npos ? value : value.substr(separator + 1);
}
std::optional<Song> parse_metadata(const std::string& xml, const std::string& filename, const std::string& chart) {
    tinyxml2::XMLDocument document;
    if (document.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) return {};
    auto music = document.FirstChildElement("MusicData");
    if (!music) return {};
    auto name = music->FirstChildElement("name");
    auto fumens = music->FirstChildElement("fumens");
    if (!name || !fumens) return {};
    for (auto fumen = fumens->FirstChildElement("MusicFumenData"); fumen; fumen = fumen->NextSiblingElement("MusicFumenData")) {
        auto file = fumen->FirstChildElement("file");
        if (lower_ascii(filename_only(text(file, "path"))) != lower_ascii(filename)) continue;
        Song song;
        song.id = text(name, "id");
        song.title = text(name, "str");
        song.difficulty = text(fumen->FirstChildElement("type"), "data");
        song.level = text(fumen, "level");
        int decimal = 0;
        if (auto element = fumen->FirstChildElement("levelDecimal")) element->QueryIntText(&decimal);
        if (decimal >= 50) song.level += "+";
        if (song.title.empty() || song.difficulty.empty() || song.level.empty()) return {};
        size_t pos = 0;
        while (pos < chart.size()) {
            size_t end = chart.find('\n', pos);
            if (end == std::string::npos) end = chart.size();
            auto line = chart.substr(pos, end - pos);
            if (line.rfind("T_FINAL_MSEC", 0) == 0 && line.size() > 12 && (line[12] == '\t' || line[12] == ' ')) {
                char* stop = nullptr;
                const char* start = line.c_str() + 12;
                unsigned long long result = strtoull(start, &stop, 10);
                if (stop != start && result <= 3600000) song.final_note_ms = result;
                break;
            }
            pos = end + 1;
        }
        return song;
    }
    return {};
}
static std::optional<std::string> read_file(const std::wstring& path, uint32_t maximum) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > maximum) { CloseHandle(file); return {}; }
    std::string content((size_t)size.QuadPart, '\0');
    DWORD total = 0;
    while (total < content.size()) {
        DWORD count = 0;
        if (!ReadFile(file, content.data() + total, (DWORD)content.size() - total, &count, nullptr) || count == 0) {
            CloseHandle(file); return {};
        }
        total += count;
    }
    CloseHandle(file);
    return content;
}
std::optional<Song> read_metadata(const std::wstring& path) {
    auto separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return {};
    auto xml = read_file(path.substr(0, separator + 1) + L"Music.xml", 2 * 1024 * 1024);
    if (!xml) return {};
    auto chart = read_file(path, 10 * 1024 * 1024);
    return parse_metadata(*xml, utf8(path.substr(separator + 1)), chart.value_or(""));
}
bool valid_app_id(const std::string& value) {
    return value.size() >= 17 && value.size() <= 20 && value[0] != '0' &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}
static std::string clipped_utf8(std::string value) {
    if (value.size() <= 128) return value;
    size_t end = 125;
    while (end > 0 && ((unsigned char)value[end] & 0xC0) == 0x80) --end;
    return value.substr(0, end) + "...";
}
nlohmann::json activity_command(const Lifetime& life, uint64_t now, uint32_t pid,
                                 const std::string& nonce, const std::string& image) {
    nlohmann::json activity = nullptr;
    if (life.active(now)) {
        activity = {{"details", clipped_utf8(life.song().title)},
                    {"state", clipped_utf8(life.song().difficulty + " | Lv. " + life.song().level)},
                    {"timestamps", {{"start", life.start()}}}, {"instance", false}};
        if (!image.empty()) activity["assets"] = {{"large_image", image}, {"large_text", "SDHD 2.50"}};
    }
    return {{"cmd", "SET_ACTIVITY"}, {"nonce", nonce}, {"args", {{"pid", pid}, {"activity", activity}}}};
}
}
