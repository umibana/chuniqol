// Run only in a Windows environment that permits this locally built DLL.
// Checks native hook transparency; song/Discord behavior needs integration tests.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
int main(int argc, char** argv) {
    if (argc != 2) { puts("Usage: hook-smoke.exe path-to-chuni-rpc.dll"); return 2; }
    auto dll = LoadLibraryA(argv[1]);
    if (!dll) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 1; }
    auto ready = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(dll, "ChuniRpcStatus"));
    if (!ready) return 1;
    for (int i = 0; i < 100 && ready() == 0; ++i) Sleep(50);
    if (ready() != 1) { printf("Hook not ready: %d\n", ready()); return 1; }
    auto missing = CreateFileW(L"Z:\\nonexistent-chuni-rpc-test\\missing.c2s", GENERIC_READ,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (missing != INVALID_HANDLE_VALUE) { CloseHandle(missing); return 1; }
    if (GetLastError() != ERROR_PATH_NOT_FOUND && GetLastError() != ERROR_FILE_NOT_FOUND &&
        GetLastError() != ERROR_INVALID_DRIVE) return 1;
    puts("Hook loaded; failing file-open behavior preserved.");
    return 0;
}
