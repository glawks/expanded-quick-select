cmake_minimum_required(VERSION 3.20)
project(expanded_quick_select LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CTest)

add_library(expanded_quick_select_core STATIC src/banks.cpp src/slot_store.cpp)
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
endif()

option(BUILD_OPENRAC_MOD "Build the native OpenRAC plugin (requires Mod SDK 1.11+)" ON)
if(BUILD_OPENRAC_MOD)
    find_package(OpenRACMod 1.11 REQUIRED)
    openrac_add_mod(expanded_quick_select
        MANIFEST openrac-mod.json
        SOURCES src/mod.cpp)
    target_link_libraries(expanded_quick_select PRIVATE expanded_quick_select_core)
endif()
