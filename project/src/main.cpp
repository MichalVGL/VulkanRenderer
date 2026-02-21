#include "HelloTriangleApp.h"
#include "TutorialSrc/HelloTriangleTut.hpp"

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