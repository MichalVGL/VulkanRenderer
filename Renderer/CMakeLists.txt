cmake_minimum_required(VERSION 3.27)
project(Renderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SHADERS COMPILATION=========================================

file(GLOB_RECURSE SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*.comp"
)
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

# DEPENDENCIES ==========================================

include(FetchContent)

# Fetch VMA
FetchContent_Declare(
    VulkanMemoryAllocator
    URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.zip
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# Fetch assimp
FetchContent_Declare(
    assimp
    URL https://github.com/assimp/assimp/archive/refs/tags/v6.0.4.zip
    GIT_SHALLOW TRUE
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

set(SOURCES
    "src/main.cpp"  
    "src/Renderer.cpp" 
    "src/Device.cpp" "src/vma.cpp" "src/Swapchain.cpp" "src/VulkanUtils.cpp" "src/Window.cpp" "src/Frame.cpp" "src/InputHandler.cpp" "src/Image.cpp" "src/Camera.cpp" "src/Pipeline.cpp" "src/Scene.cpp")


# create the executable and defined macros

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# include header only libraries
target_include_directories(${PROJECT_NAME} PRIVATE ${stb_SOURCE_DIR} ${tinyobj_SOURCE_DIR})

# setup the precompiled header
target_precompile_headers(${PROJECT_NAME} PRIVATE "src/pch.h")

target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw 
    glm 
    Vulkan::Vulkan 
    VulkanMemoryAllocator 
    assimp::assimp
)

# SHADERS DEPENDENCY =============================================

add_custom_target(Renderer_CompileShaders ALL DEPENDS ${SPIRV_FILES})
add_dependencies(${PROJECT_NAME} Renderer_CompileShaders)

# COPY RESOURCES =============================================

add_custom_target(Renderer_CopyResources ALL DEPENDS Renderer_CompileShaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMENT "Copying resources folder to build directory"
)