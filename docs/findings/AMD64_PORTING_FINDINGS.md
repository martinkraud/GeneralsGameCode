# AMD64 porting findings

Only issues that were actually encountered, reproduced and fixed in this repository are listed. Section A contains findings that are likely to matter to **any** GeneralsGameCode
64-bit effort (they live in shared engine code). Section B lists third-party / platform constraints. Section C lists decisions that belong to this fork's runtime architecture and are *not*
advice for other projects.

Not compared with the current state of other forks or of upstream: a finding marked "possible upstream applicability" means "the pre-fix code in this repository had the defect", not "upstream has it today".

## A. Pointer width and layout findings in shared engine code

### A-1. `ObjectPoolClass` block payload offset assumed a 4-byte pointer

| | |
|---|---|
| File / function | `Core/Libraries/Source/WWVegas/WWLib/mempool.h`, `ObjectPoolClass<T,BLOCK_SIZE>::Allocate_Object_Memory()` |
| Commit | `53f08fbdc` (X64.14); document `docs/modernization/X64_14_OBJECT_POOL_LAYOUT.md` |
| Original assumption | A block is `[next-block pointer][BLOCK_SIZE objects]`; the first object is at `BlockListHead + 1` where `BlockListHead` is a `uint32 *`, i.e. 4 bytes past the block start. |
| Why it works on Win32 | the stored link is `void *`, 4 bytes on Win32, so "+1 `uint32`" skips exactly the link. |
| Why it fails on AMD64 | the link is written as `*(void **)BlockListHead` (8 bytes) but the payload still starts 4 bytes in: bytes 4-7 of the link overlap the first object / the free-list pointer written there. Block traversal is structurally corrupted. |
| Observed failure | a clean exit failed with `0xC0000005` after `WinMain` returned: the dynamic atexit destructor of `AutoPoolClass<MultiListNodeClass,256>::Allocator` read a corrupt next-block link (`0x0240606400000000`, non-canonical). Resolved with the matching PDB; the faulting instruction was `mov rbx,[rax]` in `ObjectPoolClass::~ObjectPoolClass`. |
| Fix | `FreeListHead = (T*)((unsigned char *)BlockListHead + sizeof(uint32 *));` - the offset equals the stored link width: 4 bytes on Win32 (unchanged), 8 on AMD64. Allocation size, capacity, ordering and destructor are unchanged. |
| Evidence | `object_pool_test` (7 tests on both architectures): byte-level reproduction of the old overlap for 8-byte pointers, no overlap with the new offset, 513 live allocations over three blocks, LIFO reuse, and the exact static `AutoPoolClass<MultiListNodeClass,256>` instantiation exiting cleanly through its CRT static destructor. The offset in the fixed binary is `add r9,8` (was `add r9,4`). The reviewed native Generals init-only run after the fix exited 0. |
| Notes | The test uses the real WWLib template and the real `MultiListNode`, not a re-implementation. Over-aligned types would need a different offset; none of the pooled types in the reachable set are over-aligned (audited in the document). |
| Upstream applicability | **Possible / high for any 64-bit build**: the code is shared WWLib; the defect is deterministic once `sizeof(void *) == 8`. |

### A-2. `WindowMsgData` truncated pointers carried in UI messages

| | |
|---|---|
| File / function | `Core/GameEngine/Include/GameClient/GameWindow.h`, `typedef ... WindowMsgData`; producer `GadgetButtonSetText` (`GGM_SET_LABEL`), receiver `GadgetPushButtonSystem` |
| Commit | `f6e8c1aee` (X64.21); document `docs/modernization/X64_21_STATIC_MENU_COMPOSITION.md` |
| Original assumption | window messages carry two `UnsignedInt` data words, and some messages put a pointer (`&text`, a `UnicodeString *`) in one of them. |
| Why it works on Win32 | pointers are 32 bits; the cast round-trips. |
| Why it fails on AMD64 | the stack address `0x842879d758` is cast to a 32-bit value `0x2879d758`; the receiver dereferences the truncated value. |
| Observed failure | access violation (read of `0x2879d758`) while loading `Window/Menus/MainMenu.wnd`, in `UnicodeString::UnicodeString(const UnicodeString &)` called from `GadgetPushButtonSystem` <- `GadgetButtonSetText` <- `gogoGadgetPushButton`. The fault address equalled the truncated pointer exactly. Reconstructed from the dump with the matching PDB and PE unwind data (a plain stack scan produced spurious frames and was rejected). |
| Fix | `typedef uintptr_t WindowMsgData;` under `_WIN64`; the original `UnsignedInt` typedef is kept on Win32. Message ids, routing, widget semantics and the parser are unchanged; no persisted/replay/network format uses it. |
| Evidence | A dual-architecture regression drives the real `GadgetButtonSetText`, virtual manager dispatch and `UnicodeString` copy; on AMD64 it asserts that the received valid pointer exceeds 32 bits and would differ after the old cast, on Win32 it statically asserts the original type (`UIProducerTest`). Native menu run afterwards. |
| Upstream applicability | **Possible / high for any 64-bit build**: shared `Core` header. |

