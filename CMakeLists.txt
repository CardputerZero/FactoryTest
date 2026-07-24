#
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.23)

project(factory_test VERSION 0.2.8 LANGUAGES C CXX)

string(TIMESTAMP FACTORY_TEST_BUILD_TIME "%Y-%m-%d %H:%M:%S")
set(FACTORY_TEST_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${FACTORY_TEST_GENERATED_INCLUDE_DIR}/version.h"
    @ONLY
)

add_library(factory_test_version INTERFACE)
target_include_directories(factory_test_version INTERFACE
    "${FACTORY_TEST_GENERATED_INCLUDE_DIR}"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(USE_DESKTOP ON CACHE BOOL "Build the desktop emulator target with LVGL SDL support")
set(APP_NAME "${PROJECT_NAME}" CACHE STRING "Application name used for installed asset lookup")
set(APP_ASSETS_ROOT "" CACHE PATH "Optional runtime assets root. Expected subdirectories include fonts/")
set(APP_IPERF_HOST "192.168.10.187" CACHE STRING "Default iperf server hostname or IP address")
set(APP_KEY_INPUT_DEVICE "" CACHE STRING "Optional Linux evdev keyboard/keypad device path, e.g. /dev/input/event0")
set(APP_FRAMEBUFFER_DEVICE "/dev/fb0" CACHE STRING "Linux framebuffer device path used by embedded builds")
set(APP_DRM_DEVICE "/dev/dri/card1" CACHE STRING "Linux DRM device path used when APP_USE_DRM=ON")
set(APP_DRM_CONNECTOR_ID "-1" CACHE STRING "DRM connector id used when APP_USE_DRM=ON; -1 auto-selects")
set(APP_USE_DRM OFF CACHE BOOL "Enable LVGL Linux DRM/KMS backend for embedded Linux builds")
set(CM0_ALLOW_FETCH_DEPS ON CACHE BOOL "Allow downloading missing CM0 BSP/dependencies during cross builds")
set(APP_USE_LIBCAMERA 0)
set(APP_USE_LIBNM 0)
set(APP_USE_BLUEZ 0)
set(APP_USE_LIBUDEV 0)
set(APP_USE_MINIAUDIO 0)
set(APP_USE_LIBIPERF 0)
set(APP_USE_LIRC 0)
set(APP_USE_LIBGPIOD 0)
set(APP_USE_LIBSERIALPORT 0)
set(APP_USE_LIBYAML 0)
set(APP_USE_LIBCJSON 0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json" FORCE)

include(FetchContent)
find_package(Threads REQUIRED)

set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${PROJECT_SOURCE_DIR}/src/config/lv_conf.h" CACHE PATH "" FORCE)

FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.5.0
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(lvgl)
if(NOT lvgl_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(lvgl)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()
add_subdirectory("${lvgl_SOURCE_DIR}" "${lvgl_BINARY_DIR}" EXCLUDE_FROM_ALL)
set(LVGL_SOURCE_DIR "${lvgl_SOURCE_DIR}")
get_filename_component(LVGL_INCLUDE_PARENT "${LVGL_SOURCE_DIR}" DIRECTORY)

set(PLATFORM_INCLUDE_DIR)
set(PLATFORM_LIBS)
set(SERIALIZATION_INCLUDE_DIR)
set(SERIALIZATION_LIBS)
set(LVGL_INCLUDE_DIR)
set(LVGL_LIBS)

if(USE_DESKTOP)
    message(STATUS "Configuring DESKTOP build")

    find_package(PNG CONFIG QUIET)
    if(NOT TARGET PNG::PNG)
        find_package(PNG REQUIRED)
    endif()

    if(WIN32)
        find_package(libjpeg-turbo CONFIG QUIET)
        if(TARGET libjpeg-turbo::libjpeg-turbo)
            set(JPEG_TARGET libjpeg-turbo::libjpeg-turbo)
        endif()
    else()
        find_package(JPEG CONFIG QUIET)
        if(NOT TARGET JPEG::JPEG)
            find_package(JPEG REQUIRED)
        endif()
        if(TARGET JPEG::JPEG)
            set(JPEG_TARGET JPEG::JPEG)
        endif()
    endif()

    find_package(Freetype CONFIG QUIET)
    if(NOT TARGET Freetype::Freetype)
        find_package(Freetype REQUIRED)
    endif()

    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        set(SDL2_TARGET SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(SDL2_TARGET SDL2::SDL2-static)
    else()
        find_package(SDL2 REQUIRED)
        if(DEFINED SDL2_LIBRARIES)
            set(SDL2_TARGET "")
        endif()
    endif()

    find_package(fmt CONFIG REQUIRED)

    find_package(ZLIB CONFIG QUIET)
    if(NOT TARGET ZLIB::ZLIB)
        find_package(ZLIB REQUIRED)
    endif()

    if(NOT APPLE AND NOT WIN32)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(GIO QUIET IMPORTED_TARGET gio-2.0 gobject-2.0 glib-2.0)
            if(TARGET PkgConfig::GIO)
                list(APPEND PLATFORM_LIBS PkgConfig::GIO)
                set(APP_USE_BLUEZ 1)
                message(STATUS "BlueZ Bluetooth D-Bus backend enabled")
            else()
                message(WARNING "GIO/glib was not found; BlueZ Bluetooth backend will use fallback messages")
            endif()

            pkg_check_modules(LIBNM QUIET IMPORTED_TARGET libnm)
            if(TARGET PkgConfig::LIBNM)
                list(APPEND PLATFORM_LIBS PkgConfig::LIBNM)
                set(APP_USE_LIBNM 1)
                message(STATUS "NetworkManager/libnm connectivity backend enabled")
            else()
                message(WARNING "libnm was not found; connectivity service will use fallback messages")
            endif()

            pkg_check_modules(UDEV QUIET IMPORTED_TARGET libudev)
            if(TARGET PkgConfig::UDEV)
                list(APPEND PLATFORM_LIBS PkgConfig::UDEV)
                set(APP_USE_LIBUDEV 1)
                message(STATUS "libudev USB enumeration backend enabled")
            else()
                message(WARNING "libudev was not found; USB enumeration will use fallback messages")
            endif()

        endif()
    endif()

    find_path(MINIAUDIO_INCLUDE_DIR
        NAMES miniaudio.h
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/.cache/sdk_bsp-src/usr/include"
    )
    if(MINIAUDIO_INCLUDE_DIR)
        set(APP_USE_MINIAUDIO 1)
        message(STATUS "miniaudio backend enabled")
    else()
        message(FATAL_ERROR "miniaudio.h was not found; install libminiaudio-dev or provide it in .cache/sdk_bsp-src/usr/include")
    endif()

    find_path(LIBIPERF_INCLUDE_DIR iperf_api.h)
    find_library(LIBIPERF_LIBRARY iperf)
    find_library(LIBSCTP_LIBRARY sctp)
    if(LIBIPERF_INCLUDE_DIR AND LIBIPERF_LIBRARY AND LIBSCTP_LIBRARY)
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBIPERF_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS "${LIBIPERF_LIBRARY}" "${LIBSCTP_LIBRARY}")
        set(APP_USE_LIBIPERF 1)
        message(STATUS "libiperf link throughput backend enabled")
    else()
        message(WARNING "libiperf headers/library or libsctp dependency not found; link throughput test will use fallback messages")
    endif()

    find_path(LIRC_INCLUDE_DIR linux/lirc.h)
    find_library(LIRC_LIBRARY lirc)
    if(LIRC_INCLUDE_DIR AND LIRC_LIBRARY)
        list(APPEND PLATFORM_INCLUDE_DIR "${LIRC_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS "${LIRC_LIBRARY}")
        set(APP_USE_LIRC 1)
        message(STATUS "LIRC IR backend enabled")
    else()
        message(WARNING "liblirc-dev headers/library not found; IR test will use fallback messages")
    endif()

    find_path(LIBGPIOD_INCLUDE_DIR gpiod.h)
    find_library(LIBGPIOD_LIBRARY gpiod)
    if(LIBGPIOD_INCLUDE_DIR AND LIBGPIOD_LIBRARY)
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBGPIOD_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS "${LIBGPIOD_LIBRARY}")
        set(APP_USE_LIBGPIOD 1)
        message(STATUS "libgpiod external bus switch backend enabled")
    else()
        message(WARNING "libgpiod headers/library not found; external I2C/UART bus switching will report unavailable")
    endif()

    find_path(LIBSERIALPORT_INCLUDE_DIR libserialport.h)
    find_library(LIBSERIALPORT_LIBRARY serialport)
    if(LIBSERIALPORT_INCLUDE_DIR AND LIBSERIALPORT_LIBRARY)
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBSERIALPORT_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS "${LIBSERIALPORT_LIBRARY}")
        set(APP_USE_LIBSERIALPORT 1)
        message(STATUS "libserialport UART backend enabled")
    else()
        message(WARNING "libserialport headers/library not found; UART backend will report unavailable")
    endif()

    find_path(LIBYAML_INCLUDE_DIR yaml.h)
    find_library(LIBYAML_LIBRARY yaml)
    if(LIBYAML_INCLUDE_DIR AND LIBYAML_LIBRARY)
        list(APPEND SERIALIZATION_INCLUDE_DIR "${LIBYAML_INCLUDE_DIR}")
        list(APPEND SERIALIZATION_LIBS "${LIBYAML_LIBRARY}")
        set(APP_USE_LIBYAML 1)
        message(STATUS "libyaml formatted output parser enabled")
    else()
        message(WARNING "libyaml headers/library not found; YAML output parsing will report unavailable")
    endif()

    find_path(LIBCJSON_INCLUDE_DIR NAMES cjson/cJSON.h cJSON.h)
    find_library(LIBCJSON_LIBRARY NAMES cjson libcjson)
    if(LIBCJSON_INCLUDE_DIR AND LIBCJSON_LIBRARY)
        list(APPEND SERIALIZATION_INCLUDE_DIR "${LIBCJSON_INCLUDE_DIR}")
        list(APPEND SERIALIZATION_LIBS "${LIBCJSON_LIBRARY}")
        set(APP_USE_LIBCJSON 1)
        message(STATUS "cJSON formatted output parser enabled")
    else()
        message(WARNING "libcjson headers/library not found; JSON output parsing will report unavailable")
    endif()

    set(LVGL_DESKTOP_DEPS ZLIB::ZLIB PNG::PNG ${JPEG_TARGET} Freetype::Freetype ${SDL2_TARGET})
    foreach(dep IN LISTS LVGL_DESKTOP_DEPS)
        if(TARGET ${dep})
            target_link_libraries(lvgl PUBLIC ${dep})
        endif()
    endforeach()

    if(SDL2_FOUND AND NOT TARGET SDL2::SDL2)
        target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})
        target_link_libraries(lvgl PUBLIC ${SDL2_LIBRARIES})
    endif()

    target_compile_definitions(lvgl PUBLIC USE_DESKTOP=1)
    if(MSVC)
        target_compile_options(lvgl PUBLIC /utf-8)
    endif()
