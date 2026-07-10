#
# SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
#
# SPDX-License-Identifier: MIT
#
# Packaging and install layout for CardputerZero Debian builds.

include(GNUInstallDirs)

set(APP_DISPLAY_NAME "FactoryTest" CACHE STRING "Human-readable application name used by launchers and package filename")
set(APP_DEBIAN_REVISION "m5stack1" CACHE STRING "Debian package revision/vendor suffix")
set(APP_DEBIAN_ARCHITECTURE "arm64" CACHE STRING "Debian package architecture")
set(APP_MAINTAINER "M5Stack <support@m5stack.com>" CACHE STRING "Debian package maintainer")
set(APP_PACKAGE_DESCRIPTION "CardputerZero factory test application" CACHE STRING "Debian package summary")
set(APP_INSTALL_SYSTEMD_SERVICE ON CACHE BOOL "Install a systemd service file for embedded deployments")
set(APP_SET_CAP_NET_RAW ON CACHE BOOL "Grant cap_net_raw to the installed app binary for ICMP ping")

set(APP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/package")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/templates/factory_test.desktop.in"
    "${APP_GENERATED_DIR}/${PROJECT_NAME}.desktop"
    @ONLY
)

if(APP_INSTALL_SYSTEMD_SERVICE)
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/templates/factory_test.service.in"
        "${APP_GENERATED_DIR}/${PROJECT_NAME}.service"
        @ONLY
    )
endif()

if(APP_USE_LIBNM OR APP_USE_LIBOPING)
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/templates/factory-test-networkmanager.rules.in"
        "${APP_GENERATED_DIR}/50-${PROJECT_NAME}-networkmanager.rules"
        @ONLY
    )
endif()

set(APP_DEBIAN_CONTROL_EXTRA)
if(APP_SET_CAP_NET_RAW)
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/templates/factory_test.postinst.in"
        "${APP_GENERATED_DIR}/postinst"
        @ONLY
    )
    file(CHMOD "${APP_GENERATED_DIR}/postinst"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )
    list(APPEND APP_DEBIAN_CONTROL_EXTRA "${APP_GENERATED_DIR}/postinst")
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/audio"
    PATTERN ".DS_Store" EXCLUDE
)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/fonts"
    PATTERN ".DS_Store" EXCLUDE
)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/images"
    PATTERN ".DS_Store" EXCLUDE
)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/i18n/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${APP_NAME}/i18n"
    PATTERN ".DS_Store" EXCLUDE
)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/share/images"
    FILES_MATCHING PATTERN "factory_test*.png"
)

install(
    FILES "${APP_GENERATED_DIR}/${PROJECT_NAME}.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/APPLaunch/applications"
)

if(APP_INSTALL_SYSTEMD_SERVICE)
    install(
        FILES "${APP_GENERATED_DIR}/${PROJECT_NAME}.service"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
    )
endif()

if(APP_USE_LIBNM OR APP_USE_LIBOPING)
    install(
        FILES "${APP_GENERATED_DIR}/50-${PROJECT_NAME}-networkmanager.rules"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/polkit-1/rules.d"
    )
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md" DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/License.txt"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    RENAME "third-party-assets-license.txt"
)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/dist")
set(CPACK_PACKAGE_NAME "${APP_DISPLAY_NAME}")
set(CPACK_PACKAGE_VENDOR "M5Stack")
set(CPACK_PACKAGE_CONTACT "${APP_MAINTAINER}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_PACKAGE_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${APP_DISPLAY_NAME}_${PROJECT_VERSION}_${APP_DEBIAN_REVISION}_${APP_DEBIAN_ARCHITECTURE}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/License.txt")

string(TOLOWER "${APP_DISPLAY_NAME}" APP_DEBIAN_PACKAGE_NAME)
string(REGEX REPLACE "[^a-z0-9+.-]" "-" APP_DEBIAN_PACKAGE_NAME "${APP_DEBIAN_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_NAME "${APP_DEBIAN_PACKAGE_NAME}")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}-${APP_DEBIAN_REVISION}")
set(CPACK_DEBIAN_PACKAGE_RELEASE "${APP_DEBIAN_REVISION}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${APP_DEBIAN_ARCHITECTURE}")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${APP_MAINTAINER}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(APP_DEBIAN_PACKAGE_DEPENDS
    libc6
    libstdc++6
    libgcc-s1
    libfreetype6
    libpng16-16
    libjpeg62-turbo
    zlib1g
    libfmt10
    util-linux-extra
    sysbench
    stress-ng
    fio
)
if(APP_USE_LIBCAMERA)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS
        "libcamera0.7 | libcamera0.6 | libcamera0.5 | libcamera0.4 | libcamera0.3 | libcamera0.2 | libcamera0"
    )
endif()
if(APP_USE_BLUEZ OR APP_USE_LIBNM)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS "libglib2.0-0t64 | libglib2.0-0")
endif()
if(APP_USE_LIBNM)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS libnm0 network-manager polkitd)
endif()
if(APP_USE_LIBOPING)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS polkitd)
endif()
if(APP_SET_CAP_NET_RAW)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS libcap2-bin)
endif()
if(APP_USE_LIBUDEV)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS libudev1)
endif()
if(APP_USE_LIBIPERF)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS libiperf0 libsctp1 "libssl3 | libssl3t64")
endif()
if(APP_USE_LIBOPING)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS liboping0 iputils-ping pkexec)
endif()
if(APP_USE_LIRC)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS "liblirc0t64 | liblirc0")
endif()
if(APP_USE_BLUEZ)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS bluez)
endif()
if(APP_USE_LIBGPIOD)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS "libgpiod3 | libgpiod2")
endif()
if(APP_USE_LIBSERIALPORT)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS libserialport0)
endif()
if(APP_USE_LIBYAML)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS libyaml-0-2)
endif()
if(APP_USE_LIBCJSON)
    list(APPEND APP_DEBIAN_PACKAGE_DEPENDS libcjson1)
endif()
list(REMOVE_DUPLICATES APP_DEBIAN_PACKAGE_DEPENDS)
string(REPLACE ";" ", " CPACK_DEBIAN_PACKAGE_DEPENDS "${APP_DEBIAN_PACKAGE_DEPENDS}")
if(APP_DEBIAN_CONTROL_EXTRA)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${APP_DEBIAN_CONTROL_EXTRA}")
endif()
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)

include(CPack)
