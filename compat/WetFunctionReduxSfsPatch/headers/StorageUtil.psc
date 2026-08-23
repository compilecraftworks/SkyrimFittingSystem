Scriptname StorageUtil

Int Function GetIntValue(Form obj, String key, Int missing = 0) Global Native
Function SetIntValue(Form obj, String key, Int value) Global Native
Bool Function HasIntValue(Form obj, String key) Global Native
Float Function GetFloatValue(Form obj, String key, Float missing = 0.0) Global Native
Function SetFloatValue(Form obj, String key, Float value) Global Native
Bool Function HasFloatValue(Form obj, String key) Global Native
Function UnsetFloatValue(Form obj, String key) Global Native
Int Function CountFloatValuePrefix(String prefix) Global Native
Function ClearFloatValuePrefix(String prefix) Global Native
String Function GetStringValue(Form obj, String key, String missing = "") Global Native
Function SetStringValue(Form obj, String key, String value) Global Native
Int Function StringListAdd(Form obj, String key, String value, Bool allowDuplicate = True) Global Native
String[] Function StringListToArray(Form obj, String key) Global Native
