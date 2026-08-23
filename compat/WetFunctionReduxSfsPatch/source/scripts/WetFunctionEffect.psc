ScriptName WetFunctionEffect Extends ActiveMagicEffect

;;; variabels

;; externals
WetFunctionMCM Property cfg Auto
MagicEffect Property effect Auto
Spell Property ability Auto
FormList Property Furniture10 Auto

;; internal generic states (slow)
Actor player
Actor act
Bool fem
Bool isPlayer
String partNameHead
String textureHeadDefault = ""
String textureHandDefault = ""
String textureFeetDefault = ""
String textureBodyDefault = ""
String textureSchlongDefault = ""
String textureDiffuseDefault = ""
String Property actName Hidden
	String Function get()
    Return act.GetLeveledActorBase().GetName()
  EndFunction
EndProperty

;; core state (fast, also present on actor as StorageUtil floats)
; current status
Float wetnessRate = 0.0 ; absence indicates that the effect should stop itself
Float specular    = 1.0 ; absence indicates that the  effect did not start up yet
Float glossiness  = 1.0

; status variable (remains on actor after effect removal)
Float Property wetness Hidden
	Float Function get()
		Return acGetFloat("wetness", 0.0)
	EndFunction
	Function set(Float value)
		acSetFloat("wetness", value)
	EndFunction
EndProperty
; config variables (may exist, remain on actor after effect removal)
Float wetnessForce    = -1.0
Float specularForce   =  0.0
Float glossinessForce =  0.0

;; internal core state
Bool hasDrops = false
Bool hasSweat = false
Bool hasPussy = false ; not wheter the subject is female but rather its wetness due to arousal
Float wetnessMaxDyn = 0.0
Float wetnessDryDyn = 0.0
Float recentStrength = 0.0

; wetness generation var
Float lastGameTime = 0.0
Float lastStamina = 1.0
Float lastMagicka = 1.0
Float furnitureMult = 0.0
Float ZadMult = 0.0

;;; config access helpers
Bool  Function ecGetBool(string storeKey)
	Return ecGetInt(storeKey) as bool
EndFunction
Int   Function ecGetInt(string storeKey)
	Return StorageUtil.GetIntValue(cfg, storeKey)
EndFunction
Float Function ecGetFloat(string storeKey)
	Return StorageUtil.GetFloatValue(cfg, storeKey)
EndFunction
Bool Function SFSShouldOperateSlot(Int slot)
	If slot == 0 || act == None
		Return False
	EndIf
	If !SkyrimFittingSystemNative.IsRealEquipmentHiddenForActorSlots(act, slot)
		Return False
	EndIf
	Return Math.LogicalAnd(SkyrimFittingSystemNative.GetDisplayedFittingSlotMask(act), slot) == 0
EndFunction

;;; actor config
string storePrefix = "WetFunction_Actor_"
Float Function acGetFloat(string storeKey, float default=0.0)
	Return StorageUtil.GetFloatValue(act, storePrefix+storeKey, default)
EndFunction
Function acSetFloat(string storeKey, float value)
	StorageUtil.SetFloatValue(act, storePrefix+storeKey, value)
EndFunction
Bool Function acHasFloat(string storeKey)
	Return StorageUtil.HasFloatValue(act, storePrefix+storeKey)
EndFunction
Function acDelFloat(string storeKey)
	StorageUtil.UnsetFloatValue(act, storePrefix+storeKey)
EndFunction

