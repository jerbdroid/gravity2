#include "vulkan_rendering_device.hpp"

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "source/common/logging/logger.hpp"
#include "source/common/templates/bitmask.hpp"
#include "source/platform/window/window_context.hpp"
#include "source/rendering/common/rendering_type.hpp"

#include "GLFW/glfw3.h"
#include "gsl/gsl"
#include "magic_enum.hpp"
#include "source/rendering/device/rendering_device.hpp"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_enums.hpp"
#include "vulkan/vulkan_funcs.hpp"
#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_structs.hpp"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <expected>
#include <system_error>
#include <utility>
#include <vector>

#undef GRAVITY_MODULE_NAME
#define GRAVITY_MODULE_NAME "vulkan"

namespace {

using namespace gravity;

struct VulkanEnableLayerExtension {
  std::span<const char*> layers_;
  std::span<const char*> extensions_;
};

auto supported_vulkan_version() -> uint32_t {
  auto result{ vk::enumerateInstanceVersion() };
  if (result.result != vk::Result::eSuccess) {
    return VK_VERSION_1_0;
  }
  return result.value;
}

auto getRequiredInstanceLayers() -> std::unordered_map<std::string, bool> {
  std::unordered_map<std::string, bool> layers;

#if !defined(NDEBUG)
  layers["VK_LAYER_KHRONOS_validation"] = true;
  // layers["VK_LAYER_LUNARG_api_dump"] = true;
#endif

  return layers;
}

auto getRequiredInstanceExtensions() -> std::unordered_map<std::string, bool> {
  uint32_t extension_count{ 0 };

  const auto** glfw_required_extensions{ glfwGetRequiredInstanceExtensions(&extension_count) };

  std::unordered_map<std::string, bool> extensions;

  for (const auto* const extension :
       std::span<const char*>(glfw_required_extensions, extension_count)) {
    extensions.emplace(extension, true);
  }

  // VK_KHR_multiview requires VK_KHR_get_physical_device_properties2.
  extensions[VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME] = true;

#if !defined(NDEBUG)
  extensions[VK_EXT_DEBUG_UTILS_EXTENSION_NAME] = true;
#endif

  return extensions;
}

VKAPI_ATTR auto VKAPI_CALL debugMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    VkDebugUtilsMessengerCallbackDataEXT const* callback_data, void* /*user_data*/) -> VkBool32 {

  auto message = [message_severity, message_types, &callback_data]() {
    std::ostringstream message;

    message << vk::to_string(
                   static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(message_severity))
            << ": " << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(message_types))
            << ":\n";
    message << std::string("\t") << "messageIDName   = <" << callback_data->pMessageIdName << ">\n";
    message << std::string("\t") << "messageIdNumber = " << callback_data->messageIdNumber << "\n";
    message << std::string("\t") << "message         = <" << callback_data->pMessage << ">\n";
    if (0 < callback_data->queueLabelCount) {
      message << std::string("\t") << "Queue Labels:\n";

      for (const auto& queue_label : std::span<const VkDebugUtilsLabelEXT>(
               callback_data->pQueueLabels, callback_data->queueLabelCount)) {
        message << std::string("\t\t") << "labelName = <" << queue_label.pLabelName << ">\n";
      }
    }

    if (0 < callback_data->cmdBufLabelCount) {
      message << std::string("\t") << "CommandBuffer Labels:\n";
      for (const auto& cmd_buffer_label : std::span<const VkDebugUtilsLabelEXT>(
               callback_data->pCmdBufLabels, callback_data->cmdBufLabelCount)) {
        message << std::string("\t\t") << "labelName = <" << cmd_buffer_label.pLabelName << ">\n";
      }
    }

    if (0 < callback_data->objectCount) {

      message << std::string("\t") << "Objects:\n";
      size_t index = 0;
      for (const auto& object : std::span<const VkDebugUtilsObjectNameInfoEXT>(
               callback_data->pObjects, callback_data->objectCount)) {
        message << std::string("\t\t") << "Object " << index++ << "\n";
        message << std::string("\t\t\t") << "objectType   = "
                << vk::to_string(static_cast<vk::ObjectType>(object.objectType)) << "\n";
        message << std::string("\t\t\t") << "objectHandle = " << object.objectHandle << "\n";
        if (object.pObjectName != nullptr) {
          message << std::string("\t\t\t") << "objectName   = <" << object.pObjectName << ">\n";
        }
      }
    }

    return message.str();
  };

  switch (message_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      LOG_TRACE(message());
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      LOG_INFO(message());
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      LOG_WARN(message());
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      LOG_ERROR(message());
      break;
    default:
      LOG_TRACE(message());
      break;
  }

  return 0U;
}

