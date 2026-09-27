#include "credit_core.h"
#include <cstring>
namespace credits {
SeedResult State::maintain() const {
    if (!image || image_size < kRequiredSize) return SeedResult::unavailable;
    uint32_t initialized;
    std::memcpy(&initialized,image+kInitialized,sizeof(initialized));
    const unsigned nodes=image[kNodes];
    if (initialized!=1 || nodes<1 || nodes>8) return SeedResult::unavailable;
    bool changed=image[kMaximum]!=kBalance;
    image[kMaximum]=kBalance;
    for (unsigned i=0;i<nodes;++i) {
        auto* stock=image+kStock+i*2;
        changed=changed || stock[0]!=kBalance || stock[1]!=0;
        stock[0]=kBalance;
        stock[1]=0;
    }
    if (changed) {
        const uint32_t dirty=1;
        std::memcpy(image+kDirty,&dirty,sizeof(dirty));
    }
    return changed ? SeedResult::changed : SeedResult::unchanged;
}
int Hooks::get(void* output) const {
    if (output) state.maintain();
    return original_get(output);
}
int Hooks::consume(int node, uint8_t count) const {
    state.maintain();
    const int result=original_debit(node,count);
    state.maintain();
    return result;
}
}
