cmake_minimum_required(VERSION 3.21)
set(FLINT_PLATFORM_DIR "${CMAKE_CURRENT_LIST_DIR}/platform" CACHE PATH "IgniteSDK checkout (defaults to the pinned submodule)")
if(NOT EXISTS "${FLINT_PLATFORM_DIR}/include/flint/contracts.h")
    message(FATAL_ERROR "Initialize IgniteSDK: git submodule update --init --recursive")
endif()
project(FlintFlare VERSION 0.3.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 SerialPort Bluetooth Widgets ShaderTools)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 6.5 REQUIRED COMPONENTS DBus)
endif()
qt_standard_project_setup(REQUIRES 6.5)
include(CTest)
set(FLINT_PLATFORM_TESTS ${BUILD_TESTING} CACHE BOOL "Portable runtime tests" FORCE)
add_subdirectory("${FLINT_PLATFORM_DIR}" platform)
# Consume the SDK's build-local outputs, never source-tree package snapshots.
get_directory_property(FLINT_FACTORY_DIR DIRECTORY "${FLINT_PLATFORM_DIR}" DEFINITION FLINT_FACTORY_DIR)
get_directory_property(FLINT_EXAMPLES_DIR DIRECTORY "${FLINT_PLATFORM_DIR}" DEFINITION FLINT_EXAMPLES_DIR)
qt_add_executable(flint_flare
    src/main.cpp
    src/platform_app/PlatformClient.cpp src/platform_app/PlatformClient.h
    src/platform_app/IgniteHub.cpp src/platform_app/IgniteHub.h
    src/platform_app/Preview.cpp src/platform_app/Preview.h
    src/platform_app/CompanionService.cpp src/platform_app/CompanionService.h
    src/platform_app/Studio.cpp src/platform_app/Studio.h
    src/device/BleTransport.cpp src/device/BleTransport.h
    src/device/BluezDiscoverySession.cpp src/device/BluezDiscoverySession.h
    src/device/BluezPairingAgent.cpp src/device/BluezPairingAgent.h
    src/services/BackgroundAgent.cpp src/services/BackgroundAgent.h
    src/localization/Localization.cpp src/localization/Localization.h
)
target_include_directories(flint_flare PRIVATE src)
target_link_libraries(flint_flare PRIVATE flint_platform Qt6::Quick Qt6::QuickControls2 Qt6::SerialPort Qt6::Bluetooth Qt6::Widgets)
target_compile_definitions(flint_flare PRIVATE FLINT_FLARE_VERSION="${PROJECT_VERSION}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(flint_flare PRIVATE FLINT_BLUEZ_DBUS)
    target_link_libraries(flint_flare PRIVATE Qt6::DBus)
endif()
file(GLOB_RECURSE FLINT_QML CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" qml/*.qml)
foreach(qml_file IN LISTS FLINT_QML)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(flint_flare URI Flint.Flare VERSION 1.0 QML_FILES ${FLINT_QML})
file(GLOB FLINT_FACTORY CONFIGURE_DEPENDS ${FLINT_FACTORY_DIR}/*.ignit)
qt_add_resources(flint_flare factory PREFIX "/factory" BASE "${FLINT_FACTORY_DIR}" FILES ${FLINT_FACTORY})
qt_add_resources(flint_flare samples PREFIX "/samples" BASE "${FLINT_EXAMPLES_DIR}/pc-monitor" FILES ${FLINT_EXAMPLES_DIR}/pc-monitor/sample.ignit)
set(FLINT_COMPILER_SOURCES ${FLINT_PLATFORM_DIR}/tools/compile_ignite.py ${FLINT_PLATFORM_DIR}/tools/build_package.py ${FLINT_PLATFORM_DIR}/tools/flim.py)
qt_add_resources(flint_flare compiler PREFIX "/sdk" BASE "${FLINT_PLATFORM_DIR}/tools" FILES ${FLINT_COMPILER_SOURCES})
file(GLOB FLINT_ICONS CONFIGURE_DEPENDS assets/icons/*.svg)
qt_add_resources(flint_flare icons PREFIX "/icons" BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons" FILES ${FLINT_ICONS})
file(GLOB FLINT_FONTS CONFIGURE_DEPENDS assets/fonts/*.ttf)
qt_add_resources(flint_flare fonts PREFIX "/fonts" BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts" FILES ${FLINT_FONTS})
file(GLOB_RECURSE FLINT_TRANSLATIONS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" translations/*.json)
qt_add_resources(flint_flare translations PREFIX "/" FILES ${FLINT_TRANSLATIONS})
qt_add_shaders(flint_flare atmosphere PREFIX "/shaders" BASE "${CMAKE_CURRENT_SOURCE_DIR}/shaders" FILES shaders/atmosphere.frag shaders/accent.frag)
if(BUILD_TESTING)
    qt_add_executable(companion_tests
        tests/companion.cpp
        src/platform_app/IgniteHub.cpp src/platform_app/IgniteHub.h
        src/platform_app/PlatformClient.cpp src/platform_app/PlatformClient.h
        src/platform_app/CompanionService.cpp src/platform_app/CompanionService.h
        src/device/BleTransport.cpp src/device/BleTransport.h
        src/device/BluezDiscoverySession.cpp src/device/BluezDiscoverySession.h
        src/device/BluezPairingAgent.cpp src/device/BluezPairingAgent.h)
    target_include_directories(companion_tests PRIVATE src)
    target_link_libraries(companion_tests PRIVATE Flint::Platform Qt6::SerialPort Qt6::Bluetooth)
    target_compile_definitions(companion_tests PRIVATE FLINT_FLARE_VERSION="${PROJECT_VERSION}"
        FLINT_MONITOR_PACKAGE="${FLINT_EXAMPLES_DIR}/pc-monitor/sample.ignit")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(companion_tests PRIVATE FLINT_BLUEZ_DBUS)
        target_link_libraries(companion_tests PRIVATE Qt6::DBus)
    endif()
    qt_add_resources(companion_tests companion_factory PREFIX "/factory"
        BASE "${FLINT_FACTORY_DIR}" FILES ${FLINT_FACTORY})
    add_test(NAME companion_sessions COMMAND companion_tests)
    set_tests_properties(companion_sessions PROPERTIES TIMEOUT 30)
    add_test(NAME desktop_offline COMMAND flint_flare --smoke)
    set_tests_properties(desktop_offline PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 25 FAIL_REGULAR_EXPRESSION "Binding loop detected")
endif()
set_target_properties(flint_flare PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE MACOSX_BUNDLE_BUNDLE_NAME "Flint Flare" MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}" MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}" MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in" MACOSX_BUNDLE_GUI_IDENTIFIER "labs.deetech.flint.flare")
include(GNUInstallDirs)
install(TARGETS flint_flare BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_qml_app_script(TARGET flint_flare OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})

install(FILES LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DATADIR}/FlintFlare)
install(DIRECTORY docs DESTINATION ${CMAKE_INSTALL_DATADIR}/FlintFlare
    FILES_MATCHING PATTERN "*.md" PATTERN "*.txt")
install(FILES assets/fonts/OFL.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/FlintFlare/third_party/Lato)

if(BUILD_TESTING AND FLINT_ENABLE_LUA)
    qt_add_executable(preview_input_tests
        tests/preview_input.cpp
        src/platform_app/PlatformClient.cpp src/platform_app/PlatformClient.h
        src/platform_app/Preview.cpp src/platform_app/Preview.h
        src/device/BleTransport.cpp src/device/BleTransport.h
        src/device/BluezDiscoverySession.cpp src/device/BluezDiscoverySession.h
        src/device/BluezPairingAgent.cpp src/device/BluezPairingAgent.h)
    target_include_directories(preview_input_tests PRIVATE src)
    target_link_libraries(preview_input_tests PRIVATE Flint::Platform
        Qt6::Quick Qt6::SerialPort Qt6::Bluetooth Qt6::Widgets)
    target_compile_definitions(preview_input_tests PRIVATE
        FLINT_FLARE_VERSION="${PROJECT_VERSION}"
        FLINT_COUNTER_PACKAGE="${FLINT_EXAMPLES_DIR}/counter/sample.ignit")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(preview_input_tests PRIVATE FLINT_BLUEZ_DBUS)
        target_link_libraries(preview_input_tests PRIVATE Qt6::DBus)
    endif()
    qt_add_resources(preview_input_tests test_factory PREFIX "/factory"
        BASE "${FLINT_FACTORY_DIR}" FILES ${FLINT_FACTORY})
    add_test(NAME preview_application_api COMMAND preview_input_tests)
    set_tests_properties(preview_application_api PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 15)
endif()

if(BUILD_TESTING)
    qt_add_executable(studio_resource_tests tests/studio_resources.cpp
        src/platform_app/Studio.cpp src/platform_app/Studio.h)
    target_include_directories(studio_resource_tests PRIVATE src)
    target_link_libraries(studio_resource_tests PRIVATE Flint::Platform Qt6::Core)
    qt_add_resources(studio_resource_tests studio_compiler PREFIX "/sdk"
        BASE "${FLINT_PLATFORM_DIR}/tools" FILES ${FLINT_COMPILER_SOURCES})
    add_test(NAME studio_icon_resources COMMAND studio_resource_tests
        "${FLINT_PLATFORM_DIR}/examples/icons/project.json")
    set_tests_properties(studio_icon_resources PROPERTIES TIMEOUT 20)
endif()
