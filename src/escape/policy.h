#pragma once
#include <cstdint>
namespace escape {
struct Policy {
 bool held=false,pending=false,consumed=false;
 uint64_t started=0;
 void stop(){pending=false;consumed=false;}
 bool poll(bool down,bool foreground,bool safe,uint64_t now){
  bool edge=down&&!held;held=down;
  if(!edge||!foreground||!safe||pending)return false;
  pending=true;consumed=false;started=now;return true;
 }
 void observe(unsigned state,uint64_t now){
  if(pending&&(now-started>120000||(state!=11&&state!=12)))stop();
 }
 bool consume(uint64_t now){
  if(!pending||consumed||now-started>120000)return false;
  consumed=true;return true;
 }
};
}
