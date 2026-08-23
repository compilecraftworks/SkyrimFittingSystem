Scriptname HT_PlayerAlias extends ReferenceAlias  

Import PO3_Events_Alias
Import PO3_SKSEFunctions
Import DynamicArmor
Import SeasonsOfSkyrim

HT_MCM Property htMCM Auto
Actor Property PlayerRef Auto

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

Spell Property HT_CooldownSpell Auto
MagicEffect Property HT_MonitorEffect Auto
MagicEffect Property HT_CooldownEffect Auto

FormList Property HT_SafeLocations Auto
FormList Property HT_UnsafeLocations Auto
FormList Property HT_DustyWeather Auto

GlobalVariable Property HT_HelmetState Auto

Bool inDialogue
Bool isVisor
Bool isIEAInstalled
Bool isHHInstalled
Bool isSOInstalled
Int iTypeOfHelmet = -1
Armor[] fArrayEquippedHeadgear
Float fRightHand
Float fLeftHand
Bool isFirstPerson
Bool isWearingHelmet

Event OnInit()
	LoadSettings()
EndEvent

Event OnPlayerLoadGame()
	LoadSettings()
	SendModEvent("ManageConditions")
EndEvent

Function LoadSettings()
	Utility.Wait(0.2)
	RegisterDialogue(htMCM.HT_EnableDialogue.GetValue())
	fArrayEquippedHeadgear = New Armor[4]
	iTypeOfHelmet = -1
	isVisor = False
	RegisterForModEvent("ManageConditions", "OnManageConditions")
	Utility.Wait(0.1)
	RegisterForModEvent("ManageConditions", "OnManageConditions")
	isIEAInstalled = Game.IsPluginInstalled("Immersive Equipping Animations.esp")
	isHHInstalled = Game.IsPluginInstalled("HelmHair.esp")
	isSOInstalled = Game.IsPluginInstalled("ScribeOverlays.esp")

	If htMCM.HT_EnableFollowers.GetValue()
		htMCM.HT_EnableFollowers.SetValue(0)
		Utility.Wait(0.3)
		htMCM.HT_EnableFollowers.SetValue(1)
	EndIf
	
	If isSOInstalled
		Self.RegisterForCameraState()
	Else
		Self.UnregisterForCameraState()
	EndIf
EndFunction

Event OnManageConditions(string eventName, string strArg, float numArg, Form sender)
	iTypeOfHelmet = -1
	SendModEvent("ManageFollowerHelmet", numArg = 2.0)
	CheckConditions()
EndEvent

; In dialogue
Event OnMenuOpen(string menuName)
	If menuName == "Dialogue Menu"
		; Won't unequip helmet with weapons drawn or in combat/vampire state
		If PlayerRef.IsWeaponDrawn() || HasActiveMagicEffect(PlayerRef, HT_MonitorEffect)
			Return
		EndIf
		
		If !CheckHotkey()
			inDialogue = True
			ManageHelmet(PlayerRef, False)
		EndIf
	EndIf
EndEvent

Event OnMenuClose(string menuName)
	If menuName == "Dialogue Menu"
		CheckConditions()
		inDialogue = False
	EndIf
EndEvent

; Change of locations
Event OnLocationChange(Location akOldLoc, Location akNewLoc)
	CheckConditions()
EndEvent

; Check helmets equipped
Event OnObjectEquipped(Form akBaseObject, ObjectReference akReference)
	If akBaseObject
		If akBaseObject.HasKeyword(ArmorHelmet) || akBaseObject.HasKeyword(ClothingHead) || akBaseObject.HasKeyword(ClothingBody) || akBaseObject.HasKeyword(ClothingCirclet) || akBaseObject.HasKeyword(HT_ArmorHelmetAlt) \
		|| akBaseObject.HasKeyword(HT_ArmorHood) || akBaseObject.HasKeyword(HT_ArmorMask)
			iTypeOfHelmet = -1
			isVisor = False
			
			If isIEAInstalled
				Utility.Wait(0.4)
			EndIf
						
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
	PlayerRef.SetAnimationVariableInt("iGPMAAnimationType", 0)
EndEvent

