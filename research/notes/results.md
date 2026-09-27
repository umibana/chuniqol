# ESC-skipped track: native result progression (static research)

Current implementation correction: the end-credit override discussed below was
removed at the user's request. B17DF0 and its caller are now left untouched;
ESC must respect the game's normal track limit. The analysis below records the
native flow and the earlier design, not authorization to restore that override.

Same SDHD 2.50 x86 EXE profile as premium.md. All addresses are preferred-image VAs, base0x400000. This investigation did not write game memory or manipulate UI.

## Native sequence

Outer Play state12 owns `projGame::PlayMusicResult`. Its update at E11750 updates the result view and waits for the view to finish; it also retains native asynchronous handling. Constructor E11A50 stores the resultAll wrapper at game object+0x68. Wrapper+8 points to `PlayMusicResultAllObject::Impl`, vtable191C1E8, with current+0x10 and pending+0x14. Impl+0x74 is the shared input wrapper, Impl+0x7C is the score-result wrapper.

Wrapper completed getter43CE5C->D00460 returns true only when resultAll currentstate is0 or38 (0x26). Score-result completion418921->CEF690 checks its nested state at scoreImpl+0x1E8 is0 or10; resultAll state6 waits for that getter then schedules7. Do not force either completed getter or current state: remaining callbacks handle animation/resource cleanup and result processing.

## Input bridge whitelist

All use native input getter4060E6->B4D070 with x86 thiscall `(inputWrapper,int* output) -> int*`. Premium already detours this function; a second detour would conflict. The premium module now optionally calls the pinned exported `ChuniEscapeInputFilter` stdcall `(uintptr_t originalCaller,void* inputWrapper,int* output)` after successful native return. ESC module must independently constrain this callback to its active ESC session, owning outer Play object in state12, expected result object, whitelisted original caller, and native wrapper availability (vtable+8). Real input should take precedence.

Original return address | Synthetic event | Native reason
--- | --- | ---
CEEF3B | 0x41 (65) | Main score-screen finish/next
CFD9FC | runtime dword[1C20B30], currently0x19 (25) | ResultAll state7 popup advance, schedules8
CFDAFC | same25 | ResultAll state9 popup advance, schedules10
CFDCC7 | same25 | ResultAll state18 popup advance, schedules19
CFDEC7 | same25 | ResultAll state25 popup advance, schedules26
CFE2B7 | same25 | ResultAll state15 popup advance, schedules16
D0040D | same25 | Generic resultAll advance predicate; no static direct callers found, likely unused helper

The score event is different from the popup event. At CEEF36 the getter is followed by native enabled-event check4295A0. Event65 jump table goes to CEF059, calling442FBE->CF59C0. That method requires scoreImpl+BF8==false, delay timer+BFC expired, and nested state(+1E8)==6 before setting the accepted flag. Subsequent native code CEF108-144 schedules nested7 or9, performs optional rewards/network UI, and ends at10. Thus supplying65 respects the existing readiness and cleanup.

The five resultAll popup callers execute readiness checks on the popup object and an input-delay timer at Impl+224 before reaching GetEvent. Their native timeout path uses timer+248 to produce the same progression. Supplying25 at the getter shortens human/timeout waiting without skipping native readiness.

Do NOT whitelist D0F0C7: that belongs to score/tab selection, handles events18..20, and does not advance results. CE6B0B is a different selector. Do not use premium confirmation21 for result advance.

## After result completion

Outer result callback DF4FE0 waits for child completion. It performs existing manager cleanup at DF5007 and other mode-related callbacks, then calls4671CF->B17DF0 atDF5025 (returnDF502A). A false predicate schedules outer pending0; true schedules13 or16.

B17DF0 is an end-credit predicate: compares managerImpl+0x20 (track count) against native total getter46BE3C and has an additional special-mode condition for managerImpl+0x2C==1. An ESC-scoped override of this one caller can keep the outer flow on the native continue route, but it is NOT a no-side-effect generic getter: overriding it suppresses both track-limit and special-mode termination. Root/ESC implementation must tightly limit it to the requested skip, restrict unsupported modes, and clear the flag on selector arrival or timeout.

Returning false does not itself decrement track count, refund credit, or erase the skipped result. Verify final-track behavior in the actual mode before claiming preservation of remaining tracks/credit. Directly scheduling outer6 would omit the standard route via0 and its initialization.

## Verification remaining

Compare automatic ESC route to manual native TrackSkip: actual music shutdown, skipped-result marking, main score advance65, popup advance25, normal result view completion, outer0 and selector6. Check final track and unsupported modes separately; no synthetic input outside the ESC session, no ordinary completed-result changes, and no loop if network/reward UI cannot proceed. Native animations and network operations can still delay the return.
