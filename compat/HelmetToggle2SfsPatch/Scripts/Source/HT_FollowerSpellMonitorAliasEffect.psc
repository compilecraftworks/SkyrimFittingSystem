Scriptname HT_FollowerSpellMonitorAliasEffect extends ActiveMagicEffect  

Import PO3_Events_Alias
Import PO3_SKSEFunctions
Import DynamicArmor
Import SeasonsOfSkyrim

HT_MCM Property htMCM Auto

Keyword Property ArmorHelmet Auto
Keyword Property ClothingHead Auto
Keyword Property ClothingBody Auto
Keyword Property ClothingCirclet Auto
Keyword Property HT_ArmorHood Auto
Keyword Property HT_ArmorMask Auto
Keyword Property HT_ArmorFaceMask Auto
Keyword Property HT_ArmorVisor Auto
Keyword Property HT_ArmorHelmetAlt Auto
Keyword Property HT_IgnoreHeadgear Auto
Keyword Property HT_ApplyWorkaround Auto

FormList Property HT_SafeLocations Auto
FormList Property HT_UnsafeLocations Auto
FormList Property HT_DustyWeather Auto

Spell Property HT_HeadGearEquipped Auto
Spell Property HT_CooldownSpell Auto
MagicEffect Property HT_CooldownEffect Auto

Bool isVisor
Bool isWearingHelmet
Int iTypeOfHelmet = -1
Armor[] fArrayEquippedHeadgear
Actor akAnimActor
FormList Hairstyles_HumanList
FormList Hairstyles_ElfList
FormList Hairstyles_KhajiitList
FormList Hairstyles_OrcList
Armor fHairSlot

Event OnInit()
	fArrayEquippedHeadgear = New Armor[4]
EndEvent

Event OnEffectStart(Actor akTarget, Actor akCaster)
	Hairstyles_HumanList = Game.GetFormFromFile(0x001, "Dynamic NPC Hairstyles.esp") as FormList
	Hairstyles_ElfList = Game.GetFormFromFile(0x002, "Dynamic NPC Hairstyles.esp") as FormList
	Hairstyles_KhajiitList = Game.GetFormFromFile(0x003, "Dynamic NPC Hairstyles.esp") as FormList
	Hairstyles_OrcList = Game.GetFormFromFile(0x04E, "Dynamic NPC Hairstyles.esp") as FormList
	akAnimActor = akCaster
	RegisterForModEvent("ManageFollowerHelmet", "OnManageFollowerHelmet")
	Utility.Wait(0.1)
	RegisterForModEvent("ManageFollowerHelmet", "OnManageFollowerHelmet")
	CheckConditions()
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
	Utility.Wait(1.0)
	akCaster.SetAnimationVariableInt("iGPMAAnimationType", 0)
	
	If !htMCM.HT_EnableFollowers.GetValue()
		ManageHelmet(akAnimActor)
		akAnimActor.RemoveSpell(HT_HeadGearEquipped)
	EndIf
EndEvent

; Hotkey pressed or MCM settings changed
Event OnManageFollowerHelmet(string eventName, string strArg, float numArg, Form sender)
	iTypeOfHelmet = -1
	
	If numArg == 0.0
		ManageHelmet(akAnimActor, False)
	ElseIf numArg == 1.0
		ManageHelmet(akAnimActor, True)
	Else
		CheckConditions()
	EndIf
EndEvent

; Change of locations
Event OnLocationChange(Location akOldLoc, Location akNewLoc)
	If akNewLoc
		Utility.Wait(0.2)
	EndIf
	
	CheckConditions()
EndEvent

; Combat state
Event OnCombatStateChanged(Actor akTarget, int aeCombatState)
	If htMCM.HT_EnableCombat.GetValue()
		If aeCombatState > 0
			ManageHelmet(akAnimActor, True)
		Else
			Utility.Wait(1)
			CheckConditions()
		EndIf
	EndIf
EndEvent

