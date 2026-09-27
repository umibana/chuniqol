// Temporary research DLL. Records observations; does not change Discord state.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include "MinHook.h"

namespace {
// The routine has no floating-point return path. Preserve both integer return
// registers (EDX:EAX) even though observed callers ignore its return value.
using Observer = uint64_t(__cdecl*)(const char*, int, const char*);
Observer original;
using TotalGetter = unsigned(__thiscall*)(void*);
TotalGetter original_total;
volatile LONG observed_total = -1;
uintptr_t image_base;
volatile LONG lost;
constexpr unsigned CAPACITY = 4096;
struct Event { ULONGLONG tick; DWORD thread; int state, track, total; char name[192], phase[24]; };
struct Slot { std::atomic<unsigned> sequence; Event value; };
Slot queue[CAPACITY];
std::atomic<unsigned> head{0};
unsigned tail;

unsigned __fastcall observe_total(void* self, void*) {
    const unsigned returned = original_total(self);
    const auto saved = GetLastError();
    InterlockedExchange(&observed_total, (LONG)returned);
    SetLastError(saved);
    return returned;
}

bool read(const void* address, void* target, SIZE_T length) {
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, target, length, &copied) && copied == length;
}
bool string_read(const char* source, char* destination, size_t capacity) {
    if (!source) return false;
    size_t used = 0;
    while (used < capacity - 1) {
        // Read up to the next page boundary, without dereferencing a game pointer.
        size_t count = 4096 - ((uintptr_t)(source + used) & 4095);
        if (count > capacity - 1 - used) count = capacity - 1 - used;
        if (!read(source + used, destination + used, count)) return false;
        if (memchr(destination + used, 0, count)) return true;
        used += count;
    }
    destination[capacity - 1] = 0;
    return false; // Truncated names cannot safely identify a state machine.
}

uint64_t __cdecl observe(const char* name, int state, const char* phase) {
    const auto returned = original(name, state, phase);
    const auto saved = GetLastError();
    Event event{};
    if (string_read(name, event.name, sizeof(event.name)) && strstr(event.name, "projGame") &&
        string_read(phase, event.phase, sizeof(event.phase))) {
        event.tick = GetTickCount64();
        event.thread = GetCurrentThreadId();
        event.state = state;
        event.track = -1;
        event.total = (int)InterlockedCompareExchange(&observed_total, 0, 0);
        uintptr_t manager = 0, data = 0;
        // Proven getter: [global manager +4] -> data; [data +0x20] -> trackNum.
        // Zero/one-based interpretation is deliberately left to the capture.
        if (read((void*)(image_base + 0x18b969c), &manager, sizeof(manager)) && manager &&
            read((void*)(manager + 4), &data, sizeof(data)) && data)
            read((void*)(data + 0x20), &event.track, sizeof(event.track));
        bool pushed = false;
        auto position = head.load(std::memory_order_relaxed);
        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            auto& slot = queue[position % CAPACITY];
            auto sequence = slot.sequence.load(std::memory_order_acquire);
            const auto difference = (int32_t)(sequence - position);
            if (!difference) {
                if (head.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
                    slot.value = event;
                    slot.sequence.store(position + 1, std::memory_order_release);
                    pushed = true;
                    break;
                }
            } else if (difference < 0) break;
            else position = head.load(std::memory_order_relaxed);
        }
        if (!pushed) InterlockedIncrement(&lost);
    }
    SetLastError(saved);
    return returned;
}

