cmake_minimum_required(VERSION 3.21)
project(img_convert LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Use the static MSVC runtime so the DLL has minimal runtime dependencies
# when bundled with the C# single-file publish.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W3 /utf-8 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# vcpkg-provided dependencies. Most use CONFIG-mode find_package because
# vcpkg's static-triplet ports are more reliably discoverable through
# their shipped *Config.cmake files than CMake's legacy Find*.cmake.
#
# JPEG is the awkward one: vcpkg's libjpeg-turbo ships a CMake config
# file but Ubuntu's libjpeg-turbo only supports CMake's legacy FindJPEG
# module. Try CONFIG first, fall back to MODULE.
find_package(libjpeg-turbo CONFIG QUIET)
if(libjpeg-turbo_FOUND)
    if(TARGET libjpeg-turbo::jpeg-static)
        set(IC_JPEG_TARGET libjpeg-turbo::jpeg-static)
    elseif(TARGET libjpeg-turbo::jpeg)
        set(IC_JPEG_TARGET libjpeg-turbo::jpeg)
    elseif(TARGET libjpeg-turbo::turbojpeg-static)
        set(IC_JPEG_TARGET libjpeg-turbo::turbojpeg-static)
    elseif(TARGET libjpeg-turbo::turbojpeg)
        set(IC_JPEG_TARGET libjpeg-turbo::turbojpeg)
    endif()
endif()
if(NOT IC_JPEG_TARGET)
    find_package(JPEG REQUIRED)
    set(IC_JPEG_TARGET JPEG::JPEG)
endif()
message(STATUS "img_convert: linking JPEG via ${IC_JPEG_TARGET}")

find_package(PNG           REQUIRED)
find_package(TIFF          REQUIRED)
find_package(WebP    CONFIG REQUIRED)
find_package(libheif CONFIG REQUIRED)

# libheif's exported target name varies: vcpkg's port exports
# 'libheif::heif', Ubuntu's package exports plain 'heif'.
if(TARGET libheif::heif)
    set(IC_HEIF_TARGET libheif::heif)
elseif(TARGET heif)
    set(IC_HEIF_TARGET heif)
else()
    message(FATAL_ERROR "libheif found but no expected target was exported.")
endif()
message(STATUS "img_convert: linking libheif via ${IC_HEIF_TARGET}")

add_library(img_convert SHARED
    src/img_convert.cpp
    src/image.cpp
    src/codec_jpg.cpp
    src/codec_png.cpp
    src/codec_webp.cpp
    src/codec_tiff.cpp
    src/codec_heif.cpp
)

target_include_directories(img_convert PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(img_convert PRIVATE IMG_CONVERT_BUILD_DLL=1)

target_link_libraries(img_convert PRIVATE
    ${IC_JPEG_TARGET}
    PNG::PNG
    TIFF::TIFF
    WebP::webp
    ${IC_HEIF_TARGET}
)

set_target_properties(img_convert PROPERTIES
    OUTPUT_NAME  "img_convert"
    PREFIX       ""
)

# Place the built DLL where the C# project expects it (../app/runtimes/win-x64/native/).
add_custom_command(TARGET img_convert POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_SOURCE_DIR}/../app/runtimes/win-x64/native"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:img_convert>"
        "${CMAKE_SOURCE_DIR}/../app/runtimes/win-x64/native/"
)
