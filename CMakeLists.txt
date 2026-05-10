cmake_minimum_required(VERSION 3.15)
project(com.polyphase.system.character.dialogue.core)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Polyphase Engine path resolution.
#
# Priority order:
#   1. -DPOLYPHASE_PATH=... or already set in parent script
#   2. POLYPHASE_PATH environment variable
#   3. Walk up from this CMakeLists looking for Polyphase.sln
#      (catches addon-under-engine and sibling-repo layouts)
#   4. Default install locations (C:/Polyphase, /opt/Polyphase)
#   5. Engine path captured at generation time (last resort)
# Walk up from this CMakeLists looking for either:
#   - PolyphaseConfig.cmake : project-local engine path written
#                             by the editor on project open. This
#                             is the canonical answer for addons
#                             living in <project>/Packages.
#   - Polyphase.sln         : engine source root, for addons
#                             checked out under or beside the
#                             engine repo.
function(_polyphase_find_via_walk OUT_VAR)
    set(_p "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(_i RANGE 1 6)
        get_filename_component(_p "${_p}" DIRECTORY)
        if(EXISTS "${_p}/PolyphaseConfig.cmake")
            include("${_p}/PolyphaseConfig.cmake")
            if(DEFINED POLYPHASE_PATH AND NOT POLYPHASE_PATH STREQUAL "")
                set(${OUT_VAR} "${POLYPHASE_PATH}" PARENT_SCOPE)
                return()
            endif()
        endif()
        if(EXISTS "${_p}/Polyphase.sln")
            set(${OUT_VAR} "${_p}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${OUT_VAR} "" PARENT_SCOPE)
endfunction()

if(NOT DEFINED POLYPHASE_PATH OR POLYPHASE_PATH STREQUAL "")
    if(DEFINED ENV{POLYPHASE_PATH} AND NOT "$ENV{POLYPHASE_PATH}" STREQUAL "")
        set(POLYPHASE_PATH "$ENV{POLYPHASE_PATH}")
    else()
        _polyphase_find_via_walk(_walk)
        if(NOT _walk STREQUAL "")
            set(POLYPHASE_PATH "${_walk}")
        elseif(WIN32 AND EXISTS "C:/Polyphase/Polyphase.exe")
            set(POLYPHASE_PATH "C:/Polyphase")
        elseif(WIN32 AND EXISTS "C:/Program Files/Polyphase/Polyphase.exe")
            set(POLYPHASE_PATH "C:/Program Files/Polyphase")
        elseif(UNIX AND EXISTS "/opt/Polyphase/PolyphaseEditor")
            set(POLYPHASE_PATH "/opt/Polyphase")
        else()
            set(POLYPHASE_PATH "M:/Projects/Polyphase/Polyphase/CODE/mergePolyphase/polyphase-engine/")
        endif()
    endif()
endif()
message(STATUS "Polyphase: using POLYPHASE_PATH = ${POLYPHASE_PATH}")

# Gather source files
file(GLOB_RECURSE SOURCES "Source/*.cpp" "Source/*.c")
file(GLOB_RECURSE HEADERS "Source/*.h" "Source/*.hpp")

# Create shared library
add_library(com.polyphase.system.character.dialogue.core SHARED ${SOURCES} ${HEADERS})

# Find Vulkan SDK
if(DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
else()
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        get_filename_component(VULKAN_SDK_PATH ${Vulkan_INCLUDE_DIRS} DIRECTORY)
    endif()
endif()

# Include directories
target_include_directories(com.polyphase.system.character.dialogue.core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${POLYPHASE_PATH}/Engine/Source
    ${POLYPHASE_PATH}/Engine/Source/Engine
    ${POLYPHASE_PATH}/Engine/Source/Plugins
    ${POLYPHASE_PATH}/External/Lua
    ${POLYPHASE_PATH}/External/glm
    ${POLYPHASE_PATH}/External/Imgui
    ${POLYPHASE_PATH}/External/ImGuizmo
    ${POLYPHASE_PATH}/External/bullet3/src
    ${POLYPHASE_PATH}/External/Assimp
    ${POLYPHASE_PATH}/External
)

# Add Vulkan SDK include path if found
if(VULKAN_SDK_PATH)
    target_include_directories(com.polyphase.system.character.dialogue.core PRIVATE ${VULKAN_SDK_PATH}/Include)
endif()

# Compile definitions
target_compile_definitions(com.polyphase.system.character.dialogue.core PRIVATE
    OCTAVE_PLUGIN_EXPORT
    EDITOR=1
    LUA_ENABLED=1
    GLM_FORCE_RADIANS
    PLATFORM_WINDOWS=1
    API_VULKAN=1
    NOMINMAX
    COMPOLYPHASESYSTEMCHARACTERDIALOGUECORE_EXPORTS
)

# Link against Polyphase import library and dependencies
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(POLYPHASE_DEV_LIB_PATH "${POLYPHASE_PATH}/Standalone/Build/Windows/x64/DebugEditor")
        set(POLYPHASE_DEV_LUA_PATH "${POLYPHASE_PATH}/External/Lua/Build/Windows/x64/DebugEditor")
    else()
        set(POLYPHASE_DEV_LIB_PATH "${POLYPHASE_PATH}/Standalone/Build/Windows/x64/ReleaseEditor")
        set(POLYPHASE_DEV_LUA_PATH "${POLYPHASE_PATH}/External/Lua/Build/Windows/x64/ReleaseEditor")
    endif()
    target_link_directories(com.polyphase.system.character.dialogue.core PRIVATE
        "${POLYPHASE_PATH}"
        "${POLYPHASE_PATH}/lib"
        "${POLYPHASE_DEV_LIB_PATH}"
        "${POLYPHASE_DEV_LUA_PATH}")
    target_link_libraries(com.polyphase.system.character.dialogue.core PRIVATE Polyphase Lua)
endif()
