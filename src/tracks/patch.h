#pragma once
#include <cstdint>
namespace tracks {
constexpr uintptr_t kBaseTrackRva=0x3f2060;
constexpr uint8_t kOriginal[]={0xb8,0x03,0x00,0x00,0x00,0xc3};
enum class Result { applied, mismatch, protect_failed, flush_failed, restore_failed };
Result set_four(uint8_t* function);
}
