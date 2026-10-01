# OCF / SOS classification correction (v1.7.3)

## Confirmed cause

The original upper-only slot-32 heuristic appended keyword EditorIDs to armor
names and EditorIDs. Camel/snake normalization turned
`OCF_BodyTypeUnderwearF_Top` into text containing `top`; the single-word fallback
returned Reveal. Runtime synchronization then added SOS_Revealing when SOS was
detected, and the contextual virtual-token view reproduced the same decision.
The reporter observed SOS, not TNG, but both consume this shared decision.

The production-source host regression on the pre-fix implementation failed
804 of 2,684 initial assertions, including the supplied OCF reproducer, virtual
token projection and repeated ownership cleanup. This was not an installed
OCF/KID or game reproduction.

## Narrow correction

- Upper-only inference uses armor identity, never keyword taxonomy. The old
  generic-vanilla keyword exclusion list is removed rather than extended with
  another namespace blacklist. Unrelated keyword categories cannot introduce
  either an upper positive or a lower blocker into this heuristic.
- Identity fields have a phrase boundary: `Example_Bikini` plus `Top Quality
  Armor` cannot synthesize `bikini top`.
- Upper positives use complete ASCII words/phrases, including an ordinary `s`
  plural. Prefixes such as Shirtless, Vestments and BikiniTopology no longer
  match. Ambiguous Top/Upper/Chest/Crop/Tube/Breast/Halter require specific
  garment context; clear bra/shirt/vest nouns and established multilingual
  upper terms remain. This is still an identity heuristic, not mesh inspection.
- The slot-49 lower classifier and its keyword input are unchanged. Combined
  32+49 classified lower garments still conceal. SOS MCM policy retains highest
  priority, and non-32/49 items keep their inherited classification.
- Runtime synchronization still removes only SFS-owned assignments. Original
  ESP/KID keywords remain on source forms. As before, the contextual token view
  applies the higher-priority SFS 32/49 result without mutating those originals.
- No migration version bump or broad cleanup scan is added. Existing owned
  keyword removal handles corrected decisions; load/revert retains its existing
  ownership cleanup. Save formats are unchanged.

## SOS API identity

Environment detection now retains the detected SOS API FormID rather than a
separate boolean. IsSosInstalled derives from that ID. GetSosApiForm resolves
it without retaining a raw engine pointer or repeating plugin/EditorID probes.
The original plugin/FormID lookup and SOS_Misc fallback remain in the one
DataLoaded detection routine. Resolver generations, async callback ownership,
pending cleanup and explicit retry behavior are unchanged.

Physical SOS/TNG keyword additions remain gated independently by the installed
environment and available keyword forms. SOS VM requests remain SOS-only.
Internal classification can run without either mod; it is not a new dependency.
No new actor scan, polling task, inventory mutation or diagnostic code is added.

## Verification

- ArmorClassificationTests runs mechanically extracted production classifier,
  ownership synchronization, public wrappers, virtual-token view and environment
  detection against host form fakes: 2,836 passing assertions.
- SOS absent/present crossed with TNG absent/present, 128 transitions each;
  upper -> ordinary armor -> lower -> ordinary armor drains owned assignments.
  Repeated idle queries introduce no keyword writes. Missing keyword forms,
  internal armors, null input and other-mod original keywords are covered.
- Positive EN/KO/ZH cases, ordinary plurals, negative categorization metadata,
  ambiguous words, field-boundary and prefix collisions, manual policies,
  lower slot behavior and original virtual-token overlay priority are covered.
- Shared SOS API identity, missing standard-plugin fallback, absent/unresolvable
  form, explicit reinitialization and no repeated detection are covered.
  StateBoundaryTests also verifies missing-API pending cleanup/explicit retry
  and retains the 128-cycle stale/duplicate callback tests.
- All 36 regression executables/source checks and the SE/AE-only main build pass
  with the existing pinned dependencies. The separate DAV/HT2 fix remains in
  this release and its 107,520-case mask test passes.

These are host/source checks, not proof of every armor name, load order, SOS
script or in-game mesh. No reporter-side gameplay test was performed. MO2
deployment is not part of this release request. Public packaging is verified
by scripts/verify-v1.7.3-release.ps1.

Final v1.7.3 DLL SHA-256:
`391213BB4E89E0A115F1A02E665E1386D83F51C57A0979605B84327613355EEE`.
