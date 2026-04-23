#ifndef PCH_H
#define PCH_H

//most used std
#include <iostream>
#include <memory>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <cmath>
#include <filesystem>

//vulkan and glfw
#define GLFW_INCLUDE_VULKAN 
#include <GLFW/glfw3.h>

//vma
#include <vk_mem_alloc.h>

//glm
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

//assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#endif // !PCH_H