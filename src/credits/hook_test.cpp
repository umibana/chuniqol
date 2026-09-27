#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "MinHook.h"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
using Getter=int(*)(void*);
using Debit=int(*)(int,unsigned char);
static Getter original_get;
static Debit original_debit;
static unsigned getter_calls,debits;
static int get_hook(void* out) { ++getter_calls;return original_get(out); }
static int debit_hook(int node,unsigned char count) { ++debits;return original_debit(node,count)+1; }
int main() {
    // Exact target getter prefix: MOV R8,RCX / TEST RCX,RCX / short conditional
    // branch / native null error return. The success tail is a synthetic 99.
    const unsigned char get_code[]={0x4c,0x8b,0xc1,0x48,0x85,0xc9,0x75,0x06,0xb8,0x03,0,0,0x81,0xc3,0xb8,99,0,0,0,0xc3};
    // Exact first target debit instruction, then a synthetic return of count.
    const unsigned char debit_code[]={0x48,0x89,0x5c,0x24,0x08,0x0f,0xb6,0xc2,0xc3};
    auto* page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    CHECK(page);
    std::memcpy(page,get_code,sizeof(get_code));
    std::memcpy(page+64,debit_code,sizeof(debit_code));
    DWORD old=0;
    CHECK(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old));
    FlushInstructionCache(GetCurrentProcess(),page,4096);
    auto get=reinterpret_cast<Getter>(page);
    auto debit=reinterpret_cast<Debit>(page+64);
    CHECK(get(nullptr)==static_cast<int>(0x81000003u) && get(page)==99);
    CHECK(debit(0,6)==6);
    CHECK(MH_Initialize()==MH_OK);
    CHECK(MH_CreateHook(page,reinterpret_cast<void*>(&get_hook),reinterpret_cast<void**>(&original_get))==MH_OK);
    CHECK(MH_CreateHook(page+64,reinterpret_cast<void*>(&debit_hook),reinterpret_cast<void**>(&original_debit))==MH_OK);
    CHECK(MH_QueueEnableHook(page)==MH_OK && MH_QueueEnableHook(page+64)==MH_OK);
    CHECK(MH_ApplyQueued()==MH_OK);
    CHECK(get(nullptr)==static_cast<int>(0x81000003u) && get(page)==99 && getter_calls==2);
    CHECK(debit(0,6)==7 && debits==1);
    CHECK(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);
    CHECK(get(page)==99 && getter_calls==2 && debit(0,6)==6 && debits==1);
    CHECK(MH_Uninitialize()==MH_OK);
    CHECK(std::memcmp(page,get_code,sizeof(get_code))==0 && std::memcmp(page+64,debit_code,sizeof(debit_code))==0);
    VirtualFree(page,0,MEM_RELEASE);
    std::puts("PASS: x64 target prologues, queued activation, trampoline calls, native error return and exact restoration");
}
