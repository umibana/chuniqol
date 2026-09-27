#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include "patch.h"
namespace tracks {
Result set_four(uint8_t* function) {
 uint8_t bytes[sizeof(kOriginal)];SIZE_T got=0;
 if (!function || !ReadProcessMemory(GetCurrentProcess(),function,bytes,sizeof(bytes),&got) ||
     got!=sizeof(bytes) || std::memcmp(bytes,kOriginal,sizeof(bytes))) return Result::mismatch;
 DWORD old=0,ignored=0;
 if (!VirtualProtect(function,sizeof(bytes),PAGE_EXECUTE_READWRITE,&old)) return Result::protect_failed;
 // Only the low immediate byte changes. One aligned byte store cannot expose
 // a partially written multi-byte instruction to another x86 thread.
 *static_cast<volatile uint8_t*>(function+1)=4;
 if (!FlushInstructionCache(GetCurrentProcess(),function,sizeof(bytes))) {
  *static_cast<volatile uint8_t*>(function+1)=3;
  FlushInstructionCache(GetCurrentProcess(),function,sizeof(bytes));
  VirtualProtect(function,sizeof(bytes),old,&ignored);
  return Result::flush_failed;
 }
 if (!VirtualProtect(function,sizeof(bytes),old,&ignored)) {
  *static_cast<volatile uint8_t*>(function+1)=3;
  FlushInstructionCache(GetCurrentProcess(),function,sizeof(bytes));
  VirtualProtect(function,sizeof(bytes),old,&ignored);
  return Result::restore_failed;
 }
 return Result::applied;
}
}
