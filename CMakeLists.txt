cmake_minimum_required(VERSION 3.22)

project(VisualDSA
    VERSION 1.0.0
    DESCRIPTION "Interactive Binary Search Tree Operations and Traversal Visualizer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VISUALDSA_BUILD_APP   "Build the Qt desktop application"             ON)
option(VISUALDSA_BUILD_TESTS "Build the automated tests (requires Qt Test)" ON)
option(VISUALDSA_DEPLOY_QT   "Copy Qt runtime DLLs next to the app (Windows)" ON)
option(VISUALDSA_BUILD_TOOLS "Build the screenshot generator (tools/)"      OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Warnings that reveal real defects; source files are UTF-8 (they contain
# symbols such as ← and ≠ in pseudocode).
add_library(visualdsa_warnings INTERFACE)
if(MSVC)
    target_compile_options(visualdsa_warnings INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(visualdsa_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Layer A: core algorithm engine (pure C++17, no Qt dependency)
# ---------------------------------------------------------------------------
add_library(visualdsa_core STATIC
    src/core/BSTNode.h
    src/core/TreeSnapshot.h
    src/core/AlgorithmEvent.h
    src/core/OperationResult.h
    src/core/TreeValidator.h
    src/core/Pseudocode.h
    src/core/Pseudocode.cpp
    src/core/BinarySearchTree.h
    src/core/BinarySearchTree.cpp
    src/utils/Validation.h
    src/utils/Validation.cpp
)
target_include_directories(visualdsa_core PUBLIC src/core src/utils)
target_link_libraries(visualdsa_core PRIVATE visualdsa_warnings)

if(VISUALDSA_BUILD_APP OR VISUALDSA_BUILD_TESTS)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
endif()

if(VISUALDSA_BUILD_APP)
    # -----------------------------------------------------------------------
    # Layer B: visualization engine (layout, canvas, playback)
    # -----------------------------------------------------------------------
    add_library(visualdsa_visualization STATIC
        src/visualization/Palette.h
        src/visualization/Palette.cpp
        src/visualization/TreeLayout.h
        src/visualization/TreeLayout.cpp
        src/visualization/TreeCanvas.h
        src/visualization/TreeCanvas.cpp
        src/visualization/AnimationController.h
        src/visualization/AnimationController.cpp
    )
    target_include_directories(visualdsa_visualization PUBLIC src/visualization)
    target_link_libraries(visualdsa_visualization
        PUBLIC visualdsa_core Qt6::Widgets
        PRIVATE visualdsa_warnings)

    # -----------------------------------------------------------------------
    # Layer C: presentation (main window and panels)
    # -----------------------------------------------------------------------
    add_library(visualdsa_ui STATIC
        src/ui/Icons.h
        src/ui/Icons.cpp
        src/ui/MainWindow.h
        src/ui/MainWindow.cpp
        src/ui/OperationPanel.h
        src/ui/OperationPanel.cpp
        src/ui/PlaybackPanel.h
        src/ui/PlaybackPanel.cpp
        src/ui/PseudocodePanel.h
        src/ui/PseudocodePanel.cpp
        src/ui/ExplanationPanel.h
        src/ui/ExplanationPanel.cpp
        src/ui/ComplexityPanel.h
        src/ui/ComplexityPanel.cpp
        src/ui/HistoryPanel.h
        src/ui/HistoryPanel.cpp
        src/ui/Theme.h
        src/ui/Theme.cpp
        resources/resources.qrc
    )
    target_include_directories(visualdsa_ui PUBLIC src/ui)
    target_link_libraries(visualdsa_ui
        PUBLIC visualdsa_visualization Qt6::Widgets
        PRIVATE visualdsa_warnings)

    add_executable(VisualDSA WIN32 src/main.cpp)
    target_link_libraries(VisualDSA PRIVATE visualdsa_ui visualdsa_warnings)

    if(WIN32 AND VISUALDSA_DEPLOY_QT)
        find_program(VISUALDSA_WINDEPLOYQT windeployqt
            HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
        if(VISUALDSA_WINDEPLOYQT)
            add_custom_command(TARGET VisualDSA POST_BUILD
                COMMAND "${VISUALDSA_WINDEPLOYQT}" --no-translations --no-compiler-runtime
                        --no-system-d3d-compiler --no-opengl-sw "$<TARGET_FILE:VisualDSA>"
                COMMENT "Deploying Qt runtime next to VisualDSA.exe"
                VERBATIM)
        else()
            message(STATUS "windeployqt not found: add the Qt bin directory to PATH to run VisualDSA.exe")
        endif()
    endif()
endif()

if(VISUALDSA_BUILD_APP AND VISUALDSA_BUILD_TOOLS)
    # Regenerates the PNGs in screenshots/ by driving the real MainWindow.
    add_executable(capture_screenshots tools/capture_screenshots.cpp)
    target_link_libraries(capture_screenshots PRIVATE visualdsa_ui visualdsa_warnings)
endif()

if(VISUALDSA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