bool supported_executable() {
    static const BYTE expected[32] = {0x71,0xf8,0xfe,0x2d,0xc8,0xdc,0xf2,0x87,0xe5,0xea,0xa7,0x7a,0xdd,0x22,0xaf,0x11,
        0x37,0xaa,0x5a,0xa7,0xe7,0x21,0x04,0x9d,0xee,0xfd,0x79,0x89,0x0d,0x3f,0xae,0x6e};
    wchar_t path[32768];
    const auto length = GetModuleFileNameW(nullptr, path, 32768);
    if (!length || length >= 32768) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    BYTE digest[32], chunk[65536]; DWORD got;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    while (ok) {
        if (!ReadFile(file, chunk, sizeof(chunk), &got, nullptr)) { ok = false; break; }
        if (!got) break;
        ok = BCryptHashData(hash, chunk, got, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0 && !memcmp(digest, expected, sizeof(digest));
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return ok;
}

bool write(HANDLE file, const std::string& line) {
    size_t offset = 0;
    while (offset < line.size()) {
        DWORD count = 0;
        if (!WriteFile(file, line.data() + offset, (DWORD)(line.size() - offset), &count, nullptr) || !count) return false;
        offset += count;
    }
    return true;
}

DWORD WINAPI worker(void*) {
    try {
        HMODULE pinned;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                (LPCWSTR)&image_base, &pinned)) return 0;
        wchar_t path[32768];
        const auto length = GetEnvironmentVariableW(L"CHUNI_STATE_LOG", path, 32768);
        if (!length || length >= 32768) return 0;
        HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return 0;
        if (!supported_executable()) {
            write(file, "# REFUSED executable SHA256 mismatch or unreadable file\r\n"); CloseHandle(file); return 0;
        }
        image_base = (uintptr_t)GetModuleHandleW(nullptr);
        auto target = (BYTE*)(image_base + 0x1fb360);
        const BYTE prologue[] = {0x55,0x8b,0xec,0x6a,0xff,0x68};
        BYTE live[sizeof(prologue)];
        if (!read(target, live, sizeof(live)) || memcmp(live, prologue, sizeof(live))) {
            write(file, "# REFUSED unexpected live callback prologue\r\n"); CloseHandle(file); return 0;
        }
        auto total_target = (BYTE*)(image_base + 0x711520);
        const BYTE total_prologue[] = {0x8b,0x49,0x04,0xe9};
        BYTE total_live[sizeof(total_prologue)];
        if (!read(total_target, total_live, sizeof(total_live)) || memcmp(total_live, total_prologue, sizeof(total_live))) {
            write(file, "# REFUSED unexpected total-track getter prologue\r\n"); CloseHandle(file); return 0;
        }
        for (unsigned index = 0; index < CAPACITY; ++index) queue[index].sequence.store(index);
        auto result = MH_Initialize();
        if (result == MH_OK) result = MH_CreateHook(target, (void*)&observe, (void**)&original);
        if (result == MH_OK) result = MH_CreateHook(total_target, (void*)&observe_total, (void**)&original_total);
        if (result == MH_OK) result = MH_QueueEnableHook(target);
        if (result == MH_OK) result = MH_QueueEnableHook(total_target);
        if (result == MH_OK) result = MH_ApplyQueued();
        if (result != MH_OK) {
            write(file, std::string("# Hook failed: ") + MH_StatusToString(result) + "\r\n"); CloseHandle(file); return 0;
        }
        write(file, "# READY state callback probe; observations, not confirmed screen mappings\r\n");
        write(file, "tick_ms\tthread\tmachine\tstate\tphase\ttrack_raw\ttotal_observed\r\n");
        using Value = std::tuple<int, int, int, std::string>;
        std::map<std::string, Value> previous;
        unsigned lines = 0;
        for (;;) {
            const LONG dropped = InterlockedExchange(&lost, 0);
            if (dropped && !write(file, "# LOST " + std::to_string(dropped) + " events; capture incomplete\r\n")) break;
            for (unsigned count = 0; count < CAPACITY; ++count) {
                auto& slot = queue[tail % CAPACITY];
                if (slot.sequence.load(std::memory_order_acquire) != tail + 1) break;
                const Event event = slot.value;
                slot.sequence.store(tail + CAPACITY, std::memory_order_release);
                ++tail;
                const Value value{event.state, event.track, event.total, event.phase};
                const auto found = previous.find(event.name);
                if (found != previous.end() && found->second == value) continue;
                previous[event.name] = value;
                char line[512];
                snprintf(line, sizeof(line), "%llu\t%lu\t%s\t%d\t%s\t%d\t%d\r\n",
                    event.tick, event.thread, event.name, event.state, event.phase, event.track, event.total);
                if (!write(file, line)) { CloseHandle(file); return 0; }
                if (++lines >= 100000) {
                    write(file, "# LIMIT reached; capture incomplete\r\n"); CloseHandle(file); return 0;
                }
            }
            Sleep(25);
        }
        CloseHandle(file);
    } catch (...) { OutputDebugStringA("State research worker stopped after exception.\n"); }
    return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
