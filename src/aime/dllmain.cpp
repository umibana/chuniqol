#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
HMODULE module;
SRWLOCK lock = SRWLOCK_INIT;
std::wstring config, card_path, log_path;
std::array<uint8_t, 10> card{};
bool initialized = false, present = false, last_present = false;
int last_mode = -1;
bool get_logged = false, felica_logged = false;
HRESULT last_get_result = E_UNEXPECTED;
uint8_t last_get_unit = 0;
size_t last_get_size = 0;
uint8_t scan_key = VK_RETURN;
HRESULT last_error = S_OK;

struct Exclusive {
    Exclusive() { AcquireSRWLockExclusive(&lock); }
    ~Exclusive() { ReleaseSRWLockExclusive(&lock); }
};
struct Shared {
    Shared() { AcquireSRWLockShared(&lock); }
    ~Shared() { ReleaseSRWLockShared(&lock); }
};

void log(const char* message) {
    if (FILE* file = _wfopen(log_path.c_str(), L"a")) {
        std::fprintf(file, "%llu %s\n", GetTickCount64(), message);
        std::fclose(file);
    }
    // Also reaches amdaemon-console.log. Never log the card number.
    std::fprintf(stderr, "[chuni-aime] %s\n", message);
}

void log_path_value(const char* label, const std::wstring& value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1,
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) return;
    std::string text(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1,
        &text[0], size, nullptr, nullptr);
    log((std::string(label) + text.c_str()).c_str());
}

std::wstring directory(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : path.substr(0, slash + 1);
}

std::wstring absolute(const wchar_t* value, const std::wstring& base) {
    const std::wstring candidate = PathIsRelativeW(value) ? base + value : value;
    wchar_t buffer[32768];
    const DWORD size = GetFullPathNameW(candidate.c_str(), 32768, buffer, nullptr);
    return size && size < 32768 ? std::wstring(buffer, size) : L"";
}

bool whitespace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

HRESULT read_card(std::array<uint8_t, 10>& output) {
    HANDLE file = CreateFileW(card_path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    LARGE_INTEGER size{};
    HRESULT result = S_OK;
    unsigned char bytes[256];
    DWORD got = 0;
    if (!GetFileSizeEx(file, &size)) result = HRESULT_FROM_WIN32(GetLastError());
    else if (size.QuadPart <= 0 || size.QuadPart > static_cast<LONGLONG>(sizeof(bytes))) result = E_INVALIDARG;
    else if (!ReadFile(file, bytes, static_cast<DWORD>(size.QuadPart), &got, nullptr))
        result = HRESULT_FROM_WIN32(GetLastError());
    else if (got != size.QuadPart) result = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
    CloseHandle(file);
    if (FAILED(result)) return result;
    size_t begin = 0, end = got;
    if (end >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf) begin = 3;
    while (begin < end && whitespace(bytes[begin])) ++begin;
    while (end > begin && whitespace(bytes[end - 1])) --end;
    if (end - begin != 20) return E_INVALIDARG;
    for (size_t i = begin; i < end; ++i)
        if (bytes[i] < '0' || bytes[i] > '9') return E_INVALIDARG;
    for (size_t i = 0; i < output.size(); ++i)
        output[i] = static_cast<uint8_t>(((bytes[begin + i * 2] - '0') << 4)
            | (bytes[begin + i * 2 + 1] - '0'));
    return S_OK;
}

void log_read_error(HRESULT result) {
    if (result == last_error) return;
    last_error = result;
    char message[120];
    std::snprintf(message, sizeof(message),
        "ERROR reading configured Aime file (HRESULT=%08lx); require exactly 20 decimal digits; no replacement ID generated.",
        static_cast<unsigned long>(result));
    log(message);
}
}

// Legacy API 1.0 is supported by Segatools' NFC compatibility layer. It lets
// Segatools handle the reader protocol rather than replacing its wire commands.
extern "C" __declspec(dllexport) uint16_t aime_io_get_api_version() { return 0x0100; }

