Scriptname ecMCM extends ski_configbase

;;;
;;; Eeasy Config for MCM
;;;

;;; constants
string ecVersion       = "ecMCM_version"
string ecRegistry      = "ecMCM_registry"
string ecPrefixType    = "ecMCM_type_"
string ecPrefixDesc    = "ecMCM_desc_"
string ecPrefixDefault = "ecMCM_default_"
string ecPrefixMin     = "ecMCM_min_"
string ecPrefixMax     = "ecMCM_max_"
string ecPrefixStep    = "ecMCM_step_"
string ecPrefixFormat  = "ecMCM_format_"
string ecState         = "ecMCM_state_"

int ecTypeBool   = 1
int ecTypeSlider = 2
int ecTypeText   = 3

bool ecCheckOk = false
bool ecCheckRender = false

bool Property ecUpdate = false Auto Hidden ; temporarily set if you want to update stuff
bool Property ecBurnIn = false Auto Hidden ; if set you may skip chunks of non-saved/dynamic settings

Function ecStartup()
	If StorageUtil.GetIntValue(Self, ecVersion, -1) < GetVersion()
		; proper locking
		If ecBurnIn
			While ecBurnIn
				Utility.Wait(3.0)
			EndWhile
			Return
		Else
			ecBurnIn = true
		EndIf
		; now init
		string lastPage = CurrentPage
		int lastPageNum = -1
		int i = Pages.length
		While i > 0
			i -= 1
			ecPage(Pages[i])
		EndWhile
		; release lock
		StorageUtil.SetIntValue(Self, ecVersion, GetVersion())
		ecBurnIn = false
		ecConfigChanged()
	EndIf
EndFunction

Int Function ecFlags(bool disabled)
	If disabled
    Return OPTION_FLAG_DISABLED
  Else
    Return OPTION_FLAG_NONE
  EndIf
EndFunction

Function ecCloseToGame()
	UI.Invoke("Journal Menu", "_root.QuestJournalFader.Menu_mc.ConfigPanelClose")
	UI.Invoke("Journal Menu", "_root.QuestJournalFader.Menu_mc.CloseMenu")
EndFunction

;;; add functions
Function ecFillMode(bool topDown=false, bool leftRight=false)
	If !ecBurnIn
		If topDown == leftRight
				topDown = true
		EndIf
		If topDown
			SetCursorFillMode(TOP_TO_BOTTOM)
		Else
			SetCursorFillMode(LEFT_TO_RIGHT)
		EndIf
	EndIf
EndFunction

Function ecCursor(int position)
	If !ecBurnIn
		SetCursorPosition(position)
	EndIf
EndFunction

Function ecEmpty(int count=1)
	If !ecBurnIn
		While count > 0
			count -= 1
			AddEmptyOption()
		EndWhile
	EndIf
EndFunction

Function ecHeader(string label, bool disabled=false)
	If !ecBurnIn
		AddHeaderOption(label, ecFlags(disabled))
	EndIf
EndFunction

Function ecToggle(string storeKey, string label, bool default, string desc="" , bool disabled=false, bool update=false, bool saved=true)
	bool value = default
	If update || ecUpdate || !StorageUtil.HasIntValue(Self, ecPrefixType+storeKey)
		If saved
			StorageUtil.StringListAdd(Self, ecRegistry, storeKey, false)
		EndIf
		StorageUtil.SetIntValue(Self, ecPrefixType+storeKey, ecTypeBool)
		StorageUtil.SetStringValue(Self, ecPrefixDesc+storeKey, desc)
	EndIf
	StorageUtil.SetIntValue(Self, ecPrefixDefault+storeKey, default as Int)
	If saved
		value = StorageUtil.GetIntValue(Self, storeKey, default as int) as bool
		StorageUtil.SetIntValue(Self, storeKey, value as int)
	EndIf
	If !ecBurnIn
		AddToggleOptionST(ecState+storeKey, label, value, ecFlags(disabled))
	EndIf
EndFunction

