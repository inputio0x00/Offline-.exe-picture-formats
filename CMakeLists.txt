cmake_minimum_required(VERSION 3.21)
project(OfflineImageConverter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OIC_BUILD_TESTS "Build smoke tests" ON)

add_library(oic_core
    src/Format.cpp
    src/ConversionJob.cpp
)
target_include_directories(oic_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(WIN32)
    add_executable(OfflineImageConverter WIN32
        src/main_win32.cpp
        src/WicImageConverter.cpp
    )
    target_link_libraries(OfflineImageConverter PRIVATE oic_core windowscodecs ole32 shell32 comctl32 shlwapi uxtheme)
    target_compile_definitions(OfflineImageConverter PRIVATE UNICODE _UNICODE NOMINMAX)

    if(MSVC)
        set_property(TARGET OfflineImageConverter PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
else()
    add_executable(oic_smoke src/smoke_main.cpp)
    target_link_libraries(oic_smoke PRIVATE oic_core)
endif()

if(OIC_BUILD_TESTS)
    enable_testing()
    add_executable(oic_core_tests tests/core_tests.cpp)
    target_link_libraries(oic_core_tests PRIVATE oic_core)
    add_test(NAME oic_core_tests COMMAND oic_core_tests)
endif()

include(CPack)
set(CPACK_PACKAGE_NAME "OfflineImageConverter")
set(CPACK_PACKAGE_VENDOR "Offline Image Converter")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Offline batch image format converter")
