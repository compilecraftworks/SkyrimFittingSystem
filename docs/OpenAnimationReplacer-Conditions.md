# SFS Open Animation Replacer Conditions

Starting with SFS 1.4.4, SFS registers read-only custom conditions through
Open Animation Replacer's public Conditions API when a compatible OAR DLL is
already installed. SFS does not load OAR, replace its DLL, hook
`GetWornArmor`, or change actual equipment and armor keywords.

The conditions query the actor-local final rendered outfit:

- visible actual equipment;
- active, currently visible SFS registered appearances;
- the current actor's manual visibility, conditions, strip-link suppression,
  and DAVE/DAV/native display result.

The query applies no special external-strip policy. It simply follows the
current final display result: anything currently shown is included and
anything currently not shown is not. This lets animation mods react to what
is shown without changing armor stats, inventory, other actors, or save data.

## Available conditions

| Condition | OAR component | Returns true when |
| --- | --- | --- |
| `SFS_IsShownArmorEquipped` | `Armor` (Form) | That ARMO is in the actor's final rendered outfit. |
| `SFS_ShownArmorHasKeyword` | `Keyword` | Any ARMO in the actor's final rendered outfit has that keyword. |
| `SFS_IsShownArmorInSlotHasKeyword` | `Slot` (numeric) + `Keyword` | Final shown armor occupying that zero-based biped slot has that keyword. `0` is Head/30, `2` is Body/32, and `31` is FX/61. |
| `SFS_IsShownBodyNaked` | none | No final rendered armor covers vanilla Body slot 32. |

`SFS_IsShownBodyNaked` is deliberately Body-slot specific. A body item can be
real equipment or a currently visible registered appearance. Conversely, a
real body item or registered appearance that SFS hides does not prevent the
naked result unless another visible appearance or actual armor covers Body.

## OAR configuration examples

Use these conditions in an OAR mod's condition editor or `config.json` after
requiring SFS 1.4.4.

```json
{
  "condition": "SFS_IsShownBodyNaked",
  "requiredVersion": "1.4.4.0"
}
```

```json
{
  "condition": "SFS_ShownArmorHasKeyword",
  "requiredVersion": "1.4.4.0",
  "Keyword": {
    "editorID": "ArmorCuirass"
  }
}
```

```json
{
  "condition": "SFS_IsShownArmorInSlotHasKeyword",
  "requiredVersion": "1.4.4.0",
  "negated": true,
  "Slot": { "value": 2.0 },
  "Keyword": {
    "form": { "pluginName": "Skyrim.esm", "formID": "A8657" }
  }
}
```

This is the slot-preserving replacement for an OAR condition that previously
used `Actor::GetWornArmor()` semantics, such as
`IsWornInSlotHasKeyword`. Only replace the visual-equipment conditions; retain
the original pack's unrelated gameplay conditions.

```json
{
  "condition": "SFS_IsShownArmorEquipped",
  "requiredVersion": "1.4.4.0",
  "Armor": {
    "pluginName": "ExampleArmor.esp",
    "formID": "0x000800"
  }
}
```

## Compatibility-patch guidance

Existing OAR packs continue to use their own original conditions. To make an
existing pack appearance-aware, replace only the equipment/naked checks that
need visual semantics with an SFS condition. Do not globally replace unrelated
weapon, faction, actor, combat, or gameplay-equipment checks.

Because this is a public OAR API extension, a compatibility patch is normally
only a small OAR `config.json` override. It needs no ESP, Papyrus script,
runtime armor keyword mutation, or SFS DLL replacement.
