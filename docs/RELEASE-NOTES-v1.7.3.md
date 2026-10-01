# Skyrim Fitting System v1.7.3 SE-AE

## Changes

- Fixed ordinary slot-32 armor being incorrectly marked as revealing when OCF
  or other categorization keywords contain upper-garment terms. Automatic
  upper-only classification now uses the item's name and EditorID, not keyword
  names. Runtime SOS/TNG keywords and virtual-token views share this correction.
- Tightened ambiguous name matching: `Top`, `Upper`, `Chest`, `Crop`, `Tube`,
  `Breast` or `Halter` alone no longer establish exposure. Clear garment names
  and phrases such as bra, shirt, vest, bikini top and crop top remain supported,
  including ordinary plurals and existing Korean/Chinese terms. Prefixes such as
  Shirtless/Vestments and phrases accidentally spanning two fields no longer match.
- Preserved SOS manual reveal/conceal choices, slot-49 lower-garment support and
  keywords originally supplied by other mods. SOS API requests now reuse the
  detected environment's form identity instead of probing for a separate copy.
  SOS and TNG remain optional and are enabled independently.
- Fixed DAV + Helmet Toggle 2 head/hair masks being overwritten by raw actual
  armor slots, which could hide the face or hair again. DAV and DAVE now preserve
  the same variant-resolved mask while retaining registered appearances, SFS
  real-equipment visibility controls and the existing HT2 hair rule. Actual
  equipment and inventory are not equipped, unequipped or otherwise rewritten.

## Update and scope

Install the complete package. Keep existing settings, kits and saves. OCF does
not need to be removed, and SOS/TNG support does not need to be disabled.
Names remain heuristics, not mesh-geometry analysis; ambiguous upper garments
can still be explicitly marked through the existing SOS user choices.

Requirements, supported game versions, public API ABI, save formats, helper ESL
and optional Wet Function / Dynamic Footprints / Modesty patches are unchanged.
BodyMorph, high-heel transforms, dye, strip/redress transactions and IED hook
routing are not replaced by these fixes. No new actor polling, inventory scan,
retained scene node or diagnostic runtime code is introduced.

## Verification limits

The SE/AE-only releasedbg build and all 36 regression executables plus source
checks pass. Classification coverage includes 2,836 checks, four SOS/TNG
environments and 128 repeated transitions per environment; DAV/DAVE coverage
includes 107,520 mask queries. Both defects have failing-before/passing-after
production-source tests with host engine fakes. These are not in-game visual
tests or a guarantee for every load order. Reporter-side gameplay is unverified.

See [classification audit](OCF-SOS-CLASSIFICATION-FIX-2026-10-01.md) and
[DAV/HT2 audit](DAV-HT2-HEAD-MASK-FIX-2026-10-01.md).
