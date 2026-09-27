#pragma once
#include <cstdint>
namespace premium {
struct Policy {
 uint64_t started=0; unsigned pulses=0; bool armed=false;
 void begin(uint64_t now) { started=now; pulses=0; armed=true; }
 void stop() { armed=false; }
 bool allow(int state, uint64_t now) {
  if (!armed) return false;
  if (now-started>20000 || (state==2 && (pulses&1))) { stop(); return false; }
  if (state==2) return pulses==0;
  if (state==3) return (pulses&1) && !(pulses&2);
  if (state==5) return (pulses&1) && !(pulses&4);
  return false;
 }
 void injected(int state) { pulses |= state==2?1:state==3?2:4; if(state==5) stop(); }
};
}
