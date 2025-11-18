cmake_minimum_required(VERSION 3.16)
project(NordicMod_CPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt build helpers
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Ensure UIC can find all .ui files
set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/ui
)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

# ------------------------------
# Source & Header files
# ------------------------------
set(SRC_FILES
    src/main.cpp
    src/mainWindow.cpp
    src/pluginManager.cpp
    src/downloadsPanel.cpp
    src/firstrunwizard.cpp      # <-- Missing in your original list
    src/detectLootType.cpp
    src/lootManager.cpp
    src/modManager.cpp
    src/iniEditorWidget.cpp
)

set(HEADER_FILES
    ui/mainWindow.h
    ui/pluginManager.h
    ui/firstrunwizard.h         # <-- Missing in your original list
    src/downloadsPanel.h
    src/iniEditorWidget.h
)

set(UI_FILES
    ui/mainWindow.ui
    ui/firstrunwizard.ui        # <-- Missing if this exists
)

# ------------------------------
# Build the executable
# ------------------------------
qt_add_executable(NordicReliquary   # rename from plugin_scanner
    ${SRC_FILES}
    ${HEADER_FILES}
    ${UI_FILES}
)

# ------------------------------
# Include dirs
# ------------------------------
target_include_directories(NordicReliquary PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ui
    /usr/local/include
)

# Include LOOT Shim headers
target_include_directories(NordicReliquary PRIVATE
    /mnt/projects/loot-shim/include
)

target_link_directories(NordicReliquary PRIVATE
    ${CMAKE_SOURCE_DIR}/loot-shim/target/release
)

# ------------------------------
# Libraries
# ------------------------------
target_link_libraries(NordicReliquary
    PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    loot
)
