#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
// Integration check: an unrelated host must be refused before any hook is made.
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;
 wchar_t path[MAX_PATH];wcscpy(path,argv[1]);auto slash=wcsrchr(path,L'\\');if(!slash)return 2;slash[1]=0;wcscat(path,L"chuni-levels.log");
 DeleteFileW(path);
 if(!LoadLibraryW(argv[1])){fprintf(stderr,"LoadLibrary failed: %lu\n",GetLastError());return 3;}
 for(int i=0;i<50;++i){Sleep(100);FILE* f=_wfopen(path,L"rb");if(!f)continue;char log[1024]{};fread(log,1,sizeof(log)-1,f);fclose(f);
  if(strstr(log,"REFUSED EXE hash")&&!strstr(log,"READY")){puts("unsupported host refused");return 0;}
 }
 fprintf(stderr,"unsupported host was not refused\n");return 1;
}