; Check helmets equipped
Event OnObjectEquipped(Form akBaseObject, ObjectReference akReference)
	If akBaseObject
		If akBaseObject.HasKeyword(ArmorHelmet) || akBaseObject.HasKeyword(ClothingHead) || akBaseObject.HasKeyword(ClothingBody) || akBaseObject.HasKeyword(ClothingCirclet) || akBaseObject.HasKeyword(HT_ArmorHelmetAlt) \
		|| akBaseObject.HasKeyword(HT_ArmorHood) || akBaseObject.HasKeyword(HT_ArmorMask)
			iTypeOfHelmet = -1
			isVisor = False
			CheckConditions()
		EndIf
	EndIf
EndEvent

Event OnObjectUnequipped(Form akBaseObject, ObjectReference akReference)
	If akBaseObject
		If akBaseObject.HasKeyword(ArmorHelmet) || akBaseObject.HasKeyword(ClothingHead) || akBaseObject.HasKeyword(ClothingBody) || akBaseObject.HasKeyword(ClothingCirclet) || akBaseObject.HasKeyword(HT_ArmorHelmetAlt) \
		|| akBaseObject.HasKeyword(HT_ArmorHood) || akBaseObject.HasKeyword(HT_ArmorMask)
			iTypeOfHelmet = -1
			isVisor = False
		EndIf
	EndIf
EndEvent

Event OnUpdate()
	akAnimActor.SetAnimationVariableInt("iGPMAAnimationType", 0)
EndEvent

; Function responsible to check the hotkey state
Bool Function CheckHotkey()
	If htMCM.HT_HotkeyType.GetValue() == 1
		ManageHelmet(akAnimActor, htMCM.HT_HotkeyState.GetValue())
		Return True
	ElseIf htMCM.HT_HotkeyType.GetValue() == 2
		If htMCM.HT_HotkeyState.GetValue() != 0
			If htMCM.HT_HotkeyState.GetValue() == 1
				ManageHelmet(akAnimActor, True)
			Else
				ManageHelmet(akAnimActor, False)
			EndIf
			
			Return True
		EndIf	
	EndIf
	
	Return False
EndFunction

; Function responsible to check the conditions to equip or not the helmet/hood
Function CheckConditions()
	If akAnimActor.IsDead() || Self == None; somehow, the NPC is dead or "self" is none
		Self.Dispel()
		Return
	EndIf
	
	If CheckHotkey()
		Return
	EndIf
	
	If htMCM.HT_EnableCombat.GetValue() && akAnimActor.IsInCombat()
		ManageHelmet(akAnimActor, True)
		Return
	EndIf
	
	Bool doEquip = True ; To equip or not equip, that's the question
	Bool isSafeLocation
	Bool isBadWeather
	
	; Checks for safe locations
	If htMCM.HT_EnableLocation.GetValue()
		Location akCurrentLoc = akAnimActor.GetCurrentLocation()
		If akCurrentLoc && (akAnimActor.IsInInterior() || !htMCM.HT_EnableInteriors.GetValue())
			If FindLocationKeyword(akCurrentLoc)
				doEquip = False
				isSafeLocation = True
			EndIf
		EndIf
	Else
		If htMCM.HT_EnableWeather.GetValue() || htMCM.HT_EnableSeasons.GetValue() || htMCM.HT_EnableCombat.GetValue()
			doEquip = False
			isSafeLocation = True
		EndIf
	EndIf
	
	If !akAnimActor.IsInInterior()
		; Checks for bad weathers
		If htMCM.HT_EnableWeather.GetValue()
			Weather akWeather = Weather.GetCurrentWeather()
			If htMCM.HT_EnableRain.GetValue() && GetWeatherType(akWeather) == 2
				doEquip =  True
			ElseIf htMCM.HT_EnableSnow.GetValue() && GetWeatherType(akWeather) == 3
				doEquip =  True
			ElseIf htMCM.HT_EnableDusty.GetValue() && HT_DustyWeather.HasForm(akWeather)
				doEquip =  True
			ElseIf GetWeatherType(akWeather) <= 1 && isSafeLocation
				doEquip = False
			EndIf
			
			isBadWeather = doEquip
		EndIf
		
		; Checks for cold seasons
		If htMCM.HT_EnableSeasons.GetValue()
			Int iCurrentSeason = GetCurrentSeason()
			If htMCM.HT_EnableWinter.GetValue() && iCurrentSeason == 1
				doEquip =  True
			ElseIf htMCM.HT_EnableAutumn.GetValue() && iCurrentSeason == 4
				doEquip =  True
			ElseIf isSafeLocation && !isBadWeather
				doEquip = False
			EndIf
		EndIf
	EndIf
	
	ManageHelmet(akAnimActor, doEquip)
