#include "escape/policy.h"
#include <cassert>
int main(){
 escape::Policy p;
 assert(!p.poll(true,true,false,10)); // unsafe state consumes edge
 assert(!p.poll(true,true,true,11));
 p.poll(false,true,true,12);
 assert(p.poll(true,true,true,13));
 assert(p.pending && !p.consumed);
 assert(!p.poll(true,true,true,14));
 assert(p.consume(15)); assert(!p.consume(16));
 p.observe(12,17); assert(p.pending);
 p.observe(6,18); assert(!p.pending);
 assert(!p.poll(true,true,true,19)); // held key cannot rearm
 p.poll(false,true,true,20);
 assert(!p.poll(true,false,true,21)); // foreground only
 p.poll(false,true,true,22);
 assert(p.poll(true,true,true,23));
 p.observe(11,120024); assert(!p.pending);
 p.poll(false,true,true,120025);
 assert(p.poll(true,true,true,120026));
 p.observe(4,120027); assert(!p.pending);
}