Function ecSlider(string storeKey, string label, float default, float min=0.0, float max=0.0, float step=0.0, string format="",int prec=1, string desc="" , bool disabled=false, bool update=false, bool saved=true)
	float value = default
	If !format
		format = "{"+prec+"}"
	EndIf
	If update || ecUpdate || !StorageUtil.HasIntValue(Self, ecPrefixType+storeKey)
		If saved
			StorageUtil.StringListAdd(Self, ecRegistry, storeKey, false)
		EndIf
		StorageUtil.SetIntValue(Self, ecPrefixType+storeKey, ecTypeSlider)
		StorageUtil.SetStringValue(Self, ecPrefixDesc+storeKey, desc)
		StorageUtil.SetFloatValue(Self, ecPrefixMin+storeKey, min)
		StorageUtil.SetFloatValue(Self, ecPrefixMax+storeKey, max)
		StorageUtil.SetFloatValue(Self, ecPrefixStep+storeKey, step)
		StorageUtil.SetStringValue(Self, ecPrefixFormat+storeKey, format)
	EndIf
	StorageUtil.SetFloatValue(Self, ecPrefixDefault+storeKey, default)
	If saved
		value = StorageUtil.GetFloatValue(Self, storeKey, default)
		StorageUtil.SetFloatValue(Self, storeKey, value)
	EndIf
	If !ecBurnIn
		AddSliderOptionST(ecState+storeKey, label, value, format, ecFlags(disabled))
	EndIf
EndFunction

Function ecText(string storeKey, string label, string default, string desc="", bool disabled=false, bool update=false, bool saved=false)
	String value = default
	If update || ecUpdate || !StorageUtil.HasIntValue(Self, ecPrefixType+storeKey)
		If saved
			StorageUtil.StringListAdd(Self, ecRegistry, storeKey, false)
		EndIf
		StorageUtil.SetIntValue(Self, ecPrefixType+storeKey, ecTypeText)
		StorageUtil.SetStringValue(Self, ecPrefixDesc+storeKey, desc)
	EndIf
	StorageUtil.SetStringValue(Self, ecPrefixDefault+storeKey, default)
	If saved
		value = StorageUtil.GetStringValue(Self, storeKey, default)
		StorageUtil.SetStringValue(Self, storeKey, value)
	EndIf
	If !ecBurnIn
		AddTextOptionST(ecState+storeKey, label, value, ecFlags(disabled))
	EndIf
EndFunction

;;; query functions
Bool  Function ecGetBool(string storeKey)
	Return ecGetInt(storeKey) as bool
EndFunction
Int   Function ecGetInt(string storeKey)
	Return StorageUtil.GetIntValue(Self, storeKey)
EndFunction
Float Function ecGetFloat(string storeKey)
	Return StorageUtil.GetFloatValue(Self, storeKey)
EndFunction

;;; events
String Function ecKey(string s = "", int extraOff=0)
	If !s
		s = GetState()
	EndIf
	int ecStateLength = StringUtil.GetLength(ecState)
	If StringUtil.Substring(s, 0, ecStateLength) == ecState
		Return StringUtil.Substring(s, ecStateLength+extraOff)
	Else
		Return ""
	EndIf
EndFunction

Event OnHighlightST()
	string storeKey = ecKey()
	If storeKey
		SetInfoText(StorageUtil.GetStringValue(Self, ecPrefixDesc+storeKey, ""))
	EndIf
EndEvent
Event OnDefaultST()
	string storeKey = ecKey()
	If !storeKey
		Return
	EndIf
	int type = StorageUtil.GetIntValue(Self, ecPrefixType+storeKey, 0)
	If type == ecTypeBool
		bool default = StorageUtil.GetIntValue(Self, ecPrefixDefault+storeKey) as bool
		StorageUtil.SetIntValue(Self, storeKey, default as int)
		SetToggleOptionValueST(default)
		ecConfigChanged()
	ElseIf type == ecTypeSlider
		float default = StorageUtil.GetFloatValue(Self, ecPrefixDefault+storeKey)
		StorageUtil.GetFloatValue(Self, storeKey, default)
		SetSliderOptionValueST(default)
		ecConfigChanged()
	EndIf
EndEvent

Event OnSelectST()
	string storeKey = ecKey()
	If !storeKey
		Return
	EndIf
	int type = StorageUtil.GetIntValue(Self, ecPrefixType+storeKey, 0)
	If type == ecTypeBool
		bool default = StorageUtil.GetIntValue(Self, ecPrefixDefault+storeKey) as bool
		bool value = StorageUtil.GetIntValue(Self, storeKey, default as int) as bool
		value = !value
		StorageUtil.SetIntValue(Self, storeKey, value as int)
		SetToggleOptionValueST(value)
		ecConfigChanged()
	EndIf
