# OStim appearance strip/redress bridge — SFS v1.7.6 / patches v1.0.0

## Scope and status

Released with SFS v1.7.6 and two official, mutually exclusive v1.0.0
OUndress overrides. They require SFS v1.7.6 or later; the old public v1.7.5
binary does not implement these natives. Installed OStim files and MO2
profiles are not modified by the build or packaging workflow.

No new SFS dependency, game-layout adapter, RaceMenu ABI, actual inventory
operation, global suppression state or periodic actor/inventory scan is added.
The existing SE/AE-only CommonLib closure is retained. VR is not a target.

## Audited upstream contracts

- [Archived OStim](https://github.com/VersuchDrei/OStim/tree/4f56819a8281e7a0ea4e0c56569f82ead023532d):
  Scripts/Source/OUndress.psc, OData.psc, OUtils.psc,
  OSexIntegrationMain.psc, OSexIntegrationMCM.psc and OUndressScript.psc.
- [Standalone source](https://github.com/VersuchDrei/OStimNG/tree/3954683bbdfcd34b2f9157012ed0c446d6eefd1e):
  data/Scripts/Source/OUndress.psc, OData.psc, OUtils.psc,
  OSexIntegrationMain.psc; native PapyrusUndress, FormUtil, Globals,
  ThreadActor, Thread, ThreadManager, GameEvents, public PluginInterface
  prefixes and their implementations, Main and CMakeLists.

These immutable contract snapshots are not a claim that repository HEAD
matches every downloadable release. No OStim library/DLL is built or bundled.
Standalone's checked CMake project identifies itself as OStim Standalone
7.5.1.2. Its public interface's getVersion returns the packed plugin version,
not a numeric ABI counter. The scene adapter consumes only the declared v1
prefix and never reads private Thread/ThreadActor fields or game offsets.

The archived repository supplies no native interface implementation to audit.
It lacks the Standalone UndressWigs property; the old patch deliberately does
not invoke that property. Its existing wig exclusion remains unchanged.
The Standalone patch consults its own property, correcting the upstream
Papyrus path's unconditional wig exclusion to match its native setting.

## Ownership and callbacks

OUndress.Undress/UndressPartial still return ONLY real worn ARMO records.
OStim CanUndress/IsWig, actual unequip/equip calls and its real-gear cache stay
in the original framework. The partial strip return array is trimmed to
remove None entries, as required by OStim's C++ callback contract.

The SFS bridge selects existing registered appearance identities using the
existing control mask (including Direct Edit remapping), source NoStrip
keywords and observed slot-31/HairTint wig facts. A fact is only a scalar in
the existing appearance catalog; no extra NiPointer/GPU resource is retained.
Both native capture and the existing RaceMenu/DAVE attachment callback feed
the fact. Unobserved appearances are not blanket-excluded as wigs.

Tickets use restoreItemID=0 and a scoped `OStim:<threadID>:<GUID>` owner string.
There is no new actual-inventory snapshot/StripTransaction. Repeated partial
strips merge into that owner's existing ticket. Partial redress removes a
whole multi-slot appearance on control-mask intersection, not a visual-slot
fragment. Other owners, generation changes and manual overrides retain the
existing ticket behavior.

Animated redress captures the stable owner key before Utility.Wait and closes
that episode's owner prefix without restoring visibility. Thus a later strip
gets a fresh key even when an older OStim has no native scene-stop interface;
the pending animation cannot consume that newer episode. Matching appearances
restore at the original dress point. The owner string is already
serialized by SFS. Loading a co-save may reassign internal transaction IDs;
the GUID key remains stable and avoids clearing a newer player-thread-0 scene.

Standalone scene end changes only the owner prefix to `OStimEnded:`. It does
not force restoration at end, which would interfere with animated/redress-off
behavior. Starting a scene restores only ended owners for actors in that new
scene, covering native freeFast migration while preserving removed NPC timing.
Exact int32 thread IDs and actual scene membership come from the public API;
there is no lossy float ModEvent ID conversion or timing heuristic.

An explicit Papyrus Begin/EndEquipmentPass brackets the four real equipment
mutation calls used by the override. Only the selected virtual-token observer
defers cosmetic ownership for the same actor/VM stack. The actual native call,
the external actual-slot observer, displayed-body keyword answers, DD/P+
adapters and other callers are not disabled. Ending the pass removes only a
bridge-only observation; nested callers' existing catalog evidence survives.

OStim-absent startup does not register scene listeners. Patch activation comes
from the official UsePapyrusUndressing override. The optional native interface
is not required for basic old/Papyrus strip-redress support.

## Build and checks

SFS uses the existing pinned XMake 3.1.0, CommonLib v6.7.0 closure and installed
MSVC 14.51.36231. No tool/dependency upgrade was performed. Sandbox builds
failed at PDB IPC (C1902); the same pinned toolchain built successfully outside
the sandbox, without installing anything or changing the tool registry.

Papyrus build reuses the repository's existing original Creation Kit compiler
(file version 1.0.0.0) and complete installed Creation Kit/SKSE source headers.
The compiler files' own README identifies them as Creation Kit originals.
No compiler download or guessed newer tool was substituted.

- Compiler SHA256: F441AFA877CE0B315C6B613DB7FE700A7BBC66B71A72FC4D0D8BC94577CEB0CE.
- Assembler SHA256: 936A3FF039E5632056D28631EF712ECC9EBFA9F236C51AF86C34A82582280050.
- `scripts/build-ostim-patches.ps1`: both OUndress and bridge PEX variants,
  zero compiler/assembler errors or warnings.
- `tests/run-fast-regressions.ps1`: 42 executables plus source-boundary checks.
- `OStimAppearanceTests`: mechanically extracted production selection,
  ticket creation, scope restoration, pass cleanup and scene ownership.
  Engine/VM boundaries are host fakes, not an in-game emulation.

The release audit re-read the pinned public PluginInterface, InterfaceMap,
InterfaceExchangeMessage, ThreadInterface, Thread, ThreadActor and listener
headers plus ThreadInterfaceImpl, ThreadManager, Thread, ThreadActor,
PapyrusUndress, FormUtil and OUndress. The consumed v1 virtual prefix, exact
`'OST'` message / pointer-sized exchange and `"Threads"` query match upstream.
No packed plugin-version value is treated as an ABI ordinal. Start callbacks
run after initContinue has scheduled Papyrus undress; stop callbacks run while
the scene actors are still readable. No private scene object is retained.

`OStimSceneInterfaceTests` compiles the entire production adapter with host
boundaries: absent DLL, inactive patch, null scene/members, both listener slots,
exact IDs above the float exact-integer range, thread-0 reuse and 128 repeated
callbacks/initializations without duplicate registration. Papyrus source checks
also enforce both script kinds, balanced equipment passes, trimmed return arrays,
original redress masks and stable animated session keys. These complement the
production ticket and vanilla wig/long-hair suites; they are not binary gameplay
emulation or proof of external animation timing.

Compile-only OData/OUtils/OSexIntegrationMain declarations are signatures
verified against the pinned sources. They are never installed or emitted as
runtime overrides. Full SFS sources, pinned vendored dependencies and build
scripts accompany the official release; compiler binaries are not redistributed.

## Remaining in-game acceptance checks

Test on each actually used OStim binary and add-on configuration, not just
against the pinned source contracts. No scene was run during this task.

- Mod Settings / Vanilla / both Direct Edit bases; managed and unmanaged NPCs.
- Registered-only armor, empty actual feet, multi-slot armor and accessories.
- NoStrip, protected/locked slots, real/registered wigs, UndressWigs on/off.
- Full/partial/animated redress, redress disabled, scene change/add/remove/swap.
- Save/load during stripping and during a pending animated redress.
- Manual visibility changes and overlapping SexLab/SGO/Private Needs owners.
- DAVE, DAV and native rendering; late attachments, RaceMenu morphs/heels,
  dye resources, cell unload, IED and existing input/condition UI behavior.

In particular, the archived repository does not expose its companion native
binary's lifecycle implementation, and Standalone release binaries are not
runtime-tested here. Do not label this as universal in-game compatibility.
