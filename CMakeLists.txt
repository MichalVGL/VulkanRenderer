cmake_minimum_required(VERSION 3.27)

# Project Name
project(GP2_Vulkan_Renderer)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# DEPENDENCIES ==========================================

include(FetchContent)

# Fetch glfw
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# Fetch glm
FetchContent_Declare(
    glm
    URL https://github.com/g-truc/glm/releases/download/1.0.3/glm-1.0.3.zip
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# Fetch stb image 
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb
    GIT_TAG "28d546d5eb77d4585506a20480f4de2e706dff4c"
    GIT_SHALLOW    TRUE
)
FetchContent_Populate(stb)

# Fetch tiny_obj 
FetchContent_Declare(
    tinyobj
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader
    GIT_TAG v1.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_Populate(tinyobj)

# Vulkan
find_package(Vulkan REQUIRED)

add_subdirectory(Tutorial)
add_subdirectory(Renderer)