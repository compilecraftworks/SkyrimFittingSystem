Scriptname SkyrimFittingSystemNative Hidden

Int Function GetActiveFittingSlotMask(Actor akActor) Global Native
Int Function GetDisplayedFittingSlotMask(Actor akActor) Global Native
Int Function GetSexLabWeaponLikeFittingSlotMask(Actor akActor) Global Native
Int Function GetSexLabConfiguredArmorStripSlotMask(Quest akConfigQuest, Bool abFemale, Bool abLeadIn, Bool abAggressive, Bool abVictim) Global Native
Bool Function IsSexLabConfiguredWeaponStrip(Quest akConfigQuest, Bool abFemale, Bool abLeadIn, Bool abAggressive, Bool abVictim) Global Native
Form Function GetActiveFittingArmorForSlot(Actor akActor, Int aiSlotMask) Global Native
Int Function GetActiveFittingArmorSlotMaskForSlot(Actor akActor, Int aiSlotMask) Global Native
Int Function GetHiddenRealEquipmentSlotMask(Actor akActor) Global Native
Bool Function IsRealEquipmentHiddenForActorSlots(Actor akActor, Int aiSlotMask) Global Native
Bool Function HasActiveFitting(Actor akActor) Global Native
Bool Function IsSexLabAutoStripIntegrationEnabled() Global Native
Function SuppressFittingSlots(Actor akActor, Int aiSlotMask) Global Native
Function RestoreFittingSlots(Actor akActor) Global Native
Function SuppressHeadgearToggleFittingSlots(Actor akActor, Int aiSlotMask) Global Native
Function RestoreHeadgearToggleFittingSlotMask(Actor akActor, Int aiSlotMask) Global Native
Function RestoreHeadgearToggleFittingSlots(Actor akActor) Global Native
Function RestoreFittingSlotMask(Actor akActor, Int aiSlotMask) Global Native
Function CommitFittingSlotMask(Actor akActor, Int aiSlotMask) Global Native
Function CommitFittingSlots(Actor akActor) Global Native
Function RefreshActor(Actor akActor) Global Native

Bool Function SyncDeviousDevicesHiderSlotFilters(Quest akDevicesUnderneathQuest) Global Native
Int Function GetPrivateNeedsStripSlotMask(Quest akConfigQuest) Global Native
Int Function GetPrivateNeedsFittingStripSlotMask(Quest akConfigQuest, Actor akActor) Global Native
Function QueuePrivateNeedsFittingControl(Actor akActor) Global Native
Function BeginPrivateNeedsFittingControl(Actor akActor, Int aiSlotMask) Global Native
Function EndPrivateNeedsFittingControl(Actor akActor) Global Native
Function BeginSoulgemOvenFittingControl(Actor akActor, Int aiSlotMask) Global Native
Function EndSoulgemOvenFittingControl(Actor akActor) Global Native
Int Function GetBathingInSkyrimFittingStripSlotMask(Quest akConfigQuest, Actor akActor) Global Native
Function BeginBathingInSkyrimFittingControl(Actor akActor, Int aiSlotMask) Global Native
Function EndBathingInSkyrimFittingControl(Actor akActor, Bool abRestore) Global Native
Int Function GetBodySearchFittingStripSlotMask(Quest akConfigQuest, Actor akActor) Global Native
Function BeginBodySearchFittingControl(Actor akActor, Int aiSlotMask) Global Native
Function EndBodySearchFittingControl(Actor akActor, Bool abRestore) Global Native
Function BeginSOSUserArmorListSync() Global Native
Function AddSOSUserRevealingArmor(Form akArmor) Global Native
Function AddSOSUserConcealingArmor(Form akArmor) Global Native
Bool Function EndSOSUserArmorListSync() Global Native
Function ClearSOSUserArmorLists() Global Native

; Runtime-only control path for the Helmet Toggle 2 compatibility patch.
; This does not change saved SFS eye-button state or fitting registrations.
Function SetHeadgearToggleFittingSlotsHidden(Actor akActor, Int aiSlotMask, Bool abHidden) Global Native
