#include "levels/format.h"
constexpr bool same(levels::Formatted actual,const char* expected){
 if(!actual.valid)return false;
 for(size_t i=0;;++i){if(actual.text[i]!=expected[i])return false;if(!expected[i])return true;}
}
// Catches wrong fixed-point scale, integer truncation, and loss of .0.
static_assert(same(levels::title("Song",4,1320),"Song [13.2]"),"1320 must display 13.2");
static_assert(same(levels::title("Song",4,1180),"Song [11.8]"));
static_assert(same(levels::title("Song",4,1400),"Song [14.0]"));
static_assert(same(levels::title("Song",4,1375),"Song [13.75]"));
static_assert(!levels::title("Song",4,0).valid);
static_assert(!levels::title("Song",4,2001).valid);
static_assert(!levels::title("Song",4,1320,8).valid);
static_assert(same(levels::title("\xe9\xb3\xa5\xe3\x81\xae\xe8\xa9\xa9",9,1130),"\xe9\xb3\xa5\xe3\x81\xae\xe8\xa9\xa9 [11.3]"));
int main(){return 0;}
// WORLD'S END can be represented by difficulty5 or a special music record.
static_assert(levels::eligible(3,4,false),"ULTIMA is supported");
static_assert(!levels::eligible(3,5,false),"WORLD'S END difficulty must be excluded");
static_assert(!levels::eligible(3,3,true),"special music records must be excluded");
static_assert(!levels::eligible(-1,3,false),"invalid chart ID must be excluded");
// Sort signature scan: one owner found, decoys ignored, ambiguity refused.
constexpr unsigned char one[]={0x83,0xec,0x08,0x8d,0x44,0x24,0x0c,0x53,0x90,0x83,0x3d,1,2,3,4,0x02,0x75,0x00,0x83,0xec,0x08,0x8d,0x44,0x24,0x0c,0x53};
static_assert(levels::sort_sites(one,sizeof(one)).compare==0&&levels::sort_sites(one,sizeof(one)).kind==11);
constexpr unsigned char two[]={0x83,0xec,0x08,0x8d,0x44,0x24,0x0c,0x53,0x83,0x3d,1,2,3,4,0x02,0x75,0x83,0xec,0x08,0x8d,0x44,0x24,0x0c,0x53,0x83,0x3d,1,2,3,4,0x02,0x75};
static_assert(levels::sort_sites(two,sizeof(two)).compare==SIZE_MAX);
constexpr unsigned char none[]={0x83,0x3d,1,2,3,4,0x02,0x75,0x83,0xec,0x08,0x8d,0x44,0x24,0x0c,0x53};
static_assert(levels::sort_sites(none,sizeof(none)).kind==SIZE_MAX);
