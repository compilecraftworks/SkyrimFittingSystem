; SFS OStim bridge, 2026-10-09. Requires the accompanying SFSCore build.
; Cosmetic ownership only. No inventory operations, quests or polling.
ScriptName SFSOStimBridge Hidden
Bool Function IsAvailable() Global Native
Function BeginEquipmentPass(Actor Act) Global Native
Function EndEquipmentPass() Global Native
Function Strip(Actor Act, Int ThreadId, Int SlotMask, Bool UndressWigs) Global Native
Function Restore(Actor Act, Int ThreadId, Int SlotMask) Global Native
String Function GetRedressSession(Actor Act) Global Native
Int Function GetSessionMask(Actor Act, String Session) Global Native
Function RestoreSession(Actor Act, String Session, Int SlotMask) Global Native
