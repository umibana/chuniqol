#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include "MinHook.h"
#include "credit_core.h"

namespace {
constexpr char kExpectedHash[]="014992a5af1b62b5fd42d2cb0c19a38aba4b5437a3ef82d304de2c93065ed70d";
constexpr unsigned char kGetterBytes[]={0x4c,0x8b,0xc1,0x48,0x85,0xc9,0x75,0x06,0xb8,0x03,0x00,0x00,0x81,0xc3};
constexpr unsigned char kDebitBytes[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x0f,0xb6,0xfa,0x8b,0xd9,0xe8,0x8c,0xf8,0xff,0xff,0x83,0xf8,0x02};
constexpr unsigned char kSubtractBytes[]={0x40,0x28,0x7c,0x4a,0x28};
constexpr unsigned char kLockedCallBytes[]={0xff,0x15,0xd7,0x28,0x2d,0x00,0x48,0x8b,0xcb,0xe8,0x1f,0x00,0x00,0x00};
credits::Hooks hooks;
HINSTANCE own_module;
HANDLE log_file=INVALID_HANDLE_VALUE;
volatile LONG status=0;
volatile LONG observed=0;
volatile LONG active=0;

void log_line(const char* message) {
    if (log_file==INVALID_HANDLE_VALUE) return;
    char line[512];
    const int n=std::snprintf(line,sizeof(line),"[%lu] %s\r\n",GetCurrentProcessId(),message);
    if (n>0 && static_cast<unsigned>(n)<sizeof(line)) {
        DWORD written;
        WriteFile(log_file,line,static_cast<DWORD>(n),&written,nullptr);
        FlushFileBuffers(log_file);
    }
}
void open_log() {
    wchar_t path[32768];
    const DWORD n=GetModuleFileNameW(own_module,path,32768);
    if (!n || n>=32768) return;
    auto* slash=wcsrchr(path,L'\\');
    if (!slash) return;
    const wchar_t name[]=L"chuni-credits.log";
    if (static_cast<size_t>(slash-path)+1+std::size(name)>std::size(path)) return;
    std::memcpy(slash+1,name,sizeof(name));
    log_file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
}
bool hash_file(const wchar_t* path,char (&hex)[65]) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char object[8192],buffer[65536],digest[32];
    DWORD object_size=0,written=0;
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if (ok) ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof(object_size),&written,0)>=0 && object_size<=sizeof(object);
    if (ok) ok=BCryptCreateHash(algorithm,&hash,object,object_size,nullptr,0,0)>=0;
    while (ok) {
        DWORD got=0;
        if (!ReadFile(file,buffer,sizeof(buffer),&got,nullptr)) { ok=false;break; }
        if (!got) break;
        ok=BCryptHashData(hash,buffer,got,0)>=0;
    }
    if (ok) ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);
    if (ok) {
        constexpr char digits[]="0123456789abcdef";
        for (unsigned i=0;i<32;++i) { hex[i*2]=digits[digest[i]>>4];hex[i*2+1]=digits[digest[i]&15]; }
        hex[64]=0;
    }
    return ok;
}
int hooked_get(void* output) {
    if (!InterlockedCompareExchange(&active,0,0)) return hooks.original_get(output);
    const int result=hooks.get(output);
    if (output && result==0 && hooks.state.image[credits::kStock]==99 &&
        hooks.state.image[credits::kMaximum]==99 && InterlockedCompareExchange(&observed,1,0)==0) {
        log_line(hooks.state.image[0x96a05a]==1
            ? "Actual stock=99; native FREE PLAY is enabled, so the game may display FREE PLAY."
            : "Actual stock=99; native freeplay=0. Display must be verified in the game.");
    }
    return result;
}
int hooked_debit(int node,uint8_t count) {
    if (!InterlockedCompareExchange(&active,0,0)) return hooks.original_debit(node,count);
    return hooks.consume(node,count);
}

