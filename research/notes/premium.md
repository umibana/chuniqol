# PREMIUM auto-selection

Read-only investigation of `App/bin/chusanApp.exe`; no game launch, process writes, runtime calls, patches, or production-code edits. All addresses below are preferred-image VAs (base 0x00400000); runtime address = module base + VA - 0x00400000. SHA256: `71f8fe2dc8dcf287e5eaa77add22af1137aa5aa7e721049deefd79890d3fae6e`.

## Finding

There is a plausible native-flow implementation for automatic selection AND confirmation. It must still allow the game controller to process selection, purchase/credit results, and close the view. Setting the outer Play state to 6 or writing only the selected-ticket ID would omit this work. Static analysis establishes the control flow, but this is not a tested patch.

## Which PREMIUM?

**Correction after observing the game:** the requested option is **2070**: map x5,
EXP x6, avatar x2, team x8 and +2 special songs. Matches the 2070 XML and the only
PREMIUM item on sale in `App/bin/option/A001/chargeItem`. The screen shows 1 credit;
native pricing is not replaced. The 2020 choice below was an early, discarded guess,
and the +1 song for 2070 mentioned below is wrong: it's +2.

A000 has seven explicitly premium ticket records. 2020 = map x2, character EXP x3, one extra credit; 2040 = map x3, two credits; 2060 = map x4, three credits; 2070 = map x5, four credits. EXP premium alternatives are 3060 / 3090 / 3120 for EXP x6 / x9 / x12 and one / two / three credits. Ticket 2020 is a plausible basic PREMIUM choice, not something proved by the user's word PREMIUM alone. XML 2020 also enables special-mode +1 song, MAS/ULT/WORLD'S END etc. Do not choose the first premium-looking entry or change the credit requirement.

## Objects and fields

- `projGame::PlayTicketSelect` vtable 0x0192B160. Constructor 0x00E0FA70. Its +0x74 is the PlayTicketSelectObject wrapper obtained from view manager using global key at 0x01C21320. Its +0x68 is a vector of ticket records.
- Wrapper constructor 0x00D6E470, vtable 0x0191F524; +0x08 points to Impl.
- `projView::PlayTicketSelectObject::Impl` constructor 0x00D6E330, vtable 0x0191F580, size 0x120.
- Impl +0x10 current substate; +0x14 pending substate (-1 when absent); +0x68 pointer to ticket vector; +0x6C result kind; +0x70 selected ticket-record pointer; +0x74 selector wrapper; +0x9C timeout; +0xD4 confirmation animation/input delay timer; +0x11C alternate credit UI object.
- Vector begin/end at +0/+4; ticket records have stride 0x14. Record+0x10 is the ticket ID; record+0x04 is another identifier used by native purchase operations (do not conflate it with ticket ID).
- Selector wrapper+8 -> selector Impl; selector Impl+0x21C is selected display index. Use native setter instead of writing it.
- Native state transition dispatcher at 0x00D6F180 calls exit/init callbacks, resets elapsed ticks, manages children; direct writes to current +0x10 would bypass it.

## View substate map

State | Init VA | Main VA | Meaning
--- | --- | --- | ---
0 | D6D640 | D6E130 | idle
1 | D6C8F0 | D6D760 | builds selector, waits for open animation
2 | D6D070 | D6D780 | ticket selection
3 | D6D0D0 | D6DA50 | ConfirmSelection (assert names prove label)
4 | D6D1C0 | D6DB90 | Credit (assert name)
5 | D6D3A0 | D6DDD0 | ConfirmCharge (assert name)
6 | D6D500 | D6DF30 | other ticket confirmation
7 | D6D590 | D6E0C0 | selection completed (getter explicitly tests state == 7)
8 | D6D5A0 | D6E0D0 | close/delay, schedules 9
9 | D6D5E0 | D6E100 | close animation, schedules 10
10 | D6D630 | D6E120 | closed

## Native selection APIs

All are x86 thiscall unless stated otherwise.

- `0x004690B0 -> 0x00DA5770`: selector wrapper setter, `(selector, unsigned displayIndex)`, validates wrapper, calls `0x00442212 -> 0x00DA5700` on its Impl. Setter clamps index to selector list length.
- `0x004480CC -> 0x00DA5C90`: selector wrapper refresh, `(selector)`; calls `0x00412C29 -> 0x00DA5AE0`, rebuilding display from chosen index.
- `0x0045F39E -> 0x00DA4750`: get selected selector entry. Returned entry+0x10 is the original ticket-vector index, not ticket ID.
- `0x004426F4 -> 0x00DC1350`: fetch ticket-vector record `(vector, index, 0)`; indexed record is 20 bytes. Empty vector has a static fallback; reject empty/out-of-range before relying on it.
- `0x00459FD4 -> 0x00D6F250`: native choose-current-ticket handler `(Impl)`. Called from selection input case at D6D8CA. It fetches current selector entry, indexes ticket vector, sets Impl+0x70, classifies ticket with 41501E->DAD830, and schedules correct next state. Normal purchasable type1 passes eligibility `43E79D -> D6F640`, then sets result-kind1 and pending3. Type0 sets result-kind0/pending7. Types2/3 use pending6 with other result kinds. This is the native handler to preserve.
- `0x004401DD -> 0x00D6F510`: isCreditEnough `(Impl)`; asserts selected ticket exists and is chargeable, then queries credit subsystem. Do not call speculatively on arbitrary ticket kinds.
- `0x0044C2A3`: selection animation/busy check used before processing inputs in D6D780. A true return blocks selection processing.