Function OnPlayerCameraState(Int oldState, Int newState)
	If HT_HelmetState.GetValue() > 0
		If iTypeOfHelmet >= 0
			Bool isHelmetEquipped
			Bool doUnequip
			Int iArrayIndex = 3
			
			While iArrayIndex
				iArrayIndex -= 1
				If fArrayEquippedHeadgear[iArrayIndex] 
					If PlayerRef.IsEquipped(fArrayEquippedHeadgear[iArrayIndex])
						isHelmetEquipped = True
					EndIf			
				EndIf
			EndWhile
			
			If isHelmetEquipped
				If iTypeOfHelmet == 1 && htMCM.HT_EnableHelmet.GetValue()
					doUnequip = True
				ElseIf iTypeOfHelmet == 2 && htMCM.HT_EnableHood.GetValue()
					doUnequip = True
				ElseIf iTypeOfHelmet == 3 && htMCM.HT_EnableMask.GetValue()
					doUnequip = True
				ElseIf iTypeOfHelmet == 4 && htMCM.HT_EnableHat.GetValue()
					doUnequip = True
				ElseIf iTypeOfHelmet == 5 && htMCM.HT_EnableMask.GetValue()
					doUnequip = True
				EndIf
			EndIf
			
			If doUnequip
				CheckModCompatibiliy(False, fArrayEquippedHeadgear[iArrayIndex])
			EndIf
		EndIf
	EndIf	
EndFunction

; Function responsible to check the hotkey state
Bool Function CheckHotkey()
	If htMCM.HT_HotkeyType.GetValue() == 1
		ManageHelmet(PlayerRef, htMCM.HT_HotkeyState.GetValue())
		Return True
	ElseIf htMCM.HT_HotkeyType.GetValue() == 2
		If htMCM.HT_HotkeyState.GetValue() != 0
			If htMCM.HT_HotkeyState.GetValue() == 1
				ManageHelmet(PlayerRef, True)
			Else
				ManageHelmet(PlayerRef, False)
			EndIf
			
			Return True
		EndIf	
	EndIf
	
	Return False
EndFunction

; Function responsible to check the conditions to equip or not the helmet/hood
Function CheckConditions()
	If CheckHotkey()
		Return
	EndIf
		
	; Checks for combat, cold region or vampire conditions (spell)
	If HasActiveMagicEffect(PlayerRef, HT_MonitorEffect)
		ManageHelmet(PlayerRef, True)
		Return
	EndIf
	
	Bool doEquip = True ; To equip or not equip, that's the question
	Bool isSafeLocation
	Bool isBadWeather
	
	; Checks for safe locations
	If htMCM.HT_EnableLocation.GetValue()
		Location akCurrentLoc = PlayerRef.GetCurrentLocation()
		
		If akCurrentLoc && (PlayerRef.IsInInterior() || !htMCM.HT_EnableInteriors.GetValue())
			If FindLocationKeyword(akCurrentLoc)
				doEquip = False
				isSafeLocation = True
			EndIf
		EndIf
	Else
		doEquip = False
		isSafeLocation = True
	EndIf
	
	If !doEquip && !PlayerRef.IsInInterior()  
		; Checks for bad weathers
		If htMCM.HT_EnableWeather.GetValue()
			Utility.Wait(0.5)
			Weather akWeather = Weather.GetCurrentWeather()
			
			If htMCM.HT_EnableRain.GetValue() && GetWeatherType(akWeather) == 2
				doEquip =  True
			ElseIf htMCM.HT_EnableSnow.GetValue() && GetWeatherType(akWeather) == 3
				doEquip =  True
			ElseIf htMCM.HT_EnableDusty.GetValue() && HT_DustyWeather.HasForm(akWeather)
				doEquip =  True
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
			EndIf
		EndIf
	EndIf
	
	ManageHelmet(PlayerRef, doEquip)
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

; Registers dialogue menu
Function RegisterDialogue(Bool doEnable)
	If doEnable
		RegisterForMenu("Dialogue Menu")	
	Else
		UnregisterForMenu("Dialogue Menu")	
		inDialogue = False
	EndIf
EndFunction