EndFunction

; Function responsible to find safe locations by keywords
Bool Function FindLocationKeyword(Location akCurrentLoc)
	Bool isSafeLocation = True
	Keyword[] kLocKeywordArray = akCurrentLoc.GetKeywords()
	Int iArrayIndex = kLocKeywordArray.Length
	
	; Checks for unsafe locations. This is necessary because a location can have "safe" and "unsafe" keywords at the same time.
	If HT_UnsafeLocations.GetSize()
		Bool bListHasKeyword 
		While iArrayIndex && isSafeLocation
			iArrayIndex -= 1
			isSafeLocation = !HT_UnsafeLocations.HasForm(kLocKeywordArray[iArrayIndex])
		EndWhile
	EndIf
	
	If isSafeLocation
		; Checks for safe locations
		isSafeLocation = False
		iArrayIndex = kLocKeywordArray.Length
		While iArrayIndex && !isSafeLocation
			iArrayIndex -= 1
			isSafeLocation = HT_SafeLocations.HasForm(kLocKeywordArray[iArrayIndex])
		EndWhile
		
		; Checks for the parent location when the current location isn't "safe", if enabled. Won't check for unsafe locations again
		If !isSafeLocation && htMCM.HT_EnableParentLocation.GetValue()
			kLocKeywordArray = GetParentLocation(akCurrentLoc).GetKeywords()
			iArrayIndex = kLocKeywordArray.Length
			
			While iArrayIndex && !isSafeLocation
				iArrayIndex -= 1
				isSafeLocation = HT_SafeLocations.HasForm(kLocKeywordArray[iArrayIndex])
			EndWhile
		EndIf
	EndIf
	
	Return isSafeLocation
EndFunction

