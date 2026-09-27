#pragma once
#include <cstddef>
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