The selector build at D6C951 enumerates the full ticket vector in order and stores its index in each display entry. Still verify the selected entry resolves to the target ID after using a setter; never assume a fixed index across users/sessions.

## Confirmation paths and scoped input automation

Input getter `0x004060E6 -> 0x00B4D070` has signature approximately `int* __thiscall GetEvent(InputWrapper*, int* output)`; returns output pointer, fills one int event. Confirmation yes is event 0x15, no is 0x16. Selection accept is event4 (jump table maps original4 to D6D8CA).

Exact return addresses after getter calls:

- D6D811: selection state2 (accept4).
- D6DAD8: ConfirmSelection state3 (yes0x15).
- D6DE56: ConfirmCharge state5 (yes0x15).
- D6DF91: other confirmation state6 (yes0x15).
- D6DCE1: Credit state4; DO NOT synthesize an approval here. This state must keep its actual credit handling.

A narrow implementation could wrap native state2 main D6D780 to select and refresh the validated target before it computes input eligibility, then scope synthetic getter events to the addresses above. For state2 also preserve native input-enabled check `4295A0`; the original main computes enablement for the currently selected ticket before calling GetEvent. Merely changing the index inside the getter can use stale enablement computed for the previous entry. Alternative: call native selection handler D6F250 once after validated setter/refresh, with animation/busy and target eligibility checks.

ConfirmSelection main waits for its 400ms timer, then yes calls actual isCreditEnough and schedules state5 if enough, state4 if not. State4 waits for credit or alternate payment UI and eventually enters5, or returns2 on cancellation. ConfirmCharge main waits 250ms then yes performs its native sound/input reset and schedules7. Type-specific confirmation6 retains event-enable checks. No need to write pending states or call arbitrary mid-function labels.

Only synthesize when exact EXE profile matches, active outer ticket flow and current object identities match, current/pending state is expected, selected record still points inside verified vector and has the configured ID, ticket automation is armed once for this ticket session, and there is no failed/cancelled purchase retry. Disable and leave manual UI on mismatch/missing target/ineligible target/timeout. Scope by object lifetime as well as return address; never globally force the input getter.

This preserves confirmation delays; it skips human decisions, not necessarily every rendered frame. Eliminating screen rendering/animations entirely needs additional work and could break dependencies.

## Why outer state bypass is wrong

Game controller main at E0F2E0 checks view completion via 43E7E8. The getter at D6F6A0 explicitly recognizes Impl state7. Result getter 45EBE7 returns Impl+0x6C, so `[result]` is result-kind and `[result+4]` selected record pointer. Controller reads record+0x10 as actual ticket ID. Result-kind1 leads to game substate2 (purchase); other cases have separate handling. Native `45C680 -> B1C430` records selected ticket ID at `[[manager+4]+0x25CC]`, but this is only one piece of the transaction.

The game purchase/update routine E0F460 checks asynchronous completion and success, validates the result, calls credit debit path `420C34`, records flags/telemetry, calls `46C18E` with the record+4 identifier, then proceeds to closing. Failure paths set game substate3. Do not rerun automatic confirmation indefinitely on failure; do not bypass these routines or fabricate credit success.

## Required verification before calling this working

1. Read-only runtime snapshot at actual ticket UI: validate wrapper/Impl vtables, vector bounds/IDs, current/pending states, selector mapping, ticket2020 identity and displayed PREMIUM option.
2. Controlled manual selection trace: verify state2->3->5->7 and game consumer/purchase->close->outer PlaySelect with enough credits; capture exact selected ID and debit count. If actual game variant skips3 or requires different ticket, revise assumptions.
3. Compare automatic run with manual run: same ticket effects, credit debit once, no extra songs/unintended state changes, no repeated purchase, safe fallback if missing/insufficient credits.
4. Verify insufficient credit remains actionable and failed purchase/cancel does not loop. Verify manual ticket screen unaffected when automation disabled.
5. Rebase VAs to module runtime base; verify exact instruction bytes for every hook and robust prologue relocation/calling convention. Avoid process polling threads invoking thiscall routines; run within original game update thread.

Only the static facts above are verified in this subtask. No dynamic test occurred.