;;; general state handling
Event OnEffectStart(Actor target, Actor caster)
	player = Game.GetPlayer()
	act = target
	isPlayer = act == player
	effect = Self.GetBaseObject()

	; make sure the config has been loaded
	cfg.ecStartup()

	; wrong startup
	If ShouldStop()
		Log("irregular startup - self remove", -1)
		Shutdown()
		Return
	Else
		acSetFloat("wetnessRate", 0.0)
	EndIf

	; init wetness vars
	lastGameTime = Utility.GetCurrentGameTime()
	lastStamina = act.GetActorValuePercentage("Stamina")
	lastMagicka = act.GetActorValuePercentage("Magicka")

	; log exact statup circumstances
	If acHasFloat("wetness")
		If acHasFloat("specular")
			If acHasFloat("glossiness")
				Log("vanished effect restarted by itself", -1)
			Else
				Log("vanished effect restarted by external", -1)
			EndIf
		Else
			Log("regular statup (again)", 0)
		EndIf
	Else
		Log("regular startup (first time)", 0)
		lastGameTime -= ecGetFloat("autoBonusHours")
		wetness = PapyrusUtil.ClampFloat(ecGetFloat("autoBonusNormal") + ecGetFloat("autoBonusRandom") * Utility.RandomFloat(), 0.0, ecGetFloat("wetnessCap"))
	EndIf

	;; events
	If isPlayer ; watch sex change of player
		RegisterForMenu("RaceSex Menu")
	EndIf

		; register for zad events
	RegisterForModEvent("DeviceVibrateEffectStart", "OnZadVibrate")
	RegisterForModEvent("DeviceVibrateEffectStop",  "OnZadVibrate")
	RegisterForModEvent("DeviceActorOrgasm", "OnZadBump")
	RegisterForModEvent("DeviceEdgedActor",  "OnZadBump")

	; finalze
	InitActor()

	; go to correct inital state
	GoToState("Dry")

	; and start
	OnUpdate()
EndEvent

Event OnUpdate()
	If !ShouldStop()
		; update steps
		UpdateWetness() ; reads forceWetness
		UpdateState()
		UpdateVisuals() ; reads forceSpecular/Glossiness
	EndIf
	If ShouldStop()
		Log("request for shutdown", -1)
		Shutdown()
	Else
		; register for new update
		float delay = 10.0
		If wetnessForce >= 0.0
			delay = ecGetFloat("loopTimeForced")
		ElseIf isPlayer
			delay = ecGetFloat("loopTimePc")
		Else
			delay = ecGetFloat("loopTimeNpc")
		EndIf
		RegisterForSingleUpdate(delay)
	EndIf
EndEvent

Event OnEffectFinish(Actor target, Actor caster)
	bool first = true
	While ecGetBool("zombieRevive") && !ShouldStop() && act.HasSpell(ability) && !act.HasMagicEffect(effect)
		If first
			Log("effect stopped, spell present, not reuested - attempt self revival", -1)
			first = false
		Else
			Log("attempt self revival", -4)
		EndIf
		act.AddSpell(ability)
		Utility.Wait(3.0)
	EndWhile
	If act.HasSpell(ability)
		If acHasFloat("wetnessRate")
			If act.HasMagicEffect(effect)
				Log("self revival successful", -1)
			Else
				Log("self revival failed", -1)
			EndIf
		Else
			Log("effect stopped, spell present, stop requested - removing spell", -1)
			act.RemoveSpell(ability)
		EndIf
	Else
		Log("effect stopped, spell missing - all ok", 0)
		If !(acHasFloat("wetnessRate") && acHasFloat("autoStop") && acGetFloat("wetness", 0.0)>0.0 && ecGetBool("autoKeepWetness"))
			acDelFloat("wetness")
		EndIf
		acDelFloat("wetnessRate")
		acDelFloat("specular")
		acDelFloat("glossiness")
		acDelFloat("sexLabRate")
		UpdateVisuals()
	EndIf
EndEvent

Bool Function ShouldStop()
	If !acHasFloat("wetnessRate")
		Return True
	EndIf
	If acHasFloat("autoStop")
		If acGetFloat("autoStop")<Utility.GetCurrentGameTime() || !act.Is3DLoaded()
			Return True
		EndIf
		If !isPLayer
			Cell playerCell = player.GetParentCell()
			Cell actorCell  = act.GetParentCell()

			If actorCell != playerCell
				If actorCell && actorCell.IsInterior() || playerCell && playerCell.IsInterior()
					Return True ; in different cell
				Else
					If act.GetDistance(player) > (500.0 + 1.1 * ecGetFloat("autoRange"))
						Return True ; far away
					EndIf
				EndIf
			EndIf
		EndIf
	EndIf
	Return False