; Function responsible to equip or not the helmets/hoods and play the animations.
Function ManageHelmet(Actor akAnimTarget, Bool doEquip = True)
	Bool isChecking
	
	If doEquip ; Equip headgear
		If akAnimTarget.HasSpell(HT_HeadGearEquipped) ; headgear is already equipped on NPC
			isChecking = True
		Endif
	Else ; Unequip headgear
		If !akAnimTarget.HasSpell(HT_HeadGearEquipped) ; headgear is already unequipped on NPC
			isChecking = True
		Endif
	EndIf
	
	Bool doSkip = True
	Bool doAnim = True
	Int iArrayIndex
		
	If iTypeOfHelmet < 0
		fArrayEquippedHeadgear = New Armor [4]
		fArrayEquippedHeadgear[0] = akAnimTarget.GetWornForm(0x00000001) as Armor ; Slot 30 - Head
		fArrayEquippedHeadgear[1] = akAnimTarget.GetWornForm(0x00000002) as Armor ; Slot 31 - Hair
		fArrayEquippedHeadgear[2] = akAnimTarget.GetWornForm(0x00001000) as Armor ; Slot 42 - Circlet -- Not "circlet" circlets, just here as a failsafe to get helmets that are only using the circlet slot
		doAnim = False
		fHairSlot = None
		
		If Hairstyles_HumanList
			If !fArrayEquippedHeadgear[1].HasKeyword(HT_IgnoreHeadgear)
				If akAnimTarget.GetItemCount(Hairstyles_HumanList) > 0 || akAnimTarget.GetItemCount(Hairstyles_ElfList) > 0 || akAnimTarget.GetItemCount(Hairstyles_KhajiitList) > 0 || akAnimTarget.GetItemCount(Hairstyles_OrcList) > 0
					fHairSlot = fArrayEquippedHeadgear[1]
				EndIf	
			EndIf
		EndIf
		
		If htMCM.HT_EnableMask.GetValue()
			fArrayEquippedHeadgear[3] = akAnimTarget.GetWornForm(0x00004000) as Armor ; Slot 44 - Beard -- Used for face masks
		Else
			fArrayEquippedHeadgear[3] = None
		EndIf
		
		Int iArrayMatch
		iArrayIndex = 3
		
		While iArrayIndex
			iArrayIndex -= 1
			If fArrayEquippedHeadgear[iArrayIndex] 
				iArrayMatch = fArrayEquippedHeadgear.Find(fArrayEquippedHeadgear[iArrayIndex])
				If !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorHelmetAlt)
					If fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_IgnoreHeadgear) || fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ClothingCirclet)  ; Ignores circlets as well
						;ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayIndex])
						Utility.Wait(0.1)
						fArrayEquippedHeadgear[iArrayIndex] = None
					ElseIf !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ArmorHelmet) && !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorMask) && !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorHood) \
					&& !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ClothingHead) && !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorVisor) ; Ignores everything that doesn't have one of those keywords
						fArrayEquippedHeadgear[iArrayIndex] = None
					ElseIf iArrayMatch >= 0 && iArrayMatch != iArrayIndex
						fArrayEquippedHeadgear[iArrayIndex] = None
					EndIf
				Else
					If iArrayMatch >= 0 && iArrayMatch != iArrayIndex
						fArrayEquippedHeadgear[iArrayIndex] = None
					EndIf
				EndIf
			EndIf
		EndWhile
		
		iArrayIndex = -1
		
		While iArrayIndex < 2
			iArrayIndex += 1
			
			If fArrayEquippedHeadgear[iArrayIndex] && fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorMask) 
				If htMCM.HT_EnableMask.GetValue()
					doAnim = True
					iTypeOfHelmet = 3 ; Mask
					iArrayIndex = 2
				Else
					ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayIndex])
					fArrayEquippedHeadgear[iArrayIndex] = None
				EndIf
			EndIf
		EndWhile	
		
		If fArrayEquippedHeadgear[3] && fArrayEquippedHeadgear[3].HasKeyword(HT_ArmorFaceMask) && !fArrayEquippedHeadgear[3].HasKeyword(HT_IgnoreHeadgear)
			iTypeOfHelmet = 5 ; Face mask
			
			If htMCM.HT_EnableMask.GetValue()
				doAnim = True
			Else
				doAnim = False
			EndIf
		ElseIf fArrayEquippedHeadgear[3] && fArrayEquippedHeadgear[3].HasKeyword(HT_ArmorMask) && !fArrayEquippedHeadgear[3].HasKeyword(HT_IgnoreHeadgear)
			iTypeOfHelmet = 3 ; Mask
			
			If htMCM.HT_EnableMask.GetValue()
				doAnim = True
			EndIf
		ElseIf fArrayEquippedHeadgear[3]
			ResetVariant(akAnimTarget, fArrayEquippedHeadgear[3])
			fArrayEquippedHeadgear[3] = None
			
			If iTypeOfHelmet == 5
				iTypeOfHelmet = 0
			EndIf
		EndIf
					
		If iTypeOfHelmet <= 0 || iTypeOfHelmet == 5
			iArrayIndex = -1
			
			While iArrayIndex < 2
				iArrayIndex += 1
				
				If fArrayEquippedHeadgear[iArrayIndex] && (fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorHood) || fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ClothingBody)) ; Most likely a hood, as in hooded robes
					If htMCM.HT_EnableHood.GetValue()
						doAnim = True
						iTypeOfHelmet = 2 ; Hood
						iArrayIndex = 2
					Else
						ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayIndex])
						fArrayEquippedHeadgear[iArrayIndex] = None
						
						If iTypeOfHelmet <= 0
							iTypeOfHelmet = 2
						EndIf
					EndIf
				EndIf
			EndWhile
		EndIf
		
		If iTypeOfHelmet <= 0 || iTypeOfHelmet == 5
			iArrayIndex = -1
			
			While iArrayIndex < 2
				iArrayIndex += 1
				
				If fArrayEquippedHeadgear[iArrayIndex] && fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ClothingHead)
					If htMCM.HT_EnableHat.GetValue()
						doAnim = True
						iTypeOfHelmet = 4 ; Hat
						iArrayIndex = 2
					Else
						ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayIndex])
						fArrayEquippedHeadgear[iArrayIndex] = None
						
						If iTypeOfHelmet <= 0
							iTypeOfHelmet = 4
						EndIf
					EndIf
				EndIf
			EndWhile
		EndIf
				
		If iTypeOfHelmet <= 0 || iTypeOfHelmet == 5
			iArrayIndex = -1
			
			While iArrayIndex < 2
				iArrayIndex += 1	
				If fArrayEquippedHeadgear[iArrayIndex]
					If fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorHelmetAlt)
						If htMCM.HT_EnableHelmet.GetValue()
							iTypeOfHelmet = 6 ; Uses the hat animation for this helmet
							doAnim = True
							iArrayIndex = 2
						Else
							ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayIndex])
							fArrayEquippedHeadgear[iArrayIndex] = None
							
							If iTypeOfHelmet <= 0
								iTypeOfHelmet = 6
							EndIf
						EndIf
					ElseIf !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorHood) && !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorMask) && fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ArmorHelmet)
						If htMCM.HT_EnableHelmet.GetValue()
							iTypeOfHelmet = 1 ; Helmet
							doAnim = True
							
							If fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorVisor)
								isVisor = True
							EndIf
							
							iArrayIndex = 2
						Else
							ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayIndex])
							fArrayEquippedHeadgear[iArrayIndex] = None
							
							If iTypeOfHelmet <= 0
								iTypeOfHelmet = 1
							EndIf
						EndIf
					EndIf
				EndIf
			EndWhile
		Else 
			; sets the value here if iTypeOfHelmet was defined in a previous call of this function (manage helmet called twice at the same time)
			doAnim = True
		EndIf
		
		; setting this here to not call this function again unless another helmet was equipped
		If doAnim
			isWearingHelmet = True
		Else
			isWearingHelmet = False
			iTypeOfHelmet = 0
		EndIf
	EndIf
	
	iArrayIndex = -1
	
	If fArrayEquippedHeadgear[0]
		iArrayIndex = 0
	ElseIf fArrayEquippedHeadgear[1]
		iArrayIndex = 1
	EndIf
	
	If doAnim
		doSkip = False
	EndIf
	
	If isChecking || !isWearingHelmet
		doAnim = False
	EndIf
		
	If iTypeOfHelmet > 0 && doAnim
		If htMCM.HT_SkipAnimation.GetValue() == 2 || HasActiveMagicEffect(akAnimTarget, HT_CooldownEffect)
			doAnim = False
		ElseIf akAnimTarget.GetAnimationVariableBool("bInJumpState") || akAnimTarget.GetSleepState()
			doAnim = False
		ElseIf akAnimTarget.GetAnimationVariableBool("bIsInMT")
			If !akAnimTarget.GetAnimationVariableBool("isEquipping") && !akAnimTarget.GetAnimationVariableBool("isUnequipping")
				doAnim = True
			Else
				doAnim = False
			EndIf			
		ElseIf akAnimTarget.IsWeaponDrawn() && htMCM.HT_EnableWeaponDrawn.GetValue()
			If !akAnimTarget.IsUnconscious() && !akAnimTarget.GetAnimationVariableBool("isEquipping") && !akAnimTarget.GetAnimationVariableBool("isUnequipping")
				doAnim = True
			Else
				doAnim = False
			EndIf
		ElseIf akAnimTarget.GetAnimationVariableBool("bIsRiding") && !akAnimTarget.IsWeaponDrawn()
			doAnim = True
		Else
			doAnim = False
		EndIf
		
		If doAnim
			If iTypeOfHelmet == 1 ; is Helmet
				Float fRightHand
				Float fLeftHand
				Bool isWeaponOut
				
				If doEquip ; Equipping helmet animation
					If akAnimTarget.IsWeaponDrawn()
						fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", False)
						fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", False)
						
						If fRightHand == 0.01
							fRightHand = 1.00
						EndIf
						If fLeftHand == 0.01
							fLeftHand = 1.00
						EndIf
						
						NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, False)
						isWeaponOut = True
					EndIf

					If isVisor
						If akAnimTarget.GetSitState() == 0
							akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 1)
						EndIf
					
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 1)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
						Utility.Wait(0.95)
					Else
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 1)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
						Utility.Wait(1.0)

						If isWeaponOut
							NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, False)
						EndIf
						
						Utility.Wait(1.0)
					EndIf
					
					akAnimTarget.AddSpell(HT_HeadGearEquipped)
					ResetHiddenVariant(akAnimTarget)
					
					If !isVisor
						Utility.Wait(0.5)
					EndIf
					
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
					akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)			   
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
				Else ; Unequipping helmet animation
					If akAnimTarget.IsWeaponDrawn()
						fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", False)
						fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", False)
						
						If fRightHand == 0.01
						fRightHand = 1.00
						EndIf
						If fLeftHand == 0.01
							fLeftHand = 1.00
						EndIf
						
						NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, False)
						isWeaponOut = True
					EndIf

					If isVisor
						If akAnimTarget.GetSitState() == 0 
							akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 1)
						EndIf
						
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 2)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
						Utility.Wait(0.95)
						
						akAnimTarget.RemoveSpell(HT_HeadGearEquipped)
						ApplyVariant(akAnimTarget, "HT_RaisedVisorPlayer")
					Else
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 2)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
						
						If isWeaponOut
							NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, False)
						EndIf
						
						Utility.Wait(0.8)
						akAnimTarget.RemoveSpell(HT_HeadGearEquipped)
						
						If fHairSlot
							ResetHiddenVariant(akAnimTarget)
						Else
							ApplyHiddenVariant(akAnimTarget)
						EndIf
					EndIf
					
					If isVisor
						Utility.Wait(0.15)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
						akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)						
					Else
						Utility.Wait(1.15)

						If isWeaponOut
							NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, False)
						EndIf
						
						Utility.Wait(0.70)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
						akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
					EndIf
				EndIf
					
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, False)
				EndIf
			ElseIf iTypeOfHelmet == 2 ; is Hood (with or without Face mask)
				Float fRightHand
				Float fLeftHand
				Bool isWeaponOut
				
				If akAnimTarget.IsWeaponDrawn()
					fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", False)
					fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", False)
					
					If fRightHand == 0.01
						fRightHand = 1.00
					EndIf
					If fLeftHand == 0.01
						fLeftHand = 1.00
					EndIf
					
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, False)
					isWeaponOut = True
				EndIf
				
				akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)
				If doEquip ; Equipping hood animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 5)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(1.2)
					ResetHiddenVariant(akAnimTarget)
					akAnimTarget.AddSpell(HT_HeadGearEquipped)
					Utility.Wait(0.8)
				Else ; Unequipping hood animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 6)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(1.4)						
					akAnimTarget.RemoveSpell(HT_HeadGearEquipped)
					
					If fHairSlot
						ResetHiddenVariant(akAnimTarget)
					Else
						ApplyHiddenVariant(akAnimTarget)
					EndIf			
					
					Utility.Wait(0.6)
				EndIf  

				akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
				Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
					
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, False)
				EndIf
			ElseIf iTypeOfHelmet == 3 ; is Mask
				Float fRightHand
				Float fLeftHand
				Bool isWeaponOut
					
				If akAnimTarget.IsWeaponDrawn()
					fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", False)
					fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", False)
					
					If fRightHand == 0.01
						fRightHand = 1.00
					EndIf
					If fLeftHand == 0.01
						fLeftHand = 1.00
					EndIf
					
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, False)
					isWeaponOut = True
				EndIf
				
				akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)				  
				If doEquip
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 7)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")	
					Utility.Wait(2.0)
					akAnimTarget.AddSpell(HT_HeadGearEquipped)
					ResetHiddenVariant(akAnimTarget)
					Utility.Wait(0.6)
				Else 
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 8)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(0.8)
					akAnimTarget.RemoveSpell(HT_HeadGearEquipped)
					
					If fHairSlot
						ResetHiddenVariant(akAnimTarget)
					Else
						ApplyHiddenVariant(akAnimTarget)
					EndIf	
					
					Utility.Wait(1.9)
				EndIf 
				
				akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
				Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
				
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, False)
				EndIf
			ElseIf iTypeOfHelmet == 4 || iTypeOfHelmet == 6 ; is Hat
				Float fRightHand
				Float fLeftHand
				Bool isWeaponOut
				
				If akAnimTarget.IsWeaponDrawn()
					fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", False)
					fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", False)
					
					If fRightHand == 0.01
						fRightHand = 1.00
					EndIf
					If fLeftHand == 0.01
						fLeftHand = 1.00
					EndIf
					
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, False)
					isWeaponOut = True
				EndIf
				
				If doEquip ; Equipping hat animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 9)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(2.0)
					ResetHiddenVariant(akAnimTarget)						
					akAnimTarget.AddSpell(HT_HeadGearEquipped)
				Else ; Unequipping hat animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 10)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(2.0)
					akAnimTarget.RemoveSpell(HT_HeadGearEquipped)
					
					If fHairSlot
						ResetHiddenVariant(akAnimTarget)
					Else
						ApplyHiddenVariant(akAnimTarget)
					EndIf	
				EndIf

				akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
				Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
					
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, False)
				EndIf
			ElseIf iTypeOfHelmet == 5 ; is Face mask
				Float fRightHand
				Bool isWeaponOut
					
				If akAnimTarget.IsWeaponDrawn()
					fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", False)
					
					If fRightHand == 0.01
						fRightHand = 1.00
					EndIf
					
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, False)
					isWeaponOut = True
				EndIf
				
				akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 1)
				
				If doEquip ; Equipping face mask animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 11)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")	
					Utility.Wait(0.95)
					
					If fArrayEquippedHeadgear[3]
						ResetVariant(akAnimTarget, fArrayEquippedHeadgear[3])
					EndIf
					
					akAnimTarget.AddSpell(HT_HeadGearEquipped)
				Else ; Unequipping face mask animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 12)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(0.95)
					ApplyVariant(akAnimTarget, "HT_HiddenMaskPlayer")
					akAnimTarget.RemoveSpell(HT_HeadGearEquipped)
				EndIf  
					
				akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
				akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)	  
				Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
				
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, False)
				EndIf
			EndIf
		EndIf
	EndIf
	
	RegisterForSingleUpdate(3.0)
	
	; Avoid a while loop here because it has a chance to break
	
	If akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") > 0 && akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") != 3
		Utility.Wait(1.0)
	EndIf
	
	If akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") > 0 && akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") != 3
		Utility.Wait(0.9)
	EndIf
	
	If akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") > 0 && akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") != 3
		Utility.Wait(0.8)
	EndIf
	
	UnregisterForUpdate()
	
	If !doAnim
		If doEquip
			akAnimTarget.AddSpell(HT_HeadGearEquipped) ; Headgear shown
			
			If iTypeOfHelmet > 0 && isWearingHelmet
				ResetHiddenVariant(akAnimTarget)
			EndIf
		Else
			akAnimTarget.RemoveSpell(HT_HeadGearEquipped) ; Headgear hidden
			
			If !doSkip
				If fHairSlot
					ResetHiddenVariant(akAnimTarget)
				Else
					ApplyHiddenVariant(akAnimTarget)
				EndIf	
			EndIf
		EndIf
	Else
		If htMCM.HT_EnableCooldown.GetValue()
			HT_CooldownSpell.Cast(akAnimTarget)
		EndIf
	EndIf
	SyncSfsHeadgearFitting(akAnimTarget)
