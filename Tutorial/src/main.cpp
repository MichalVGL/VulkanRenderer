#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "HelloTriangleApp.h"
#include "TutorialSrc/HelloTriangle_Tutorial.hpp"
#include "TutorialSrc/HelloTriangle_Tutorial2.hpp"

// ctrl M + O to collapse all functions in the file

int main() {
    //HelloTriangleApplication app{};
    HelloTriangleApplication_Tutorial app{};

	//app.checkValidationLayerSupport();
	//app.printAvailableExtensions();

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}