; Function responsible to equip or not the helmets/hoods and play the animations.
Function ManageHelmet(Actor akAnimTarget, Bool doEquip = True)
	Bool isChecking
	
	If doEquip ; Equip headgear
		If HT_HelmetState.GetValue() == 0 ; headgear is already equipped on player
			isChecking = True
		Endif
	Else ; Unequip headgear
		If HT_HelmetState.GetValue() > 0 ; headgear is already unequipped on player
			isChecking = True
		Endif
	EndIf
	
	Bool doSkip = True
	Bool doAnim = True
	Int iArrayIndex
	
	If iTypeOfHelmet < 0
		fArrayEquippedHeadgear = New Armor [5]
		fArrayEquippedHeadgear[0] = akAnimTarget.GetWornForm(0x00000001) as Armor ; Slot 30 - Head
		fArrayEquippedHeadgear[1] = akAnimTarget.GetWornForm(0x00000002) as Armor ; Slot 31 - Hair
		fArrayEquippedHeadgear[2] = akAnimTarget.GetWornForm(0x00001000) as Armor ; Slot 42 - Circlet -- Not "circlet" circlets, just here as a failsafe to get helmets that are only using the circlet slot
		doAnim = False
		
		If htMCM.HT_EnableMask.GetValue()
			fArrayEquippedHeadgear[3] = akAnimTarget.GetWornForm(0x00004000) as Armor ; Slot 44 - Beard -- Used for face masks
			fArrayEquippedHeadgear[4] = akAnimTarget.GetWornForm(0x02000000) as Armor ; Slot 55 - Face? -- Used for some masks
			
			If fArrayEquippedHeadgear[4] && (!fArrayEquippedHeadgear[4].HasKeyword(HT_ArmorMask) || fArrayEquippedHeadgear[4].HasKeyword(HT_IgnoreHeadgear))
				ResetVariant(akAnimTarget, fArrayEquippedHeadgear[4])
				fArrayEquippedHeadgear[4] = None
			EndIf
		Else
			fArrayEquippedHeadgear[3] = None
			fArrayEquippedHeadgear[4] = None
		EndIf
		
		Int iArrayMatch
		iArrayIndex = 3
		
		While iArrayIndex
			iArrayIndex -= 1
			
			If fArrayEquippedHeadgear[iArrayIndex] 
				iArrayMatch = fArrayEquippedHeadgear.Find(fArrayEquippedHeadgear[iArrayIndex])
				
				If !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorHelmetAlt) && !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorVisor)
					If fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_IgnoreHeadgear) || fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ClothingCirclet) ; Ignores circlets as well
						ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayIndex])
						Utility.Wait(0.1)
						fArrayEquippedHeadgear[iArrayIndex] = None
					ElseIf !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ArmorHelmet) && !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorMask) \
					&& !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(HT_ArmorHood) && !fArrayEquippedHeadgear[iArrayIndex].HasKeyword(ClothingHead) ; Ignores everything that doesn't have one of those keywords
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
			doAnim = True
		ElseIf fArrayEquippedHeadgear[3] && fArrayEquippedHeadgear[3].HasKeyword(HT_ArmorMask) && !fArrayEquippedHeadgear[3].HasKeyword(HT_IgnoreHeadgear)
			iTypeOfHelmet = 3 ; Mask
			doAnim = True
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
			
			If fArrayEquippedHeadgear[4]
				doAnim = True
				iTypeOfHelmet = 3 ; Mask
			EndIf
			
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

	If isChecking
		SyncSfsHeadgearFitting(akAnimTarget)
	EndIf
		
	If iTypeOfHelmet > 0 && doAnim && !Game.IsPluginInstalled("SkyrimVR.esm") && (akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") == 0 || akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") == 3)
		isFirstPerson = akAnimTarget.GetAnimationVariableInt("i1stPerson") as Bool
	
		If (htMCM.HT_SkipAnimation.GetValue() == 1 && isFirstPerson) || htMCM.HT_SkipAnimation.GetValue() == 2 || HasActiveMagicEffect(PlayerRef, HT_CooldownEffect)
			doAnim = False
		ElseIf akAnimTarget.GetAnimationVariableBool("bInJumpState") || akAnimTarget.GetAnimationVariableInt("SkyparkourOngoing") == 1 || akAnimTarget.GetSleepState()
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
				Bool isWeaponOut

				If doEquip ; Equipping helmet animation
					If akAnimTarget.IsWeaponDrawn()
						fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", isFirstPerson)
						fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", isFirstPerson)
						
						If fRightHand == 0.01
							fRightHand = 1.00
						EndIf
						If fLeftHand == 0.01
							fLeftHand = 1.00
						EndIf
						
						NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, isFirstPerson)
						isWeaponOut = True
					EndIf
					
					If isVisor || inDialogue
						If akAnimTarget.GetSitState() == 0 && !isFirstPerson
							akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 1)
						EndIf
					EndIf

					If isVisor
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 1)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
						Utility.Wait(0.95)
					Else
						If !inDialogue
							akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 1)
							Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
							Utility.Wait(1.0)
						Else
							akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 4)
							Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
							Utility.Wait(0.7)
						EndIf

						If isWeaponOut
							NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, isFirstPerson)
						EndIf
						
						Utility.Wait(1.0)
					EndIf
					
					HT_HelmetState.SetValue(0)
					ResetHiddenVariant(akAnimTarget)
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
					akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)
					
					If !isVisor
						If iArrayIndex >= 0
							CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
						EndIf
						
						Utility.Wait(0.6)
					EndIf

					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
				Else ; Unequipping helmet animation
					If akAnimTarget.IsWeaponDrawn()
						fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", isFirstPerson)
						fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", isFirstPerson)
						
						If fRightHand == 0.01
							fRightHand = 1.00
						EndIf
						If fLeftHand == 0.01
							fLeftHand = 1.00
						EndIf
						
						NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, isFirstPerson)
						isWeaponOut = True
					EndIf

					If isVisor
						If akAnimTarget.GetSitState() == 0 && !isFirstPerson
							akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 1)
						EndIf

						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 2)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
						Utility.Wait(0.95)
						ApplyVariant(akAnimTarget, "HT_RaisedVisorPlayer")
						SuppressSfsHeadgearFitting(akAnimTarget)
					Else
						If !inDialogue
							akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 2)
						Else
							akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 3)
						EndIf
						
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
						
						If isWeaponOut
							NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, isFirstPerson)
						EndIf
						
						Utility.Wait(0.7)
						ApplyHiddenVariant(akAnimTarget)
					EndIf
					
					HT_HelmetState.SetValue(1)
					
					If isVisor
						Utility.Wait(0.15)
						Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
						akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)
						akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)						
					Else
						If iArrayIndex >= 0 
							CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
						EndIf
						
						Utility.Wait(1.15)
						
						If isWeaponOut
							NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, isFirstPerson)
						EndIf
						
						Utility.Wait(0.70)
						
						If !inDialogue
							Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
							akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)
							akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
						EndIf
					EndIf
				EndIf
					
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, isFirstPerson)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, isFirstPerson)
				EndIf
			ElseIf iTypeOfHelmet == 2 ; is Hood (with or without Face mask)
				Bool isWeaponOut
				
				If akAnimTarget.IsWeaponDrawn()
					fRightHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Weapon", isFirstPerson)
					fLeftHand = NetImmerse.GetNodeScale(akAnimTarget as ObjectReference, "Shield", isFirstPerson)

					If fRightHand == 0.01
						fRightHand = 1.00
					EndIf
					If fLeftHand == 0.01
						fLeftHand = 1.00
					EndIf
					
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", 0.0100000, isFirstPerson)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", 0.0100000, isFirstPerson)
					isWeaponOut = True
				EndIf
				
				akAnimTarget.SetAnimationVariableInt("iGPMAOffsetType", 0)
				Float fCount
				
				If doEquip ; Equipping hood animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 5)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(1.2)
					ResetHiddenVariant(akAnimTarget)
					HT_HelmetState.SetValue(0)
					
					If iArrayIndex >= 0
						CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
					EndIf
						
					Utility.Wait(0.8)
				Else ; Unequipping hood animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 6)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(1.4)						
					ApplyHiddenVariant(akAnimTarget)
					HT_HelmetState.SetValue(1)
					
					If iArrayIndex >= 0
						CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
					EndIf
									
					Utility.Wait(0.6 - fCount)
				EndIf  

				akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
				Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
					
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, isFirstPerson)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, isFirstPerson)
				EndIf
			ElseIf iTypeOfHelmet == 3 ; is Mask
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
					HT_HelmetState.SetValue(0)
					
					If iArrayIndex >= 0
						CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
					EndIf
					
					ResetHiddenVariant(akAnimTarget)
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)					
					Utility.Wait(0.5)
				Else 
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 8)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(0.8)
					ApplyHiddenVariant(akAnimTarget)
					HT_HelmetState.SetValue(1)
					
					If iArrayIndex >= 0
						CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
					EndIf
				
					Utility.Wait(1.9)
				EndIf 
				
				akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
				Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
				
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, False)
				EndIf
			ElseIf iTypeOfHelmet == 4 || iTypeOfHelmet == 6; is Hat
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
				
				If doEquip ; Equipping hat animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 9)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(1.8)
					ResetHiddenVariant(akAnimTarget)
					HT_HelmetState.SetValue(0)
				Else ; Unequipping hat animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 10)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(2.0)
					ApplyHiddenVariant(akAnimTarget)
					HT_HelmetState.SetValue(1)
				EndIf

				If iArrayIndex >= 0
					CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
				EndIf
				
				akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 0)
				Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMAStop")
					
				If isWeaponOut
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Weapon", fRightHand, False)
					NetImmerse.SetNodeScale(akAnimTarget as ObjectReference, "Shield", fLeftHand, False)
				EndIf
			ElseIf iTypeOfHelmet == 5 ; is Face mask
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
					RestoreSfsHeadgearFitting(akAnimTarget)
					
					HT_HelmetState.SetValue(0)
				Else ; Unequipping face mask animation
					akAnimTarget.SetAnimationVariableInt("iGPMAAnimationType", 12)
					Debug.SendAnimationEvent(akAnimTarget, "OffsetGPMA")
					Utility.Wait(0.95)
					
					If fArrayEquippedHeadgear[3]
						ApplyVariant(akAnimTarget, "HT_HiddenMaskPlayer")
					EndIf
					SuppressSfsHeadgearFitting(akAnimTarget)
					
					HT_HelmetState.SetValue(1)
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
	
	While akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") > 0 && akAnimTarget.GetAnimationVariableInt("iGPMAAnimationType") != 3
		Utility.Wait(0.1)
	EndWhile
	
	UnregisterForUpdate()
	
	If !doAnim
		If !inDialogue || iTypeOfHelmet != 1
			If doEquip
				HT_HelmetState.SetValue(0) ; Headgear shown
				
				If iTypeOfHelmet > 0 && isWearingHelmet
					ResetHiddenVariant(akAnimTarget)
						
					If iArrayIndex >= 0
						CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
					EndIf
				EndIf
			Else
				HT_HelmetState.SetValue(1) ; Headgear hidden
				
				If !doSkip
					If isVisor
						ApplyVariant(akAnimTarget, "HT_RaisedVisorPlayer")
						SuppressSfsHeadgearFitting(akAnimTarget)
					Else
						ApplyHiddenVariant(akAnimTarget)
					EndIf
					
					If iArrayIndex >= 0
						Utility.Wait(1.1)
						CheckModCompatibiliy(doEquip, fArrayEquippedHeadgear[iArrayIndex])
					EndIf
				EndIf
			EndIf
		EndIf
	Else
		If htMCM.HT_EnableCooldown.GetValue()
			HT_CooldownSpell.Cast(PlayerRef)
		EndIf
	EndIf

	SyncSfsHeadgearFitting(akAnimTarget)
