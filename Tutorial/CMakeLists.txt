cmake_minimum_required(VERSION 3.27)
project(Tutorial)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SHADERS COMPILATION=========================================

file(GLOB_RECURSE SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.comp")
message("Found shaders: ${SHADERS}")

set(SPIRV_FILES)
foreach(shader ${SHADERS})
    # get the relative path of the shader file
    file(RELATIVE_PATH relPath "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders" "${shader}")

    # append .spv extension
    set(outputFile "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/${relPath}.spv")

    # ensure the output directory exists
    get_filename_component(outputDir "${outputFile}" DIRECTORY)
    file(MAKE_DIRECTORY "${outputDir}")

    # add custom command to compile the shader (with -g for debug info)
    add_custom_command(
        OUTPUT ${outputFile}
        COMMAND glslangValidator -V -g ${shader} -I${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/ -o ${outputFile}
        DEPENDS ${shader}
        COMMENT "Compiling shader: ${filename} to SPIR-V"
        VERBATIM
    )

    list(APPEND SPIRV_FILES ${outputFile}) # add files for dependency
endforeach()

## DEPENDENCIES ==========================================
#
#include(FetchContent)
#
## Fetch glfw
#FetchContent_Declare(
#    glfw
#    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
#    GIT_SHALLOW    TRUE
#)
#FetchContent_MakeAvailable(glfw)
#
## Fetch glm
#FetchContent_Declare(
#    glm
#    URL https://github.com/g-truc/glm/releases/download/1.0.3/glm-1.0.3.zip
#    GIT_SHALLOW    TRUE
#)
#FetchContent_MakeAvailable(glm)
#
## Fetch stb image 
#FetchContent_Declare(
#    stb
#    GIT_REPOSITORY https://github.com/nothings/stb
#    GIT_TAG "28d546d5eb77d4585506a20480f4de2e706dff4c"
#    GIT_SHALLOW    TRUE
#)
#FetchContent_Populate(stb)
#
## Fetch tiny_obj 
#FetchContent_Declare(
#    tinyobj
#    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader
#    GIT_TAG v1.0.6
#    GIT_SHALLOW    TRUE
#)
#FetchContent_Populate(tinyobj)
#
## Vulkan
#find_package(Vulkan REQUIRED)

set(SOURCES
    "src/main.cpp"  
    "src/HelloTriangleApp.cpp"
    "src/TutorialSrc/HelloTriangle_Tutorial.hpp"
    "src/TutorialSrc/HelloTriangle_Tutorial2.hpp")

add_executable(${PROJECT_NAME} ${SOURCES})

# include header only libraries
target_include_directories(${PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR} ${tinyobj_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE glfw glm Vulkan::Vulkan)

# SHADERS DEPENDENCY =============================================

add_custom_target(Tutorial_CompileShaders ALL DEPENDS ${SPIRV_FILES})
add_dependencies(${PROJECT_NAME} Tutorial_CompileShaders)

# COPY RESOURCES =============================================

add_custom_target(Tutorial_CopyResources ALL DEPENDS Tutorial_CompileShaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMENT "Copying resources folder to build directory")