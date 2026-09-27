#pragma once
#include <cstddef>
#include <cstdint>

namespace credits {
constexpr std::size_t kInitialized = 0x96a050;
constexpr std::size_t kStock = 0x96a078;
constexpr std::size_t kDirty = 0x96a148;
constexpr std::size_t kMaximum = 0x96a40c;
constexpr std::size_t kNodes = 0x96a40e;
constexpr std::size_t kRequiredSize = kNodes + 1;
constexpr std::size_t kGetter = 0x2deb30;
constexpr std::size_t kDebit = 0x2de4c0;
constexpr uint8_t kBalance = 99;
enum class SeedResult { unavailable, unchanged, changed };

// Access only while the original amdaemon credit critical section is held.
struct State {
    uint8_t* image = nullptr;
    std::size_t image_size = 0;
    SeedResult maintain() const;
};
using Getter = int (*)(void*);
using Debit = int (*)(int, uint8_t);
struct Hooks {
    State state;
    Getter original_get = nullptr;
    Debit original_debit = nullptr;
    int get(void* output) const;
    int consume(int node, uint8_t count) const;
};
}
