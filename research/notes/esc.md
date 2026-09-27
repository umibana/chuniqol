# ESC native-abort static research (bounded, 2026-09-27)

Binary: App/bin/chusanApp.exe, preferred image base 0x400000. Research used static disassembly only. No executable, runtime, or game data was changed.

## Conclusion

No verified immediate song-abort-to-selection path was established. In particular, setting outer Play pending state to 6 is not proven safe. The PlayMusic destructor does not imply all normal post-song cleanup occurs: its current Play exit callback is empty, while meaningful resource/global cleanup lives in the normal PlayAfter state initializer.

## Verified addresses and behavior

- PlayMusic vtable 0x1924BF8 slots resolve to: +00 DDB0F0; +04 A4E770; +08 DD58D0 (deleting destructor); +0C DD6740; +10 DD68B0; +14 DD6950; +18 DD67E0; +1C DD69C0.
- Destructor thunk 4066B3 resolves to DD53C0. It clears [global 1CB96CC]+35C if nonnull, invokes DD67E0, and destroys owned member containers/timers and the base object.
- DD67E0 clears/deletes child +40, then invokes state exit callbacks through handler-vtable+0C for current state +10. Thus destructor invokes the active state exit, not all later state initializers.
- Handler setup starts DD34C0 and follows thunked tail jumps. State callback triplets are init/update/exit.

| State | Init | Update | Exit |
| --- | --- | --- | --- |
| 15 Play | DD2FF0 | DD42D0 | DD0E30 |
| 16 PartyWait | DD2930 | DD3BD0 | DD0DA0 |
| 17 PartyEnd | DD2970 | DD3CC0 | DD0DB0 |
| 18 PlayFinish | DD29D0 | DD3E10 | DD0DC0 |
| 19 PlayAfter | DD2A70 | DD3ED0 | DD0DD0 |
| 20 Calc | DD2BB0 | DD4020 | DD0DE0 |

- DD0E30 (Play exit) is a bare RET. PlayFinish exit DD0DC0 is also RET. The PartyEnd/PlayAfter/Calc exits merely set [this+64]=1 to propagate handler dispatch.
- Play update DD42D0 calls its regular update and party score/status communication helpers. Its completion branch uses 467E63->BBAF10 in the non-alternate path, writes a timestamp-like return value to music-context+870, then queues internal 16 or 17. It does not expose a direct select return in this function.
- BBAF10 checks context+18 via 406DF7, then flags +874/+875/+876, choosing context+770 via 406762 or context+350 via 451DE3. The precise semantics of those completion tests were not established.
- Normal PlayFinish init DD29D0 conditionally calls 453FB7->A9D110 for global resource IDs at 1C21EA0 and 1C21EA4. A9D110 resolves the resource and invokes virtual+28. Resource identity and full lifecycle remain unproven.
- Normal PlayAfter init DD2A70 calls resource virtual+1C on [this+70] and [this+74], clears [1CB96CC]+35C, and calls 41583E, 449F85, 46249A on three global managers. It also handles party/network end-state behavior before resetting local timers/UI. These calls are not reproduced by bare Play exit.
- Calc init DD2BB0 calls a context/result-related routine 45FD80 after obtaining three global/context pointers and later calls 40ABA0 with numerous timing/status values. This is not evidence that result saving occurs exactly there, but establishes that forcing normal completion through Calc cannot be claimed to discard partial results without further tracing.
- 44AACF->DDA390 is a periodic party/status packet builder (reads score, combo and status-like context calls, then dispatches through party object or virtual+4C), not an identified abort helper.

## What is still required before a patch is justified

Trace the outer Play state's own music exit callback; determine lifecycle obligations of music context/audio/chart/UI globals; identify a real cancellation contract (if one exists); locate partial-result persistence and track/credit bookkeeping boundaries; then validate a disposable local session. This bounded static pass does not justify injecting an ESC state write or calling PlayAfter by hand.

## Follow-up: outer Play state 11 exit was traced

The outer exit gap above is now closed, and does not supply missing cleanup.

