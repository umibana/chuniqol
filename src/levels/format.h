#pragma once
#include <cstddef>
#include <cstdint>
namespace levels {
struct Formatted { char text[1100]{}; size_t size=0; bool valid=false; };
constexpr Formatted title(const char* original,size_t length,int value,size_t capacity=1100){
 Formatted out{};
 if(!original||!length||length>1024||value<100||value>2000)return out;
 for(size_t i=0;i<length;++i){if(!original[i])return {};out.text[out.size++]=original[i];}
 out.text[out.size++]=' ';out.text[out.size++]='[';
 const int integer=value/100,fraction=value%100;
 if(integer>=10)out.text[out.size++]=char('0'+integer/10);
 out.text[out.size++]=char('0'+integer%10);out.text[out.size++]='.';
 out.text[out.size++]=char('0'+fraction/10);
 if(fraction%10)out.text[out.size++]=char('0'+fraction%10);
 out.text[out.size++]=']';out.text[out.size]=0;
 out.valid=out.size<capacity;return out;
}
}
namespace levels { constexpr bool eligible(int music,unsigned difficulty,bool special){return music>0&&difficulty<=4&&!special;} }

namespace levels {
// Level sort sites, signatures from beer-psi/chunithm-mods internal-level-sort.
// kind = offset of the sort-kind variable address (imm32 of `cmp dword [kind],2`).
struct SortSites{size_t compare=SIZE_MAX,kind=SIZE_MAX;};
constexpr bool match(const unsigned char* p,const int* pattern,size_t n){for(size_t i=0;i<n;++i)if(pattern[i]>=0&&p[i]!=pattern[i])return false;return true;}
// Refuses (both SIZE_MAX) unless exactly one comparator owns a sort-kind compare within 500 bytes.
constexpr SortSites sort_sites(const unsigned char* data,size_t size){
 constexpr int compare[8]={0x83,0xec,0x08,0x8d,0x44,0x24,0x0c,0x53},kind[8]={0x83,0x3d,-1,-1,-1,-1,0x02,0x75};
 SortSites out{};
 for(size_t i=0;i+8<=size;++i){
  if(!match(data+i,compare,8))continue;
  for(size_t j=i;j+8<=i+500&&j+8<=size;++j)if(match(data+j,kind,8)){if(out.compare!=SIZE_MAX)return {};out={i,j+2};break;}
 }
 return out;
}
}
