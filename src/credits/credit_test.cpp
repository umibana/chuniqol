#include "credit_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static uint8_t* image;
static unsigned calls;
static int requested_node;
static uint8_t requested_count;
static int native_get(void* out) {
    ++calls;
    if (!out) return -3;
    if (!*reinterpret_cast<uint32_t*>(image + credits::kInitialized)) return -4;
    std::memcpy(out, image + credits::kStock, 16);
    return 0;
}
static int native_debit(int node, uint8_t count) {
    ++calls; requested_node=node; requested_count=count;
    if (node < 0 || node >= image[credits::kNodes]) return 0;
    auto& stock=image[credits::kStock + node*2];
    if (stock<count) return 0;
    stock-=count;
    return 1;
}
int main() {
    std::vector<uint8_t> bytes(credits::kRequiredSize+32,0);
    image=bytes.data();
    credits::State state{image,bytes.size()};
    CHECK(state.maintain()==credits::SeedResult::unavailable);
    CHECK(bytes==std::vector<uint8_t>(bytes.size(),0));
    *reinterpret_cast<uint32_t*>(image+credits::kInitialized)=1;
    CHECK(state.maintain()==credits::SeedResult::unavailable);
    image[credits::kNodes]=9;
    CHECK(state.maintain()==credits::SeedResult::unavailable);
    image[credits::kNodes]=2;
    image[credits::kMaximum]=24;
    image[credits::kStock+1]=3;
    image[credits::kStock+3]=4;
    image[credits::kStock+4]=0x5a;
    CHECK(state.maintain()==credits::SeedResult::changed);
    CHECK(image[credits::kStock]==99 && image[credits::kStock+2]==99);
    CHECK(image[credits::kStock+1]==0 && image[credits::kStock+3]==0);
    CHECK(image[credits::kStock+4]==0x5a && image[credits::kMaximum]==99);
    CHECK(*reinterpret_cast<uint32_t*>(image+credits::kDirty)==1);
    *reinterpret_cast<uint32_t*>(image+credits::kDirty)=0;
    CHECK(state.maintain()==credits::SeedResult::unchanged);
    CHECK(*reinterpret_cast<uint32_t*>(image+credits::kDirty)==0);
    credits::Hooks hooks{state,native_get,native_debit};
    CHECK(hooks.get(nullptr)==-3 && calls==1);
    uint8_t out[16]{};
    image[credits::kStock]=0;
    CHECK(hooks.get(out)==0 && out[0]==99 && calls==2);
    CHECK(hooks.consume(0,6)==1 && image[credits::kStock]==99);
    CHECK(requested_node==0 && requested_count==6);
    CHECK(hooks.consume(0,100)==0 && image[credits::kStock]==99);
    CHECK(hooks.consume(8,1)==0 && image[credits::kStock]==99);
    CHECK(requested_node==8 && requested_count==1);
    // Native reinitialization must restore usable stock on the next getter.
    std::memset(image+credits::kStock,0,4);
    image[credits::kMaximum]=24;
    CHECK(hooks.get(out)==0 && out[0]==99 && out[2]==99);
    credits::State short_image{image,credits::kRequiredSize-1};
    CHECK(short_image.maintain()==credits::SeedResult::unavailable);
    std::puts("PASS: initialization guards, real stock, fractions, node bounds, dirty flag, debit and reinitialization");
}