- Outer constructor DF40A0 invokes base constructor 427FB6 -> DF3EA0. Base creates the state handler via 45F1F5 -> DF3FD0, handler vtable 192A0B0, context pointer +D0. Registration continues through 4309AE and tail jumps.
- Outer state 11 PlayMusic callbacks: init 422070 -> DF36F0; update 46BC34 -> DF3DA0; exit 41DF48 -> DF31F0.
- DF31F0 is exactly `mov byte ptr [ecx+64],1; ret`. It propagates handler dispatch and performs no music stop/release.
- Outer DF4510 clears and deletes its child at +40 before calling handlers' current-state exit callbacks. Thus the relevant child path is precisely the DD53C0 destructor -> DD67E0 -> Play exit DD0E30 described above.
- DF3DA0 waits for missing/finished child, then updates global mode-like state through 43BE35 and sets outer pending +14=12 (PlayMusicResult). It has no selection-abort branch.
- Selection state6 initializer DF3290 calls 420685 with zero on global1CB969C, clears global1CB96CC+35C, and clears bytes +368/+37C. It does not visibly replay the omitted PlayFinish/PlayAfter sequence, and the semantics of 420685 have not been traced.

Concrete remaining uncertainty: whether omitted PlayFinish resource virtual+28 calls and PlayAfter global/resource reset calls are obligatory during an active-song cancellation, whether a separate music-context cancellation API supplies them, and where partial result/track bookkeeping must be rolled back. The outer exit itself is not such an API. No safe cancellation implementation is established.

## Native TrackSkip path and implementation (2026-09-27 continuation)

The user accepts the game's normal skipped/failed-track semantics; suppressing partial results is not a requirement.

- Context flags +874/+875/+876 are mode flags set during GameSetup DD1C1D/DD1C5D/DD1C89, not skip requests.
- TrackSkip threshold is in 40D157 -> BB51E0, a __thiscall function with two pointer arguments (input, output), callee ret8. Its output is double gauge at +0, finish flag at +8, TrackSkip flag at +9 (at least 10 bytes).
- It fetches TrackSkip option ID0x20 at BB560D. The threshold test compares input+F8 to threshold+1 at BB5706. Native skip sets output double to0, word+8 to0101 (BB571D..BB5726), and marks evaluator bytes+11E/+11F (BB5748..BB5752).
- Caller BB96D0 passes gauge evaluator this+2AC and consumes output into gauge this+C8 and finish/TrackSkip this+2A8/+2A9. Gauge object is musicContext+350. Therefore evaluator identity is musicContext+5FC, finish flag musicContext+5F8, and TrackSkip flag musicContext+5F9.
- Native music completion predicate BBAF10 observes gauge finish via BB9190. Native Play update DD42D0 then follows normal PartyEnd/PlayFinish/PlayAfter/Calc and result paths.
- Current musicContext getter 46D2D2 -> B106C0 returns [manager+4]+1830. Normal mode verified against local endtrack predicate B17DF0, which reads trackstate at manager+4 and mode at trackstate+2C. Supported mode is0 only.

Implementation added separately at native-presence/src/escape/dllmain.cpp, with policy.h and tests/escape-policy-test.cpp, build-escape.ps1. It latches a foreground ESC edge only on outerPlay11 / childPlayMusic15 and normal solo-mode identity checks; wraps BB51E0 and mirrors the exact native TrackSkip output/flags after the original calculation. This preserves the caller's normal consumption and state-machine cleanup. No arbitrary state6 write is used.

After normal result completion, B17DF0 is overridden to false only for return address DF502A, manager identity, active consumed ESC session, normal mode0 and outerPlay12. This chooses the native pending0 route into the next selection cycle, including last-track cases, without globally rewriting track count. The session resets as soon as outer state leaves11/12 or after120seconds.

Result autoadvance uses the optional premium DLL bridge to exported ChuniEscapeInputFilter@12; the ESC DLL does not hook shared input getter B4D070. Exact result callsites CFD9FC/CFDAFC/CFDCC7/CFDEC7/CFE2B7 receive event25 only after original getter returns -1 and wrapper availability succeeds. Score-specific CEEF3B receives event65; its native readiness/timer and event-enabled logic remain active. Generic D0040D is excluded pending caller analysis.

Verification: policy tests first failed due missing implementation, then passed for unsafe-edge consumption, key debounce, foreground-only activation, single consumption, result continuation, menu reset and timeout. DLL builds cleanly with Zig0.13.0 for machine14C (x86). Export ChuniEscapeInputFilter@12 verified. Exact binary hash and three eight-byte hook prologues guard loading. Runtime behavior has not yet been tested by this agent; parent coordinates installation and user test.