EndFunction

Int Function GetSfsHeadgearSlotMask()
	; The entries remaining in this array are exactly the forms HT2 will route
	; through HT_HiddenHelmet*, HT_HiddenCirclet*, or HT_HiddenMask*.  Keep the
	; SFS transition limited to those real HT2 controller slots for this follower.
	If fArrayEquippedHeadgear == None
		Return 0
	EndIf

	Int iSlotMask = 0
	If fArrayEquippedHeadgear.Length > 0 && fArrayEquippedHeadgear[0]
		iSlotMask = Math.LogicalOr(iSlotMask, 0x00000001) ; 30 - Head
	EndIf
	If fArrayEquippedHeadgear.Length > 1 && fArrayEquippedHeadgear[1]
		iSlotMask = Math.LogicalOr(iSlotMask, 0x00000002) ; 31 - Hair
	EndIf
	If fArrayEquippedHeadgear.Length > 2 && fArrayEquippedHeadgear[2]
		iSlotMask = Math.LogicalOr(iSlotMask, 0x00001000) ; 42 - Circlet
	EndIf
	If fArrayEquippedHeadgear.Length > 3 && fArrayEquippedHeadgear[3]
		iSlotMask = Math.LogicalOr(iSlotMask, 0x00004000) ; 44 - Beard / mask
	EndIf
	Return iSlotMask
