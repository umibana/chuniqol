#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include "patch.h"
namespace {
constexpr char kGameHash[]="71f8fe2dc8dcf287e5eaa77add22af1137aa5aa7e721049deefd79890d3fae6e";
volatile LONG status=0;
wchar_t logpath[MAX_PATH];
void log(const char* message) {
 FILE* f=_wfopen(logpath,L"a");
 if(f){std::fprintf(f,"%llu %s\n",GetTickCount64(),message);std::fclose(f);}
}
bool supported() {
 wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);
 if(!n || n>=32768)return false;
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 unsigned char object[8192],buffer[65536],digest[32];DWORD object_size=0,returned=0;
 bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof(object_size),&returned,0)>=0&&object_size<=sizeof(object);
 if(ok)ok=BCryptCreateHash(algorithm,&hash,object,object_size,nullptr,0,0)>=0;
 while(ok){DWORD got=0;if(!ReadFile(file,buffer,sizeof(buffer),&got,nullptr)){ok=false;break;}if(!got)break;ok=BCryptHashData(hash,buffer,got,0)>=0;}
 if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);
 if(!ok)return false;
 char hex[65];constexpr char digits[]="0123456789abcdef";
 for(unsigned i=0;i<32;++i){hex[i*2]=digits[digest[i]>>4];hex[i*2+1]=digits[digest[i]&15];}hex[64]=0;
 return std::strcmp(hex,kGameHash)==0;
}
bool match(uint8_t* image,uintptr_t rva,const uint8_t* bytes,size_t size) {
 uint8_t actual[32];SIZE_T got=0;
 return size<=sizeof(actual)&&ReadProcessMemory(GetCurrentProcess(),image+rva,actual,size,&got)&&got==size&&!std::memcmp(actual,bytes,size);
}
DWORD WINAPI initialize(void* module) {
 wchar_t config[MAX_PATH];DWORD n=GetModuleFileNameW(static_cast<HMODULE>(module),config,MAX_PATH);
 if(!n || n>=MAX_PATH){InterlockedExchange(&status,-1);return 0;}
 auto* slash=wcsrchr(config,L'\\');if(!slash){InterlockedExchange(&status,-1);return 0;}slash[1]=0;
 if(wcslen(config)+24>=MAX_PATH){InterlockedExchange(&status,-1);return 0;}
 wcscpy(logpath,config);wcscat(logpath,L"chuni-tracks.log");wcscat(config,L"chuni-tracks.ini");
 if(FILE* f=_wfopen(logpath,L"w"))std::fclose(f);
 if(GetPrivateProfileIntW(L"tracks",L"enabled",0,config)!=1){log("DISABLED: tracks.enabled must be 1.");InterlockedExchange(&status,2);return 0;}
 if(!supported()){log("REFUSED: unsupported executable SHA256; no modification.");InterlockedExchange(&status,-1);return 0;}
 auto* image=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
 const uint8_t end_bytes[]={0x53,0x56,0x8b,0x71,0x04,0x32,0xdb,0x57,0x8b,0xce,0xe8,0x3d,0x40,0x95,0xff};
 // Refuse the former endless-credit hook as well as unexpected default bytes.
 if(!match(image,tracks::kBaseTrackRva,tracks::kOriginal,sizeof(tracks::kOriginal)) ||
    !match(image,0x717df0,end_bytes,sizeof(end_bytes))){log("REFUSED: default-track or native end-credit bytes changed.");InterlockedExchange(&status,-1);return 0;}
 const auto result=tracks::set_four(image+tracks::kBaseTrackRva);
 if(result!=tracks::Result::applied){log("ERROR: runtime patch failed; restart the game before retrying.");InterlockedExchange(&status,-1);return 0;}
 log("READY: ordinary base tracks 3 -> 4; native bonuses, Course/NationalMatching and end-credit predicate retained.");
 InterlockedExchange(&status,1);return 0;
}
}
extern "C" __declspec(dllexport) LONG __cdecl ChuniTracksStatus(){return InterlockedCompareExchange(&status,0,0);}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(nullptr,0,initialize,module,0,nullptr);if(thread)CloseHandle(thread);else InterlockedExchange(&status,-1);}
 return TRUE;
}
