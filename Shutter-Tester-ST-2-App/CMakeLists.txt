cmake_minimum_required(VERSION 3.16)

project(ShutterTesterApp VERSION 0.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Company/publisher shown in file properties; change as needed
set(COMPANY_NAME "Alexander Litvinov")

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets SerialPort PrintSupport Charts)

qt_standard_project_setup(REQUIRES 6.8)

# Generate Windows VERSIONINFO resource from template (Windows only)
if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
    set(PLATFORM_SOURCES resources.rc ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

# --- FOR MACOS ---
# 1. Collect all files from assets folder
file(GLOB_RECURSE ASSET_FILES "assets/*")

# 2. Set path inside macOS bundle (Contents/Resources/assets)
set_source_files_properties(${ASSET_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/assets")

# 3. Include report_style_configs.json in macOS bundle (Contents/MacOS — next to the binary)
if(APPLE)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/report_style_configs.json" PROPERTIES MACOSX_PACKAGE_LOCATION "MacOS")
    set(REPORT_STYLE_CONFIG_SOURCE "${CMAKE_SOURCE_DIR}/report_style_configs.json")
else()
    set(REPORT_STYLE_CONFIG_SOURCE "")
endif()
# ---------------------------

qt_add_executable(appShutterTesterApp
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    SerialController.cpp
    SerialController.h
    FirmwareUpdateDialog.cpp
    FirmwareUpdateDialog.h
    LightSetupDialog.cpp
    LightSetupDialog.h
    ConnectionDialog.cpp
    ConnectionDialog.h
    ReportSettingsDialog.cpp
    ReportSettingsDialog.h
    ReportStyleSelectionDialog.cpp
    ReportStyleSelectionDialog.h
    resources.qrc
    ${PLATFORM_SOURCES}
    ${ASSET_FILES}
    ${REPORT_STYLE_CONFIG_SOURCE}
)

set_target_properties(appShutterTesterApp PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "Shutter Tester ST-2 App"
)

if(NOT APPLE)
    add_custom_command(TARGET appShutterTesterApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:appShutterTesterApp>/assets"
        COMMENT "Copying assets directory to build output..."
    )
    # Copy report_style_configs.json to build output (next to executable)
    add_custom_command(TARGET appShutterTesterApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/report_style_configs.json"
        "$<TARGET_FILE_DIR:appShutterTesterApp>/report_style_configs.json"
        COMMENT "Copying report_style_configs.json to build output..."
    )
endif()

target_link_libraries(appShutterTesterApp
    PRIVATE Qt6::Widgets Qt6::SerialPort Qt6::PrintSupport Qt6::Charts
)

include(GNUInstallDirs)
install(TARGETS appShutterTesterApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)