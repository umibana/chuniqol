#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
int main(int argc,char** argv) {
    if (argc!=2) return 2;
    HMODULE dll=LoadLibraryA(argv[1]);
    if (!dll) { std::fprintf(stderr,"LoadLibrary error %lu\n",GetLastError());return 1; }
    auto status=reinterpret_cast<LONG(WINAPI*)()>(GetProcAddress(dll,"ChuniCreditsStatus"));
    if (!status) return 1;
    LONG result=0;
    for (unsigned i=0;i<100 && result==0;++i) { Sleep(50);result=status(); }
    if (result!=-1) { std::fprintf(stderr,"Expected rejection of unsupported host, got %ld\n",result);return 1; }
    // Exit rather than unloading a DLL whose initialization thread may be returning.
    std::puts("PASS: DLL loads in x64 host and refuses unsupported executable without activating hooks");
}
