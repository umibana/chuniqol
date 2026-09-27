# Credits research (read-only, 2026-09-27)

Target: `App/bin/amdaemon.exe`, x64 image base 0x140000000, SHA256
014992a5af1b62b5fd42d2cb0c19a38aba4b5437a3ef82d304de2c93065ed70d.
All offsets below are RVAs, so add the actual loaded module base.
No game binary, config, launcher, or repository code was modified. No game process launched.

## Proven representation and numeric limit

- Legacy credit manager global: 0x96a050. Its initialized DWORD is offset +0.
- Actual spendable whole credit: BYTE at 0x96a078 + node*2. Fraction is next BYTE.
- Maximum stock BYTE: 0x96a40c. Number of nodes BYTE: 0x96a40e.
- Shared/individual-credit setting: BYTE 0x96a058; zero maps all node indices to stock 0.
- Freeplay setting: BYTE 0x96a05a, equals 1 for freeplay path.
- Dirty credit record flag: DWORD 0x96a148.
- Credit manager lock: CRITICAL_SECTION at 0xabb940. IAT 0x5b13e0 and
  0x5b13e8 are EnterCriticalSection and LeaveCriticalSection, verified from imports.
- Raw initializer 0x2ddf30 clears manager, takes max stock in R9B, and at
  0x2de155 clamps restored whole credits to the max BYTE.
- Modern manager refresh at 0x22c5f0 calls 0x2c76b0 (legacy getter wrapper).
  At 0x22c6e0 and 0x22c6e6 it zero-extends fractional and whole BYTE values,
  and sends them to the modern CreditValue via 0x223310. Modern representation
  has DWORDs but is derived from legacy BYTE stock.
- Server IPC CreditController::loadData(CreditValue&) at 0x2bf40 obtains the
  modern CreditValue through 0x216320 / 0x22bce0, then copies whole/fractional
  DWORDs via getter functions 0x223350 and 0x223360.
- Config `max_credit` validation reaches 0x14a8d0 and allows only -1 (sentinel)
  or 1..99. Constants: 0x650e78 double -1; 0x62bf70 double 1;
  0x650e60 double 99. Local config currently says max_credit:24.

Conclusion: true 999 stock is NOT compatible with current credit storage.
999 in config fails validation. 999 written to stock byte truncates to231.
999 in only the modern DWORD object is cosmetic and overwritten on refresh.
Normal supported ceiling is99; storage ceiling255 is outside config's valid range.

## Debit/freeze

Function 0x2de4c0 takes node index ECX and requested count DL.
It calls readiness checker0x2ddd60, requires EAX==2, then subtracts count from
actual stock at0x2de4f7: bytes `40 28 7c 4a 28`.
0x2de4f8 is the opcode byte in the known patch-finder SUB->OR patch.
The OR patch does not provide exact freezing:99 OR4 becomes103.
Replacing all five instruction bytes with five NOPs preserves exact stock;
the immediately following CMP resets flags, so removed SUB flags are unused.
The debit function still performs availability check and existing bookkeeping.

## Suggested reversible runtime implementation

Use an x64 injected DLL verified against the exact EXE hash and original bytes.
Install modifications while target is suspended / before execution reaches them.
No disk EXE modifications needed.

1. Configure normal max_credit99, or runtime increase maximum BYTE to99 when seeding.
2. Replace the single CALL at0x2deb0c (expected `e8 1f 00 00 00`) with a rel32
   CALL to a nearby relay. Relay performs absolute JMP to DLL C wrapper.
   The original callee remains unmodified at0x2deb30.
3. That call occurs inside locked getter0x2dead0 AFTER EnterCriticalSection;
   wrapper receives original void* output in RCX. If output nonnull and initialized
   DWORD0x96a050!=0, seed whole stock0 at0x96a078 to99, fractional0x96a079 to0,
   and mark DWORD0x96a148=1 only when changing stock. Then call original raw getter
   int(__fastcall*)(void*) at0x2deb30 and return its result.
   This is actual spendable stock, so no fake display and no coin key events.
   Optionally populate every valid node (count must be1..8), though single-node
   SDHD only needs node0. Do not write past stock array.
4. NOP five bytes at0x2de4f7 to keep99 after consumption.
5. Wrapper will be reached by regular credit refresh, after initialization,
   without a polling worker or timing guesses. First refresh after seeding may
   update presentation on the next frame because refresh consumes dirty flag
   before calling the getter; this is expected and still before play begins.

Nearby relay required if DLL is outside rel32 range. A relay can contain
`FF 25 00 00 00 00` followed by pointer to wrapper (14 bytes, no register clobber).
Do not relocate/overwrite raw getter prologue: it has short conditional branches.
Preserve original bytes for disable/restore. Do not unload DLL while hooks remain.

This is static evidence and a reviewable design, not a runtime-tested patch.
Validation must observe initial displayed actual99, successful start/purchase
without coin insertion, stock remaining99 after debit, and game restart behavior.