EndFunction
Function Shutdown()
	GoToState("Shutdown")
	act.RemoveSpell(ability)
EndFunction

;;; generic state stuff
Event OnMenuClose(string menu)
	If menu == "RaceSex Menu"
		While Utility.IsInMenuMode()
			Utility.wait(0.1)
		EndWhile
		If isPlayer
			InitActor()
		EndIf
	EndIf
EndEvent

Function InitActor()
	; init character sex
	fem = act.GetActorBase().GetSex() as bool
	Log("is: "+Bool2AB(fem, "female", "male"), -2)

	; init head part
	ActorBase ab = act.GetActorBase()
	int n = ab.GetNumHeadParts()
	int i = n
	String tex = ""
	While i > 0
		i -= 1
		partNameHead = ab.GetNthHeadPart(i).GetPartName()
		If StringUtil.Find(partNameHead, "Head") >= 0
			Log("found head [" + partNameHead + "] in " + n + " headparts", -2)
			tex = NiOverride.GetNodePropertyString(act, false, partNameHead, nifTextureKey, nifTextureIndex)
			If tex && StringUtil.Find(tex, "\\WetFunction\\")>=0
				tex = NiOverride.GetNodePropertyString(act, true, partNameHead, nifTextureKey, nifTextureIndex)
			EndIf
			If tex && StringUtil.Find(tex, "\\WetFunction\\")>=0
				tex = ""
			EndIf
			If tex
				i = -2
			Else
				partNameHead = ""
			EndIf
		EndIf
	EndWhile

	; head default texture
	If partNameHead
		textureHeadDefault = tex
		Log("found head default texture:" + textureHeadDefault, -2)
	EndIf
EndFunction

;;; wetness updating
Float Function GetWeatherWetness(Weather w)
	int type = w.GetClassification()
	If act.IsInInterior()
		type = -1
	EndIf
	If type == 0
		Return ecGetFloat("weatherPleasant")
	ElseIf type == 1
		Return ecGetFloat("weatherCloudy")
	ElseIf type == 2
		If isPlayer && cfg.Frostfall && FrostUtil.IsPlayerTakingShelter()
			Return ecGetFloat("frostfallShelterRain")
		Else
			Return ecGetFloat("weatherRainy")
		EndIf
	ElseIf type == 3
		If isPlayer && cfg.Frostfall && FrostUtil.IsPlayerTakingShelter()
			Return ecGetFloat("frostfallShelterSnow")
		Else
			Return ecGetFloat("weatherSnow")
		EndIf
	Else
		Return ecGetFloat("weatherNone")
	EndIf