EndFunction

Int Function GetSfsHeadgearSlotMask()
	; Keep SFS scoped to only the final, HT2-managed equipment entries.  HT2
	; clears ignored, duplicate, and unsupported forms from this array before
	; it applies HT_HiddenHelmet*, HT_HiddenCirclet*, or HT_HiddenMask*.
	; This mirrors those named variants instead of assuming every toggle owns
	; the old fixed head|circlet pair.
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
	If fArrayEquippedHeadgear.Length > 4 && fArrayEquippedHeadgear[4]
		iSlotMask = Math.LogicalOr(iSlotMask, 0x02000000) ; 55 - Face / mask
	EndIf
	Return iSlotMask
EndFunction

Function SuppressSfsHeadgearFitting(Actor akAnimTarget)
	If akAnimTarget != None
		SkyrimFittingSystemNative.SuppressHeadgearToggleFittingSlots(akAnimTarget, GetSfsHeadgearSlotMask())
	EndIf
EndFunction

Function RestoreSfsHeadgearFitting(Actor akAnimTarget)
	If akAnimTarget != None
		; HT2 can clear or replace its equipped array before this shown transition.
		; Clear only this actor's temporary HT2 state so an old hidden slot cannot
		; leak into the next equipment set.
		SkyrimFittingSystemNative.RestoreHeadgearToggleFittingSlots(akAnimTarget)
	EndIf
