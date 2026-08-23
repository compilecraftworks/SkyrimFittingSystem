Scriptname JsonUtil

Function SetIntValue(String fileName, String key, Int value) Global Native
Function SetFloatValue(String fileName, String key, Float value) Global Native
Function SetStringValue(String fileName, String key, String value) Global Native
Function Unload(String fileName, Bool saveChanges = True, Bool minify = False) Global Native
Function Load(String fileName) Global Native
Bool Function IsGood(String fileName) Global Native
Bool Function HasIntValue(String fileName, String key) Global Native
Bool Function HasFloatValue(String fileName, String key) Global Native
Bool Function HasStringValue(String fileName, String key) Global Native
Int Function GetIntValue(String fileName, String key, Int missing = 0) Global Native
Float Function GetFloatValue(String fileName, String key, Float missing = 0.0) Global Native
String Function GetStringValue(String fileName, String key, String missing = "") Global Native
String Function GetErrors(String fileName) Global Native
