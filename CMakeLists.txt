cmake_minimum_required(VERSION 3.20)
project(expanded_quick_select LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CTest)

add_library(expanded_quick_select_core STATIC src/banks.cpp src/slot_store.cpp src/sessions.cpp)
target_include_directories(expanded_quick_select_core PUBLIC include)

if(BUILD_TESTING)
    add_executable(test_smoke tests/test_smoke.cpp)
    target_include_directories(test_smoke PRIVATE include)
    add_test(NAME smoke COMMAND test_smoke)

    add_executable(test_banks tests/test_banks.cpp)
    target_link_libraries(test_banks PRIVATE expanded_quick_select_core)
    add_test(NAME banks COMMAND test_banks)

    add_executable(test_slot_store tests/test_slot_store.cpp)
    target_link_libraries(test_slot_store PRIVATE expanded_quick_select_core)
    add_test(NAME slot_store COMMAND test_slot_store)

    add_executable(test_sessions tests/test_sessions.cpp)
    target_link_libraries(test_sessions PRIVATE expanded_quick_select_core)
    add_test(NAME sessions COMMAND test_sessions)
endif()

option(BUILD_OPENRAC_MOD "Build the native OpenRAC plugin (requires Mod SDK 1.11+)" ON)
if(BUILD_OPENRAC_MOD)
    if(NOT OpenRACMod_DIR)
        if(DEFINED OPENRAC_SDK)
            set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
        elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock")
            file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" lock)
            string(JSON sdk_version GET "${lock}" version)
            set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
        endif()
    endif()
    # CMake package version is 1.0; manifest/API require runtime 1.12+.
    find_package(OpenRACMod 1.0 REQUIRED)
    openrac_add_mod(expanded_quick_select
        MANIFEST openrac-mod.json
        SOURCES src/mod.cpp)
    target_link_libraries(expanded_quick_select PRIVATE expanded_quick_select_core)

    # openrac_add_mod copies only the plugin, manifest, README and LICENSE.
    # The SDK packager validates the manifest's "touch" assets in the
    # package directory, so copy our touch JSON/SVGs on every build (even
    # when only an icon changes and the native library is already up to date).
    add_custom_target(expanded_quick_select_touch_assets ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/touch"
            "${CMAKE_BINARY_DIR}/package/expanded_quick_select/touch"
        DEPENDS expanded_quick_select
        COMMENT "Adding Quick Select touch buttons to the mod package"
        VERBATIM)
endif()
