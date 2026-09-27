#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <ctime>
#include <cstring>
#include <atomic>
#include "MinHook.h"
#include "core.h"
#include "discord_ipc.h"

#ifndef CHUNI_APPLICATION_ID
#define CHUNI_APPLICATION_ID "1552492287634837536"
#endif

namespace {
using CreateFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
    PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
CreateFn original_create;
HMODULE module_handle;
volatile LONG status_code, worker_id, dropped;
constexpr unsigned CAPACITY = 4096, PATH_CAPACITY = 2048;
struct Event { wchar_t path[PATH_CAPACITY]; uint64_t tick; DWORD thread; };
struct Slot { std::atomic<uint32_t> sequence; Event event; };
Slot queue[CAPACITY];
std::atomic<uint32_t> head{0};
uint32_t tail;
HANDLE log_handle = INVALID_HANDLE_VALUE;
uint64_t log_bytes;

bool readable(const void* from, void* to, SIZE_T size) {
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), from, to, size, &copied) && copied == size;
}

NTSTATUS NTAPI observe(PHANDLE handle, ACCESS_MASK access, POBJECT_ATTRIBUTES attrs,
    PIO_STATUS_BLOCK io, PLARGE_INTEGER allocation, ULONG file_attrs, ULONG share,
    ULONG disposition, ULONG options, PVOID ea, ULONG ea_size) {
    const NTSTATUS result = original_create(handle, access, attrs, io, allocation,
        file_attrs, share, disposition, options, ea, ea_size);
    const DWORD saved = GetLastError();
    const DWORD thread = GetCurrentThreadId();
    // Excluding the worker prevents recursive observations from metadata reads.
    if (result >= 0 && thread != (DWORD)InterlockedCompareExchange(&worker_id, 0, 0) &&
        (access & (GENERIC_READ | FILE_READ_DATA))) {
        OBJECT_ATTRIBUTES attributes;
        UNICODE_STRING name;
        wchar_t suffix[4];
        if (readable(attrs, &attributes, sizeof(attributes)) &&
            readable(attributes.ObjectName, &name, sizeof(name)) && name.Buffer &&
            name.Length >= sizeof(suffix) && !(name.Length & 1) &&
            (ULONG_PTR)name.Buffer <= UINTPTR_MAX - name.Length &&
            readable(reinterpret_cast<const char*>(name.Buffer) + name.Length - sizeof(suffix), suffix, sizeof(suffix)) &&
            suffix[0] == L'.' && (suffix[1] == L'c' || suffix[1] == L'C') &&
            suffix[2] == L'2' && (suffix[3] == L's' || suffix[3] == L'S')) {
            Event event{};
            // Resolve the already-open handle: also works for relative NT paths
            // and observes the path after segatools VFS translation.
            HANDLE opened;
            DWORD length = 0;
            if (readable(handle, &opened, sizeof(opened)))
                length = GetFinalPathNameByHandleW(opened, event.path, PATH_CAPACITY, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (!length || length >= PATH_CAPACITY) {
                InterlockedExchange(&dropped, 1);
            } else {
                event.tick = GetTickCount64();
                event.thread = thread;
                // Bounded MPSC queue: a preempted worker never stalls a game
                // thread. Sequence numbers publish only fully copied events.
                bool pushed = false;
                auto position = head.load(std::memory_order_relaxed);
                for (unsigned attempt = 0; attempt < 64; ++attempt) {
                    auto& slot = queue[position % CAPACITY];
                    const auto sequence = slot.sequence.load(std::memory_order_acquire);
                    const int32_t difference = (int32_t)(sequence - position);
                    if (difference == 0) {
                        if (head.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
                            slot.event = event;
                            slot.sequence.store(position + 1, std::memory_order_release);
                            pushed = true;
                            break;
                        }
                    } else if (difference < 0) break;
                    else position = head.load(std::memory_order_relaxed);
                }
                if (!pushed) InterlockedExchange(&dropped, 1);
            }
        }
    }
    SetLastError(saved);
    return result;
}

// Worker-only logging; bounded to 1 MiB per process run, no personal Discord data.
void log(const std::string& message) {
    if (log_handle == INVALID_HANDLE_VALUE || log_bytes >= 1024 * 1024) return;
    const auto line = std::to_string(GetTickCount64()) + " " + message + "\r\n";
    DWORD written;
    if (WriteFile(log_handle, line.data(), (DWORD)line.size(), &written, nullptr)) log_bytes += written;
}

DWORD WINAPI worker(void*) {
    InterlockedExchange(&worker_id, (LONG)GetCurrentThreadId());
    // Pin before installing pointers into this module. No teardown under loader lock.
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&module_handle), &pinned)) {
        InterlockedExchange(&status_code, -1); return 0;
    }
    try {
        wchar_t path[32768];
        const DWORD length = GetModuleFileNameW(module_handle, path, 32768);
        if (!length || length >= 32768) { InterlockedExchange(&status_code, -1); return 0; }
        const std::wstring module_path(path, length);
        const auto directory = module_path.substr(0, module_path.find_last_of(L"\\/") + 1);
        const auto ini = directory + L"chuni-rpc.ini";
        if (!GetPrivateProfileIntW(L"presence", L"enabled", 1, ini.c_str())) {
            InterlockedExchange(&status_code, 2); return 0;
        }
        wchar_t configured[256];
        GetPrivateProfileStringW(L"presence", L"application_id", L"", configured, 256, ini.c_str());
        const std::string app_id = configured[0] ? chuni::utf8(configured) : CHUNI_APPLICATION_ID;
        GetPrivateProfileStringW(L"presence", L"large_image", L"", configured, 256, ini.c_str());
        const std::string image = chuni::utf8(configured);
        if (GetPrivateProfileIntW(L"presence", L"logging", 1, ini.c_str()))
            log_handle = CreateFileW((directory + L"chuni-rpc.log").c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!chuni::valid_app_id(app_id)) { log("Invalid public application ID."); InterlockedExchange(&status_code, -1); return 0; }
        for (unsigned index = 0; index < CAPACITY; ++index) queue[index].sequence.store(index, std::memory_order_relaxed);
        const auto target = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateFile"));
        MH_STATUS mh = MH_Initialize();
        if (mh == MH_OK) mh = MH_CreateHook(target, reinterpret_cast<void*>(&observe), reinterpret_cast<void**>(&original_create));
        if (mh == MH_OK) mh = MH_EnableHook(target);
        if (mh != MH_OK) {
            log(std::string("Hook failed: ") + MH_StatusToString(mh));
            InterlockedExchange(&status_code, -1); return 0;
        }
        chuni::ChartFilter filter;
        chuni::Lifetime life;
        chuni::DiscordIpc discord(app_id, log);
        InterlockedExchange(&status_code, 1);
        log("Hook ready; waiting for initial chart scan.");
        bool had_activity = false;
        bool invalidated = false;
        for (;;) {
            // Once observation is incomplete, suppress activity until restart.
            // Rescanning after a dropped gameplay event could misclassify music.
            if (InterlockedExchange(&dropped, 0)) {
                invalidated = true;
                life.clear();
                log("Chart observations lost; presence disabled until game restart.");
            }
            for (unsigned count = 0; count < CAPACITY; ++count) {
                Event event;
                auto& slot = queue[tail % CAPACITY];
                const bool present = slot.sequence.load(std::memory_order_acquire) == tail + 1;
                if (present) {
                    event = slot.event;
                    slot.sequence.store(tail + CAPACITY, std::memory_order_release);
                    ++tail;
                }
                if (!present) break;
                if (invalidated || !filter.accept({event.path, event.tick, event.thread})) continue;
                auto song = chuni::read_metadata(event.path);
                if (!song) { life.clear(); log("Chart metadata unavailable; activity cleared."); continue; }
                const auto now = GetTickCount64();
                life.load(*song, event.tick, (int64_t)std::time(nullptr) - (int64_t)((now - event.tick) / 1000));
                log("Chart: " + song->title + " | " + song->difficulty + " | Lv. " + song->level);
            }
            const auto now = GetTickCount64();
            const bool active = life.active(now);
            if (had_activity && !active) log("Song activity cleared (timeout or unavailable metadata).");
            had_activity = active;
            discord.desired(chuni::activity_command(life, now, GetCurrentProcessId(), "", image));
            discord.poll(now);
            Sleep(25);
        }
    } catch (...) {
        // Never propagate optional integration failure into the game.
        InterlockedExchange(&status_code, -1);
        OutputDebugStringA("SDHD Rich Presence worker stopped after an exception.\n");
    }
    return 0;
}
}
extern "C" __declspec(dllexport) int __cdecl ChuniRpcStatus() {
    return (int)InterlockedCompareExchange(&status_code, 0, 0);
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module_handle = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
        else InterlockedExchange(&status_code, -1);
    }
    return TRUE;
}
