#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_set>

#include <fmt/core.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    vk::KHRSwapchainExtensionName};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

static std::vector<char> readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
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
  vk::SwapchainKHR swapChain;
  std::vector<vk::Image> swapChainImages;
  vk::Format swapChainFormat;
  vk::Extent2D swapChainExtent;
  std::vector<vk::ImageView> swapChainImageViews;
  vk::RenderPass renderPass;
  vk::PipelineLayout pipelineLayout;
  vk::Pipeline graphicsPipeline;
  std::vector<vk::Framebuffer> swapChainFrameBuffers;
  vk::CommandPool commandPool;
  std::vector<vk::CommandBuffer> commandBuffers;
  std::vector<vk::Semaphore> imageAvailableSemaphores;
  std::vector<vk::Semaphore> renderFinishedSemaphores;
  std::vector<vk::Fence> inFlightFences;
  uint32_t current_frame = 0;
  bool framebufferResized = false;

public:
  Application() {
    initWindow();
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
  }

  void loop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      drawFrame();
    }

    device.waitIdle();
  }

  void cleanup() {
    cleanupSwapChain();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      device.destroySemaphore(imageAvailableSemaphores[i]);
      device.destroySemaphore(renderFinishedSemaphores[i]);
      device.destroyFence(inFlightFences[i]);
    }
    device.destroyCommandPool(commandPool);
    device.destroyPipeline(graphicsPipeline);
    device.destroyPipelineLayout(pipelineLayout);
    device.destroyRenderPass(renderPass);
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "mcanim_vk", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  }

  static void framebufferResizeCallback(GLFWwindow *window, int width,
                                        int height) {
    auto *app =
        reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
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

  SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device) {
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);

    return details;
  }

  bool isDeviceSuitable(vk::PhysicalDevice device) {
    // auto properties = device.getProperties();
    // auto features = device.getFeatures();
    auto queueFamilies = findQueueFamilies(device);

    std::unordered_set<std::string> missingExtensions(deviceExtensions.begin(),
                                                      deviceExtensions.end());
    for (auto extension : device.enumerateDeviceExtensionProperties()) {
      missingExtensions.erase(extension.extensionName);
    }
    if (!missingExtensions.empty())
      return false;

    auto swapChainSupport = querySwapChainSupport(device);

    return queueFamilies.isComplete() && !swapChainSupport.formats.empty() &&
           !swapChainSupport.presentModes.empty();
  }

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
          availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  vk::PresentModeKHR chooseSwapPresentMode(
      const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode : availablePresentModes) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        return availablePresentMode;
      }
    }

    return vk::PresentModeKHR::eFifo;
  }

  vk::Extent2D
  chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      VkExtent2D actualExtent = {static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height)};

      actualExtent.width =
          std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                     capabilities.maxImageExtent.width);
      actualExtent.height =
          std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.height);

      return actualExtent;
    }
  }

  void createSwapChain() {
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(physicalDevice);
    vk::SurfaceFormatKHR surfaceFormat =
        chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode =
        chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo(
        {}, surface, imageCount, surfaceFormat.format, surfaceFormat.colorSpace,
        extent, 1, vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive, 0, nullptr,
        swapChainSupport.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, vk::True, nullptr);

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamiltIndices[] = {indices.graphicsFamily.value(),
                                     indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamiltIndices;
    }

    if (device.createSwapchainKHR(&createInfo, nullptr, &swapChain) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("failed to create swap chain");
    }

    swapChainImages = device.getSwapchainImagesKHR(swapChain);
    swapChainFormat = surfaceFormat.format;
    swapChainExtent = extent;
  }

  void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
      vk::ImageSubresourceRange subresourceRange(
          vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
      vk::ImageViewCreateInfo createInfo({}, swapChainImages[i],
                                         vk::ImageViewType::e2D,
                                         swapChainFormat, {}, subresourceRange);

      swapChainImageViews[i] = device.createImageView(createInfo);
    }
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

    vk::DeviceCreateInfo createInfo(
        {}, queueCreateInfos.size(), queueCreateInfos.data(), 0, nullptr,
        deviceExtensions.size(), deviceExtensions.data(), &deviceFeatures);
    if (physicalDevice.createDevice(&createInfo, nullptr, &device) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("failed to create logical device");
    }

    graphicsQueue = device.getQueue(*indices.graphicsFamily, 0);
    presentQueue = device.getQueue(*indices.presentFamily, 0);
  }

  void createRenderPass() {
    vk::AttachmentDescription colorAttachment(
        {}, swapChainFormat, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
    vk::AttachmentReference colorAttachmentRef(
        0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics);
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency(
        vk::SubpassExternal, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite);

    vk::RenderPassCreateInfo createInfo({}, 1, &colorAttachment, 1, &subpass, 1,
                                        &dependency);
    renderPass = device.createRenderPass(createInfo);
  }

  vk::ShaderModule createShaderModule(const std::vector<char> &code) {
    vk::ShaderModuleCreateInfo createInfo(
        {}, code.size(), reinterpret_cast<const uint32_t *>(code.data()));
    return device.createShaderModule(createInfo);
  }

  void createGraphicsPipeline() {
    auto vertShaderCode = readFile("assets/shader.vert.spv");
    auto fragShaderCode = readFile("assets/shader.frag.spv");
    auto vertShaderModule = createShaderModule(vertShaderCode);
    auto fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
        {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
        {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main");
    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates.size(),
                                                    dynamicStates.data());

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 0, nullptr, 0,
                                                           nullptr);

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        {}, vk::PrimitiveTopology::eTriangleList, vk::False);

    // We don't need to pass it in here because it's dynamic state, so we'll
    // specify them at render time
    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1,
                                                      nullptr);

    vk::PipelineRasterizationStateCreateInfo rasterizer(
        {}, vk::False, vk::False, vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise, vk::False, 0.0f,
        0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling(
        {}, vk::SampleCountFlagBits::e1, vk::False, 1.0f, nullptr, vk::False,
        vk::False);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment(vk::False);
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo colorBlending(
        {}, vk::False, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo({}, 0, nullptr, 0,
                                                          nullptr);
    pipelineLayout = device.createPipelineLayout(pipelineLayoutCreateInfo);

    vk::GraphicsPipelineCreateInfo createInfo(
        {}, 2, shaderStages, &vertexInputInfo, &inputAssembly, nullptr,
        &viewportState, &rasterizer, &multisampling, nullptr, &colorBlending,
        &dynamicState, pipelineLayout, renderPass, 0);

    graphicsPipeline =
        device.createGraphicsPipelines(nullptr, {createInfo}).value[0];

    device.destroyShaderModule(vertShaderModule);
    device.destroyShaderModule(fragShaderModule);
  }

  void createFramebuffers() {
    swapChainFrameBuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
      vk::ImageView attachments[] = {swapChainImageViews[i]};

      vk::FramebufferCreateInfo createInfo({}, renderPass, 1, attachments,
                                           swapChainExtent.width,
                                           swapChainExtent.height, 1);
      swapChainFrameBuffers[i] = device.createFramebuffer(createInfo);
    }
  }

  void createCommandPool() {
    auto queueFamilyIndices = findQueueFamilies(physicalDevice);
    vk::CommandPoolCreateInfo createInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        *queueFamilyIndices.graphicsFamily);
    commandPool = device.createCommandPool(createInfo);
  }

  void createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocInfo(
        commandPool, vk::CommandBufferLevel::ePrimary, 2);
    commandBuffers = device.allocateCommandBuffers(allocInfo);
  }

  void recordCommandBuffer(vk::CommandBuffer commandBuffer,
                           uint32_t imageIndex) {
    vk::CommandBufferBeginInfo beginInfo({}, nullptr);
    if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
      throw std::runtime_error("failed to begin recording command buffer");
    }

    vk::ClearValue clearColor = {{0.0f, 0.0f, 1.0f, 1.0f}};
    vk::RenderPassBeginInfo renderPassInfo(
        renderPass, swapChainFrameBuffers[imageIndex],
        {{0, 0}, swapChainExtent}, 1, &clearColor);
    commandBuffer.beginRenderPass(&renderPassInfo,
                                  vk::SubpassContents::eInline);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               graphicsPipeline);

    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width),
                          static_cast<float>(swapChainExtent.height), 0.0f,
                          1.0f);
    commandBuffer.setViewport(0, {viewport});

    VkRect2D scissor{{0, 0}, swapChainExtent};
    commandBuffer.setScissor(0, {scissor});

    commandBuffer.draw(3, 1, 0, 0);
    commandBuffer.endRenderPass();
    commandBuffer.end();
  }

  void createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      if (device.createSemaphore(&semaphoreInfo, nullptr,
                                 &imageAvailableSemaphores[i]) !=
              vk::Result::eSuccess ||
          device.createSemaphore(&semaphoreInfo, nullptr,
                                 &renderFinishedSemaphores[i]) !=
              vk::Result::eSuccess ||
          device.createFence(&fenceInfo, nullptr, &inFlightFences[i]) !=
              vk::Result::eSuccess) {
        throw std::runtime_error("failed to create semaphores");
      }
    }
  }

  void cleanupSwapChain() {
    for (auto framebuffer : swapChainFrameBuffers) {
      device.destroyFramebuffer(framebuffer);
    }
    for (auto imageView : swapChainImageViews) {
      device.destroyImageView(imageView);
    }
    device.destroySwapchainKHR(swapChain);
  }

  void recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    // Wait until the framebuffer has a nonzero size
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    device.waitIdle();
    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createFramebuffers();
  }

  void drawFrame() {
    if (device.waitForFences({inFlightFences[current_frame]}, vk::True,
                             std::numeric_limits<uint64_t>::max()) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("failed to wait for in flight fence");
    }

    auto acquireResult = device.acquireNextImageKHR(
        swapChain, std::numeric_limits<uint64_t>::max(),
        imageAvailableSemaphores[current_frame], nullptr);
    if (acquireResult.result == vk::Result::eErrorOutOfDateKHR) {
      recreateSwapChain();
      return;
    } else if (acquireResult.result != vk::Result::eSuccess &&
               acquireResult.result != vk::Result::eSuboptimalKHR) {
      throw std::runtime_error("failed to acquire swap chain image");
    }
    uint32_t imageIndex = acquireResult.value;

    device.resetFences(inFlightFences[current_frame]);

    commandBuffers[current_frame].reset();
    recordCommandBuffer(commandBuffers[current_frame], imageIndex);

    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo submitInfo(1, &imageAvailableSemaphores[current_frame],
                              waitStages, 1, &commandBuffers[current_frame], 1,
                              &renderFinishedSemaphores[current_frame]);
    if (graphicsQueue.submit(1, &submitInfo, inFlightFences[current_frame]) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("failed to submit draw command buffer");
    }

    // TODO: If the second frame finishes first, the first frame will have to
    // finish rendering and presenting first before the second frame can start
    // presenting.
    vk::PresentInfoKHR presentInfo(1, &renderFinishedSemaphores[current_frame],
                                   1, &swapChain, &imageIndex, nullptr);
    auto presentResult = presentQueue.presentKHR(&presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
        presentResult == vk::Result::eSuboptimalKHR || framebufferResized) {
      recreateSwapChain();
      framebufferResized = false;
    } else if (presentResult != vk::Result::eSuccess) {
      throw std::runtime_error("failed to present swap chain image");
    }

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
  }
};

int main() {
  Application app;
  app.loop();
}