auto getPhysicalDeviceLimitsString(const vk::PhysicalDeviceLimits& limits) -> std::string {
  std::stringstream string_builder;
  string_builder << "\n\tmaxImageDimension1D: " << limits.maxImageDimension1D;
  string_builder << "\n\tmaxImageDimension2D: " << limits.maxImageDimension2D;
  string_builder << "\n\tmaxImageDimension3D: " << limits.maxImageDimension3D;
  string_builder << "\n\tmaxImageDimensionCube: " << limits.maxImageDimensionCube;
  string_builder << "\n\tmaxImageArrayLayers: " << limits.maxImageArrayLayers;
  string_builder << "\n\tmaxTexelBufferElements: " << limits.maxTexelBufferElements;
  string_builder << "\n\tmaxUniformBufferRange: " << limits.maxUniformBufferRange;
  string_builder << "\n\tmaxStorageBufferRange: " << limits.maxStorageBufferRange;
  string_builder << "\n\tmaxPushConstantsSize: " << limits.maxPushConstantsSize;
  string_builder << "\n\tmaxMemoryAllocationCount: " << limits.maxMemoryAllocationCount;
  string_builder << "\n\tmaxSamplerAllocationCount: " << limits.maxSamplerAllocationCount;
  string_builder << "\n\tbufferImageGranularity: " << limits.bufferImageGranularity;
  string_builder << "\n\tsparseAddressSpaceSize: " << limits.sparseAddressSpaceSize;
  string_builder << "\n\tmaxBoundDescriptorSets: " << limits.maxBoundDescriptorSets;
  string_builder << "\n\tmaxPerStageDescriptorSamplers: " << limits.maxPerStageDescriptorSamplers;
  string_builder << "\n\tmaxPerStageDescriptorUniformBuffers: "
                 << limits.maxPerStageDescriptorUniformBuffers;
  string_builder << "\n\tmaxPerStageDescriptorStorageBuffers: "
                 << limits.maxPerStageDescriptorStorageBuffers;
  string_builder << "\n\tmaxPerStageDescriptorSampledImages: "
                 << limits.maxPerStageDescriptorSampledImages;
  string_builder << "\n\tmaxPerStageDescriptorStorageImages: "
                 << limits.maxPerStageDescriptorStorageImages;
  string_builder << "\n\tmaxPerStageDescriptorInputAttachments: "
                 << limits.maxPerStageDescriptorInputAttachments;
  string_builder << "\n\tmaxPerStageResources: " << limits.maxPerStageResources;
  string_builder << "\n\tmaxDescriptorSetSamplers: " << limits.maxDescriptorSetSamplers;
  string_builder << "\n\tmaxDescriptorSetUniformBuffers: " << limits.maxDescriptorSetUniformBuffers;
  string_builder << "\n\tmaxDescriptorSetUniformBuffersDynamic: "
                 << limits.maxDescriptorSetUniformBuffersDynamic;
  string_builder << "\n\tmaxDescriptorSetStorageBuffers: " << limits.maxDescriptorSetStorageBuffers;
  string_builder << "\n\tmaxDescriptorSetStorageBuffersDynamic: "
                 << limits.maxDescriptorSetStorageBuffersDynamic;
  string_builder << "\n\tmaxDescriptorSetSampledImages: " << limits.maxDescriptorSetSampledImages;
  string_builder << "\n\tmaxDescriptorSetStorageImages: " << limits.maxDescriptorSetStorageImages;
  string_builder << "\n\tmaxDescriptorSetInputAttachments: "
                 << limits.maxDescriptorSetInputAttachments;
  string_builder << "\n\tmaxVertexInputAttributes: " << limits.maxVertexInputAttributes;
  string_builder << "\n\tmaxVertexInputBindings: " << limits.maxVertexInputBindings;
  string_builder << "\n\tmaxVertexInputAttributeOffset: " << limits.maxVertexInputAttributeOffset;
  string_builder << "\n\tmaxVertexInputBindingStride: " << limits.maxVertexInputBindingStride;
  string_builder << "\n\tmaxVertexOutputComponents: " << limits.maxVertexOutputComponents;
  string_builder << "\n\tmaxTessellationGenerationLevel: " << limits.maxTessellationGenerationLevel;
  string_builder << "\n\tmaxTessellationPatchSize: " << limits.maxTessellationPatchSize;
  string_builder << "\n\tmaxTessellationControlPerVertexInputComponents: "
                 << limits.maxTessellationControlPerVertexInputComponents;
  string_builder << "\n\tmaxTessellationControlPerVertexOutputComponents: "
                 << limits.maxTessellationControlPerVertexOutputComponents;
  string_builder << "\n\tmaxTessellationControlPerPatchOutputComponents: "
                 << limits.maxTessellationControlPerPatchOutputComponents;
  string_builder << "\n\tmaxTessellationControlTotalOutputComponents: "
                 << limits.maxTessellationControlTotalOutputComponents;
  string_builder << "\n\tmaxTessellationEvaluationInputComponents: "
                 << limits.maxTessellationEvaluationInputComponents;
  string_builder << "\n\tmaxTessellationEvaluationOutputComponents: "
                 << limits.maxTessellationEvaluationOutputComponents;
  string_builder << "\n\tmaxGeometryShaderInvocations: " << limits.maxGeometryShaderInvocations;
  string_builder << "\n\tmaxGeometryInputComponents: " << limits.maxGeometryInputComponents;
  string_builder << "\n\tmaxGeometryOutputComponents: " << limits.maxGeometryOutputComponents;
  string_builder << "\n\tmaxGeometryOutputVertices: " << limits.maxGeometryOutputVertices;
  string_builder << "\n\tmaxGeometryTotalOutputComponents: "
                 << limits.maxGeometryTotalOutputComponents;
  string_builder << "\n\tmaxFragmentInputComponents: " << limits.maxFragmentInputComponents;
  string_builder << "\n\tmaxFragmentOutputAttachments: " << limits.maxFragmentOutputAttachments;
  string_builder << "\n\tmaxFragmentDualSrcAttachments: " << limits.maxFragmentDualSrcAttachments;
  string_builder << "\n\tmaxFragmentCombinedOutputResources: "
                 << limits.maxFragmentCombinedOutputResources;
  string_builder << "\n\tmaxComputeSharedMemorySize: " << limits.maxComputeSharedMemorySize;
  string_builder << "\n\tmaxComputeWorkGroupCount X: " << limits.maxComputeWorkGroupCount[0];
  string_builder << "\n\tmaxComputeWorkGroupCount Y: " << limits.maxComputeWorkGroupCount[1];
  string_builder << "\n\tmaxComputeWorkGroupCount Z: " << limits.maxComputeWorkGroupCount[2];
  string_builder << "\n\tmaxComputeWorkGroupInvocations: " << limits.maxComputeWorkGroupInvocations;
  string_builder << "\n\tmaxComputeWorkGroupSize X: " << limits.maxComputeWorkGroupSize[0];
  string_builder << "\n\tmaxComputeWorkGroupSize Y: " << limits.maxComputeWorkGroupSize[1];
  string_builder << "\n\tmaxComputeWorkGroupSize Z: " << limits.maxComputeWorkGroupSize[2];
  string_builder << "\n\tsubPixelPrecisionBits: " << limits.subPixelPrecisionBits;
  string_builder << "\n\tsubTexelPrecisionBits: " << limits.subTexelPrecisionBits;
  string_builder << "\n\tmipmapPrecisionBits: " << limits.mipmapPrecisionBits;
  string_builder << "\n\tmaxDrawIndexedIndexValue: " << limits.maxDrawIndexedIndexValue;
  string_builder << "\n\tmaxDrawIndirectCount: " << limits.maxDrawIndirectCount;
  string_builder << "\n\tmaxSamplerLodBias: " << limits.maxSamplerLodBias;
  string_builder << "\n\tmaxSamplerAnisotropy: " << limits.maxSamplerAnisotropy;
  string_builder << "\n\tmaxViewports: " << limits.maxViewports;
  string_builder << "\n\tmaxViewportDimensions X: " << limits.maxViewportDimensions[0];
  string_builder << "\n\tmaxViewportDimensions Y: " << limits.maxViewportDimensions[1];
  string_builder << "\n\tviewportBoundsRange Minimum: " << limits.viewportBoundsRange[0];
  string_builder << "\n\tviewportBoundsRange Maximum: " << limits.viewportBoundsRange[1];
  string_builder << "\n\tviewportSubPixelBits: " << limits.viewportSubPixelBits;
  string_builder << "\n\tminMemoryMapAlignment: " << limits.minMemoryMapAlignment;
  string_builder << "\n\tminTexelBufferOffsetAlignment: " << limits.minTexelBufferOffsetAlignment;
  string_builder << "\n\tminUniformBufferOffsetAlignment: "
                 << limits.minUniformBufferOffsetAlignment;
  string_builder << "\n\tminStorageBufferOffsetAlignment: "
                 << limits.minStorageBufferOffsetAlignment;
  string_builder << "\n\tminTexelOffset: " << limits.minTexelOffset;
  string_builder << "\n\tmaxTexelOffset: " << limits.maxTexelOffset;
  string_builder << "\n\tminTexelGatherOffset: " << limits.minTexelGatherOffset;
  string_builder << "\n\tmaxTexelGatherOffset: " << limits.maxTexelGatherOffset;
  string_builder << "\n\tminInterpolationOffset: " << limits.minInterpolationOffset;
  string_builder << "\n\tmaxInterpolationOffset: " << limits.maxInterpolationOffset;
  string_builder << "\n\tsubPixelInterpolationOffsetBits: "
                 << limits.subPixelInterpolationOffsetBits;
  string_builder << "\n\tmaxFramebufferWidth: " << limits.maxFramebufferWidth;
  string_builder << "\n\tmaxFramebufferHeight: " << limits.maxFramebufferHeight;
  string_builder << "\n\tmaxFramebufferLayers: " << limits.maxFramebufferLayers;
  /*
  string_builder << "\n\tframebufferColorSampleCounts: "
     <<  limits.framebufferColorSampleCounts;
  string_builder << "\n\tframebufferDepthSampleCounts: "
     << limits.framebufferDepthSampleCounts;
  string_builder << "\n\tframebufferStencilSampleCounts: "
     << limits.framebufferStencilSampleCounts;
  string_builder << "\n\tframebufferNoAttachmentsSampleCounts: "
     << limits.framebufferNoAttachmentsSampleCounts;
  string_builder << "\n\tmaxColorAttachments: " << limits.maxColorAttachments;
  string_builder << "\n\tsampledImageColorSampleCounts: "
     << limits.sampledImageColorSampleCounts;
  string_builder << "\n\tsampledImageIntegerSampleCounts: "
     << limits.sampledImageIntegerSampleCounts;
  string_builder << "\n\tsampledImageDepthSampleCounts: "
     << limits.sampledImageDepthSampleCounts;
  string_builder << "\n\tsampledImageStencilSampleCounts: "
     << limits.sampledImageStencilSampleCounts;
  string_builder << "\n\tstorageImageSampleCounts: " <<
  limits.storageImageSampleCounts;
  */
  string_builder << "\n\tmaxSampleMaskWords: " << limits.maxSampleMaskWords;
  string_builder << "\n\ttimestampComputeAndGraphics: " << limits.timestampComputeAndGraphics;
  string_builder << "\n\ttimestampPeriod: " << limits.timestampPeriod;
  string_builder << "\n\tmaxClipDistances: " << limits.maxClipDistances;
  string_builder << "\n\tmaxCullDistances: " << limits.maxCullDistances;
  string_builder << "\n\tmaxCombinedClipAndCullDistances: "
                 << limits.maxCombinedClipAndCullDistances;
  string_builder << "\n\tdiscreteQueuePriorities: " << limits.discreteQueuePriorities;
  string_builder << "\n\tpointSizeRange Minimum: " << limits.pointSizeRange[0];
  string_builder << "\n\tpointSizeRange Maximum: " << limits.pointSizeRange[1];
  string_builder << "\n\tlineWidthRange Minimum: " << limits.lineWidthRange[0];
  string_builder << "\n\tlineWidthRange Maximum: " << limits.lineWidthRange[1];
  string_builder << "\n\tpointSizeGranularity: " << limits.pointSizeGranularity;
  string_builder << "\n\tlineWidthGranularity: " << limits.lineWidthGranularity;
  string_builder << "\n\tstrictLines: " << limits.strictLines;
  string_builder << "\n\tstandardSampleLocations: " << limits.standardSampleLocations;
  string_builder << "\n\toptimalBufferCopyOffsetAlignment: "
                 << limits.optimalBufferCopyOffsetAlignment;
  string_builder << "\n\toptimalBufferCopyRowPitchAlignment: "
                 << limits.optimalBufferCopyRowPitchAlignment;
  string_builder << "\n\tnonCoherentAtomSize: " << limits.nonCoherentAtomSize;

  return string_builder.str();
}

auto makeInstanceCreateInfoChain(
    const vk::ApplicationInfo& application_info,
    VulkanEnableLayerExtension& enabled_layers_extensions)
    -> vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> {
  vk::DebugUtilsMessageSeverityFlagsEXT severity_flags{
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
  };

  vk::DebugUtilsMessageTypeFlagsEXT message_type_flags{
    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
  };

  vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>
      instance_create_info{ { {},
                              &application_info,
                              enabled_layers_extensions.layers_,
                              enabled_layers_extensions.extensions_ },
                            { {}, severity_flags, message_type_flags, &debugMessageCallback } };
  return instance_create_info;
}

auto getDeviceRating(vk::PhysicalDevice device, vk::SurfaceKHR surface) -> int32_t {
  auto device_properties{ device.getProperties() };
  auto device_features{ device.getFeatures() };

  constexpr int32_t DedicatedGpuScore{ 200 };
  constexpr int32_t IntegratedGpuScore{ 50 };

  int32_t score{ 0 };
  if (device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
    score += DedicatedGpuScore;
  } else if (device_properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
    score += IntegratedGpuScore;
  } else {
    return 0;
  }

  bool surface_is_supported{ false };
  uint32_t index{ 0 };
  for (const auto& queue_family : device.getQueueFamilyProperties()) {
    if (!(queue_family.queueFlags & vk::QueueFlagBits::eGraphics)) {
      ++index;
      continue;
    }

    if (device.getSurfaceSupportKHR(index, surface).value != VK_FALSE) {
      surface_is_supported = true;
      break;
    }
    ++index;
  }

  if (!surface_is_supported) {
    return 0;
  }

  return score;
}

void displayPhysicalDeviceProperties(vk::PhysicalDevice device) {
  const auto& device_properties{ device.getProperties() };
  std::string device_name{ device_properties.deviceName.data() };

  LOG_INFO(
      "{}:\n\tvendor_id: {}\n\tdevice_id: {}\n\tdevice_type: "
      "{}\n\nLimits:{}",
      device_name, device_properties.vendorID, device_properties.deviceID,
      magic_enum::enum_name(device_properties.deviceType),
      getPhysicalDeviceLimitsString(device_properties.limits));
}

auto findGraphicsQueueFamilyIndex(std::vector<vk::QueueFamilyProperties> queue_family_properties)
    -> uint32_t {
  auto graphics_queue_family_property = std::find_if(
      queue_family_properties.begin(), queue_family_properties.end(),
      [](const vk::QueueFamilyProperties& queue_family_properties) {
        return queue_family_properties.queueFlags & vk::QueueFlagBits::eGraphics;
      });

  assert(graphics_queue_family_property != queue_family_properties.end());
  return static_cast<uint32_t>(
      std::distance(queue_family_properties.begin(), graphics_queue_family_property));
}