EndFunction

Function SyncSfsHeadgearFitting(Actor akAnimTarget)
	If akAnimTarget == None
		Return
	EndIf

	If HT_HelmetState.GetValue() > 0
		SuppressSfsHeadgearFitting(akAnimTarget)
	Else
		RestoreSfsHeadgearFitting(akAnimTarget)
	EndIf
EndFunction


Function ApplyHiddenVariant(Actor akAnimTarget)
	SuppressSfsHeadgearFitting(akAnimTarget)

	Int iArrayToApply = 5
	
	While iArrayToApply
		iArrayToApply -= 1
		
		If fArrayEquippedHeadgear[iArrayToApply] 
			If iArrayToApply == 4
				ApplyVariant(akAnimTarget, "HT_HiddenMaskAltPlayer")
			ElseIf iArrayToApply == 3
				ApplyVariant(akAnimTarget, "HT_HiddenMaskPlayer")
			ElseIf iArrayToApply == 2
				ApplyVariant(akAnimTarget, "HT_HiddenCircletPlayer")
			ElseIf iArrayToApply == 1
				ApplyVariant(akAnimTarget, "HT_HiddenHelmetHairOnlyPlayer")
			Else
				ApplyVariant(akAnimTarget, "HT_HiddenHelmetPlayer")	
			EndIf
		EndIf
	EndWhile
