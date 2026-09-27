#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "MinHook.h"
#include "policy.h"
namespace {
using Fn=void(__thiscall*)(void*);
using InputFn=int*(__thiscall*)(void*,int*);
Fn init_original, select_original, confirm_original, charge_original;
InputFn input_original;
using EscapeFilter=void(__stdcall*)(uintptr_t,void*,int*);
thread_local EscapeFilter escape_filter=nullptr;
thread_local ULONGLONG next_escape_lookup=0;
void filter_escape(uintptr_t caller,void* input,int* output){
 if(!escape_filter){
  const auto now=GetTickCount64();if(now<next_escape_lookup)return;next_escape_lookup=now+1000;
  const auto module=GetModuleHandleW(L"chuni-escape.dll");if(!module)return;
  auto address=GetProcAddress(module,"ChuniEscapeInputFilter");
  if(!address)address=GetProcAddress(module,"ChuniEscapeInputFilter@12");
  if(!address)address=GetProcAddress(module,"_ChuniEscapeInputFilter@12");
  HMODULE pinned=nullptr;
  if(address&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                               (LPCWSTR)address,&pinned))escape_filter=(EscapeFilter)address;
 }
 if(escape_filter)escape_filter(caller,input,output);
}
volatile LONG active=0;
uintptr_t base; unsigned ticket=2070; wchar_t config[MAX_PATH], logpath[MAX_PATH];
struct Session { void* object=nullptr; premium::Policy policy; bool prepared=false; };
thread_local Session session;
struct Scope { void* object; int state; uintptr_t caller; };
thread_local Scope scope{};
void log(const char* text) { FILE* f=_wfopen(logpath,L"a"); if(f){fprintf(f,"%llu %s\n",GetTickCount64(),text);fclose(f);} }
uintptr_t at(uintptr_t va){return base+va-0x400000;}
bool read(uintptr_t address,void* out,size_t n) { SIZE_T got=0; return ReadProcessMemory(GetCurrentProcess(),(void*)address,out,n,&got)&&got==n; }
bool word(uintptr_t p,uint32_t& v){return p&&read(p,&v,4);}
uint32_t get(uintptr_t p){uint32_t v=0;word(p,v);return v;}
bool valid(void* obj,int state) {auto p=(uintptr_t)obj; return p&&get(p)==at(0x191f580)&&get(p+0x10)==(unsigned)state&&get(p+0x14)==0xffffffff;}
bool bounds(void* obj,uint32_t& begin,uint32_t& end){
 auto v=get((uintptr_t)obj+0x68);
 return v&&word(v,begin)&&word(v+4,end)&&begin&&end>begin&&end-begin<=4096*20&&(end-begin)%20==0;
}
bool selected(void* obj){uint32_t b,e;auto r=get((uintptr_t)obj+0x70);return bounds(obj,b,e)&&r>=b&&r<e&&(r-b)%20==0&&get(r+0x10)==ticket;}
void stop(const char* reason){if(session.policy.armed)log(reason);session.policy.stop();}
void __fastcall init_hook(void* self,void*) {
 init_original(self); if(!InterlockedCompareExchange(&active,0,0))return; session={};
 if(get((uintptr_t)self)!=at(0x191f580))return;
 session.object=self;session.policy.begin(GetTickCount64());log("ARM ticket session");
}
bool prepare(void* self){
 uint32_t b,e;if(!bounds(self,b,e))return false;
 unsigned found=0,index=0;
 for(uint32_t p=b;p<e;p+=20)if(get(p+16)==ticket){++found;index=(p-b)/20;}
 if(found!=1)return false;
 auto selector=get((uintptr_t)self+0x74);if(!selector||!get(selector+8))return false;
 // Purchasable premium class1; allow native code to evaluate availability/credit.
 using Classify=int(__thiscall*)(void*);
 if(((Classify)at(0xdad830))((void*)(b+index*20))!=1)return false;
 using Set=void(__thiscall*)(void*,unsigned);
 ((Set)at(0xda5770))((void*)selector,index);
 ((Fn)at(0xda5c90))((void*)selector);
 using Entry=void*(__thiscall*)(void*);
 auto entry=(uintptr_t)((Entry)at(0xda4750))((void*)selector);
 if(!entry||get(entry+16)!=index)return false;
 session.prepared=true;log("SELECT target2070 prepared");return true;
}
void run(void* self,int state,uintptr_t caller,Fn original){
 if(!InterlockedCompareExchange(&active,0,0)){original(self);return;}
 const auto prior=scope;scope={};
 if(session.object==self&&valid(self,state)&&session.policy.armed){
  if(!session.policy.allow(state,GetTickCount64())) { if(state==2)log("MANUAL return-to-selection or deadline"); }
  else if(state==2){
   if(!session.prepared&&!prepare(self))stop("MANUAL target missing/ambiguous/unavailable");
   if(session.policy.armed)scope={self,state,at(caller)};
  } else if(selected(self))scope={self,state,at(caller)};
  else stop("MANUAL selected ticket changed");
 }
 original(self);scope=prior;
}
void __fastcall select_hook(void* self,void*){run(self,2,0xd6d811,select_original);}
void __fastcall confirm_hook(void* self,void*){run(self,3,0xd6dad8,confirm_original);}
void __fastcall charge_hook(void* self,void*){run(self,5,0xd6de56,charge_original);}
int* __fastcall input_hook(void* self,void*,int* out){
 auto caller=(uintptr_t)__builtin_return_address(0);
 int* result=input_original(self,out);
 if(!InterlockedCompareExchange(&active,0,0))return result;
 if(result==out&&out)filter_escape(caller,self,out);
 if(!scope.object||caller!=scope.caller||session.object!=scope.object)return result;
 if(result!=out||!out)return result;
 // Original getter returns the same -1 for inactive wrapper and no event.
 // Preserve its native vtable+8 availability check before supplying a pulse.
 using Available=bool(__thiscall*)(void*);
 auto vtable=get((uintptr_t)self); auto available=get(vtable+8);
 if(!available||!((Available)available)(self))return result;
 if(*out!=-1){stop("MANUAL real input takes precedence");return result;}
 if(!valid(scope.object,scope.state)||!session.policy.allow(scope.state,GetTickCount64()))return result;
 if(scope.state!=2&&!selected(scope.object)){stop("MANUAL selected ticket changed");return result;}
 *out=scope.state==2?4:0x15;session.policy.injected(scope.state);
 log(scope.state==2?"AUTO selection pulse":scope.state==3?"AUTO confirmation pulse":"AUTO charge confirmation pulse");
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
bool restore_credits(){
 BYTE expected[]={0x38,0xc0,0x75,0x10,0x6a,0x09,0x68,0xd4,0x95,0x8e,0x01};BYTE actual[11];auto target=(void*)at(0x7e3214);
 if(!read((uintptr_t)target,actual,11)||memcmp(actual,expected,11)){log("REFUSED credit display bytes");return false;}
 DWORD old;if(!VirtualProtect(target,2,PAGE_EXECUTE_READWRITE,&old))return false;
 const BYTE replacement[]={0x3c,0x01};SIZE_T written=0;
 bool ok=WriteProcessMemory(GetCurrentProcess(),target,replacement,2,&written)&&written==2;
 DWORD unused;bool restored=VirtualProtect(target,2,old,&unused)!=0;FlushInstructionCache(GetCurrentProcess(),target,2);
 log(ok&&restored?"RESTORED native credit display comparison":"ERROR restoring credit display");return ok&&restored;
}
DWORD WINAPI worker(void* module){
 auto length=GetModuleFileNameW((HMODULE)module,config,MAX_PATH);if(!length||length>=MAX_PATH)return 0;
 auto slash=wcsrchr(config,L'\\');if(!slash)return 0;slash[1]=0;
 if(wcslen(config)+20>=MAX_PATH)return 0;
 wcscpy(logpath,config);wcscat(logpath,L"chuni-premium.log");wcscat(config,L"chuni-premium.ini");
 if(!supported()){log("REFUSED EXE hash");return 0;}base=(uintptr_t)GetModuleHandleW(nullptr);
 if(GetPrivateProfileIntW(L"display",L"restoreCredits",0,config))restore_credits();
 if(!GetPrivateProfileIntW(L"premium",L"enabled",0,config)){log("DISABLED");return 0;}
 ticket=GetPrivateProfileIntW(L"premium",L"ticketId",2070,config);
 if(ticket!=2070){log("REFUSED unsupported target ticket ID");return 0;}
 struct Hook {uintptr_t va;BYTE bytes[8];void* detour;void** original;};
 Hook hooks[]={
 {0xd6c8f0,{0x55,0x8b,0xec,0x6a,0xff,0x68,0x85,0x1b},(void*)init_hook,(void**)&init_original},
 {0xd6d780,{0x83,0xec,0x10,0x53,0x56,0x57,0xff,0x35},(void*)select_hook,(void**)&select_original},
 {0xd6da50,{0x83,0xec,0x08,0x56,0x8b,0x35,0xa0,0x96},(void*)confirm_hook,(void**)&confirm_original},
 {0xd6ddd0,{0x83,0xec,0x10,0x56,0x8b,0xf1,0x8d,0x8e},(void*)charge_hook,(void**)&charge_original},
 {0xb4d070,{0x57,0x8b,0xf9,0x8b,0x07,0x8b,0x40,0x08},(void*)input_hook,(void**)&input_original}};
 for(const auto& h:hooks){BYTE b[8];if(!read(at(h.va),b,8)||memcmp(b,h.bytes,8)){log("REFUSED hook bytes");return 0;}}
 HMODULE pinned;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&base,&pinned))return 0;
 if(MH_Initialize()!=MH_OK){log("ERROR MinHook initialize");return 0;}
 for(const auto& h:hooks)if(MH_CreateHook((void*)at(h.va),h.detour,h.original)!=MH_OK){log("ERROR hook creation");MH_Uninitialize();return 0;}
 if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){log("ERROR hook enable; pinned passthrough only");return 0;}
 InterlockedExchange(&active,1);
 log("READY native PREMIUM2070 automation; waiting for next ticket session");return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);HANDLE t=CreateThread(nullptr,0,worker,module,0,nullptr);if(t)CloseHandle(t);}
 return TRUE;
}