extern "C" __declspec(dllexport) HRESULT aime_io_init() {
    Exclusive guard;
    initialized = present = last_present = false;
    last_mode = -1;
    get_logged = felica_logged = false;
    last_error = S_OK;
    wchar_t path[32768];
    DWORD size = GetModuleFileNameW(module, path, 32768);
    if (!size || size >= 32768) return E_FAIL;
    const auto base = directory(std::wstring(path, size));
    config = base + L"chuni-aime.ini";
    log_path = base + L"chuni-aime.log";
    if (FILE* file = _wfopen(log_path.c_str(), L"w")) std::fclose(file);

    size = GetEnvironmentVariableW(L"SEGATOOLS_CONFIG_PATH", path, 32768);
    if (size >= 32768) { log("ERROR Segatools configuration path is too long."); return E_FAIL; }
    const auto sega = size ? absolute(path, L"") : base + L"segatools.ini";
    if (sega.empty()) { log("ERROR invalid Segatools configuration path."); return E_FAIL; }
    GetPrivateProfileStringW(L"aime", L"aimePath", L"DEVICE\\aime.txt", path, 32768, sega.c_str());
    card_path = absolute(path, directory(sega));
    if (card_path.empty()) { log("ERROR invalid Aime file path."); return E_FAIL; }
    log_path_value("CONFIG Segatools: ", sega);
    log_path_value("CONFIG card file: ", card_path);
    log_path_value("CONFIG automation: ", config);
    const UINT key = GetPrivateProfileIntW(L"aime", L"scan", VK_RETURN, sega.c_str());
    if (key > 255) { log("ERROR invalid manual scan virtual-key code."); return E_INVALIDARG; }
    scan_key = static_cast<uint8_t>(key);
    const HRESULT result = read_card(card);
    if (FAILED(result)) { log_read_error(result); return result; }
    initialized = true;
    log("READY AimeIO API 1.0; configured current card validated; awaiting reader polls.");
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT aime_io_nfc_poll(uint8_t unit) {
    if (unit != 0) return S_OK;
    Exclusive guard;
    present = false;
    if (!initialized) return E_UNEXPECTED;
    const bool automatic = GetPrivateProfileIntW(L"aime", L"enabled", 0, config.c_str()) == 1;
    if (last_mode != automatic) {
        last_mode = automatic;
        log(automatic ? "AUTO enabled: present current card when reader polls."
                      : "MANUAL: automation disabled; hold configured scan key (default ENTER).");
    }
    const bool sense = automatic || (scan_key && (GetAsyncKeyState(scan_key) & 0x8000));
    if (sense) {
        const HRESULT result = read_card(card);
        if (FAILED(result)) { last_present = false; log_read_error(result); return result; }
        if (FAILED(last_error)) log("RECOVERED configured Aime file is readable again.");
        last_error = S_OK;
        present = true;
    }
    if (present != last_present) {
        log(present ? "CARD present; identity supplied from configured Aime file."
                    : "CARD absent; manual scan key released.");
        last_present = present;
    }
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT aime_io_nfc_get_aime_id(uint8_t unit, uint8_t* luid, size_t size) {
    Exclusive guard;
    const HRESULT result = !luid || size != card.size() ? E_INVALIDARG
        : unit != 0 || !initialized || !present ? S_FALSE : S_OK;
    if (!get_logged || result != last_get_result || unit != last_get_unit || size != last_get_size) {
        char message[160];
        std::snprintf(message, sizeof(message),
            "GET AIME: unit=%u size=%llu result=%08lx; no card ID logged.",
            static_cast<unsigned>(unit), static_cast<unsigned long long>(size),
            static_cast<unsigned long>(result));
        log(message);
        get_logged = true;
        last_get_result = result;
        last_get_unit = unit;
        last_get_size = size;
    }
    if (result == S_OK) std::memcpy(luid, card.data(), card.size());
    return result;
}

extern "C" __declspec(dllexport) HRESULT aime_io_nfc_get_felica_id(uint8_t, uint64_t* id) {
    Exclusive guard;
    if (!felica_logged) {
        log("GET FELICA: unsupported; current configured Aime card is the only identity.");
        felica_logged = true;
    }
    return id ? S_FALSE : E_INVALIDARG;
}

extern "C" __declspec(dllexport) void aime_io_led_set_color(uint8_t, uint8_t, uint8_t, uint8_t) {}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
