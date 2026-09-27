#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "MinHook.h"
#include "format.h"
namespace {
using Title=void*(__thiscall*)(void*,void*);
using Level=int(__cdecl*)(int,uint8_t*);
using Special=bool(__thiscall*)(void*);
using Assign=void*(__thiscall*)(void*,const char*,size_t);
Title original;
uintptr_t base;
volatile LONG active=0,applied_logs=0;
wchar_t config[MAX_PATH],logpath[MAX_PATH];
SRWLOCK loglock=SRWLOCK_INIT;
void log(const char* message){AcquireSRWLockExclusive(&loglock);FILE* f=_wfopen(logpath,L"a");if(f){fprintf(f,"%llu %s\n",GetTickCount64(),message);fclose(f);}ReleaseSRWLockExclusive(&loglock);}
uintptr_t at(uintptr_t va){return base+va-0x400000;}
bool read(uintptr_t address,void* out,size_t size){SIZE_T got=0;return address&&ReadProcessMemory(GetCurrentProcess(),(void*)address,out,size,&got)&&got==size;}
struct NativeString{union{char small[16];uint32_t pointer;};uint32_t length,capacity;};
static_assert(sizeof(NativeString)==24);
uint32_t word(const uint8_t* p){uint32_t result;memcpy(&result,p,4);return result;}
struct MusicEntry{int32_t music;uint8_t difficulty;};
using Compare=bool(__cdecl*)(MusicEntry*,MusicEntry*);
Compare original_compare;
volatile int32_t* sort_kind; // 2 = level ascending, 10 = level descending.
bool __cdecl compare_hook(MusicEntry* a,MusicEntry* b){
 uint8_t da=a->difficulty,db=b->difficulty;
 const int la=((Level)at(0x1218b60))(a->music,&da),lb=((Level)at(0x1218b60))(b->music,&db);
 if(la==lb)return original_compare(a,b); // Same internal level: keep native order.
 return *sort_kind==2?la<lb:la>lb;
}
void* __fastcall title_hook(void* self,void*,void* out){
 const auto caller=(uintptr_t)__builtin_return_address(0);
 void* result=original(self,out);
 if(!InterlockedCompareExchange(&active,0,0)||caller!=at(0xcb68de)||result!=out||!out)return result;
 uint8_t model[0x15a];if(!read((uintptr_t)self,model,sizeof(model)))return result;
 const int music=(int)word(model);uint8_t difficulty=model[4];
 // This callsite paints nine selection cards. Only index4 owns this group.
 if(model[0x158]!=1||word(model+0x120)!=0x10f0073||word(model+0x128)!=0x2010023||!levels::eligible(music,difficulty,false))return result;
 if(!levels::eligible(music,difficulty,((Special)at(0xb720a0))(self)))return result; // Native WORLD'S END predicate.
 const int value=((Level)at(0x1218b60))(music,&difficulty);
 NativeString text{};if(!read((uintptr_t)out,&text,sizeof(text))||!text.length||text.length>1024||text.length>text.capacity||text.capacity>1048576)return result;
 char content[1025]{};
 const uintptr_t data=text.capacity<16?(uintptr_t)out:(uintptr_t)text.pointer;
 if(!read(data,content,text.length))return result;
 const auto formatted=levels::title(content,text.length,value);
 if(!formatted.valid)return result;
 ((Assign)at(0x557d70))(out,formatted.text,formatted.size);
 thread_local int last_music=-1,last_value=-1,last_difficulty=-1;
 if(last_music!=music||last_value!=value||last_difficulty!=difficulty){
  last_music=music;last_value=value;last_difficulty=difficulty;
  if(InterlockedIncrement(&applied_logs)<=128){char message[160];snprintf(message,sizeof(message),"APPLIED song=%d difficulty=%u centilevel=%d",music,(unsigned)difficulty,value);log(message);}
 }
 return result;
}
bool supported(){
 const BYTE expected[32]={0x71,0xf8,0xfe,0x2d,0xc8,0xdc,0xf2,0x87,0xe5,0xea,0xa7,0x7a,0xdd,0x22,0xaf,0x11,0x37,0xaa,0x5a,0xa7,0xe7,0x21,0x04,0x9d,0xee,0xfd,0x79,0x89,0x0d,0x3f,0xae,0x6e};
 wchar_t exe[32768];if(!GetModuleFileNameW(nullptr,exe,32768))return false;
 HANDLE f=CreateFileW(exe,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;BYTE digest[32],buf[65536];DWORD got;
 bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)>=0;
 while(ok){if(!ReadFile(f,buf,sizeof(buf),&got,nullptr)){ok=false;break;}if(!got)break;ok=BCryptHashData(h,buf,got,0)>=0;}
 if(ok)ok=BCryptFinishHash(h,digest,32,0)>=0&&!memcmp(digest,expected,32);
 if(h)BCryptDestroyHash(h);if(alg)BCryptCloseAlgorithmProvider(alg,0);CloseHandle(f);return ok;
}

DWORD WINAPI worker(void* module){
 auto length=GetModuleFileNameW((HMODULE)module,config,MAX_PATH);if(!length||length>=MAX_PATH)return 0;
 auto slash=wcsrchr(config,L'\\');if(!slash)return 0;slash[1]=0;
 if(wcslen(config)+20>=MAX_PATH)return 0;
 wcscpy(logpath,config);wcscat(logpath,L"chuni-levels.log");wcscat(config,L"chuni-levels.ini");
 if(!supported()){log("REFUSED EXE hash");return 0;}
 if(!GetPrivateProfileIntW(L"levels",L"enabled",0,config)){log("DISABLED");return 0;}
 base=(uintptr_t)GetModuleHandleW(nullptr);
 struct Expected{uintptr_t va;BYTE bytes[12];};
 const Expected checks[]={
 {0xb704e0,{0x55,0x8b,0xec,0x6a,0xff,0x68,0xf5,0x5c,0x60,0x01,0x64,0xa1}},
 {0x1218b60,{0x51,0xe8,0x45,0x63,0x21,0xff,0x3c,0x01,0x0f,0x85,0xc0,0x00}},
 {0x557d70,{0x53,0x8b,0x5c,0x24,0x08,0x56,0x8b,0xf1,0x85,0xdb,0x74,0x57}},
 {0xb720a0,{0xff,0x31,0xe8,0x69,0xce,0x8f,0xff,0x83,0xc4,0x04,0xc3,0xcc}},
 {0xcb68d9,{0xe8,0x74,0xd2,0x74,0xff,0x80,0xbe,0x58,0x01,0x00,0x00,0x00}}};
 for(const auto& check:checks){BYTE actual[12];if(!read(at(check.va),actual,sizeof(actual))||memcmp(actual,check.bytes,sizeof(actual))){log("REFUSED native bytes");return 0;}}
 HMODULE pinned;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&base,&pinned)){log("ERROR module pin");return 0;}
 if(MH_Initialize()!=MH_OK){log("ERROR MinHook initialize");return 0;}
 if(MH_CreateHook((void*)at(0xb704e0),(void*)title_hook,(void**)&original)!=MH_OK)log("ERROR hook creation");
 else if(MH_EnableHook((void*)at(0xb704e0))!=MH_OK)log("ERROR hook enable; pinned passthrough only");
 else{InterlockedExchange(&active,1);log("READY central selection title decimals");}
 // ponytail: runtime signature scan, EXE is hash-pinned; pin the logged VAs once confirmed in-game.
 const auto nt=(IMAGE_NT_HEADERS*)(base+((IMAGE_DOS_HEADER*)base)->e_lfanew);
 const auto sites=levels::sort_sites((const unsigned char*)base,nt->OptionalHeader.SizeOfImage);
 if(sites.compare==SIZE_MAX){log("REFUSED sort signatures");return 0;}
 const uintptr_t compare=base+sites.compare;sort_kind=(volatile int32_t*)(uintptr_t)word((const uint8_t*)(base+sites.kind));
 char message[96];snprintf(message,sizeof(message),"SORT comparator=%#x kind=%#x",(unsigned)(compare-base+0x400000),(unsigned)((uintptr_t)sort_kind-base+0x400000));log(message);
 if(MH_CreateHook((void*)compare,(void*)compare_hook,(void**)&original_compare)!=MH_OK)log("ERROR sort hook creation");
 else if(MH_EnableHook((void*)compare)!=MH_OK)log("ERROR sort hook enable");
 else log("READY internal level sort");
 return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);HANDLE t=CreateThread(nullptr,0,worker,module,0,nullptr);if(t)CloseHandle(t);}
 return TRUE;
}
