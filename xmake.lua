-- set minimum xmake version
set_xmakever("3.1.0")

set_config("skse_xbyak", true)
set_config("skyrim_vr", false)

-- Pinned vendored CommonLibSSE-NG v6.7.0 for all normal and release builds.
-- SFS carries a narrow SE/AE TESObjectREFR vtable-layout correction; see
-- third_party/CommonLibSSE-NG/SFS_LOCAL_PATCHES.md.
includes("third_party/CommonLibSSE-NG")

-- Keep this fallback aligned with VERSION. Release scripts pass the VERSION
-- value through SFS_BUILD_VERSION; the literal also supports direct xmake use.
local build_version = os.getenv("SFS_BUILD_VERSION") or "1.6.3"
local build_version_string = os.getenv("SFS_BUILD_VERSION_STRING") or build_version
local major, minor, patch = build_version:match("^(%d+)%.(%d+)%.(%d+)$")
if not major then
    error("SFS_BUILD_VERSION must be in major.minor.patch format, got " .. build_version)
end

set_project("SkyrimFittingSystem")
set_version(build_version)
set_license("GPL-3.0")

set_languages("c++23")
set_warnings("allextra")

set_policy("package.requires_lock", true)

add_rules("mode.release", "mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_requires("nlohmann_json v3.12.0")

target("SkyrimFittingSystem")
    set_version(build_version)
    set_encodings("utf-8")
    set_basename("SFSCore")

    -- Always put the runtime DLL and local debugging PDB on the canonical
    -- build path even if a developer previously configured a separate PoC
    -- object directory. Packaging and MO2 deployment intentionally omit PDBs.
    set_targetdir("build/v" .. build_version .. "/windows/x64/$(mode)")

    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")

    add_rules("commonlibsse-ng.plugin", {
        name = "Skyrim Fitting System",
        author = "PenguinToast",
        description = "SKSE64 plugin using CommonLibSSE-NG and Dear ImGui"
    })

    add_defines("SFS_VERSION_MAJOR=" .. major)
    add_defines("SFS_VERSION_MINOR=" .. minor)
    add_defines("SFS_VERSION_PATCH=" .. patch)
    add_defines('SFS_VERSION_STRING="' .. build_version_string .. '"')

    add_files("src/**.cpp")
    add_files(
        "lib/imgui/imgui.cpp",
        "lib/imgui/imgui_draw.cpp",
        "lib/imgui/imgui_tables.cpp",
        "lib/imgui/imgui_widgets.cpp",
        "lib/imgui/backends/imgui_impl_dx11.cpp",
        "lib/imgui/backends/imgui_impl_win32.cpp"
    )
    add_headerfiles("src/**.h")
    add_includedirs("src", "lib/imgui", "lib/imgui/backends")
    add_syslinks("d3d11", "dxgi", "d3dcompiler", "windowscodecs", "ole32")
    set_pcxxheader("src/pch.h")

target("KitGeneratorLogicTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")
    add_defines("SFS_INTEGRATED_KIT_GENERATOR_TEST=1")
    add_files(
        "tests/KitGeneratorLogicTests.cpp",
        "src/kit_generator/Localization.cpp",
        "src/ui/Localization.cpp"
    )
    add_includedirs("src", "lib/imgui", "lib/imgui/backends")
    set_pcxxheader("src/pch.h")

target("RuntimeLayoutTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_deps("commonlibsse-ng")
    add_files("tests/RuntimeLayoutTests.cpp")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

target("BodyFamilyLogicTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files(
        "tests/BodyFamilyLogicTests.cpp",
        "src/catalog/BodyFamilyRules.cpp"
    )
    add_includedirs("src")

target("ConditionCnfLogicTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/ConditionCnfLogicTests.cpp")
    add_includedirs("src")

target("ConditionDropLogicTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/ConditionDropLogicTests.cpp")
    add_includedirs("src")

target("ConditionFormTokenTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/ConditionFormTokenTests.cpp", "src/conditions/FormTokens.cpp")
    add_includedirs("tests/condition_stubs", "src")

target("ConditionDropdownTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/ConditionDropdownTests.cpp", "src/ui/components/EditableCombo.cpp",
        "lib/imgui/imgui.cpp", "lib/imgui/imgui_draw.cpp",
        "lib/imgui/imgui_tables.cpp", "lib/imgui/imgui_widgets.cpp")
    add_includedirs("tests/condition_ui_stubs", "src", "lib/imgui")

target("ConditionStringLifetimeTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/ConditionStringLifetimeTests.cpp")
    add_includedirs("tests/condition_string_stubs", "src")

target("ConditionValueParsingTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/ConditionValueParsingTests.cpp", "src/conditions/ValueParsing.cpp")
    add_includedirs("tests/condition_value_stubs", "src")

target("FittingDyeRulesTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files(
        "tests/FittingDyeRulesTests.cpp",
        "src/native/FittingDyeRules.cpp"
    )
    add_includedirs("src")

target("KitListNavigationTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/KitListNavigationTests.cpp")
    add_includedirs("src")

target("CoreBehaviorRegressionTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/CoreBehaviorRegressionTests.cpp")
    add_includedirs("src")

target("CustomSkinningRegressionTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_packages("xbyak")
    add_files("tests/CustomSkinningRegressionTests.cpp")
    add_includedirs("src", "build/.gens/custom-skinning-tests")
    before_build(function (target)
        os.execv("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
            "tests/Generate-CustomSkinningTestSource.ps1", "-OutputDirectory",
            "build/.gens/custom-skinning-tests"})
    end)

target("IntegrationInitializationTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/IntegrationInitializationTests.cpp")
    add_includedirs("src", "build/.gens/integration-init-tests")
    before_build(function (target)
        os.execv("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
            "tests/Generate-CustomSkinningTestSource.ps1", "-OutputDirectory",
            "build/.gens/integration-init-tests"})
    end)

target("RaceMenuInterfaceTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/RaceMenuInterfaceTests.cpp",
              "tests/RaceMenuInterfaceCalls.cpp",
              "src/native/RaceMenuActorUpdateManager.cpp")
    add_includedirs("src")
    if is_plat("windows") then
        add_cxxflags("/EHsc")
    end

target("RaceMenuMorphTrackingTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "/tests")
    add_files("tests/RaceMenuMorphTrackingTests.cpp")
    add_includedirs("src", "build/.gens/morph-tracking-tests")
    before_build(function (target)
        os.execv("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
            "tests/Generate-MorphTrackingTestSource.ps1", "-Output",
            "build/.gens/morph-tracking-tests/RaceMenuMorphTracking.production.inc"})
    end)

-- The private personal-completion experiment is intentionally not part of the
-- public GPL source archive. Keep its optional local targets available for the
-- maintainer without emitting missing-file warnings for public builders.
if os.isfile("private/personal_kit_completion/PersonalKitCompletion.cpp") then
target("SkyrimFittingSystemPersonal")
    set_default(false)
    set_version(build_version)
    set_encodings("utf-8")
    set_basename("SFSCore")
    set_targetdir("build/v" .. build_version .. "-personal/windows/x64/$(mode)")

    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")

    add_rules("commonlibsse-ng.plugin", {
        name = "Skyrim Fitting System - Personal Kit Completion",
        author = "PenguinToast",
        description = "Private SFS build with personal kit candidate completion"
    })

    add_defines("SFS_VERSION_MAJOR=" .. major)
    add_defines("SFS_VERSION_MINOR=" .. minor)
    add_defines("SFS_VERSION_PATCH=" .. patch)
    add_defines('SFS_VERSION_STRING="' .. build_version_string .. '-personal"')
    add_defines("SFS_PERSONAL_KIT_COMPLETION=1")

    add_files("src/**.cpp")
    add_files("private/personal_kit_completion/PersonalKitCompletion.cpp")
    add_files(
        "lib/imgui/imgui.cpp",
        "lib/imgui/imgui_draw.cpp",
        "lib/imgui/imgui_tables.cpp",
        "lib/imgui/imgui_widgets.cpp",
        "lib/imgui/backends/imgui_impl_dx11.cpp",
        "lib/imgui/backends/imgui_impl_win32.cpp"
    )
    add_headerfiles("src/**.h")
    add_headerfiles("private/personal_kit_completion/**.h")
    add_includedirs("src", "private/personal_kit_completion", "lib/imgui", "lib/imgui/backends")
    add_syslinks("d3d11", "dxgi", "d3dcompiler", "windowscodecs", "ole32")
    set_pcxxheader("src/pch.h")
end

target("SFSDynamicFootprintsPatch")
    set_default(false)
    set_version(build_version)
    set_encodings("utf-8")
    set_basename("SFS_DynamicFootprintsPatch")
    set_targetdir("build/v" .. build_version .. "/compat/dynamic-footprints/windows/x64/$(mode)")

    add_deps("commonlibsse-ng")
    add_rules("commonlibsse-ng.plugin", {
        name = "SFS Dynamic Footprints Compatibility Patch",
        author = "PenguinToast",
        description = "Dynamic Footprints displayed footwear bridge for Skyrim Fitting System"
    })

    add_files("compat/DynamicFootprintsSfsPatch/src/**.cpp")
    add_headerfiles("compat/DynamicFootprintsSfsPatch/src/**.h")
    add_includedirs("compat/DynamicFootprintsSfsPatch/src", "src")
    add_syslinks("bcrypt")
    set_pcxxheader("src/pch.h")

if os.isfile("private/personal_kit_completion/PersonalKitCompletionTests.cpp") then
target("PersonalKitCompletionTests")
    set_default(false)
    set_kind("binary")
    set_encodings("utf-8")
    set_targetdir("build/v" .. build_version .. "-personal/tests")
    add_files(
        "private/personal_kit_completion/PersonalKitCompletion.cpp",
        "private/personal_kit_completion/PersonalKitCompletionTests.cpp"
    )
    add_includedirs("src", "private/personal_kit_completion")
end