EndFunction

Function ResetHiddenVariant(Actor akAnimTarget)
	Int iArrayToReset = 5
	
	While iArrayToReset
		iArrayToReset -= 1
		
		If fArrayEquippedHeadgear[iArrayToReset] 
			ResetVariant(akAnimTarget, fArrayEquippedHeadgear[iArrayToReset])
						
			If iArrayToReset == 3 || iArrayToReset == 4
				Utility.wait(0.2)
			EndIf
		EndIf
	EndWhile

	RestoreSfsHeadgearFitting(akAnimTarget)
EndFunction

Function CheckModCompatibiliy(Bool doEquip, Armor fEquippedHeadgear)
	; Helm Hair workaround that sends the OnObjectEquipped/OnObjectUnequipped events when the headgear is hidden or displayed by this mod
	If isHHInstalled ; Checks if Helm Hair is installed
		Quest HH_Quest = Game.GetFormFromFile(0x000800, "HelmHair.esp") as Quest
		HelmHair_Alias HelmHairAlias = HH_Quest.GetNthAlias(0) as HelmHair_Alias
	
		If doEquip
			HelmHairAlias.OnObjectEquipped(fEquippedHeadgear, None)
		Else
			HelmHairAlias.OnObjectUnequipped(fEquippedHeadgear, None)
		EndIf
	EndIf
	
	; Helmet Overlay
	If isSOInstalled && PlayerRef.GetAnimationVariableInt("i1stPerson") == 1; Checks if HelmetOverlay is installed
		ScribeHelmet_Widget SHWidget = (Game.GetFormFromFile(0x000D63, "ScribeOverlays.esp") as Quest) as ScribeHelmet_Widget
		SHWidget.Visible = doEquip
	EndIf
EndFunction