EndEvent

Event OnSliderOpenST()
	string storeKey = ecKey()
	If !storeKey
		Return
	EndIf
	int type = StorageUtil.GetIntValue(Self, ecPrefixType+storeKey, 0)
	If type == ecTypeSlider
		float default = StorageUtil.GetFloatValue(Self, ecPrefixDefault+storeKey)
		float value = StorageUtil.GetFloatValue(Self, storeKey, default)
		float min = StorageUtil.GetFloatValue(Self, ecPrefixMin+storeKey)
		float max = StorageUtil.GetFloatValue(Self, ecPrefixMax+storeKey)
		float step = StorageUtil.GetFloatValue(Self, ecPrefixStep+storeKey)
		ecSliderHook(value, default, min, max, step)
	EndIf
EndEvent
Function ecSliderHook(float value, float default, float min, float max, float step)
	float minLimit = ecSliderHookMinLimit(value, default, min, max, step)
	float maxLimit = ecSliderHookMaxLimit(value, default, min, max, step)
	ecSliderSetValues(PapyrusUtil.ClampFloat(value, minLimit, maxLimit), PapyrusUtil.ClampFloat(default, minLimit, maxLimit), PapyrusUtil.ClampFloat(min, minLimit, maxLimit), PapyrusUtil.ClampFloat(max, minLimit, maxLimit), step)
EndFunction
Float Function ecSliderHookMinLimit(float value, float default, float min, float max, float step)
		Return min
EndFunction
Float Function ecSliderHookMaxLimit(float value, float default, float min, float max, float step)
		Return max
EndFunction
Function ecSliderSetValues(float value, float default, float min, float max, float step)
	SetSliderDialogStartValue(value)
	SetSliderDialogDefaultValue(default)
	SetSliderDialogRange(min, max)
	SetSliderDialogInterval(step)
EndFunction
Event OnSliderAcceptST(float newValue)
	string storeKey = ecKey()
	If !storeKey
		Return
	EndIf
	int type = StorageUtil.GetIntValue(Self, ecPrefixType+storeKey, 0)
	If type == ecTypeSlider
		StorageUtil.SetFloatValue(Self, storeKey, newValue)
		ecSliderUpdate(newValue, storeKey)
		ecConfigChanged()
	EndIf
EndEvent
Function ecSliderUpdate(float newValue, string storeKey="")
	If !storeKey
		storeKey = ecKey()
	EndIf
	If !storeKey
		Return
	EndIf
	SetSliderOptionValueST(newValue, StorageUtil.GetStringValue(Self, ecPrefixFormat+storeKey))
EndFunction

Function ecTextUpdate(String newValue)
	SetTextOptionValueST(newValue)
EndFunction

Function ecFlagsUpdate(bool disabled=false)
	SetOptionFlagsST(ecFlags(disabled))
EndFunction

; json
Function ecExport(string fileName)
	ecFlagsUpdate(disabled=true)
	ecTextUpdate("Exporting ...")
	string[] storeKeys = StorageUtil.StringListToArray(Self, ecRegistry)
	int i = storeKeys.length
	While i > 0
		i -= 1
		String storeKey = storeKeys[i]
		int type = StorageUtil.GetIntValue(Self, ecPrefixType+storeKey, 0)
		If type == ecTypeBool
			JsonUtil.SetIntValue(fileName, storeKey, StorageUtil.GetIntValue(Self, storeKey))
		ElseIf type == ecTypeSlider
			JsonUtil.SetFloatValue(fileName, storeKey, StorageUtil.GetFloatValue(Self, storeKey))
		ElseIf type == ecTypeText
			JsonUtil.SetStringValue(fileName, storeKey, StorageUtil.GetStringValue(Self, storeKey))
		EndIf
	EndWhile

	ecTextUpdate("Saving ...")
	JsonUtil.Unload(fileName, true, false)

	ecTextUpdate("Done!")
	ecFlagsUpdate(disabled=false)