### A-3. Audio completion handles cast to `UnsignedInt` (lost sounds over a long session)

| | |
|---|---|
| File / function | `Core/GameEngineDevice/Source/MilesAudioDevice/MilesAudioManager.cpp`: `setSampleCompleted`, `set3DSampleCompleted`, `setStreamCompleted` (end-of-sample callbacks), `notifyOfAudioCompletion`, `findPlayingAudioFrom`; interface in `Core/GameEngine/Include/Common/GameAudio.h` |
| Commit | `082ae9e64` (X64.31 fix3); evidence in `docs/modernization/NORMAL_LAUNCH.md` ("Confirmed root cause: 32 bit completion handles") |
| Original assumption | an audio sample / stream handle (an `HSAMPLE` / `H3DSAMPLE` / `HSTREAM`, a pointer in the audio library) fits `UnsignedInt`. |
| Why it works on Win32 | the handle is a 32-bit pointer. |
| Why it fails on AMD64 | the handle is a heap pointer above 4 GB; the callback cast it to `UnsignedInt`, `findPlayingAudioFrom` cast it back and never matched ("Audio has completed playing, but we can't seem to find it", a debug-only assert) and the completion was silently dropped. |
| Observed failure | a finished sound stayed in the playing list, its pooled handle never returned to the free list, and its priority slot was never released. Over a long match the pool filled: new SFX/voices were rejected or evicted by priority while music (a dedicated stream with its own slot) kept playing. The symptom was "audio degrades during a long session", not a crash. |
| Fix | new pointer-sized type `UnsignedPtr` (`uintptr_t`, `BaseTypes.h`); the completion interface and lookup take it; no `(UnsignedInt)` casts remain on the path. |
| Evidence | `native_miles_test` (20 tests): 3 000 rounds x 6 overlapping plays against pools of 8 handles with the engine's return-on-callback behaviour (no leak, no rejection drift, every completion carries the exact handle value), a 64-bit handle round trip, looped samples, stop/reuse 200x, handle accounting. |
| Upstream applicability | **Possible**, for builds that use a 64-bit audio library (here a native AMD64 mixer, see B-3); the engine-side interface type is shared code. |

### A-4. Windows diagnostics and stack walking assumed the x86 `CONTEXT`

| | |
|---|---|
| Files | `Core/Libraries/Source/WWVegas/WWLib/Except.cpp`, `Core/Libraries/Source/debug/debug_except.cpp`, `debug_stack.cpp`, `debug_debug.cpp`, `Dependencies/DbgHelp/DbgHelpLoader.*`, `Generals[MD]/Code/GameEngine/Source/Common/System/StackDump.cpp` |
| Commit | `923f730fe` (X64.2); `docs/modernization/X64_2_WINDOWS_DIAGNOSTICS.md` |
| Original assumption | exception/stack code reads `Eip/Esp/Ebp/Eax...` and `FloatSave` from `CONTEXT`, uses x86 inline assembly, and a 32-bit `StackWalk` callback signature. |
| Failure | 92 reproduced compile-error occurrences (Except.cpp 51, debug_except.cpp 25, debug_stack.cpp 8, debug_debug.cpp 8): missing register members, four unsupported inline-asm diagnostics, incompatible `StackWalk64` callbacks, parser cascades. |
| Fix | pointer-width-neutral accessors (`Dependencies/Utility/Utility/windows_diagnostics.h`) and AMD64 register selection; a standalone platform test (`windows_diagnostics_test`). |
| Upstream applicability | **Possible**; the compile errors are deterministic for any `_WIN64` build of these files. |

### A-5. Floating-point control: the x87 precision request does not exist on AMD64

See `REPLAY_DETERMINISM_FINDINGS.md` R-3 and `docs/modernization/X64_11_NATIVE_STARTUP_READINESS.md`. `setFPMode()` (`GameLogic.cpp`) asks for `_PC_24`; the AMD64 CRT ignores it without error. This is a
*characterisation*, not a fix: no simulation code was changed, and equivalence between a 32-bit and a 64-bit simulation is **not claimed**. **Possible upstream relevance** (cross-architecture
replays / lockstep).

### A-6. Pointer-truncation compiler scan (negative evidence)

