#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "patch.h"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1; } } while (0)
int main() {
 auto* page=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
 CHECK(page);memcpy(page,tracks::kOriginal,sizeof(tracks::kOriginal));
 DWORD old;CHECK(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old));
 using Fn=unsigned(*)();auto fn=reinterpret_cast<Fn>(page);
 CHECK(fn()==3);
 CHECK(tracks::set_four(page)==tracks::Result::applied);
 CHECK(fn()==4);
 MEMORY_BASIC_INFORMATION info{};
 CHECK(VirtualQuery(page,&info,sizeof(info))==sizeof(info) && info.Protect==PAGE_EXECUTE_READ);
 CHECK(page[0]==0xb8 && page[2]==0 && page[3]==0 && page[4]==0 && page[5]==0xc3);
 CHECK(tracks::set_four(page)==tracks::Result::mismatch && fn()==4);
 CHECK(VirtualProtect(page,4096,PAGE_READWRITE,&old));
 const uint8_t different[]={0xb8,0x03,0,0,0,0x90};memcpy(page,different,sizeof(different));
 CHECK(tracks::set_four(page)==tracks::Result::mismatch);
 CHECK(memcmp(page,different,sizeof(different))==0);
 CHECK(tracks::set_four(nullptr)==tracks::Result::mismatch);
 VirtualFree(page,0,MEM_RELEASE);
 std::puts("PASS: executable default3->4, RX protection restored, unrelated/mismatched bytes untouched");
}
