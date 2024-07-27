#include <optional>
#include <stdexcept>
#include <unordered_set>

#include <fmt/core.h>
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

class Application {
private:
  GLFWwindow *window;
  vk::Instance instance;
  vk::PhysicalDevice physicalDevice;
  vk::Device device;
  vk::Queue graphicsQueue;
  vk::Queue presentQueue;
  vk::SurfaceKHR surface;

public:
  Application() {
    initWindow();
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
  }

  void loop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    device.destroy();
    instance.destroySurfaceKHR(surface);
    instance.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
  }

private:
  void initWindow() {
    glfwInit();

    // glfw is made for opengl, but we're using vulkan
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "mcanim_vk", nullptr, nullptr);
  }

  void createInstance() {
    const vk::ApplicationInfo appInfo("mcanim_vk", vk::ApiVersion10,
                                      "No Engine", vk::ApiVersion10,
                                      vk::ApiVersion10);

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> availableLayers;
    if (enableValidationLayers) {
      for (auto layer : vk::enumerateInstanceLayerProperties()) {
        for (auto *requestedLayer : validationLayers) {
          if (strcmp(layer.layerName, requestedLayer) == 0) {
            availableLayers.push_back(requestedLayer);
            fmt::println("Adding validation layer: {}", requestedLayer);
          }
        }
      }
    }

    const vk::InstanceCreateInfo createInfo(
        {}, &appInfo, availableLayers.size(), availableLayers.data(),
        glfwExtensionCount, glfwExtensions);

    if (vk::createInstance(&createInfo, nullptr, &instance) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("failed to create instance!");
    }
  }

  void createSurface() {
    VkSurfaceKHR surface_;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface_) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface");
    }
    surface = surface_;
  }

  bool isDeviceSuitable(vk::PhysicalDevice device) {
    // auto properties = device.getProperties();
    // auto features = device.getFeatures();
    auto queueFamilies = findQueueFamilies(device);
    return queueFamilies.isComplete();
  }

  void pickPhysicalDevice() {
    for (auto device : instance.enumeratePhysicalDevices()) {
      if (isDeviceSuitable(device)) {
        physicalDevice = device;
      }
    }

    if (!physicalDevice) {
      throw std::runtime_error("could not find a suitable physical device");
    }
  }

  QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t i = 0;
    for (const auto &queueFamily : device.getQueueFamilyProperties()) {
      if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
        indices.graphicsFamily = i;
      }
      if (device.getSurfaceSupportKHR(i, surface)) {
        indices.presentFamily = i;
      }

      if (indices.isComplete())
        break;
      i++;
    }

    return indices;
  }

  void createLogicalDevice() {
    auto indices = findQueueFamilies(physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::unordered_set<uint32_t> uniqueQueueFamilies = {*indices.graphicsFamily,
                                                        *indices.presentFamily};
    auto queuePriority = 1.0f;
    for (auto queueFamily : uniqueQueueFamilies) {
      queueCreateInfos.push_back({{}, queueFamily, 1, &queuePriority});
    }

    vk::PhysicalDeviceFeatures deviceFeatures;

    vk::DeviceCreateInfo createInfo({}, queueCreateInfos.size(),
                                    queueCreateInfos.data(), 0, nullptr, 0,
                                    nullptr, &deviceFeatures);
    if (physicalDevice.createDevice(&createInfo, nullptr, &device) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("failed to create logical device");
    }

    graphicsQueue = device.getQueue(*indices.graphicsFamily, 0);
    presentQueue = device.getQueue(*indices.presentFamily, 0);
  }
};

int main() {
  Application app;
  app.loop();
}
