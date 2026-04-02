#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>
#include <optional>

#include "HelloTriangleDefinitions.h"

class HelloTriangleApplication {
public:
    void run() {
		initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

    bool checkValidationLayerSupport();
	void printAvailableExtensions();

private:

    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void createSurface();
	void createSwapChain();
	void createImageViews();

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    GLFWwindow* window{};
    VkInstance instance{};
    VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
    VkDevice device{};
	VkQueue graphicsQueue{};

    VkDebugUtilsMessengerEXT debugMessenger{};

    VkSurfaceKHR surface{};
    VkQueue presentQueue{};

    VkSwapchainKHR swapChain{};
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkImageView> swapChainImageViews;
};