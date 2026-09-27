#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "MinHook.h"
#include "policy.h"
namespace {
using Frame=void(__thiscall*)(void*);
using Calculate=void(__thiscall*)(void*,void*,void*);
Frame frame_original;Calculate calculate_original;
uintptr_t base=0,outer=0,music=0,context=0;
volatile LONG active=0;
SRWLOCK lock=SRWLOCK_INIT;
escape::Policy policy;
unsigned logged_outer=~0u,logged_music=~0u,pulse_mask=0;
wchar_t config[MAX_PATH],logpath[MAX_PATH];
struct Guard { Guard(){AcquireSRWLockExclusive(&lock);} ~Guard(){ReleaseSRWLockExclusive(&lock);} };
uintptr_t at(uintptr_t va){return base+va-0x400000;}
bool read(uintptr_t p,void* out,size_t n){SIZE_T got=0;return p&&ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got)&&got==n;}
uint32_t get(uintptr_t p){uint32_t v=0;read(p,&v,4);return v;}
uint8_t byte(uintptr_t p){uint8_t v=0xff;read(p,&v,1);return v;}
void log(const char* s){FILE* f=_wfopen(logpath,L"a");if(f){fprintf(f,"%llu %s\n",GetTickCount64(),s);fclose(f);}}
uintptr_t manager(){return get(at(0x1cb969c));}
bool normal(){auto m=manager();auto t=get(m+4);uint32_t mode=~0u;return m&&t&&read(t+0x2c,&mode,4)&&mode==0;}
uintptr_t current_context(){auto m=manager();auto t=get(m+4);return m&&t?t+0x1830:0;}
bool outer_valid(unsigned state){return outer&&get(outer)==at(0x1929e48)&&get(outer+0x10)==state;}
bool safe_play(){
 return normal()&&outer_valid(11)&&get(outer+0x14)==0xffffffff&&
  music&&get(outer+0x40)==music&&get(music)==at(0x1924bf8)&&
  get(music+0x10)==15&&get(music+0x14)==0xffffffff&&byte(music+0x65)==0&&
  byte(music+0x5f8)==0&&context&&context==current_context()&&
  byte(context+0x874)==0&&byte(context+0x875)==0&&byte(context+0x876)==0;
}
bool foreground(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
void __fastcall frame_hook(void* self,void*){
 if(InterlockedCompareExchange(&active,0,0)){
  Guard guard;
  auto p=(uintptr_t)self;
  if(get(p)==at(0x1929e48)){
   if(outer!=p){policy.stop();outer=p;}
   const bool was=policy.pending;
   const unsigned outer_state=get(p+0x10);
   policy.observe(get(p+0x10),GetTickCount64());
   if(was&&!policy.pending)log("RESET menu, state change or deadline");
   if(!policy.pending){music=get(p+0x40);context=current_context();}
   const bool down=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0,focused=foreground();
   const bool safe=safe_play();
   if(down&&!policy.held&&focused){
    char message[320];auto m=manager();auto t=get(m+4);
    snprintf(message,sizeof(message),"KEY ESC safe=%u outer=%08lx state=%u music=%08lx state=%u mode=%u context=%08lx modes=%u/%u/%u party=%u pending=%u",
     safe,(unsigned long)outer,outer_state,(unsigned long)music,get(music+0x10),get(t+0x2c),
     (unsigned long)context,byte(context+0x874),byte(context+0x875),byte(context+0x876),byte(music+0x5f8),policy.pending);
    log(message);
   }
   if(policy.poll(down,focused,safe,GetTickCount64())){
    pulse_mask=0;logged_outer=logged_music=~0u;log("ARM ESC native track skip");
   }
   if(policy.pending){
    auto child=get(p+0x40);unsigned child_state=get(child+0x10);
    if(outer_state!=logged_outer||child_state!=logged_music){
     char message[160];snprintf(message,sizeof(message),"STATE outer=%u child=%u childptr=%08lx consumed=%u",outer_state,child_state,(unsigned long)child,policy.consumed);
     log(message);logged_outer=outer_state;logged_music=child_state;
    }
   }
  }
 }
 frame_original(self);
}
void __fastcall calculate_hook(void* self,void*,void* input,void* output){
 calculate_original(self,input,output);
 if(!InterlockedCompareExchange(&active,0,0)||!output)return;
 Guard guard;
 if((uintptr_t)self!=context+0x5fc||!safe_play()||!policy.consume(GetTickCount64()))return;
 // Exact native TrackSkip result from BB571D..BB5752. The original caller
 // consumes this result, updates gameplay flags and follows normal cleanup.
 auto out=(uint8_t*)output;double zero=0;memcpy(out,&zero,8);out[8]=out[9]=1;
 auto state=(uint8_t*)self;state[0x11e]=state[0x11f]=1;
 log("TRIGGER native TrackSkip result; normal finish pipeline retained");
}
// Do not hook the end-credit predicate. A skipped track consumes a track and
// the game alone decides whether to continue or finish the current credit.
bool supported();
DWORD WINAPI worker(void* module){
 auto n=GetModuleFileNameW((HMODULE)module,config,MAX_PATH);if(!n||n>=MAX_PATH)return 0;
 auto slash=wcsrchr(config,L'\\');if(!slash)return 0;slash[1]=0;
 if(wcslen(config)+20>=MAX_PATH)return 0;
 wcscpy(logpath,config);wcscat(logpath,L"chuni-escape.log");wcscat(config,L"chuni-escape.ini");
 if(!GetPrivateProfileIntW(L"escape",L"enabled",0,config)){log("DISABLED");return 0;}
 if(!supported()){log("REFUSED EXE hash");return 0;}base=(uintptr_t)GetModuleHandleW(nullptr);
 struct Hook{uintptr_t va;BYTE bytes[8];void* detour;void** original;};
 Hook hooks[]={
  {0xdf50f0,{0x56,0x8b,0xf1,0x80,0x7e,0x65,0x00,0x75},(void*)frame_hook,(void**)&frame_original},
  {0xbb51e0,{0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec},(void*)calculate_hook,(void**)&calculate_original}};
 for(auto& h:hooks){BYTE b[8];if(!read(at(h.va),b,8)||memcmp(b,h.bytes,8)){log("REFUSED hook bytes");return 0;}}
 HMODULE pinned;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&base,&pinned))return 0;
 if(MH_Initialize()!=MH_OK){log("ERROR hook initialize");return 0;}
 for(auto& h:hooks)if(MH_CreateHook((void*)at(h.va),h.detour,h.original)!=MH_OK){log("ERROR create hook");MH_Uninitialize();return 0;}
 if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){log("ERROR enable hook; pinned passthrough");return 0;}
 InterlockedExchange(&active,1);log("READY ESC native skip; ordinary solo play only; native track limit preserved");return 0;
}
}
extern "C" __declspec(dllexport) void __stdcall ChuniEscapeInputFilter(uintptr_t caller,void* wrapper,int* out){
 if(!InterlockedCompareExchange(&active,0,0)||!out||*out!=-1)return;
 Guard guard;
 if(!policy.pending||!policy.consumed||!outer_valid(12)||!normal()||GetTickCount64()-policy.started>120000)return;
 // Result-specific update callers only; they retain native timing and readiness gates.
 const uintptr_t callers[]={0xcfd9fc,0xcfdafc,0xcfdcc7,0xcfdec7,0xcfe2b7};
 unsigned pulse=caller==at(0xceef3b)?32:0;
 for(unsigned i=0;i<5;++i)if(caller==at(callers[i]))pulse=1u<<i;
 if(!pulse)return;
 auto vtable=get((uintptr_t)wrapper);auto available=get(vtable+8);if(!available)return;
 using Available=bool(__thiscall*)(void*);if(!((Available)available)(wrapper))return;
 if(get(at(0x1c20b30))!=25)return;
 *out=caller==at(0xceef3b)?65:25;
 if(!(pulse_mask&pulse)){
  char message[120];snprintf(message,sizeof(message),"RESULT pulse caller=%08lx event=%d",(unsigned long)(caller-base+0x400000),*out);log(message);pulse_mask|=pulse;
 }
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);auto t=CreateThread(nullptr,0,worker,module,0,nullptr);if(t)CloseHandle(t);}
 return TRUE;
}
namespace {
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

}
