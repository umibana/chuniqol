#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv){
 if(argc!=3)return 2;
 HMODULE dll=LoadLibraryA(argv[1]);if(!dll){std::fprintf(stderr,"LoadLibrary error %lu\n",GetLastError());return 1;}
 auto status=reinterpret_cast<LONG(__cdecl*)()>(GetProcAddress(dll,"ChuniTracksStatus"));if(!status)return 1;
 LONG result=0;for(unsigned i=0;i<200&&result==0;++i){Sleep(50);result=status();}
 if(result!=std::atoi(argv[2])){std::fprintf(stderr,"Unexpected DLL status %ld, wanted %s\n",result,argv[2]);return 1;}
 std::printf("PASS: DLL host status=%ld (2=disabled, -1=unsupported executable refused)\n",result);
}
