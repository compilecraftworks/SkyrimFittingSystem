# Skyrim Fitting System v1.5.7

- Fixed live RaceMenu BodyMorph and OBody updates on visible registered
  appearances when DAVE is used with an AE-to-SE RaceMenu backport. The
  backport reports ActorUpdateManager version 0 but uses the later public
  vtable, so the previous version-only route called the wrong function and
  never registered SFS's late-attachment observer.
- Added read-only layout verification for all currently supported RaceMenu
  paths: original SE version 0, public-layout version-0 backports, and public
  versions 1/2. Each route calls only its verified AddInterface slot; an
  unknown layout fails closed and retains native scene capture.
- Preserved the verified BodyMorph v4/v5 call layout and actor-local live morph
  tracking across DAVE, DAV, and native display. No periodic polling, global
  actor scan, forced Update3D fallback, or body-family-specific behavior was
  added.
- Removed the unsuccessful DAVE forced-refresh experiment. Fitting Dye,
  Helmet Toggle 2, SexLab P+, SOS/TNG, conditions, kits, appearance locks,
  high heels, and save data keep their v1.5.6 behavior.

The runtime remains one Skyrim SE/AE `SFSCore.dll`. PDB files are excluded from
both runtime and source packages. Independent ABI tests cover legacy v0,
public-layout v0, public v1/v2, BodyMorph v4/v5, callback arguments,
idempotent retry, and unknown-layout rejection. The full fast regression suite
also covers DAVE/DAV/native tracking and the unchanged feature boundaries.