EndFunction

Function SyncSfsHeadgearFitting(Actor akAnimTarget)
	If akAnimTarget == None
		Return
	EndIf

	If akAnimTarget.HasSpell(HT_HeadGearEquipped)
		; A shown transition is authoritative even when HT2 has already replaced
		; its array.  Clear only this actor's temporary HT2 suppression state.
		SkyrimFittingSystemNative.RestoreHeadgearToggleFittingSlots(akAnimTarget)
	Else
		SkyrimFittingSystemNative.SuppressHeadgearToggleFittingSlots(akAnimTarget, GetSfsHeadgearSlotMask())
	EndIf
EndFunction

Function ApplyHiddenVariant(Actor akAnimTarget)
	Int iArrayToApply = 4
					
	While iArrayToApply
		iArrayToApply -= 1
		
		If fArrayEquippedHeadgear[iArrayToApply] 
			If iArrayToApply == 3
				ApplyVariant(akAnimTarget, "HT_HiddenMaskPlayer")
			ElseIf iArrayToApply == 2
				ApplyVariant(akAnimTarget, "HT_HiddenCircletPlayer")
			ElseIf akAnimTarget.HasKeyword(HT_ApplyWorkaround) || htMCM.HT_EnableUnequip.GetValue()
				ApplyVariant(akAnimTarget, "HT_HiddenHelmetWorkaround")
			Else
				If iArrayToApply == 0
					ApplyVariant(akAnimTarget, "HT_HiddenHelmetPlayer")
				Else
					ApplyVariant(akAnimTarget, "HT_HiddenHelmetHairOnlyPlayer")
				EndIf
			EndIf
		EndIf
	EndWhile
EndFunction

Function ResetHiddenVariant(Actor akAnimTarget)
	Int iArrayToReset = 4

	While iArrayToReset
		iArrayToReset -= 1
		
		If fArrayEquippedHeadgear[iArrayToReset] 
			ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayToReset])
						
			If iArrayToReset == 3
				Utility.wait(0.2)
			EndIf
		EndIf
	EndWhile
EndFunction