EndFunction
Function ecImport(string fileName)
	ecFlagsUpdate(disabled=true)
	ecTextUpdate("Loading ...")
	JsonUtil.Load(fileName)

	If JsonUtil.IsGood(fileName)
		ecTextUpdate("Importing ...")
		string[] storeKeys = StorageUtil.StringListToArray(Self, ecRegistry)
		int i = storeKeys.length
		int missing = 0
		While i > 0
			i -= 1
			String storeKey = storeKeys[i]
			int type = StorageUtil.GetIntValue(Self, ecPrefixType+storeKey, 0)
			If type == ecTypeBool
				If JsonUtil.HasIntValue(fileName, storeKey)
					StorageUtil.SetIntValue(Self, storeKey, JsonUtil.GetIntValue(fileName, storeKey))
				Else
					type = -1
				EndIf
			ElseIf type == ecTypeSlider
				If JsonUtil.HasFloatValue(fileName, storeKey)
					StorageUtil.SetFloatValue(Self, storeKey, JsonUtil.GetFloatValue(fileName, storeKey))
				Else
					type = -1
				EndIf
			ElseIf type == ecTypeText
				If JsonUtil.HasStringValue(fileName, storeKey)
					StorageUtil.SetStringValue(Self, storeKey, JsonUtil.GetStringValue(fileName, storeKey))
				Else
					type = -1
				EndIf
			EndIf
			If type == -1
				missing += 1
				Log("missing setting entry when loading: "+storeKey, 1, false)
			EndIf
		EndWhile
		JsonUtil.Unload(fileName, false, false)
		ecTextUpdate("Done!")
		If missing > 0
			ShowMessage("Some setting entries ("+missing+") were missing and were skipped.\nMaybe the setting imported are from and older version?\nMore information were written to the console.", false, "Dismiss")
		EndIf
		ecConfigChanged()
	Else
		string error = JsonUtil.GetErrors(fileName)
		Log("error loading settings from: "+fileName+"\n"+error, 1, false)
		ecTextUpdate("Error!")
		ShowMessage("Failed to load file. Detailed error is printed to console.", false, "Dismiss")
	EndIf
	ecFlagsUpdate(disabled=false)
EndFunction
String[] Function ecDump(Bool asc=true)
	string[] Items = StorageUtil.StringListToArray(Self, ecRegistry)
	PapyrusUtil.SortStringArray(Items, asc)
	int i = Items.length
	While i > 0
		i -= 1
		String Item = Items[i]
		int type = StorageUtil.GetIntValue(Self, ecPrefixType+Item, 0)
		If type == ecTypeBool
			Items[i] = Item+"="+StorageUtil.GetIntValue(Self, Item)
		ElseIf type == ecTypeSlider
			Items[i] = Item+"="+StorageUtil.GetFloatValue(Self, Item)
		ElseIf type == ecTypeText
			Items[i] = Item+"="+StorageUtil.GetStringValue(Self, Item)
		EndIf
	EndWhile
	Return Items
EndFunction

;;; enc checking
Bool Function ecCheckItem(String name, String needs, bool ok, bool broken=false)
	If ecCheckRender
		ecHeader(name, disabled=True)
		String val = "ok"
		If !ok
			If broken
				val = "broken"
			Else
				val = "missing"
			EndIf
		EndIf
		AddTextOption(needs, val, ecFlags(disabled=True))
	EndIf
	Return ok
EndFunction

Bool Function ecCheck()
	int skser = SKSE.GetVersionRelease()
	Bool ok = True
	ok = ecCheckItem("SKSE Plugin", "1.7.3+", skser>=48, skser!=SKSE.GetScriptVersionRelease()) && ok
	ok = ecCheckItem("PapyrusUtil", "3.2+", PapyrusUtil.GetVersion()>=32) && ok
	Return ok
EndFunction

Event OnGameReload()
	ecCheckOk = False
	Parent.OnGameReload()
EndEvent

Function ecCheckPage()
	ecFillMode(leftRight=True)
	ecHeader("Requirements")
	ecEmpty(3)
	ecCheckRender = True
	ecCheck()
	ecCheckRender = False
EndFunction

Event OnPageReset(string page) ; no override
	If !ecCheckOk
		ecCheckOk = ecCheck()
	EndIf
	If ecCheckOk
		ecPage(page)
	Else
		ecCheckPage()
	EndIf
EndEvent

Event ecPage(string page) ; to be overridden
EndEvent

Event ecConfigChanged()
EndEvent

;;; empty logging function
Function Log(String msg, Int level = 0, bool notify=true) ;pirorities: -2 debug, -1 info, 0 normal, 1 error, 2 critical
EndFunction
