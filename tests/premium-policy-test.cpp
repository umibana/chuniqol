#include "premium/policy.h"
#include <cassert>
int main() {
 premium::Policy p;
 assert(!p.allow(2, 0));
 p.begin(100);
 assert(p.allow(2, 100));
 p.injected(2);
 assert(!p.allow(2, 101)); // returning to selection must never purchase twice
 assert(!p.allow(3, 102)); // cancelled session stays manual
 p.begin(200);
 assert(p.allow(2, 201)); p.injected(2);
 assert(p.allow(3, 202)); p.injected(3);
 assert(!p.allow(3, 203)); // no second confirmation pulse
 assert(p.allow(5, 204)); p.injected(5);
 assert(!p.allow(5, 205));
 p.begin(300); assert(!p.allow(2, 20301));
 p.begin(400); p.stop(); assert(!p.allow(2, 401));
}