EndFunction
Function UpdateWetness()
	;; check time passage
	float curGameTime = Utility.GetCurrentGameTime()
	float hoursPassed = (curGameTime - lastGameTime) * 24.0
	lastGameTime = curGameTime

	;; direct setting
	; forced
	wetnessForce = acGetFloat("wetnessForce", ecGetFloat("wetnessForce"))
	If wetnessForce >= 0.0
		wetness = wetnessForce
		Return
	EndIf

	; swimming
	If act.IsSwimming()
		wetness = ecGetFloat("wetnessCap")
		recentStrength = 1.0 ; dont trigger sweat adding just because we are in the water again.
		Return ;we are at the max
	EndIf

	;; rate settings
	; temporary variable for collection
	float wetnessPoll = 0.0

	; stamina
	float curStamina = act.GetActorValuePercentage("Stamina")
	If curStamina < lastStamina
		wetnessPoll += ((lastStamina - curStamina) * ecGetFloat("generateStamina"))
	EndIF
	lastStamina = curStamina

	; magicka
	float curMagicka = act.GetActorValuePercentage("Magicka")
	If curMagicka < lastMagicka
		wetnessPoll += ((lastMagicka - curMagicka) * ecGetFloat("generateMagicka"))
	EndIF
	lastMagicka = curMagicka

	; turn this into a rate
	If hoursPassed  ; avoid 0div
		wetnessRate = wetnessPoll / hoursPassed
	Else
		wetnessRate = 0.0
	EndIf

	; movement
	If act.IsSprinting()
		If act.IsOnMount()
			wetnessRate += ecGetFloat("generateGallop")
		Else
			wetnessRate += ecGetFloat("generateSprinting")
		EndIf
	ElseIf act.IsRunning()
		wetnessRate += ecGetFloat("generateRunning")
	ElseIf act.IsSneaking()
		wetnessRate += ecGetFloat("generateSneaking")
	EndIf

	; furniture
	wetnessRate += (furnitureMult * ecGetFloat("generateWorking"))

	; sexlab
	wetnessRate += acGetFloat("sexLabRate", 0.0)

	; zad
	wetnessRate += (ZadMult * ecGetFloat("zadVibrate"))

	; frostfall
	If isPlayer && cfg.Frostfall && FrostUtil.IsPlayerNearFire()
		wetnessRate += ((FrostUtil.GetPlayerHeatSourceLevel() as Float) * ecGetFloat("frostfallFire"))
	EndIf

	; skooma whore
	If isPlayer && cfg.SLSWai
		wetnessRate += ( \
			0.01 * ( \
				cfg.SLSWp.GetValue() * ecGetFloat("skoomaPhysical") + \
				cfg.SLSWm.GetValue() * ecGetFloat("skoomaMental") + \
				cfg.SLSWmk.GetValue() * ecGetFloat("skoomaMagical") \
			) + \
			(cfg.SLSWai.GetValue() * ecGetFloat("skoomaAddicted")) \
		)
	EndIf

	; weather
	float t = Weather.GetCurrentWeatherTransition()
	wetnessRate += (t * GetWeatherWetness(Weather.GetCurrentWeather()))
	If t < 1.0
		wetnessRate += ((1.0-t) * GetWeatherWetness(Weather.GetOutgoingWeather()))
	EndIf

	; arousal
	int arousal = -3
	If cfg.SLAfac
		arousal = act.GetFactionRank(cfg.SLAfac)
		If arousal >= 0
			wetnessRate += (arousal * ecGetFloat("generateArousal") * 0.01)
			hasPussy = (arousal > ecGetFloat("arousedThreshold"))
		EndIf
	EndIf

	; non-player bonus
	If !isPlayer
		wetnessRate += ecGetFloat("otherAdd")
		wetnessRate *= ecGetFloat("otherMult")
	EndIf

	; multiply
	wetnessRate *= ecGetFloat("multGlobal")

	; update values
	If acHasFloat("wetnessRate")
		acSetFloat("wetnessRate", wetnessRate)
	EndIf
	wetness = PapyrusUtil.ClampFloat(wetness + (wetnessRate * hoursPassed), 0.0, ecGetFloat("wetnessCap"))

	;;; log all the infos
	Log("Update wetness: state:"+GetState()+" wetness:"+wetness+" rate:"+wetnessRate+" arousal:"+arousal+" hours passed:"+hoursPassed+" strength:"+GetEffectStrength()+" drops:"+hasDrops+" sweat:"+hasSweat+" pussy:"+hasPussy, -3)
EndFunction

;;; furniture stuff
Event OnSit(ObjectReference furnitureObject)
	If Furniture10.HasForm(furnitureObject.GetBaseObject())
		furnitureMult = 1.0
	EndIf
EndEvent
Event OnGetUp(ObjectReference furniture)
	furnitureMult = 0.0
EndEvent

;;; zad stuff
Event OnZadVibrate(string e, string actorName, float strength, form s)
	If actorName != actName
		Return
	EndIf
	If e == "DeviceVibrateEffectStart"
		If !isPlayer && ecGetBool("zadPlayerOnly")
			Return
		EndIf
		ZadMult = strength
	Else
		ZadMult = 0.0
	EndIf