auto findGraphicsAndPresentQueueFamilyIndex(vk::PhysicalDevice device, vk::SurfaceKHR surface)
    -> std::expected<std::pair<uint32_t, uint32_t>, std::error_code> {
  auto queue_family_properties{ device.getQueueFamilyProperties() };
  assert(queue_family_properties.size() < std::numeric_limits<uint32_t>::max());

  auto graphics_queue_family_index{ findGraphicsQueueFamilyIndex(queue_family_properties) };

  if (device.getSurfaceSupportKHR(graphics_queue_family_index, surface).value != 0U) {
    return std::pair<uint32_t, uint32_t>{ graphics_queue_family_index,
                                          graphics_queue_family_index };
  }

  for (uint32_t index = 0; index < queue_family_properties.size(); index++) {
    if (device.getSurfaceSupportKHR(index, surface).value != 0U) {
      return std::pair<uint32_t, uint32_t>{ graphics_queue_family_index, index };
    }
  }

  LOG_ERROR("unable to find graphics and present queues on physical display device");
  return std::unexpected(gravity::Error::InternalError);
}

auto getRequiredDeviceExtensions(std::unordered_set<std::string>& enabled_instance_extension)
    -> std::unordered_map<std::string, bool> {
  std::unordered_map<std::string, bool> extensions;

  extensions[VK_KHR_SWAPCHAIN_EXTENSION_NAME] = true;

  if (enabled_instance_extension.find(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) !=
      enabled_instance_extension.end()) {
    extensions[VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME] = true;
    // VK_KHR_create_renderpass2 requires VK_KHR_multiview and
    // VK_KHR_maintenance2
    extensions[VK_KHR_MULTIVIEW_EXTENSION_NAME] = true;
    extensions[VK_KHR_MAINTENANCE_2_EXTENSION_NAME] = true;
    extensions[VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME] = true;
  }

  return extensions;
}

template <typename T, typename U>
void enableIfFeature(T& physical_device_features, const U& features) {
  if (features) {
    physical_device_features = features;
  }
}

auto getRequiredDeviceFeatures(const vk::PhysicalDevice& physical_device)
    -> DeviceFeaturesWithTimeline {
  auto available_device_features{ physical_device.getFeatures() };
  DeviceFeaturesWithTimeline device_features;

#define DEVICE_FEATURE_ENABLE_IF(x) \
  enableIfFeature(device_features.core_features_.x, available_device_features.x)

  DEVICE_FEATURE_ENABLE_IF(fullDrawIndexUint32);
  // DEVICE_FEATURE_ENABLE_IF(robustBufferAccess);
  DEVICE_FEATURE_ENABLE_IF(fullDrawIndexUint32);
  DEVICE_FEATURE_ENABLE_IF(imageCubeArray);
  DEVICE_FEATURE_ENABLE_IF(independentBlend);
  DEVICE_FEATURE_ENABLE_IF(geometryShader);
  DEVICE_FEATURE_ENABLE_IF(tessellationShader);
  DEVICE_FEATURE_ENABLE_IF(sampleRateShading);
  DEVICE_FEATURE_ENABLE_IF(dualSrcBlend);
  DEVICE_FEATURE_ENABLE_IF(logicOp);
  DEVICE_FEATURE_ENABLE_IF(multiDrawIndirect);
  DEVICE_FEATURE_ENABLE_IF(drawIndirectFirstInstance);
  DEVICE_FEATURE_ENABLE_IF(depthClamp);
  DEVICE_FEATURE_ENABLE_IF(depthBiasClamp);
  DEVICE_FEATURE_ENABLE_IF(fillModeNonSolid);
  DEVICE_FEATURE_ENABLE_IF(depthBounds);
  DEVICE_FEATURE_ENABLE_IF(wideLines);
  DEVICE_FEATURE_ENABLE_IF(largePoints);
  DEVICE_FEATURE_ENABLE_IF(alphaToOne);
  DEVICE_FEATURE_ENABLE_IF(multiViewport);
  DEVICE_FEATURE_ENABLE_IF(samplerAnisotropy);
  DEVICE_FEATURE_ENABLE_IF(textureCompressionETC2);
  DEVICE_FEATURE_ENABLE_IF(textureCompressionASTC_LDR);
  DEVICE_FEATURE_ENABLE_IF(textureCompressionBC);
  // DEVICE_FEATURE_ENABLE_IF(occlusionQueryPrecise);
  // DEVICE_FEATURE_ENABLE_IF(pipelineStatisticsQuery);
  DEVICE_FEATURE_ENABLE_IF(vertexPipelineStoresAndAtomics);
  DEVICE_FEATURE_ENABLE_IF(fragmentStoresAndAtomics);
  DEVICE_FEATURE_ENABLE_IF(shaderTessellationAndGeometryPointSize);
  DEVICE_FEATURE_ENABLE_IF(shaderImageGatherExtended);
  DEVICE_FEATURE_ENABLE_IF(shaderStorageImageExtendedFormats);
  // Intel Arc doesn't support shaderStorageImageMultisample (yet? could be a
  // driver thing), so it's better for Validation to scream at us if we use it.
  // Furthermore MSAA Storage is a huge red flag for performance.
  // DEVICE_FEATURE_ENABLE_IF(shaderStorageImageMultisample);
  DEVICE_FEATURE_ENABLE_IF(shaderStorageImageReadWithoutFormat);
  DEVICE_FEATURE_ENABLE_IF(shaderStorageImageWriteWithoutFormat);
  DEVICE_FEATURE_ENABLE_IF(shaderUniformBufferArrayDynamicIndexing);
  DEVICE_FEATURE_ENABLE_IF(shaderSampledImageArrayDynamicIndexing);
  DEVICE_FEATURE_ENABLE_IF(shaderStorageBufferArrayDynamicIndexing);
  DEVICE_FEATURE_ENABLE_IF(shaderStorageImageArrayDynamicIndexing);
  DEVICE_FEATURE_ENABLE_IF(shaderClipDistance);
  DEVICE_FEATURE_ENABLE_IF(shaderCullDistance);
  DEVICE_FEATURE_ENABLE_IF(shaderFloat64);
  DEVICE_FEATURE_ENABLE_IF(shaderInt64);
  DEVICE_FEATURE_ENABLE_IF(shaderInt16);
  // DEVICE_FEATURE_ENABLE_IF(shaderResourceResidency);
  DEVICE_FEATURE_ENABLE_IF(shaderResourceMinLod);
  // We don't use sparse features and enabling them cause extra internal
  // allocations inside the Vulkan driver we don't need.
  // DEVICE_FEATURE_ENABLE_IF(sparseBinding);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidencyBuffer);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidencyImage2D);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidencyImage3D);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidency2Samples);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidency4Samples);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidency8Samples);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidency16Samples);
  // DEVICE_FEATURE_ENABLE_IF(sparseResidencyAliased);
  DEVICE_FEATURE_ENABLE_IF(variableMultisampleRate);
  // DEVICE_FEATURE_ENABLE_IF(inheritedQueries);

#undef DEVICE_FEATURE_ENABLE_IF

  // Query Vulkan 1.2 features (including timelineSemaphore)

  device_features.vulkan_12_features_.sType = vk::StructureType::ePhysicalDeviceVulkan12Features;

  // Vulkan-Hpp requires you to use getFeatures2 for extended features
  vk::PhysicalDeviceFeatures2 features{};
  features.pNext = &device_features.vulkan_12_features_;
  physical_device.getFeatures2(&features);

  // Enable timelineSemaphore only if supported
  if (device_features.vulkan_12_features_.timelineSemaphore != 0U) {
    device_features.vulkan_12_features_.timelineSemaphore = VK_TRUE;
  } else {
    LOG_WARN("timelineSemaphore feature not supported by this device");
  }

  return device_features;
}

auto pickSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& supported_surface_formats,
    const std::vector<vk::Format>& preferred_formats) -> vk::SurfaceFormatKHR {
  assert(!supported_surface_formats.empty());

  auto picked_format{ supported_surface_formats[0] };
  auto preferred_color_space{ vk::ColorSpaceKHR::eSrgbNonlinear };

  if (supported_surface_formats.size() == 1) {
    if (picked_format.format == vk::Format::eUndefined) {
      picked_format.format = vk::Format::eB8G8R8A8Unorm;
      picked_format.colorSpace = preferred_color_space;
    }

    assert(picked_format.colorSpace == preferred_color_space);
    return picked_format;
  }

  for (const auto& preferred_format : preferred_formats) {
    auto format_iterator = std::find_if(
        supported_surface_formats.begin(), supported_surface_formats.end(),
        [preferred_format, preferred_color_space](const vk::SurfaceFormatKHR& format) {
          return (format.format == preferred_format) &&
                 (format.colorSpace == preferred_color_space);
        });
    if (format_iterator != supported_surface_formats.end()) {
      picked_format = *format_iterator;
      break;
    }
  }

  assert(picked_format.colorSpace == preferred_color_space);
  return picked_format;
}

auto computeSwapchainExtent(
    vk::SurfaceCapabilitiesKHR& surface_capabilities, const gravity::WindowContext& window_handler)
    -> vk::Extent2D {
  vk::Extent2D swapchain_extent;
  if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
    // If the surface size is undefined, the size is set to the size of the
    // images requested.

    // TODO
    // auto window_pixel_extent = window_handler.getPixelExtent();
    // swapchain_extent.width =
    //     glm::clamp(window_pixel_extent.width_, surface_capabilities.minImageExtent.width,
    //                surface_capabilities.maxImageExtent.width);
    // swapchain_extent.height =
    //     glm::clamp(window_pixel_extent.height_, surface_capabilities.minImageExtent.height,
    //                surface_capabilities.maxImageExtent.height);
  } else {
    // If the surface size is defined, the swap chain size must match
    swapchain_extent = surface_capabilities.currentExtent;
  }
  return swapchain_extent;
}