bool matches(const uint8_t* image,size_t rva,const unsigned char* expected,size_t n) {
    return std::memcmp(image+rva,expected,n)==0;
}
DWORD WINAPI initialize(void*) {
    open_log();
    log_line("Checking amdaemon build; hooks inactive.");
    wchar_t path[32768];
    DWORD n=GetModuleFileNameW(nullptr,path,32768);
    auto* image=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    char hash[65]{};
    if (!n || n>=32768 || !image || !hash_file(path,hash) || std::strcmp(hash,kExpectedHash)!=0) {
        log_line("DISABLED: executable SHA256 does not match the supported amdaemon; nothing patched.");
        InterlockedExchange(&status,-1);return 0;
    }
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || nt->Signature!=IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage<credits::kRequiredSize ||
        !matches(image,credits::kGetter,kGetterBytes,sizeof(kGetterBytes)) ||
        !matches(image,credits::kDebit,kDebitBytes,sizeof(kDebitBytes)) ||
        !matches(image,0x2de4f7,kSubtractBytes,sizeof(kSubtractBytes)) ||
        !matches(image,0x2deb03,kLockedCallBytes,sizeof(kLockedCallBytes))) {
        log_line("DISABLED: loaded image/prologue mismatch; nothing patched.");
        InterlockedExchange(&status,-1);return 0;
    }
    // Pin before publishing any callbacks. Removing -k and restarting disables it.
    HMODULE pinned=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&hooked_get),&pinned)) {
        log_line("DISABLED: cannot pin DLL; nothing patched.");
        InterlockedExchange(&status,-1);return 0;
    }
    hooks.state={image,nt->OptionalHeader.SizeOfImage};
    MH_STATUS mh=MH_Initialize();
    if (mh!=MH_OK) {
        log_line("DISABLED: MinHook initialization failed; nothing patched.");
        InterlockedExchange(&status,-1);return 0;
    }
    void* getter=image+credits::kGetter;
    void* debit=image+credits::kDebit;
    bool getter_created=false,debit_created=false;
    mh=MH_CreateHook(getter,reinterpret_cast<void*>(&hooked_get),reinterpret_cast<void**>(&hooks.original_get));
    if (mh==MH_OK) {
        getter_created=true;
        mh=MH_CreateHook(debit,reinterpret_cast<void*>(&hooked_debit),reinterpret_cast<void**>(&hooks.original_debit));
        debit_created=mh==MH_OK;
    }
    if (mh==MH_OK) mh=MH_QueueEnableHook(getter);
    if (mh==MH_OK) mh=MH_QueueEnableHook(debit);
    if (mh==MH_OK) mh=MH_ApplyQueued();
    if (mh!=MH_OK) {
        log_line(MH_StatusToString(mh));
        // ApplyQueued can enable one hook before a later protection failure.
        // Disable before removing; retain trampolines if rollback fails.
        const MH_STATUS disabled=MH_DisableHook(MH_ALL_HOOKS);
        if (disabled==MH_OK || disabled==MH_ERROR_DISABLED) {
            if (getter_created) MH_RemoveHook(getter);
            if (debit_created) MH_RemoveHook(debit);
            MH_Uninitialize();
            log_line("DISABLED: hook transaction failed; hooks rolled back.");
        } else {
            log_line("ERROR: hook rollback failed; restart amdaemon before continuing.");
        }
        InterlockedExchange(&status,-1);return 0;
    }
    log_line("Hooks ready. Waiting for initialized native credit manager; target actual stock=99.");
    InterlockedExchange(&active,1);
    InterlockedExchange(&status,1);
    return 0;
}
}
extern "C" __declspec(dllexport) LONG WINAPI ChuniCreditsStatus() {
    return InterlockedCompareExchange(&status,0,0);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) {
        own_module=module;
        DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,initialize,nullptr,0,nullptr);
        if (thread) CloseHandle(thread);
        else InterlockedExchange(&status,-1);
    }
    return TRUE;
}