EndEvent
Event OnZadBump(string e, string actorName, float n, form s)
	If !isPlayer && ecGetBool("zadPlayerOnly")
		Return
	EndIf
	If actorName != actName
		Return
	EndIf
	If e == "DeviceActorOrgasm"
		wetness += ecGetFloat("zadOrgasm")
	ElseIf e == "DeviceEdgedActor"
		wetness += ecGetFloat("zadEdge")
	EndIf
EndEvent

;;; state calculation
Float Function GetEffectStrength()
	Return (wetness - wetnessDryDyn) / (ecGetFloat("wetnessCap") - ecGetFloat("wetnessDry"))
EndFunction

Function UpdateState()
	GoToState("Dry")
EndFunction

State Dry
	Event OnBeginState()
		hasDrops = false
		hasSweat = false
	EndEvent
	Function UpdateState()
		If wetness > ecGetFloat("wetnessSoaked")
			GoToState("Soaked")
		ElseIf wetness > ecGetFloat("wetnessStart")
			GoToState("Sweat")
		EndIf
	EndFunction
	Float Function GetEffectStrength()
		Return 0.0
	EndFunction
EndState

State Sweat
	Event OnBeginState()
		hasSweat = true
		wetnessMaxDyn = ecGetFloat("wetnessStart")
		wetnessDryDyn = wetnessMaxDyn
	EndEvent
	Function UpdateState()
		If wetness > ecGetFloat("wetnessSoaked")
			GoToState("Soaked")
			Return
		EndIf
		If wetness > wetnessMaxDyn
			Float wetnessDry   = ecGetFloat("wetnessDry")
			Float wetnessStart = ecGetFloat("wetnessStart")
			wetnessMaxDyn = wetness
			wetnessDryDyn = wetnessDry - (wetnessDry - wetnessStart) * (1.0 - (wetness - wetnessStart) / (ecGetFloat("wetnessSoaked") - wetnessStart))
			Log("Updated dynamic limit: wetnessDryDyn:"+wetnessDryDyn+" wetnessMaxDyn:"+wetnessMaxDyn, -4)
		EndIf
		If wetness <= wetnessDryDyn
			GoToState("Dry")
		EndIf
	EndFunction
EndState

State Soaked
	Event OnBeginState()
		hasDrops = true
		wetnessDryDyn = ecGetFloat("wetnessDry")
		If !hasSweat
			recentStrength = GetEffectStrength()
		EndIf
	EndEvent
	Function UpdateState()
		If wetness <= wetnessDryDyn
			GoToState("Dry")
			Return
		EndIf
		If !hasSweat
			float strength = GetEffectStrength()
			If strength < recentStrength
				recentStrength = strength
			ElseIf strength + ecGetFloat("lateSweat") > recentStrength
				Log("late sweating started", -2)
				hasSweat = true
			EndIf
		EndIf
	EndFunction
EndState

State Shutdown
	Event OnUpdate()
	EndEvent
EndState

;;; visual update
bool firstPerson   = false
bool alwaysOperate = false

int nifTextureKey    = 9
int nifTextureIndex  = 7
int nifSpecularKey   = 3
int nifGlossinessKey = 2
int nifDiffuseIndex  = 0

bool Function texEnabled(string storeKey, bool autoMale=false)
	If autoMale && !fem
		storeKey = "Male" + storeKey
	EndIf
	storeKey = "texture" + storeKey
	Return (StorageUtil.GetIntValue(cfg, "wf_aux_" + storeKey) as bool) && ecGetBool(storeKey)
EndFunction