auto pickPresentMode(const std::vector<vk::PresentModeKHR>& present_modes) -> vk::PresentModeKHR {
  auto picked_mode = vk::PresentModeKHR::eFifo;
  for (const auto& present_mode : present_modes) {
    if (present_mode == vk::PresentModeKHR::eMailbox) {
      picked_mode = present_mode;
      break;
    }

    if (present_mode == vk::PresentModeKHR::eImmediate) {
      picked_mode = present_mode;
    }
  }
  return picked_mode;
}

auto toVulkan(BufferUsage usage) -> VkBufferUsageFlags {
  VkBufferUsageFlags flags{};

  if (hasFlag(usage, BufferUsage::TransferSource)) {
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  if (hasFlag(usage, BufferUsage::TransferDestination)) {
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  if (hasFlag(usage, BufferUsage::ReadOnlyTexel)) {
    flags |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
  }
  if (hasFlag(usage, BufferUsage::ReadWriteTexel)) {
    flags |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
  }
  if (hasFlag(usage, BufferUsage::ReadOnly)) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if (hasFlag(usage, BufferUsage::ReadWrite)) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if (hasFlag(usage, BufferUsage::Index)) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (hasFlag(usage, BufferUsage::Vertex)) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (hasFlag(usage, BufferUsage::Indirect)) {
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }

  return flags;
}

auto toVulkan(ImageUsage usage) -> VkImageUsageFlags {
  VkImageUsageFlags flags{};

  if (hasFlag(usage, ImageUsage::TransferSource)) {
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (hasFlag(usage, ImageUsage::TransferDestination)) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  if (hasFlag(usage, ImageUsage::Sampled)) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (hasFlag(usage, ImageUsage::ColorAttachment)) {
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if (hasFlag(usage, ImageUsage::DepthStencilAttachment)) {
    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }

  return flags;
}

auto toVulkan(Format format) -> VkFormat {
  switch (format) {
    case Format::Undefined:
      return VK_FORMAT_UNDEFINED;
    case Format::ColorRgba8UnsignedNormalized:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::ColorRgba8SignedNormalized:
      return VK_FORMAT_R8G8B8A8_SNORM;
    case Format::ColorRgba8sRgb:
      return VK_FORMAT_R8G8B8A8_SRGB;
    case Format::ColorRg32SignedFloat:
      return VK_FORMAT_R32G32_SFLOAT;
    case Format::ColorRgb32SignedFloat:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case Format::ColorRgba32UnsignedInt:
      return VK_FORMAT_R32G32B32A32_UINT;
    case Format::Depth32SignedFloat:
      return VK_FORMAT_D32_SFLOAT;
    case Format::Depth24UnsignedNormalizedStencil8UnsignedInteger:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case Format::Depth32SignedFloatStencil8UnsignedInt:
      return VK_FORMAT_D32_SFLOAT_S8_UINT;
  }
}

auto toVulkan(ImageSamples image_sample) -> VkSampleCountFlagBits {
  switch (image_sample) {
    case ImageSamples::S1:
      return VK_SAMPLE_COUNT_1_BIT;
    case ImageSamples::S2:
      return VK_SAMPLE_COUNT_2_BIT;
    case ImageSamples::S4:
      return VK_SAMPLE_COUNT_4_BIT;
    case ImageSamples::S8:
      return VK_SAMPLE_COUNT_8_BIT;
    case ImageSamples::S16:
      return VK_SAMPLE_COUNT_16_BIT;
    case ImageSamples::S32:
      return VK_SAMPLE_COUNT_32_BIT;
    case ImageSamples::S64:
      return VK_SAMPLE_COUNT_64_BIT;
  }
}

auto toVulkan(SamplerFilter sampler_filter) -> VkFilter {
  switch (sampler_filter) {
    case SamplerFilter::Nearest:
      return VK_FILTER_NEAREST;
    case SamplerFilter::Linear:
      return VK_FILTER_LINEAR;
    case SamplerFilter::Cubic:
      return VK_FILTER_CUBIC_EXT;
  }
}

auto toVulkan(SamplerMipMapMode sampler_mipmap_mode) -> VkSamplerMipmapMode {
  switch (sampler_mipmap_mode) {
    case SamplerMipMapMode::Nearest:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case SamplerMipMapMode::Linear:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

auto toVulkan(SamplerAddressMode sampler_address_mode) -> VkSamplerAddressMode {
  switch (sampler_address_mode) {
    case SamplerAddressMode::Repeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case SamplerAddressMode::MirroredRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case SamplerAddressMode::ClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case SamplerAddressMode::ClampToBorder:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case SamplerAddressMode::MirrorClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  }
}

auto toVulkan(CompareOperation compare_operation) -> VkCompareOp {
  switch (compare_operation) {
    case CompareOperation::Never:
      return VK_COMPARE_OP_NEVER;
    case CompareOperation::Less:
      return VK_COMPARE_OP_LESS;
    case CompareOperation::Equal:
      return VK_COMPARE_OP_EQUAL;
    case CompareOperation::LessOrEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOperation::Greater:
      return VK_COMPARE_OP_GREATER;
    case CompareOperation::NotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOperation::GreaterOrEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOperation::Always:
      return VK_COMPARE_OP_ALWAYS;
  }
}

auto toVulkan(BorderColor border_color) -> VkBorderColor {
  switch (border_color) {
    case BorderColor::FloatOpaqueBlack:
      return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
  }
}

auto toVulkan(ShaderStage usage) -> VkShaderStageFlags {
  VkShaderStageFlags flags{};

  if (hasFlag(usage, ShaderStage::Vertex)) {
    flags |= VK_SHADER_STAGE_VERTEX_BIT;
  }
  if (hasFlag(usage, ShaderStage::Fragment)) {
    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  if (hasFlag(usage, ShaderStage::Compute)) {
    flags |= VK_SHADER_STAGE_COMPUTE_BIT;
  }
  if (hasFlag(usage, ShaderStage::Geometry)) {
    flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
  }
  if (hasFlag(usage, ShaderStage::TesselationControl)) {
    flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  }
  if (hasFlag(usage, ShaderStage::TesselationEvaluation)) {
    flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  }
  return flags;
}

}  // namespace

namespace gravity {

using boost::asio::co_spawn;
using boost::asio::use_awaitable;

auto operator==(const ShaderModuleDescriptor& descriptor, const ShaderModuleDescriptor& other_description)
    -> bool {
  return descriptor.hash_ == other_description.hash_ &&
         descriptor.stage_ == other_description.stage_;
}

VulkanRenderingDevice::~VulkanRenderingDevice() {
  sync();

  for (auto& buffer : buffers_) {
    auto result = boost::asio::co_spawn(
        strands_.getStrand(StrandLanes::Buffer),
        doDestroyBuffer({ .index_ = buffer.index_, .generation_ = buffer.generation_ }),
        boost::asio::use_future);
    result.wait();
  }

  for (auto& image : images_) {
    auto result = boost::asio::co_spawn(
        strands_.getStrand(StrandLanes::Buffer),
        doDestroyImage({ .index_ = image.index_, .generation_ = image.generation_ }),
        boost::asio::use_future);
    result.wait();
  }

  for (auto& shader_module : shader_modules_) {
    auto result = boost::asio::co_spawn(
        strands_.getStrand(StrandLanes::Buffer),
        doDestroyShader(
            { .index_ = shader_module.index_, .generation_ = shader_module.generation_ }),
        boost::asio::use_future);
    result.wait();
  }

  auto result = boost::asio::co_spawn(
      strands_.getStrand(StrandLanes::Buffer),
      [this] -> boost::asio::awaitable<void> {
        collectPendingDestroy();
        co_return;
      },
      boost::asio::use_future);
  result.wait();

  vmaDestroyAllocator(memory_allocator_);
}

VulkanRenderingDevice::VulkanRenderingDevice(WindowContext& window_context, StrandGroup strands)
    : window_context_{ window_context }, strands_{ std::move(strands) } {}

auto VulkanRenderingDevice::initialize() -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Initialize), doInitialize(), boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::prepareBuffers() -> boost::asio::awaitable<std::error_code> {
  constexpr std::chrono::microseconds WaitDuration{ 50 };

  auto executor = co_await boost::asio::this_coro::executor;
  auto& sync = frames_.at(current_frame_);

  while (device_->waitForFences({ *sync.in_flight_ }, VK_TRUE, 0) == vk::Result::eTimeout) {
    co_await boost::asio::steady_timer(executor, WaitDuration)
        .async_wait(boost::asio::use_awaitable);
  }

  while (true) {
    auto [result, image_index]{ swapchain_resources_.swapchain_->acquireNextImage(
        0, *sync.image_available_, nullptr) };

    if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR) {
      if (frames_in_flight_[image_index] != nullptr) {
        while (device_->waitForFences({ *frames_in_flight_[image_index] }, VK_TRUE, 0) ==
               vk::Result::eTimeout) {
          co_await boost::asio::steady_timer(executor, WaitDuration)
              .async_wait(boost::asio::use_awaitable);
        }
      }
      frames_in_flight_[image_index] = &(*sync.in_flight_);

      swapchain_resources_.current_buffer_ = image_index;

      device_->resetFences({ *sync.in_flight_ });
      sync.command_pool_->reset();
      sync.command_buffers_.clear();

      co_return Error::OK;
    }

    if (result == vk::Result::eNotReady) {
      co_await boost::asio::steady_timer(executor, WaitDuration)
          .async_wait(boost::asio::use_awaitable);
      continue;
    }

    if (result == vk::Result::eErrorOutOfDateKHR) {
      if (auto error{ co_await updateSwapchain() }; error) {
        co_return error;
      }
      continue;
    }

    LOG_ERROR("Unexpected Vulkan result: {}", vk::to_string(result));
    co_return Error::InternalError;
  }
}

auto VulkanRenderingDevice::swapBuffers() -> boost::asio::awaitable<std::error_code> {
  std::vector<vk::SubmitInfo> submit_info_list;

  vk::PipelineStageFlags wait_destination_stage_mask{
    vk::PipelineStageFlagBits::eColorAttachmentOutput
  };

  auto& sync = frames_.at(current_frame_);

  vk::Semaphore image_available = **sync.image_available_;
  vk::Semaphore render_finished = **sync.render_finished_;
  vk::Semaphore timeline_semaphore = **timeline_semaphore_;

  std::vector<vk::CommandBuffer> command_buffers{ sync.command_buffers_.begin(),
                                                  sync.command_buffers_.end() };

  auto& submit_info{ submit_info_list.emplace_back(
      image_available, wait_destination_stage_mask, command_buffers, render_finished) };

  vk::TimelineSemaphoreSubmitInfo timeline_submit_info{ 0, nullptr, 1, &timeline_value_ };

  submit_info.pNext = &timeline_submit_info;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &timeline_semaphore;

  graphics_queue_->submit(submit_info, *sync.in_flight_);

  ++timeline_value_;

  vk::SwapchainKHR swapchain = **swapchain_resources_.swapchain_;
  vk::PresentInfoKHR present_info{ render_finished, swapchain,
                                   swapchain_resources_.current_buffer_ };

  auto result{ present_queue_->presentKHR(present_info) };

  switch (result) {
    case vk::Result::eSuccess:
      break;
    case vk::Result::eErrorOutOfDateKHR:
      [[fallthrough]];
    case vk::Result::eSuboptimalKHR: {
      if (auto error{ co_await updateSwapchain() }; error) {
        co_return error;
      }
    } break;
    default:
      LOG_ERROR("unexpected present result");
      co_return Error::InternalError;
  }

  current_frame_ = (current_frame_ + 1) % frames_.size();

  co_return Error::OK;
}

auto VulkanRenderingDevice::createBuffer(const BufferDescriptor& descriptor)
    -> boost::asio::awaitable<std::expected<BufferHandle, std::error_code>> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Buffer), doCreateBuffer(descriptor),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::destroyBuffer(BufferHandle buffer_handle)
    -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Buffer), doDestroyBuffer(buffer_handle),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::createImage(const ImageDescriptor& descriptor)
    -> boost::asio::awaitable<std::expected<ImageHandle, std::error_code>> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Buffer), doCreateImage(descriptor),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::destroyImage(ImageHandle image_handle)
    -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Buffer), doDestroyImage(image_handle),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::createSampler(const SamplerDescriptor& descriptor)
    -> boost::asio::awaitable<std::expected<SamplerHandle, std::error_code>> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Sampler), doCreateSampler(descriptor),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::destroySampler(SamplerHandle sampler_handle)
    -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Sampler), doDestroySampler(sampler_handle),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::createShaderModule(ShaderModuleDescriptor descriptor)
    -> boost::asio::awaitable<std::expected<ShaderModuleHandle, std::error_code>> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Shader), doCreateShader(descriptor),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::destroyShaderModule(ShaderModuleHandle shader_handle)
    -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Shader), doDestroyShader(shader_handle),
      boost::asio::use_awaitable);
}

