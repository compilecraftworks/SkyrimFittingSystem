Scriptname NiOverride Hidden

Int Function GetScriptVersion() Global Native
String Function GetNodePropertyString(ObjectReference ref, Bool firstPerson, String node, Int key, Int index) Global Native
Float Function GetNodePropertyFloat(ObjectReference ref, Bool firstPerson, String node, Int key, Int index) Global Native
String Function GetSkinPropertyString(ObjectReference ref, Bool firstPerson, Int slotMask, Int key, Int index) Global Native
Float Function GetSkinPropertyFloat(ObjectReference ref, Bool firstPerson, Int slotMask, Int key, Int index) Global Native
Function AddSkinOverrideString(ObjectReference ref, Bool isFemale, Bool firstPerson, Int slotMask, Int key, Int index, String value, Bool persist) Global Native
Function AddSkinOverrideFloat(ObjectReference ref, Bool isFemale, Bool firstPerson, Int slotMask, Int key, Int index, Float value, Bool persist) Global Native
Function AddNodeOverrideString(ObjectReference ref, Bool isFemale, String node, Int key, Int index, String value, Bool persist) Global Native
Function AddNodeOverrideFloat(ObjectReference ref, Bool isFemale, String node, Int key, Int index, Float value, Bool persist) Global Native