Function UpdateVisuals(bool force=false)
	If !force && Utility.IsInMenuMode()
		Log("Menu is open: don't update visuals", -4)
		Return
	EndIf

	;; textures
	firstPerson   = ecGetBool("firstPersonToo")
	alwaysOperate = ecGetBool("alwaysOperate")
	bool useDrops
	bool useSweat
	bool usePussy

	; body
	useDrops = hasDrops && ecGetBool("textureBodyDrops")
	useSweat = hasSweat && ecGetBool("textureBodySweat")
	usePussy = hasPussy && ecGetBool("textureBodyPussy")
	String textureBody = GetBodyTexture(useDrops, useSweat, usePussy)

	; feet
	useDrops = hasDrops && ecGetBool("textureFeetDrops")
	useSweat = hasSweat && ecGetBool("textureFeetSweat")
	usePussy = usePussy && (useDrops || useSweat) ; try to get the same texture twice
	String textureFeet = GetBodyTexture(useDrops, useSweat, usePussy)

	; rest
	String textureHand =    Bool2AB((hasSweat || hasDrops) && texEnabled("Hand", true), GetTexture("wethand_s")   , "")
	String textureHead =    Bool2AB((hasSweat || hasDrops) && texEnabled("Head", true), GetTexture("wethead_s")   , "")
	String textureSchlong = Bool2AB(!fem && hasPussy && texEnabled("MaleSchlong")     , GetTexture("wetschlong_s"), "")
	String textureDiffuse = Bool2AB(!fem && hasDrops && texEnabled("MaleBodyDiffuse") , GetTexture("wet_d")       , "")

	;; floats
	specular   = acGetFloat("specularForce"  , ecGetFloat("specularForce"))
	glossiness = acGetFloat("glossinessForce", ecGetFloat("glossinessForce"))
	Float strength = GetEffectStrength()

	; get the values (if they are not forced)
	If specular <= 0.0
		specular = ecGetFloat("specularMin") + (ecGetFloat("specularMax") - ecGetFloat("specularMin")) * strength
	EndIf
	If glossiness <= 0.0
		glossiness = ecGetFloat("glossinessMin") + (ecGetFloat("glossinessMax") - ecGetFloat("glossinessMin")) * strength
	EndIf

	;; now update overrides
	Bool specularBody = ecGetBool("specularBody")
	Bool glossinessBody = ecGetBool("glossinessBody")

	textureSchlongDefault = UpdateSN(textureSchlongDefault, textureSchlong, specularBody             , glossinessBody             , slot=0x400000, skipTex=textureBody)
	textureHandDefault    = UpdateSN(textureHandDefault   , textureHand   , ecGetBool("specularHand"), ecGetBool("glossinessHand"), slot=0x08, skipTex=textureBody)
	textureFeetDefault    = UpdateSN(textureFeetDefault   , textureFeet   , ecGetBool("specularFeet"), ecGetBool("glossinessFeet"), slot=0x80, skipTex=textureBody)
	textureBodyDefault    = UpdateSN(textureBodyDefault   , textureBody   , specularBody             , glossinessBody             , slot=0x04)
	textureHeadDefault    = UpdateSN(textureHeadDefault   , textureHead   , ecGetBool("specularHead"), ecGetBool("glossinessHead"), node=partNameHead, skipTex=textureBody, regenHead=True)

	; update used values
	acSetFloat("specular"  , specular)
	acSetFloat("glossiness", glossiness)
EndFunction