auto VulkanRenderingDevice::ShaderHash::operator()(const ShaderModuleDescriptor& descriptor) const
    -> HashType {
  HashType hash = std::hash<int>{}(static_cast<int>(descriptor.stage_));
  hashCombine(hash, descriptor.hash_);
  return hash;
}

auto VulkanRenderingDevice::doInitialize() -> boost::asio::awaitable<std::error_code> {
  if (auto error{ co_await initializeVulkanInstance() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeSurface() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializePhysicalDevice() }; error) [[unlikely]] {
    co_return error;
  }

  if (auto error{ co_await initializeQueueIndex() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeLogicalDevice() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeDynamicDispatcher() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeAllocator() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeDescriptorSetAllocator() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeQueues() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeSynchronization() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeSurfaceFormat() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializePrimaryRenderPass() }; error) [[unlikely]] {
    co_return error;
  }

  if (auto error{ co_await initializeSwapchain() }; error) [[unlikely]] {
    co_return error;
  }

  if (auto error{ co_await initializePipelineCache() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeCommandPool() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeCommandBuffers() }; error) {
    co_return error;
  }

  co_return Error::OK;
}

struct BufferCreateInfo {
  VkBufferCreateInfo buffer_create_info_;
  VmaAllocationCreateInfo vma_allocation_create_info_;
};

auto buildBufferCreateInfo(uint32_t size, BufferUsage usage, Visibility visibility)
    -> BufferCreateInfo {
  BufferCreateInfo create_info{ .buffer_create_info_ = { .sType =
                                                             VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                         .size = size,
                                                         .usage = toVulkan(usage),
                                                         .sharingMode = VK_SHARING_MODE_EXCLUSIVE },
                                .vma_allocation_create_info_ = { .usage = VMA_MEMORY_USAGE_AUTO } };

  auto is_source{ hasFlag(usage, BufferUsage::TransferSource) };
  auto is_destination{ hasFlag(usage, BufferUsage::TransferDestination) };

  if (visibility == Visibility::Host && (is_source || is_destination)) {
    create_info.vma_allocation_create_info_.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }

  if (is_source && !is_destination) {
    create_info.vma_allocation_create_info_.flags |=
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  } else if (!is_source && is_destination) {
    create_info.vma_allocation_create_info_.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
  }

  return create_info;
}

auto VulkanRenderingDevice::doCreateBuffer(BufferDescriptor descriptor)
    -> boost::asio::awaitable<std::expected<BufferHandle, std::error_code>> {
  LOG_DEBUG(
      "created buffer; size: {}, usage: {}, visibility: {}, buffer_allocator_size: {}",
      descriptor.size_, magic_enum::enum_name(descriptor.usage_),
      magic_enum::enum_name(descriptor.visibility_), buffers_.size());

  auto build_buffer_create_info{ buildBufferCreateInfo(
      descriptor.size_, descriptor.usage_, descriptor.visibility_) };

  Buffer buffer{
    .size_ = descriptor.size_,
  };

  auto buffer_create_status{ vmaCreateBuffer(
      memory_allocator_, &build_buffer_create_info.buffer_create_info_,
      &build_buffer_create_info.vma_allocation_create_info_, &buffer.buffer_, &buffer.allocation_,
      &buffer.allocation_info_) };

  if (buffer_create_status != VkResult::VK_SUCCESS) [[unlikely]] {
    LOG_DEBUG(
        "created buffer failed; size: {}, usage: {}, visibility: {}, buffer_allocator_size: {}",
        descriptor.size_, magic_enum::enum_name(descriptor.usage_),
        magic_enum::enum_name(descriptor.visibility_), buffers_.size());
    co_return std::unexpected(Error::InternalError);
  }

  size_t slot_index{ 0 };
  if (!buffer_free_list_.empty()) {
    slot_index = buffer_free_list_.back();
    buffer_free_list_.pop_back();
  } else {
    buffers_.emplace_back();
    slot_index = buffers_.size() - 1;
  }
  buffers_[slot_index].index_ = slot_index;
  buffers_[slot_index].buffer_ = buffer;

  LOG_DEBUG(
      "created buffer success; size: {}, usage: {}, visibility: {}, index: {}, generation: {}, "
      "buffer_allocator_size: {}",
      descriptor.size_, magic_enum::enum_name(descriptor.usage_),
      magic_enum::enum_name(descriptor.visibility_), buffers_[slot_index].index_,
      buffers_[slot_index].generation_, buffers_.size());

  co_return BufferHandle{ .index_ = buffers_[slot_index].index_,
                          .generation_ = buffers_[slot_index].generation_ };
}

auto VulkanRenderingDevice::doDestroyBuffer(BufferHandle buffer_handle)
    -> boost::asio::awaitable<std::error_code> {
  LOG_DEBUG(
      "destroy buffer; index: {}, handler generation: {}, storage_generation: {}, "
      "current_timeline_value: {}",
      buffer_handle.index_, buffer_handle.generation_, buffers_[buffer_handle.index_].generation_,
      timeline_value_);

  if (buffers_[buffer_handle.index_].buffer_.buffer_ == VK_NULL_HANDLE) {
    LOG_TRACE("destroy buffer buffer already destroyed");
    co_return Error::OK;
  }

  assert(buffer_handle.generation_ == buffers_[buffer_handle.index_].generation_);

  buffers_[buffer_handle.index_].generation_++;

  pending_destroy_buffers_.emplace_back(
      PendingDestroy{ .index_ = buffer_handle.index_, .fence_value_ = timeline_value_ });

  co_return Error::OK;
}