else()
    message(STATUS "Configuring CM0/aarch64 Linux build")

    if(NOT CM0_SDK_ROOT OR NOT EXISTS "${CM0_SDK_ROOT}")
        message(FATAL_ERROR
            "CM0 SDK sysroot not found or invalid.\n"
            "The cp0 toolchain should download the BSP before project() runs, "
            "or configure with -DCM0_SDK_ROOT=<path_to_sdk_sysroot>."
        )
    endif()

    if(CMAKE_SYSROOT)
        set(CM0_SYSROOT "${CMAKE_SYSROOT}")
    else()
        set(CM0_SYSROOT "${CM0_SDK_ROOT}")
    endif()
    if(NOT CM0_MULTIARCH)
        set(CM0_MULTIARCH "aarch64-linux-gnu")
    endif()
    message(STATUS "Using CM0 SDK root: ${CM0_SDK_ROOT}")
    message(STATUS "Using CM0 dependency root: ${CM0_SYSROOT}")

    set(CM0_DEPENDENCY_ROOTS "${CM0_SYSROOT}" "${CM0_SDK_ROOT}")
    list(REMOVE_DUPLICATES CM0_DEPENDENCY_ROOTS)
    set(CM0_INCLUDE_HINTS)
    set(CM0_LIBRARY_HINTS)
    foreach(CM0_DEPENDENCY_ROOT IN LISTS CM0_DEPENDENCY_ROOTS)
        list(APPEND CM0_INCLUDE_HINTS
            "${CM0_DEPENDENCY_ROOT}/usr/include/${CM0_MULTIARCH}"
            "${CM0_DEPENDENCY_ROOT}/usr/include"
            "${CM0_DEPENDENCY_ROOT}/usr/local/include"
            "${CM0_DEPENDENCY_ROOT}/include/${CM0_MULTIARCH}"
            "${CM0_DEPENDENCY_ROOT}/include"
        )
        list(APPEND CM0_LIBRARY_HINTS
            "${CM0_DEPENDENCY_ROOT}/usr/lib/${CM0_MULTIARCH}"
            "${CM0_DEPENDENCY_ROOT}/usr/lib"
            "${CM0_DEPENDENCY_ROOT}/usr/local/lib/${CM0_MULTIARCH}"
            "${CM0_DEPENDENCY_ROOT}/usr/local/lib"
            "${CM0_DEPENDENCY_ROOT}/lib/${CM0_MULTIARCH}"
            "${CM0_DEPENDENCY_ROOT}/lib"
        )
    endforeach()
    list(REMOVE_DUPLICATES CM0_INCLUDE_HINTS)
    list(REMOVE_DUPLICATES CM0_LIBRARY_HINTS)

    function(cm0_find_header out_var header_name)
        find_path(${out_var} NAMES ${header_name} HINTS ${ARGN} ${CM0_INCLUDE_HINTS} NO_DEFAULT_PATH)
        set(${out_var} "${${out_var}}" PARENT_SCOPE)
    endfunction()

    function(cm0_find_library out_var)
        find_library(${out_var} NAMES ${ARGN} HINTS ${CM0_LIBRARY_HINTS} NO_DEFAULT_PATH)
        set(${out_var} "${${out_var}}" PARENT_SCOPE)
    endfunction()

    function(cm0_add_imported_library target_name library_path include_dirs)
        if(NOT TARGET ${target_name})
            add_library(${target_name} UNKNOWN IMPORTED)
            set_target_properties(${target_name} PROPERTIES
                IMPORTED_LOCATION "${library_path}"
                INTERFACE_INCLUDE_DIRECTORIES "${include_dirs}"
            )
        endif()
    endfunction()

    if(APP_USE_DRM)
        cm0_find_header(DRM_XF86_INCLUDE_DIR xf86drm.h "${CM0_SYSROOT}/usr/include" "${CM0_SYSROOT}/usr/include/libdrm")
        cm0_find_header(DRM_XF86_MODE_INCLUDE_DIR xf86drmMode.h "${CM0_SYSROOT}/usr/include" "${CM0_SYSROOT}/usr/include/libdrm")
        cm0_find_header(DRM_MODE_INCLUDE_DIR drm.h "${CM0_SYSROOT}/usr/include/drm" "${CM0_SYSROOT}/usr/include/libdrm" "${CM0_SYSROOT}/usr/include")
        cm0_find_library(DRM_LIBRARY drm libdrm.so libdrm.so.2)
        if(NOT DRM_XF86_INCLUDE_DIR OR NOT DRM_XF86_MODE_INCLUDE_DIR OR NOT DRM_MODE_INCLUDE_DIR OR NOT DRM_LIBRARY)
            message(FATAL_ERROR "APP_USE_DRM=ON but libdrm headers/library were not found in BSP sysroot.")
        endif()
        cm0_add_imported_library(CM0::DRM "${DRM_LIBRARY}" "${DRM_XF86_INCLUDE_DIR};${DRM_XF86_MODE_INCLUDE_DIR};${DRM_MODE_INCLUDE_DIR}")
        message(STATUS "CM0 DRM display support enabled")
    endif()

    cm0_find_header(FREETYPE_INCLUDE_DIR ft2build.h "${CM0_SYSROOT}/usr/include/freetype2" "${CM0_SYSROOT}/usr/include")
    cm0_find_library(FREETYPE_LIBRARY freetype)
    if(NOT FREETYPE_INCLUDE_DIR OR NOT FREETYPE_LIBRARY)
        message(FATAL_ERROR "Missing Freetype header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(CM0::Freetype "${FREETYPE_LIBRARY}" "${FREETYPE_INCLUDE_DIR}")

    cm0_find_header(PNG_INCLUDE_DIR png.h "${CM0_SYSROOT}/usr/include/libpng16" "${CM0_SYSROOT}/usr/include/libpng" "${CM0_SYSROOT}/usr/include")
    cm0_find_library(PNG_LIBRARY png16 png)
    if(NOT PNG_INCLUDE_DIR OR NOT PNG_LIBRARY)
        message(FATAL_ERROR "Missing PNG header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(CM0::PNG "${PNG_LIBRARY}" "${PNG_INCLUDE_DIR}")

    cm0_find_header(JPEG_INCLUDE_DIR jpeglib.h "${CM0_SYSROOT}/usr/include")
    cm0_find_library(JPEG_LIBRARY jpeg)
    if(NOT JPEG_INCLUDE_DIR OR NOT JPEG_LIBRARY)
        message(FATAL_ERROR "Missing JPEG header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(CM0::JPEG "${JPEG_LIBRARY}" "${JPEG_INCLUDE_DIR}")

    cm0_find_header(ZLIB_INCLUDE_DIR zlib.h "${CM0_SYSROOT}/usr/include")
    cm0_find_library(ZLIB_LIBRARY z)
    if(NOT ZLIB_INCLUDE_DIR OR NOT ZLIB_LIBRARY)
        message(FATAL_ERROR "Missing ZLIB header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    cm0_add_imported_library(ZLIB::ZLIB "${ZLIB_LIBRARY}" "${ZLIB_INCLUDE_DIR}")

    set(CM0_MULTIARCH_INCLUDE_DIRS)
    foreach(CM0_INCLUDE_DIR IN ITEMS "${CM0_SYSROOT}/usr/include/${CM0_MULTIARCH}" "${CM0_SYSROOT}/include/${CM0_MULTIARCH}")
        if(EXISTS "${CM0_INCLUDE_DIR}/bits/timesize.h" OR EXISTS "${CM0_INCLUDE_DIR}/bits/wordsize.h")
            list(APPEND CM0_MULTIARCH_INCLUDE_DIRS "${CM0_INCLUDE_DIR}")
            message(STATUS "Found multiarch include: ${CM0_INCLUDE_DIR}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES CM0_MULTIARCH_INCLUDE_DIRS)

    cm0_find_header(FMT_INCLUDE_DIR fmt/format.h "${CM0_SYSROOT}/usr/include")
    cm0_find_library(FMT_LIBRARY fmt libfmt.so.10 libfmt.so.10.1.0)
    if(FMT_INCLUDE_DIR AND FMT_LIBRARY)
        cm0_add_imported_library(fmt::fmt "${FMT_LIBRARY}" "${FMT_INCLUDE_DIR}")
    elseif(CM0_ALLOW_FETCH_DEPS)
        message(STATUS "Missing fmt in BSP sysroot, fetching from GitHub because CM0_ALLOW_FETCH_DEPS=ON")
        FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 10.1.1 GIT_SHALLOW TRUE)
        set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install rules for app packages" FORCE)
        set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
        set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
        FetchContent_MakeAvailable(fmt)
        if(TARGET fmt)
            target_include_directories(fmt SYSTEM BEFORE PRIVATE ${CM0_MULTIARCH_INCLUDE_DIRS})
        endif()
        if(TARGET fmt AND NOT TARGET fmt::fmt)
            add_library(fmt::fmt ALIAS fmt)
        endif()
    else()
        message(FATAL_ERROR "Missing fmt header/library in BSP sysroot: ${CM0_SYSROOT}")
    endif()

    set_target_properties(CM0::PNG PROPERTIES INTERFACE_LINK_LIBRARIES ZLIB::ZLIB)
    set_target_properties(CM0::Freetype PROPERTIES INTERFACE_LINK_LIBRARIES "CM0::PNG;ZLIB::ZLIB")

    cm0_find_library(PTHREAD_LIBRARY pthread libpthread.so.0 libpthread.a)
    cm0_find_library(M_LIBRARY m libm.so libm.so.6 libm.a)
    if(NOT PTHREAD_LIBRARY)
        message(FATAL_ERROR "Missing pthread library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    if(NOT M_LIBRARY)
        message(FATAL_ERROR "Missing math library in BSP sysroot: ${CM0_SYSROOT}")
    endif()
    set(PLATFORM_INCLUDE_DIR ${CM0_MULTIARCH_INCLUDE_DIRS})
    set(PLATFORM_LIBS ${PTHREAD_LIBRARY} ${M_LIBRARY})

    cm0_find_header(LIBCAMERA_INCLUDE_DIR libcamera/libcamera.h "${CM0_SYSROOT}/usr/include")
    cm0_find_library(LIBCAMERA_LIBRARY camera libcamera.so)
    cm0_find_library(LIBCAMERA_BASE_LIBRARY camera-base libcamera-base.so)
    if(LIBCAMERA_INCLUDE_DIR AND LIBCAMERA_LIBRARY AND LIBCAMERA_BASE_LIBRARY)
        cm0_add_imported_library(CM0::Libcamera "${LIBCAMERA_LIBRARY}" "${LIBCAMERA_INCLUDE_DIR}")
        cm0_add_imported_library(CM0::LibcameraBase "${LIBCAMERA_BASE_LIBRARY}" "${LIBCAMERA_INCLUDE_DIR}")
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBCAMERA_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS CM0::Libcamera CM0::LibcameraBase)
        set(APP_USE_LIBCAMERA 1)
        message(STATUS "Native libcamera preview support enabled")
    else()
        message(WARNING "libcamera headers/libraries not found; native camera service will be disabled")
    endif()

    cm0_find_header(LIBNM_INCLUDE_DIR NetworkManager.h "${CM0_SYSROOT}/usr/include/libnm" "${CM0_SYSROOT}/usr/include")
    cm0_find_header(GLIB_INCLUDE_DIR glib.h "${CM0_SYSROOT}/usr/include/glib-2.0" "${CM0_SYSROOT}/usr/include")
    cm0_find_header(GLIB_CONFIG_INCLUDE_DIR glibconfig.h "${CM0_SYSROOT}/usr/lib/${CM0_MULTIARCH}/glib-2.0/include" "${CM0_SYSROOT}/usr/lib/glib-2.0/include" "${CM0_SYSROOT}/lib/${CM0_MULTIARCH}/glib-2.0/include" "${CM0_SYSROOT}/lib/glib-2.0/include")
    cm0_find_library(LIBNM_LIBRARY nm libnm.so)
    cm0_find_header(LIBUDEV_INCLUDE_DIR libudev.h "${CM0_SYSROOT}/usr/include")
    cm0_find_library(LIBUDEV_LIBRARY udev libudev.so)
    cm0_find_library(GIO_LIBRARY gio-2.0 libgio-2.0.so)
    cm0_find_library(GOBJECT_LIBRARY gobject-2.0 libgobject-2.0.so)
    cm0_find_library(GLIB_LIBRARY glib-2.0 libglib-2.0.so)
    if(GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND GIO_LIBRARY AND GOBJECT_LIBRARY AND GLIB_LIBRARY)
        list(APPEND PLATFORM_INCLUDE_DIR "${GLIB_INCLUDE_DIR}" "${GLIB_CONFIG_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS ${GIO_LIBRARY} ${GOBJECT_LIBRARY} ${GLIB_LIBRARY})
        set(APP_USE_BLUEZ 1)
        message(STATUS "BlueZ Bluetooth D-Bus backend enabled")
    else()
        message(WARNING "GIO/glib headers or libraries not found; BlueZ Bluetooth backend will use fallback messages")
    endif()

    if(LIBNM_INCLUDE_DIR AND GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND LIBNM_LIBRARY AND GIO_LIBRARY AND GOBJECT_LIBRARY AND GLIB_LIBRARY)
        cm0_add_imported_library(CM0::Libnm "${LIBNM_LIBRARY}" "${LIBNM_INCLUDE_DIR};${GLIB_INCLUDE_DIR};${GLIB_CONFIG_INCLUDE_DIR}")
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBNM_INCLUDE_DIR}" "${GLIB_INCLUDE_DIR}" "${GLIB_CONFIG_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS CM0::Libnm ${GIO_LIBRARY} ${GOBJECT_LIBRARY} ${GLIB_LIBRARY})
        set(APP_USE_LIBNM 1)
        message(STATUS "NetworkManager/libnm connectivity backend enabled")
    else()
        message(WARNING "libnm/glib headers or libraries not found; connectivity service will use fallback messages")
    endif()

    if(LIBUDEV_INCLUDE_DIR AND LIBUDEV_LIBRARY)
        cm0_add_imported_library(CM0::Libudev "${LIBUDEV_LIBRARY}" "${LIBUDEV_INCLUDE_DIR}")
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBUDEV_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS CM0::Libudev)
        set(APP_USE_LIBUDEV 1)
        message(STATUS "libudev USB enumeration backend enabled")
    else()
        message(WARNING "libudev headers or libraries not found; USB enumeration will use fallback messages")
    endif()

    cm0_find_header(MINIAUDIO_INCLUDE_DIR miniaudio.h "${CM0_SYSROOT}/usr/include" "${CM0_SDK_ROOT}/usr/include")
    if(MINIAUDIO_INCLUDE_DIR)
        set(APP_USE_MINIAUDIO 1)
        message(STATUS "miniaudio backend enabled")
    else()
        message(FATAL_ERROR "miniaudio.h was not found in the CM0 SDK sysroot")
    endif()

    cm0_find_header(LIBIPERF_INCLUDE_DIR iperf_api.h "${CM0_SYSROOT}/usr/include")
    cm0_find_library(LIBIPERF_LIBRARY iperf libiperf.so libiperf.a)
    cm0_find_library(LIBSCTP_LIBRARY sctp libsctp.so libsctp.so.1)
    if(LIBIPERF_INCLUDE_DIR AND LIBIPERF_LIBRARY AND LIBSCTP_LIBRARY)
        cm0_add_imported_library(CM0::Libiperf "${LIBIPERF_LIBRARY}" "${LIBIPERF_INCLUDE_DIR}")
        cm0_add_imported_library(CM0::Libsctp "${LIBSCTP_LIBRARY}" "${LIBIPERF_INCLUDE_DIR}")
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBIPERF_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS CM0::Libiperf CM0::Libsctp)
        set(APP_USE_LIBIPERF 1)
        message(STATUS "libiperf link throughput backend enabled")
    else()
        message(WARNING "libiperf headers/library or libsctp dependency not found; link throughput test will use fallback messages")
    endif()

    cm0_find_header(LIRC_INCLUDE_DIR linux/lirc.h "${CM0_SYSROOT}/usr/include")
    cm0_find_library(LIRC_LIBRARY lirc liblirc.so liblirc.a)
    if(LIRC_INCLUDE_DIR AND LIRC_LIBRARY)
        cm0_add_imported_library(CM0::Lirc "${LIRC_LIBRARY}" "${LIRC_INCLUDE_DIR}")
        list(APPEND PLATFORM_INCLUDE_DIR "${LIRC_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS CM0::Lirc)
        set(APP_USE_LIRC 1)
        message(STATUS "LIRC IR backend enabled")
    else()
        message(WARNING "liblirc-dev headers/library not found; IR test will use fallback messages")
    endif()

    unset(LIBGPIOD_INCLUDE_DIR CACHE)
    unset(LIBGPIOD_LIBRARY CACHE)
    cm0_find_header(LIBGPIOD_INCLUDE_DIR gpiod.h "${CM0_SYSROOT}/usr/include" "${CM0_SDK_ROOT}/usr/include")
    cm0_find_library(LIBGPIOD_LIBRARY gpiod libgpiod.so libgpiod.so.3 libgpiod.so.3.1.1 libgpiod.a)
    if(LIBGPIOD_INCLUDE_DIR AND LIBGPIOD_LIBRARY)
        cm0_add_imported_library(CM0::Libgpiod "${LIBGPIOD_LIBRARY}" "${LIBGPIOD_INCLUDE_DIR}")
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBGPIOD_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS CM0::Libgpiod)
        set(APP_USE_LIBGPIOD 1)
        message(STATUS "libgpiod external bus switch backend enabled")
    else()
        message(WARNING "libgpiod headers/library not found; external I2C/UART bus switching will report unavailable")
    endif()

    cm0_find_header(LIBSERIALPORT_INCLUDE_DIR libserialport.h "${CM0_SYSROOT}/usr/include" "${CM0_SDK_ROOT}/usr/include")
    cm0_find_library(LIBSERIALPORT_LIBRARY serialport libserialport.so libserialport.a)
    if(LIBSERIALPORT_INCLUDE_DIR AND LIBSERIALPORT_LIBRARY)
        cm0_add_imported_library(CM0::Libserialport "${LIBSERIALPORT_LIBRARY}" "${LIBSERIALPORT_INCLUDE_DIR}")
        list(APPEND PLATFORM_INCLUDE_DIR "${LIBSERIALPORT_INCLUDE_DIR}")
        list(APPEND PLATFORM_LIBS CM0::Libserialport)
        set(APP_USE_LIBSERIALPORT 1)
        message(STATUS "libserialport UART backend enabled")
    else()
        message(WARNING "libserialport headers/library not found; UART backend will report unavailable")
    endif()

    cm0_find_header(LIBYAML_INCLUDE_DIR yaml.h "${CM0_SYSROOT}/usr/include" "${CM0_SDK_ROOT}/usr/include")
    cm0_find_library(LIBYAML_LIBRARY yaml libyaml.so libyaml.a)
    if(LIBYAML_INCLUDE_DIR AND LIBYAML_LIBRARY)
        cm0_add_imported_library(CM0::Libyaml "${LIBYAML_LIBRARY}" "${LIBYAML_INCLUDE_DIR}")
        list(APPEND SERIALIZATION_INCLUDE_DIR "${LIBYAML_INCLUDE_DIR}")
        list(APPEND SERIALIZATION_LIBS CM0::Libyaml)
        set(APP_USE_LIBYAML 1)
        message(STATUS "libyaml formatted output parser enabled")
    else()
        message(WARNING "libyaml headers/library not found; YAML output parsing will report unavailable")
    endif()

    cm0_find_header(LIBCJSON_INCLUDE_DIR cjson/cJSON.h "${CM0_SYSROOT}/usr/include" "${CM0_SDK_ROOT}/usr/include")
    if(NOT LIBCJSON_INCLUDE_DIR)
        cm0_find_header(LIBCJSON_INCLUDE_DIR cJSON.h "${CM0_SYSROOT}/usr/include" "${CM0_SDK_ROOT}/usr/include")
    endif()
    cm0_find_library(LIBCJSON_LIBRARY cjson libcjson.so libcjson.a)
    if(LIBCJSON_INCLUDE_DIR AND LIBCJSON_LIBRARY)
        cm0_add_imported_library(CM0::Libcjson "${LIBCJSON_LIBRARY}" "${LIBCJSON_INCLUDE_DIR}")
        list(APPEND SERIALIZATION_INCLUDE_DIR "${LIBCJSON_INCLUDE_DIR}")
        list(APPEND SERIALIZATION_LIBS CM0::Libcjson)
        set(APP_USE_LIBCJSON 1)
        message(STATUS "cJSON formatted output parser enabled")
    else()
        message(WARNING "libcjson headers/library not found; JSON output parsing will report unavailable")
    endif()

    set(LVGL_INCLUDE_DIR ${FREETYPE_INCLUDE_DIR} ${PNG_INCLUDE_DIR} ${JPEG_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})
    set(LVGL_LIBS CM0::Freetype CM0::PNG CM0::JPEG ZLIB::ZLIB)
    if(APP_USE_DRM)
        list(APPEND LVGL_INCLUDE_DIR ${DRM_XF86_INCLUDE_DIR} ${DRM_XF86_MODE_INCLUDE_DIR} ${DRM_MODE_INCLUDE_DIR})
        list(APPEND LVGL_LIBS CM0::DRM)
    endif()

    list(REMOVE_DUPLICATES PLATFORM_INCLUDE_DIR)
    list(REMOVE_DUPLICATES PLATFORM_LIBS)
    list(REMOVE_DUPLICATES SERIALIZATION_INCLUDE_DIR)
    list(REMOVE_DUPLICATES SERIALIZATION_LIBS)
    list(REMOVE_DUPLICATES LVGL_INCLUDE_DIR)
    list(REMOVE_DUPLICATES LVGL_LIBS)

    target_compile_definitions(lvgl PUBLIC USE_DESKTOP=0 APP_USE_DRM=$<BOOL:${APP_USE_DRM}>)
    target_include_directories(lvgl PUBLIC ${LVGL_INCLUDE_DIR} ${PLATFORM_INCLUDE_DIR})
    target_link_libraries(lvgl PUBLIC ${LVGL_LIBS} ${PLATFORM_LIBS})
endif()

if(USE_DESKTOP)
    list(REMOVE_DUPLICATES SERIALIZATION_INCLUDE_DIR)
    list(REMOVE_DUPLICATES SERIALIZATION_LIBS)
endif()

if(APP_USE_MINIAUDIO AND UNIX AND NOT APPLE)
    list(APPEND PLATFORM_LIBS ${CMAKE_DL_LIBS} m)
endif()
list(REMOVE_DUPLICATES PLATFORM_INCLUDE_DIR)
list(REMOVE_DUPLICATES PLATFORM_LIBS)

file(GLOB_RECURSE SERIALIZATION_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/serialization/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/serialization/*.cpp")
add_library(serialization STATIC ${SERIALIZATION_SOURCES})
target_include_directories(serialization PUBLIC ${SERIALIZATION_INCLUDE_DIR} ${PLATFORM_INCLUDE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src/serialization")
target_compile_definitions(serialization PUBLIC
    APP_USE_LIBYAML=${APP_USE_LIBYAML}
    APP_USE_LIBCJSON=${APP_USE_LIBCJSON}
)
target_link_libraries(serialization PUBLIC ${SERIALIZATION_LIBS})

file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.cpp")
add_library(${PROJECT_NAME}_platform STATIC ${PLATFORM_SOURCES})
target_include_directories(${PROJECT_NAME}_platform PUBLIC ${LVGL_INCLUDE_DIR} ${PLATFORM_INCLUDE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src/platform" "${CMAKE_CURRENT_SOURCE_DIR}/src/serialization" "${CMAKE_CURRENT_SOURCE_DIR}/src/logger" "${CMAKE_CURRENT_SOURCE_DIR}/src/view" "${CMAKE_CURRENT_SOURCE_DIR}/src/view/widgets")
target_compile_definitions(${PROJECT_NAME}_platform PUBLIC
    APP_KEY_INPUT_DEVICE="${APP_KEY_INPUT_DEVICE}"
    APP_USE_LIBCAMERA=${APP_USE_LIBCAMERA}
    APP_USE_LIBNM=${APP_USE_LIBNM}
    APP_USE_BLUEZ=${APP_USE_BLUEZ}
    APP_USE_LIBUDEV=${APP_USE_LIBUDEV}
    APP_USE_MINIAUDIO=${APP_USE_MINIAUDIO}
    APP_MINIAUDIO_HEADER="${MINIAUDIO_INCLUDE_DIR}/miniaudio.h"
    APP_USE_LIBIPERF=${APP_USE_LIBIPERF}
    APP_USE_LIRC=${APP_USE_LIRC}
    APP_USE_LIBGPIOD=${APP_USE_LIBGPIOD}
    APP_USE_LIBSERIALPORT=${APP_USE_LIBSERIALPORT}
    APP_FRAMEBUFFER_DEVICE="${APP_FRAMEBUFFER_DEVICE}"
    APP_DRM_DEVICE="${APP_DRM_DEVICE}"
    APP_DRM_CONNECTOR_ID=\(${APP_DRM_CONNECTOR_ID}\)
)
target_link_libraries(${PROJECT_NAME}_platform PUBLIC lvgl fmt::fmt Threads::Threads serialization ${PLATFORM_LIBS})

# model src
file(GLOB_RECURSE MODEL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/model/*.cpp")
add_library(${PROJECT_NAME}_model STATIC ${MODEL_SOURCES})
target_include_directories(${PROJECT_NAME}_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/model")
target_link_libraries(${PROJECT_NAME}_model PRIVATE ${PROJECT_NAME}_platform serialization)

# lvgl reactive src
file(GLOB_RECURSE REACTIVE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/reactive/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/reactive/*.cpp")
add_library(${PROJECT_NAME}_reactive STATIC ${REACTIVE_SOURCES})
target_include_directories(${PROJECT_NAME}_reactive PUBLIC ${LVGL_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/reactive ${CMAKE_CURRENT_SOURCE_DIR}/src/view)
target_link_libraries(${PROJECT_NAME}_reactive PUBLIC lvgl)

# view (ui) src
file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/view/*.cpp")
add_library(${PROJECT_NAME}_view STATIC ${UI_SOURCES})
target_include_directories(${PROJECT_NAME}_view PUBLIC
    ${LVGL_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view/screens
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/reactive
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)
target_link_libraries(${PROJECT_NAME}_view PUBLIC lvgl ${PROJECT_NAME}_reactive ${PROJECT_NAME}_platform factory_test_version)

# viewmodel src
file(GLOB_RECURSE VIEWMODEL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel/*.cpp")
add_library(${PROJECT_NAME}_viewmodel STATIC ${VIEWMODEL_SOURCES})
target_include_directories(${PROJECT_NAME}_viewmodel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel" "${CMAKE_CURRENT_SOURCE_DIR}/src/model" ${LVGL_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME}_viewmodel
    PUBLIC lvgl ${PROJECT_NAME}_reactive ${PROJECT_NAME}_model
    PRIVATE ${PROJECT_NAME}_platform
)

# app src
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp")
file(GLOB_RECURSE LOGGER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/logger/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/logger/*.cpp")
add_library(logger STATIC ${LOGGER_SOURCES})
target_include_directories(logger PUBLIC ${PLATFORM_INCLUDE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src/logger")
target_link_libraries(logger PUBLIC fmt::fmt)

add_library(${PROJECT_NAME}_core STATIC ${APP_SOURCES})
target_include_directories(${PROJECT_NAME}_core PUBLIC
    ${LVGL_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger
    ${CMAKE_CURRENT_SOURCE_DIR}/src/serialization
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view/screens
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viewmodel
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)
target_compile_definitions(${PROJECT_NAME}_core PUBLIC
    APP_NAME="${APP_NAME}"
    APP_CMAKE_ASSETS_ROOT="${APP_ASSETS_ROOT}"
    APP_SOURCE_ASSETS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    APP_FRAMEBUFFER_DEVICE="${APP_FRAMEBUFFER_DEVICE}"
    APP_DRM_DEVICE="${APP_DRM_DEVICE}"
    APP_DRM_CONNECTOR_ID=\(${APP_DRM_CONNECTOR_ID}\)
)
target_link_libraries(${PROJECT_NAME}_core PUBLIC lvgl logger ${PROJECT_NAME}_view ${PROJECT_NAME}_viewmodel ${PROJECT_NAME}_platform)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_platform ${PROJECT_NAME}_core ${PROJECT_NAME}_model ${PROJECT_NAME}_view ${PROJECT_NAME}_viewmodel)

include(cmake/cm0-package.cmake)