String Function UpdateSN(string defaultTex, string requestedTex, bool specularEnabled, bool glossinessEnabled, string skipTex="", bool diffuse=false, int slot=0, string node="", bool regenHead=False)
	int nifIndex = nifTextureIndex
	If diffuse
		nifIndex = nifDiffuseIndex
	EndIf
	;; check current texture
	string currentTex = ""
	If slot
		currentTex = NiOverride.GetSkinPropertyString(act, false, slot, nifTextureKey, nifTextureIndex)
	Else
		currentTex = NiOverride.GetNodePropertyString(act, false, node, nifTextureKey, nifTextureIndex)
	EndIf
	; doesn't even have skin and we are allowed to skip
	If currentTex=="" && !alwaysOperate && !SFSShouldOperateSlot(slot)
		Return defaultTex
	EndIf
	; we got the default - update it
	If StringUtil.Find(currentTex, "\\WetFunction\\")<0
		If defaultTex != currentTex
			Log("Updated default texture for "+LabelSN(slot, node)+" to ["+currentTex+"] from ["+defaultTex+"]", -2)
			defaultTex = currentTex
		EndIf
	Else
		; no override requested - override with default (because NiOverride reset is broken)
		If requestedTex == ""
			requestedTex = defaultTex
		EndIf
	EndIf
	Bool updateTex = requestedTex && (((requestedTex!=currentTex) && (skipTex!=currentTex) && StringUtil.Find(requestedTex, currentTex)<0 && StringUtil.Find(skipTex, currentTex)<0) || currentTex=="")
	If updateTex
		Log("Updated texture for "+LabelSN(slot, node)+" to ["+requestedTex+"] from ["+currentTex+"]", -4)
		If slot
			NiOverride.AddSkinOverrideString(act, fem, false, slot, nifTextureKey, nifIndex, requestedTex, false)
			If firstPerson
				NiOverride.AddSkinOverrideString(act, fem, true, slot, nifTextureKey, nifIndex, requestedTex, false)
			EndIf
		ElseIf node
			NiOverride.AddNodeOverrideString(act, fem, node, nifTextureKey, nifTextureIndex, requestedTex, false)
		EndIf
	EndIf
	If specularEnabled
		UpdateSNFloat(specular, nifSpecularKey, slot, node)
	EndIf
	If glossinessEnabled
		UpdateSNFloat(glossiness, nifGlossinessKey, slot, node)
	EndIf
	If regenHead && updateTex && isPlayer
		act.RegenerateHead()
	EndIf
	Return defaultTex
EndFunction
Function UpdateSNFloat(float value, int nifKey, int slot=0, string node="")
	Float currentValue
	If slot
		currentValue = NiOverride.GetSkinPropertyFloat(act, false, slot, nifKey, -1)
	ElseIf Node
		currentValue = NiOverride.GetNodePropertyFloat(act, false, node, nifKey, -1)
	Else
		Return
	EndIf
	If currentValue!=value
		Log("Updated float for "+LabelSN(slot, node)+" of nifKey ["+nifKey+"] to ["+value+"] from ["+currentValue+"]", -4)
		If slot
			NiOverride.AddSkinOverrideFloat(act, fem, false, slot, nifKey, -1, value, false)
			If firstPerson
				NiOverride.AddSkinOverrideFloat(act, fem, true, slot, nifKey, -1, value, false)
			EndIf
		Else
			NiOverride.AddNodeOverrideFloat(act, fem,  node, nifKey, -1, value, false)
		EndIf
	EndIf
EndFunction
String Function LabelSN(int slot=0, string node="")
	If slot
		Return "slot ["+slot+"]"
	Else
		Return "node ["+node+"]"
	EndIf
EndFunction

String Function GetTexture(string name)
	Return "textures\\actors\\character\\WetFunction\\" + Bool2AB(fem, "", "male_") + name + ".dds"
EndFunction
String Function GetBodyTexture(Bool drops, Bool sweat, Bool pussy)
	If (drops || sweat || (pussy && fem)) && texEnabled("Body", true)
		Return GetTexture("wet_" + Bool210(drops) + Bool210(sweat) + Bool2AB(fem, Bool210(pussy), "") + "_s")
	Else
		Return ""
	EndIf
EndFunction

;;; some string switches
String Function Bool2AB(Bool b, String s1, String s0)
	If b
		Return s1
	Else
		Return s0
	EndIf
EndFunction
String Function Bool210(Bool b)
	Return Bool2AB(b, "1", "0")
EndFunction

;;; loggin function
Function Log(String msg, Int level = 0, Bool notify = true) ;pirorities: -2 debug, -1 info, 0 normal, 1 error, 2 critical
	string msgLong = "[WF @ "+act+": "+actName+"] "+msg
	If level >= ecGetFloat("logLevel") as int
		MiscUtil.PrintConsole(msgLong)
		If notify && level > 0
			Debug.Notification(msg)
		EndIf
	EndIf
	If level >= ecGetFloat("logFile") as int
		WetFunctionMCM.FLog(msgLong, level)
	EndIf
EndFunction
