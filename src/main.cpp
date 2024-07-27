#include <stdexcept>

#include <fmt/core.h>
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Application {
private:
  GLFWwindow *window;
  vk::Instance instance;

public:
  Application() {
    initWindow();
    initVulkan();
  }

  void loop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    instance.destroy(nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
  }

private:
  void initWindow() {
    glfwInit();

    // glfw is made for opengl, but we're using vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(800, 600, "mcanim_vk", nullptr, nullptr);
  }

  void initVulkan() {
    const vk::ApplicationInfo appInfo("mcanim_vk", vk::ApiVersion10,
                                      "No Engine", vk::ApiVersion10,
                                      vk::ApiVersion10);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    const vk::InstanceCreateInfo createInfo({}, &appInfo, 0, nullptr,
                                            glfwExtensionCount, glfwExtensions);

    if (vk::createInstance(&createInfo, nullptr, &instance) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("failed to create instance!");
    }

    for (auto extension : vk::enumerateInstanceExtensionProperties(nullptr)) {
      fmt::println("Found vulkan extension: {}", extension.extensionName.data());
    }
  }
};

int main() {
  Application app;
  app.loop();
}
