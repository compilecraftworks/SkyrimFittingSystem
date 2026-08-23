#pragma once

#ifndef SFS_VERSION_MAJOR
#error "SFS_VERSION_MAJOR must be provided by the build system"
#endif

#ifndef SFS_VERSION_MINOR
#error "SFS_VERSION_MINOR must be provided by the build system"
#endif

#ifndef SFS_VERSION_PATCH
#error "SFS_VERSION_PATCH must be provided by the build system"
#endif

#ifndef SFS_VERSION_STRING
#error "SFS_VERSION_STRING must be provided by the build system"
#endif

namespace Plugin {
inline constexpr auto NAME = "Skyrim Fitting System"sv;
inline constexpr REL::Version VERSION{SFS_VERSION_MAJOR, SFS_VERSION_MINOR,
                                      SFS_VERSION_PATCH, 0};
inline constexpr std::string_view VERSION_STRING{SFS_VERSION_STRING};
inline constexpr auto AUTHOR = "PenguinToast"sv;
} // namespace Plugin