auto VulkanRenderingDevice::doCreateImage(ImageDescriptor descriptor)
    -> boost::asio::awaitable<std::expected<ImageHandle, std::error_code>> {
  ImageSlot* image_slot{ nullptr };

  if (!buffer_free_list_.empty()) {
    auto index = image_free_list_.back();
    image_free_list_.pop_back();
    image_slot = &images_.at(index);
  } else {
    image_slot = &images_.emplace_back();
    image_slot->index_ = images_.size() - 1;
  }

  auto& image{ image_slot->image_ };

  auto& image_create_info{ image.image_create_info_ };

  image_create_info = {};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = toVulkan(descriptor.format_);
  image_create_info.extent = { .width = descriptor.extent_.width_,
                               .height = descriptor.extent_.height_,
                               .depth = 1 };
  image_create_info.mipLevels = descriptor.mip_level_;
  image_create_info.arrayLayers = descriptor.layers_;
  image_create_info.samples = toVulkan(descriptor.samples_);
  image_create_info.tiling =
      descriptor.visibility_ == Visibility::Host ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
  image_create_info.usage = toVulkan(descriptor.usage_);
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  if (descriptor.type_ == ImageType::Cube) {
    image_create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  VmaAllocationCreateInfo image_allocation_create_info{
    .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO
  };

  auto result{ vmaCreateImage(
      memory_allocator_, &image_create_info, &image_allocation_create_info, &image.image_,
      &image.allocation_, &image.allocation_info_) };

  if (result != VK_SUCCESS) {
    LOG_TRACE("unable to allocate memory for image");
    co_return std::unexpected(Error::InternalError);
  }

  auto& image_view_create_info{ image.image_view_create_info_ };

  image_view_create_info = {};
  image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_create_info.image = image.image_;
  image_view_create_info.viewType =
      descriptor.type_ == ImageType::Cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format = image_create_info.format;
  image_view_create_info.subresourceRange.baseMipLevel = 0;
  image_view_create_info.subresourceRange.levelCount = image_create_info.mipLevels;
  image_view_create_info.subresourceRange.baseArrayLayer = 0;
  image_view_create_info.subresourceRange.layerCount = image_create_info.arrayLayers;
  image_view_create_info.subresourceRange.aspectMask =
      hasFlag(descriptor.usage_, ImageUsage::DepthStencilAttachment) ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                                     : VK_IMAGE_ASPECT_COLOR_BIT;

  auto image_view_expect{ device_->createImageView(image_view_create_info) };
  if (!image_view_expect) {
    LOG_ERROR("unable to create image view");
    co_return std::unexpected(Error::InternalError);
  }
  image.image_view_ = image_view_expect->release();

  co_return ImageHandle{ .index_ = image_slot->index_, .generation_ = image_slot->generation_ };
}

auto VulkanRenderingDevice::doDestroyImage(ImageHandle image_handle)
    -> boost::asio::awaitable<std::error_code> {
  LOG_DEBUG(
      "destroy image; index: {}, handler generation: {}, storage_generation: {}, "
      "current_timeline_value: {}",
      image_handle.index_, image_handle.generation_, images_[image_handle.index_].generation_,
      timeline_value_);

  assert(image_handle.generation_ == images_[image_handle.index_].generation_);

  images_[image_handle.index_].generation_++;

  pending_destroy_images_.emplace_back(
      PendingDestroy{ .index_ = image_handle.index_, .fence_value_ = timeline_value_ });

  co_return Error::OK;
}

auto VulkanRenderingDevice::doCreateSampler(SamplerDescriptor descriptor)
    -> boost::asio::awaitable<std::expected<SamplerHandle, std::error_code>> {

  if (descriptor.anisotropy_enabled_) {
    if (device_features_.core_features_.samplerAnisotropy == VK_FALSE) {
      co_return std::unexpected(Error::FeatureNotSupported);
    }
  }

  vk::SamplerCreateInfo sampler_create_info{
    vk::SamplerCreateFlags(),
    static_cast<vk::Filter>(toVulkan(descriptor.magnification_filter_)),
    static_cast<vk::Filter>(toVulkan(descriptor.minification_filter_)),
    static_cast<vk::SamplerMipmapMode>(toVulkan(descriptor.mipmap_mode_)),
    static_cast<vk::SamplerAddressMode>(toVulkan(descriptor.address_mode_u_)),
    static_cast<vk::SamplerAddressMode>(toVulkan(descriptor.address_mode_v_)),
    static_cast<vk::SamplerAddressMode>(toVulkan(descriptor.address_mode_w_)),
    descriptor.mip_lod_bias_,
    descriptor.anisotropy_enabled_ ? VK_TRUE : VK_FALSE,
    std::min(descriptor.max_anisotropy_, device_limits_.maxSamplerAnisotropy),
    descriptor.compare_enabled_ ? VK_TRUE : VK_FALSE,
    static_cast<vk::CompareOp>(toVulkan(descriptor.comparison_operation_)),
    descriptor.min_lod_,
    descriptor.max_lod_,
    static_cast<vk::BorderColor>(toVulkan(descriptor.border_color_)),
    VK_FALSE
  };

  auto sampler_expect{ device_->createSampler(sampler_create_info) };

  if (!sampler_expect) {
    LOG_ERROR("unable to create sampler");
    co_return std::unexpected(Error::InternalError);
  }

  auto& sampler_slot{ samplers_.emplace_back(
      SamplerSlot{ .sampler_ = { .sampler_ = std::move(*sampler_expect),
                                 .sampler_create_info_ = sampler_create_info } }) };

  sampler_slot.index_ = samplers_.size() - 1;

  co_return SamplerHandle{ .index_ = sampler_slot.index_, .generation_ = 0 };
}

auto VulkanRenderingDevice::doDestroySampler(SamplerHandle image_handle)
    -> boost::asio::awaitable<std::error_code> {

  co_return Error::OK;
}

auto VulkanRenderingDevice::doCreateShader(ShaderModuleDescriptor descriptor)
    -> boost::asio::awaitable<std::expected<ShaderModuleHandle, std::error_code>> {

  constexpr std::chrono::microseconds WaitDuration{ 50 };
  auto executor = co_await boost::asio::this_coro::executor;

  if (auto iterator = shader_module_cache_.find(descriptor);
      iterator != shader_module_cache_.end()) {
    assert(iterator->second.generation_ == shader_modules_[iterator->second.index_].generation_);
    shader_modules_[iterator->second.index_].reference_counter_++;

    while (shader_modules_[iterator->second.index_].loading_) {
      co_await boost::asio::steady_timer(executor, WaitDuration)
          .async_wait(boost::asio::use_awaitable);
    }
    assert(shader_modules_[iterator->second.index_].loaded_);
    LOG_DEBUG(
        "create shader cache hit; shader_type: {}, index: {}, generation: {}, "
        "shader_module_allocator_size: {}",
        magic_enum::enum_name(shader_modules_[iterator->second.index_].shader_->stage_),
        shader_modules_[iterator->second.index_].index_,
        shader_modules_[iterator->second.index_].generation_, shader_modules_.size());
    co_return iterator->second;
  }

  size_t slot_index{ 0 };
  if (!shader_module_free_list_.empty()) {
    slot_index = shader_module_free_list_.back();
    shader_module_free_list_.pop_back();
  } else {
    shader_modules_.emplace_back();
    slot_index = shader_modules_.size() - 1;
  }

  auto guard = gsl::finally([&] {
    if (shader_modules_[slot_index].loading_) {
      LOG_DEBUG("creating shader aborted");
      shader_module_cache_.erase(descriptor);
      shader_module_free_list_.push_back(slot_index);
    }
  });
  shader_modules_[slot_index].index_ = slot_index;
  shader_modules_[slot_index].loading_ = true;

  auto [iter, inserted] = shader_module_cache_.emplace(
      descriptor, ShaderModuleHandle{ .index_ = shader_modules_[slot_index].index_,
                                .generation_ = shader_modules_[slot_index].generation_ });

  LOG_DEBUG(
      "create shader; shader_type: {}, shader_module_allocator_size: {}",
      magic_enum::enum_name(descriptor.stage_), shader_modules_.size());

  vk::ShaderModuleCreateInfo createInfo{ vk::ShaderModuleCreateFlags(), descriptor.spirv_ };

  auto shader_module_expect{ device_->createShaderModule(createInfo) };
  if (!shader_module_expect) {
    LOG_ERROR(
        "created shader failed;  shader_type: {}, shader_module_allocator_size: "
        "{}",
        magic_enum::enum_name(descriptor.stage_), shader_modules_.size());
    co_return std::unexpected(Error::InternalError);
  }

  shader_modules_[slot_index].shader_ =
      std::make_unique<ShaderSourceResource>(std::move(*shader_module_expect), descriptor.stage_);
  shader_modules_[slot_index].reference_counter_++;
  shader_modules_[slot_index].loaded_ = true;
  shader_modules_[slot_index].loading_ = false;

  LOG_DEBUG(
      "created shader success; shader_type: {}, index: {}, generation: {}, "
      "shader_module_allocator_size: {}",
      magic_enum::enum_name(shader_modules_[slot_index].shader_->stage_),
      shader_modules_[slot_index].index_, shader_modules_[slot_index].generation_,
      shader_modules_.size());

  co_return ShaderModuleHandle{ .index_ = shader_modules_[slot_index].index_,
                          .generation_ = shader_modules_[slot_index].generation_ };
}

auto VulkanRenderingDevice::doDestroyShader(ShaderModuleHandle shader_handle)
    -> boost::asio::awaitable<std::error_code> {
  LOG_DEBUG(
      "destroy shader; index: {}, handler generation: {}, storage_generation: {}, "
      "current_timeline_value: {}",
      shader_handle.index_, shader_handle.generation_,
      shader_modules_[shader_handle.index_].generation_, timeline_value_);

  assert(shader_handle.generation_ == shader_modules_[shader_handle.index_].generation_);

  if (shader_modules_[shader_handle.index_].reference_counter_-- == 0) {
    shader_modules_[shader_handle.index_].generation_++;
    shader_modules_[shader_handle.index_].loaded_ = false;

    pending_destroy_shader_modules_.emplace_back(
        PendingDestroy{ .index_ = shader_handle.index_, .fence_value_ = timeline_value_ });
  }

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeVulkanInstance() -> boost::asio::awaitable<std::error_code> {
  if (supported_vulkan_version() < VK_API_VERSION_1_3) {
    LOG_ERROR("platform does not support vulkan 1.3 and up");
    co_return Error::InternalError;
  }

  // const auto& application_config{gravity::ApplicationConfig::getInstance()};
  // const auto& engine_config{gravity::EngineConfig::getInstance()};

  vk::ApplicationInfo application_info{
    // application_config.applicationName().c_str(), application_config.applicationVersion(),
    // engine_config.engineName().c_str(), engine_config.engineVersion(),
    "Gravity Engine", VK_MAKE_VERSION(0, 1, 0), "Gravity Engine", VK_MAKE_VERSION(0, 1, 0),
    VK_API_VERSION_1_3
  };

  auto required_layers{ getRequiredInstanceLayers() };

  auto enumerate_layer_properties{ vk::enumerateInstanceLayerProperties() };
  if (enumerate_layer_properties.result != vk::Result::eSuccess) {
    LOG_ERROR("unable to enumerate instance layer properties");
    co_return Error::InternalError;
  }

  std::unordered_set<std::string> available_instance_layer_names;
  for (const auto& available_layer : enumerate_layer_properties.value) {
    available_instance_layer_names.insert(available_layer.layerName);
    // TODO(jerbdroid): clang-cl does not build same spdlog as msvc check defines
    // LOG_INFO(available_layer.descriptor);
  }

  for (const auto& required_layer : required_layers) {
    if (available_instance_layer_names.find(required_layer.first) ==
        available_instance_layer_names.end()) {
      if (required_layer.second) {
        LOG_ERROR("required vulkan layer ({}) is not supported", required_layer.first);
        co_return Error::InternalError;
      }
      LOG_WARN("failed to enable a requested vulkan layer ({})", required_layer.first);
    } else {
      enabled_instance_layer_names_.insert(required_layer.first);
    }
  }

  auto required_extensions{ getRequiredInstanceExtensions() };
  auto enumerate_extension_properties{ vk::enumerateInstanceExtensionProperties() };
  if (enumerate_extension_properties.result != vk::Result::eSuccess) {
    LOG_ERROR("unable to enumerate instance layer properties");
    co_return Error::InternalError;
  }

  std::unordered_set<std::string> available_instance_extension_names;
  for (const auto& available_extension : enumerate_extension_properties.value) {
    available_instance_extension_names.insert(available_extension.extensionName);
  }

  for (const auto& required_extension : required_extensions) {
    if (available_instance_extension_names.find(required_extension.first) ==
        available_instance_extension_names.end()) {
      if (required_extension.second) {
        LOG_ERROR("required vulkan extension ({}) is not supported", required_extension.first);
        co_return Error::InternalError;
      }
      LOG_WARN("failed to enable a requested vulkan extension ({})", required_extension.first);
    } else {
      enabled_instance_extension_names_.insert(required_extension.first);
    }
  }

  std::vector<const char*> enabled_layers;
  std::ranges::transform(
      enabled_instance_layer_names_, std::back_inserter(enabled_layers),
      [](const std::string& layer) { return layer.c_str(); });

  std::vector<const char*> enabled_extensions;
  std::ranges::transform(
      enabled_instance_extension_names_, std::back_inserter(enabled_extensions),
      [](const std::string& extension) { return extension.c_str(); });

  VulkanEnableLayerExtension enabled_layers_extensions{ .layers_ = enabled_layers,
                                                        .extensions_ = enabled_extensions };
  auto [result, instance]{ vk::createInstance(
      makeInstanceCreateInfoChain(application_info, enabled_layers_extensions)
          .get<vk::InstanceCreateInfo>()) };

  if (result != vk::Result::eSuccess) {
    LOG_ERROR("unable to create vulkan instance");
    co_return Error::InternalError;
  }

  instance_.emplace(vk::raii::Instance{ vk_context_, instance });

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeSurface() -> boost::asio::awaitable<std::error_code> {

  vk::SurfaceKHR surface;

  if (auto error{
          window_context_.getRenderingSurface(RenderingApi::Vulkan, **instance_, &surface) };
      error) {
    LOG_ERROR("unable to create vulkan window surface");
    co_return Error::InternalError;
  }

  surface_.emplace(vk::raii::SurfaceKHR{ *instance_, surface });

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializePhysicalDevice() -> boost::asio::awaitable<std::error_code> {
  std::multimap<int32_t, vk::PhysicalDevice> device_candidates;
  for (const auto& device : instance_->enumeratePhysicalDevices().value()) {
    auto device_score{ getDeviceRating(device, *surface_) };
    device_candidates.emplace(device_score, device);

#if !defined(NDEBUG)
    displayPhysicalDeviceProperties(device);
#endif
  }

  const auto& optimal_device_iter{ device_candidates.rbegin() };

  if (optimal_device_iter == device_candidates.rend() || optimal_device_iter->first <= 0) {
    LOG_ERROR("unable to find a physical display device");
    co_return Error::InternalError;
  }
  physical_device_.emplace(*instance_, optimal_device_iter->second);
  device_limits_ = physical_device_->getProperties().limits;

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeQueueIndex() -> boost::asio::awaitable<std::error_code> {
  if (auto result{ findGraphicsAndPresentQueueFamilyIndex(*physical_device_, *surface_) };
      result.has_value()) {
    graphics_family_queue_index_ = result.value().first;
    present_family_queue_index_ = result.value().second;
  } else {
    co_return result.error();
  }
  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeLogicalDevice() -> boost::asio::awaitable<std::error_code> {
  auto queue_priority{ 0.0F };
  std::vector<vk::DeviceQueueCreateInfo> device_queue_create_info;

  device_queue_create_info.emplace_back(
      vk::DeviceQueueCreateInfo({}, graphics_family_queue_index_, 1, &queue_priority));

  if (separate_queues_) {
    device_queue_create_info.emplace_back(
        vk::DeviceQueueCreateInfo({}, present_family_queue_index_, 1, &queue_priority));
  }

  auto enumerate_extension_properties{ physical_device_->enumerateDeviceExtensionProperties() };

  std::unordered_set<std::string> available_device_extension_names;
  for (const auto& available_extension : enumerate_extension_properties) {
    available_device_extension_names.insert(available_extension.extensionName);
  }

  auto required_extensions{ getRequiredDeviceExtensions(enabled_instance_extension_names_) };
  for (const auto& required_extension : required_extensions) {
    if (available_device_extension_names.find(required_extension.first) ==
        available_device_extension_names.end()) {
      if (required_extension.second) {
        LOG_ERROR(
            "required vulkan device extension ({}) is not supported", required_extension.first);
        co_return Error::InternalError;
      }
      LOG_WARN(
          "failed to enable a requested vulkan device extension ({})", required_extension.first);
    } else {
      enabled_device_extension_names_.insert(required_extension.first);
    }
  }

  std::vector<const char*> enabled_extensions;
  std::ranges::transform(
      enabled_device_extension_names_, std::back_inserter(enabled_extensions),
      [](const std::string& extension) { return extension.c_str(); });

  device_features_ = getRequiredDeviceFeatures(**physical_device_);

  vk::DeviceCreateInfo device_create_info(
      {}, device_queue_create_info, {}, enabled_extensions, &device_features_.core_features_,
      nullptr);

  device_create_info.pNext = &device_features_.vulkan_12_features_;

  auto deviceExpect{ physical_device_->createDevice(device_create_info) };

  if (deviceExpect) {
    device_ = std::move(*deviceExpect);
    co_return Error::OK;
  }

  LOG_ERROR(
      "failed to create logical device, vk error: {}", magic_enum::enum_name(deviceExpect.error()));

  co_return Error::InternalError;
}

auto VulkanRenderingDevice::initializeDynamicDispatcher()
    -> boost::asio::awaitable<std::error_code> {
  dynamic_dispatcher_.init(**instance_, vkGetInstanceProcAddr, **device_);
  // dynamic_dispatcher_.init(vkGetSemaphoreCounterValue);
  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeAllocator() -> boost::asio::awaitable<std::error_code> {
  VmaAllocatorCreateInfo allocator_create_info{};
  allocator_create_info.physicalDevice = **physical_device_;
  allocator_create_info.device = **device_;
  allocator_create_info.instance = **instance_;
  if (vmaCreateAllocator(&allocator_create_info, &memory_allocator_) != VkResult::VK_SUCCESS)
      [[unlikely]] {
    LOG_ERROR("unable to create video memory allocator");
    co_return Error::InternalError;
  }

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeDescriptorSetAllocator()
    -> boost::asio::awaitable<std::error_code> {
  descriptor_allocator_static_ = DescriptorAllocatorPool::create(**device_, 1);

  if (descriptor_allocator_static_ == nullptr) {
    LOG_ERROR("unable to initialize descriptor set allocator");
    co_return Error::InternalError;
  }

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeQueues() -> boost::asio::awaitable<std::error_code> {
  auto graphics_queue_expect{ device_->getQueue(graphics_family_queue_index_, 0) };
  if (!graphics_queue_expect) {
    LOG_ERROR("unable to get graphics queue from logical device");
    co_return Error::InternalError;
  }
  graphics_queue_ = std::move(*graphics_queue_expect);

  auto present_queue_expect{ device_->getQueue(present_family_queue_index_, 0) };
  if (!present_queue_expect) {
    LOG_ERROR("unable to get present queue from logical device");
    co_return Error::InternalError;
  }
  present_queue_ = std::move(*present_queue_expect);

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeSynchronization() -> boost::asio::awaitable<std::error_code> {
  for (auto& frame : frames_) {
    auto fence_expect{ device_->createFence(
        vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)) };
    if (!fence_expect) {
      LOG_ERROR("unable to create draw fence");
      co_return Error::InternalError;
    }
    frame.in_flight_ = std::move(*fence_expect);

    auto semaphore_expect{ device_->createSemaphore(vk::SemaphoreCreateInfo()) };
    if (!semaphore_expect) {
      LOG_ERROR("unable to create draw complete semaphore");
      co_return Error::InternalError;
    }
    frame.render_finished_ = std::move(*semaphore_expect);
  }

  vk::SemaphoreTypeCreateInfo timeline_info{ vk::SemaphoreType::eTimeline, timeline_value_ };
  vk::SemaphoreCreateInfo semaphore_info;
  semaphore_info.pNext = &timeline_info;
  auto semaphore_expect{ device_->createSemaphore(semaphore_info) };
  vk::SemaphoreCreateFlagBits semaphore_create_flags{};
  if (!semaphore_expect) {
    LOG_ERROR("unable to create image available semaphore");
    co_return Error::InternalError;
  }
  timeline_semaphore_ = std::move(*semaphore_expect);

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeSurfaceFormat() -> boost::asio::awaitable<std::error_code> {
  auto formats{ physical_device_->getSurfaceFormatsKHR(*surface_) };
  surface_format_ = pickSurfaceFormat(
      formats, { vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm, vk::Format::eB8G8R8Unorm,
                 vk::Format::eR8G8B8Unorm });
  co_return Error::OK;
}

auto VulkanRenderingDevice::initializePrimaryRenderPass()
    -> boost::asio::awaitable<std::error_code> {

  std::vector<vk::AttachmentDescription2> attachments{ vk::AttachmentDescription2{
      vk::AttachmentDescriptionFlags(), surface_format_.format, vk::SampleCountFlagBits::e1,
      vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
      vk::ImageLayout::ePresentSrcKHR } };

  vk::AttachmentReference2 color{ 0, vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageAspectFlagBits::eColor };

  std::vector<vk::SubpassDescription2> subpass_description;

  std::vector<vk::AttachmentReference2> input_attachments;
  std::vector<vk::AttachmentReference2> color_attachments;
  color_attachments.emplace_back(color);
  std::vector<vk::AttachmentReference2> depth_attachment;
  std::vector<vk::AttachmentReference2> resolve_attachments;
  std::vector<uint32_t> preserve_attachments;

  subpass_description.emplace_back(
      vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 0, input_attachments,
      color_attachments, resolve_attachments,
      depth_attachment.size() > 0 ? depth_attachment.data() : nullptr, preserve_attachments);

  std::vector<vk::SubpassDependency2> subpass_dependencies;
  subpass_dependencies.emplace_back(
      ~0U, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eNone,
      vk::AccessFlagBits::eColorAttachmentWrite);

  vk::RenderPassCreateInfo2 render_pass_create_info{
    vk::RenderPassCreateFlags(), attachments, subpass_description, subpass_dependencies, {}
  };

  auto render_pass_expect{ device_->createRenderPass2(render_pass_create_info) };
  if (!render_pass_expect) {
    LOG_ERROR("unable to create primary render pass");
    co_return Error::InternalError;
  }

  render_pass_ = std::move(*render_pass_expect);

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeSwapchain() -> boost::asio::awaitable<std::error_code> {

  while (window_context_.getResolution().width_ == 0 ||
         window_context_.getResolution().height_ == 0) {
    window_context_.pollEvents();
  }

  auto surface_capabilities{ physical_device_->getSurfaceCapabilitiesKHR(*surface_) };

  auto swapchain_extent{ computeSwapchainExtent(surface_capabilities, window_context_) };

  auto pre_transform{ (surface_capabilities.supportedTransforms &
                       vk::SurfaceTransformFlagBitsKHR::eIdentity)
                          ? vk::SurfaceTransformFlagBitsKHR::eIdentity
                          : surface_capabilities.currentTransform };

  auto composite_alpha{
    (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
    : (surface_capabilities.supportedCompositeAlpha &
       vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
        ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
    : (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit)
        ? vk::CompositeAlphaFlagBitsKHR::eInherit
        : vk::CompositeAlphaFlagBitsKHR::eOpaque
  };

  auto available_present_mode{ physical_device_->getSurfacePresentModesKHR(*surface_) };
  auto present_mode{ pickPresentMode(available_present_mode) };

  vk::SwapchainCreateInfoKHR swapchain_create_info(
      {}, *surface_, surface_capabilities.minImageCount, surface_format_.format,
      surface_format_.colorSpace, swapchain_extent, 1,
      { vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc },
      vk::SharingMode::eExclusive, {}, pre_transform, composite_alpha, present_mode, VK_TRUE,
      nullptr);

  if (separate_queues_) {

    auto result{ findGraphicsAndPresentQueueFamilyIndex(*physical_device_, *surface_) };
    if (!result.has_value()) {
      co_return result.error();
    }

    auto [graphics_family_queue_index, present_family_queue_index]{ result.value() };

    std::array<uint32_t, 2> queue_family_indicies{ graphics_family_queue_index,
                                                   present_family_queue_index };

    // If the graphics and present queues are from different queue families, we
    // either have to explicitly transfer ownership of images between the
    // queues, or we have to create the swapchain with imageSharingMode as
    // vk::SharingMode::eConcurrent

    // TODO:
    // if (VulkanRenderingDeviceConfig.swapchainImageSharing()) {
    //   swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
    //   swapchain_create_info.queueFamilyIndexCount =
    //       static_cast<uint32_t>(queue_family_indicies.size());
    //   swapchain_create_info.pQueueFamilyIndices = queue_family_indicies.data();
    // }
  }

  auto swapchain_expect{ device_->createSwapchainKHR(swapchain_create_info) };
  if (!swapchain_expect) {
    LOG_ERROR("unable to create swapchain");
    co_return Error::InternalError;
  }

  swapchain_resources_.swapchain_ = std::move(*swapchain_expect);

  vk::ImageViewCreateInfo image_view_create_info_(
      {}, {}, vk::ImageViewType::e2D, surface_format_.format, {},
      { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

  for (auto& image : swapchain_resources_.swapchain_->getImages()) {
    image_view_create_info_.setImage(image);
    auto image_view_expect{ device_->createImageView(image_view_create_info_) };

    if (!image_view_expect) {
      LOG_ERROR("unable to create swapchain image view");
      co_return Error::InternalError;
    }

    swapchain_resources_.images_.emplace_back(std::move(*image_view_expect));
  }

  auto window_resolution{ window_context_.getResolution() };

  for (auto& image : swapchain_resources_.images_) {
    std::vector<vk::ImageView> attachment_image_views{ *image };
    vk::FramebufferCreateInfo framebuffer_create_info{
      vk::FramebufferCreateFlags(), *render_pass_,
      attachment_image_views,       window_resolution.width_,
      window_resolution.height_,    1
    };

    auto framebuffer_expect{ device_->createFramebuffer(framebuffer_create_info) };

    if (framebuffer_expect) {
      swapchain_resources_.framebuffers_.emplace_back(std::move(*framebuffer_expect));
    } else {
      LOG_ERROR("unable to create swapchain framebuffers");
      co_return Error::InternalError;
    }
  }

  for (auto& frame : frames_) {
    if (!frame.image_available_) {
      continue;
    }

    auto semaphore_expect{ device_->createSemaphore(vk::SemaphoreCreateInfo()) };
    if (!semaphore_expect) {
      LOG_ERROR("unable to create semaphore");
      co_return Error::InternalError;
    }
    frame.image_available_ = std::move(*semaphore_expect);
  }

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializePipelineCache() -> boost::asio::awaitable<std::error_code> {
  vk::PipelineCacheCreateInfo pipeline_cache_create_Info{};

  auto pipeline_cache_expect{ device_->createPipelineCache(pipeline_cache_create_Info) };

  if (!pipeline_cache_expect) {
    LOG_ERROR("unable to create pipeline cache");
    co_return Error::InternalError;
  }

  pipeline_cache_ = std::move(*pipeline_cache_expect);

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeCommandPool() -> boost::asio::awaitable<std::error_code> {
  vk::CommandPoolCreateInfo command_pool_create_info{
    vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient,
    graphics_family_queue_index_
  };

  for (auto& frame : frames_) {
    auto command_pool_expect{ device_->createCommandPool(command_pool_create_info) };
    if (!command_pool_expect) {
      LOG_ERROR("unable to create command pool for frame {}", &frame - &frames_.front());
      co_return Error::InternalError;
    }

    frame.command_pool_ = std::move(*command_pool_expect);
  }

  co_return Error::OK;
}

auto VulkanRenderingDevice::initializeCommandBuffers() -> boost::asio::awaitable<std::error_code> {
  for (auto& frame : frames_) {
    vk::CommandBufferAllocateInfo command_buffer_info{ **frame.command_pool_,
                                                       vk::CommandBufferLevel::ePrimary, 1 };

    auto command_buffers_expect{ device_->allocateCommandBuffers(command_buffer_info) };

    if (!command_buffers_expect) {
      LOG_ERROR("unable to allocate command buffers for frame {}", &frame - &frames_.front());
      co_return Error::InternalError;
    }

    frame.command_buffers_ = std::move(command_buffers_expect.value());
  }

  co_return Error::OK;
}

auto VulkanRenderingDevice::updateSwapchain() -> boost::asio::awaitable<std::error_code> {
  sync();

  cleanupSwapchain();
  cleanupRenderPass();

  if (auto error{ co_await initializePrimaryRenderPass() }; error) {
    co_return error;
  }

  if (auto error{ co_await initializeSwapchain() }; error) {
    co_return error;
  }

  co_return Error::OK;
}

void VulkanRenderingDevice::cleanupSwapchain() {
  swapchain_resources_.framebuffers_.clear();

  swapchain_resources_.images_.clear();

  swapchain_resources_.swapchain_.reset();
}

void VulkanRenderingDevice::cleanupRenderPass() {
  render_pass_.reset();
}

void VulkanRenderingDevice::collectPendingDestroy() {
  uint64_t completed;
  auto result = (**device_).getSemaphoreCounterValueKHR(
      **timeline_semaphore_, &completed, dynamic_dispatcher_);
  if (result != vk::Result::eSuccess) {
    LOG_ERROR("pending deleted buffer collector failed to get semaphore counter value");
    return;
  }

  std::erase_if(pending_destroy_buffers_, [&completed, this](auto& pending_destroy) {
    if (pending_destroy.fence_value_ <= completed) {
      if (buffers_[pending_destroy.index_].buffer_.buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            memory_allocator_, buffers_[pending_destroy.index_].buffer_.buffer_,
            buffers_[pending_destroy.index_].buffer_.allocation_);

        buffers_[pending_destroy.index_].buffer_.buffer_ = {};
        buffer_free_list_.emplace_back(pending_destroy.index_);
      }
      return true;
    }
    return false;
  });

  std::erase_if(pending_destroy_images_, [&completed, this](auto& pending_destroy) {
    if (pending_destroy.fence_value_ <= completed) {
      if (images_[pending_destroy.index_].image_.image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(
            memory_allocator_, images_[pending_destroy.index_].image_.image_,
            images_[pending_destroy.index_].image_.allocation_);
        images_[pending_destroy.index_].image_.image_ = {};
        image_free_list_.emplace_back(pending_destroy.index_);
      }
      return true;
    }
    return false;
  });

  std::erase_if(pending_destroy_shader_modules_, [&completed, this](auto& pending_destroy) {
    if (pending_destroy.fence_value_ <= completed) {
      shader_modules_[pending_destroy.index_].shader_.release();
      shader_modules_[pending_destroy.index_].index_ = 0;
      shader_module_free_list_.emplace_back(pending_destroy.index_);
      return true;
    }
    return false;
  });
}

void VulkanRenderingDevice::sync() {
  device_->waitIdle();
}

}  // namespace gravity