A whole-engine scan with `C4311/C4302/C4312` over 3 024 translation units of the Generals and Core game-engine and device sources gave 194 distinct hits. Every hit on the path
special-power activation -> `OCLSpecialPower` -> `DeliverPayload` -> locomotor was a small integer stored in a `void *` user-data slot (module type, bit mask, player index) that round-trips. The
remaining hits are LAN / GameSpy / UI list-box item data and were **not individually audited**. Treat this as "no truncation of an object, module, handle or callback pointer found on that path",
not as "the engine is pointer-clean".

### A-7. The native terrain atlas was keyed by a per-map counter (black strips on the second map)

The native terrain pass uploads a CPU-built tile atlas once and re-uploads it only when `WorldHeightMap::m_nativeAtlasVersion` changes. The version was a **per-map** counter (`++m_nativeAtlasVersion`), so every freshly built map had version 1.
The terrain render object outlives its maps; when the next map had the same draw size the vertex-buffer tiles were reused (no reallocation, no release of the native resources) and the new map was drawn with the previous map's atlas. The new map's UVs address its own class layout, so
they landed on other textures and on uncovered atlas texels: regular black strips across the Zero Hour terrain after the shell map (first Zero Hour native run, 2026-10-17). The same map started without a shell map before it rendered correctly; fix: a process-wide unique serial
(`WorldHeightMap.cpp`). Found by an unattended reproduction of the player's shell -> Skirmish -> Start sequence and an A/B of that single line (`docs/modernization/evidence/x64-45-zh-terrain-strips/`). **Shared code**; it only shows when the consecutive maps keep the same
draw size and have different class layouts, which is why the earlier Generals runs did not show it (an unfixed Generals build did not reproduce it in the one sequence tried). **MKRaudstein-specific** (the native terrain path), but the pattern - a cache key that restarts at 1 for each new owner while the cache outlives the owners - is general.

## B. Third-party and platform constraints encountered

### B-1. The DirectX 8 / D3DX helper dependency is not available as AMD64 binaries

The engine called D3DX math, FVF-size, texture-creation, filter, surface-load and error-string helpers (17 unresolved helper names per title at the first native link probe, 3 external resource helpers
left at the end) whose implementation exists only as 32-bit libraries. The header-only declarations of the pinned DX8 SDK compile on AMD64; the binaries do not link.

| step | commit | content |
|---|---|---|
| X64.3 | `91c1e3e12` | separate DX8 declarations from x86 linkage |
| X64.4 | `1bddbfc42` | D3DX matrix / vector helpers as native CPU code |
| X64.5 | `84e5d2ec1` | FVF stride computation |
| X64.6 | `68956fd65` | texture-creation helpers |
| X64.7 | `6cb8b0d34` | legacy pixel-shader byte code pinned as reference-tested data |
| X64.8 | `328321802` | D3DX error strings |
| X64.9 / X64.10 | `9afa8cfac`, `09b2983af` | characterisation tests, then the last three file/filter/surface helpers (external helper count 3 -> 0 per title) |

Method worth copying: characterise the legacy behaviour with tests *before* replacing a helper, keep numerical policy explicit (`/fp:strict` for the compatibility library) and reject safely, rather than
reproduce, a quarantined packet-spanning RLE anomaly (the X64.10 document records it as rejected, not reproduced as heap corruption). **Architecture-specific** to a project that keeps the original engine as a
reference; other forks may prefer a different strategy.

### B-2. Third-party audio is 32-bit

The original Miles Sound System binaries are 32-bit. This fork replaced it with a native AMD64 mixer behind the existing `MilesLoader` interface (`bb98b8c01`, X64.31) and found the following
while doing so (all in `docs/modernization/NORMAL_LAUNCH.md`): the end-of-sample callbacks must not run the engine's synchronous file I/O on the mixer thread (millisecond crackle), samples
must not be decoded under the global lock, and voices must not be silenced beyond `maxDist`. These are consequences of this fork's replacement mixer, not defects of the original engine.
**MKRaudstein-specific.**

## C. MKRaudstein-specific runtime decisions (not general advice)

* A D3D11 backend with CPU-backed legacy buffers, a native terrain/world renderer and a retained-display lifecycle, selected by a normal-launch profile (X64.15-X64.31).
* The normal-launch log `generals64.log` and the diagnostic gates (`StartupDiagnostics`); see `PERFORMANCE_FINDINGS.md` P-1 for a hazard that this log created.
* Staged, isolated native runtime candidates under `build/dev-runtimes/` and hash-preflighted launchers; not part of the engine.
* Modern display settings, Render Scale and the Enhanced HUD (separate documents).
