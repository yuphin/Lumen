/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2018-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include "extensions_vk.hpp"

/* NVVK_GENERATE_VERSION_INFO */
// Generated using Vulkan 176
/* NVVK_GENERATE_VERSION_INFO */

/* clang-format off */

/* NVVK_GENERATE_STATIC_PFN */
#ifdef VK_AMD_buffer_marker
static PFN_vkCmdWriteBufferMarkerAMD pfn_vkCmdWriteBufferMarkerAMD= 0;
#endif /* VK_AMD_buffer_marker */
#ifdef VK_AMD_display_native_hdr
static PFN_vkSetLocalDimmingAMD pfn_vkSetLocalDimmingAMD= 0;
#endif /* VK_AMD_display_native_hdr */
#ifdef VK_AMD_draw_indirect_count
static PFN_vkCmdDrawIndexedIndirectCountAMD pfn_vkCmdDrawIndexedIndirectCountAMD= 0;
static PFN_vkCmdDrawIndirectCountAMD pfn_vkCmdDrawIndirectCountAMD= 0;
#endif /* VK_AMD_draw_indirect_count */
#ifdef VK_AMD_shader_info
static PFN_vkGetShaderInfoAMD pfn_vkGetShaderInfoAMD= 0;
#endif /* VK_AMD_shader_info */
#ifdef VK_ANDROID_external_memory_android_hardware_buffer
static PFN_vkGetAndroidHardwareBufferPropertiesANDROID pfn_vkGetAndroidHardwareBufferPropertiesANDROID= 0;
static PFN_vkGetMemoryAndroidHardwareBufferANDROID pfn_vkGetMemoryAndroidHardwareBufferANDROID= 0;
#endif /* VK_ANDROID_external_memory_android_hardware_buffer */
#ifdef VK_EXT_acquire_xlib_display
static PFN_vkAcquireXlibDisplayEXT pfn_vkAcquireXlibDisplayEXT= 0;
static PFN_vkGetRandROutputDisplayEXT pfn_vkGetRandROutputDisplayEXT= 0;
#endif /* VK_EXT_acquire_xlib_display */
#ifdef VK_EXT_buffer_device_address
static PFN_vkGetBufferDeviceAddressEXT pfn_vkGetBufferDeviceAddressEXT= 0;
#endif /* VK_EXT_buffer_device_address */
#ifdef VK_EXT_calibrated_timestamps
static PFN_vkGetCalibratedTimestampsEXT pfn_vkGetCalibratedTimestampsEXT= 0;
static PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT= 0;
#endif /* VK_EXT_calibrated_timestamps */
#ifdef VK_EXT_color_write_enable
static PFN_vkCmdSetColorWriteEnableEXT pfn_vkCmdSetColorWriteEnableEXT= 0;
#endif /* VK_EXT_color_write_enable */
#ifdef VK_EXT_conditional_rendering
static PFN_vkCmdBeginConditionalRenderingEXT pfn_vkCmdBeginConditionalRenderingEXT= 0;
static PFN_vkCmdEndConditionalRenderingEXT pfn_vkCmdEndConditionalRenderingEXT= 0;
#endif /* VK_EXT_conditional_rendering */
#ifdef VK_EXT_debug_marker
static PFN_vkCmdDebugMarkerBeginEXT pfn_vkCmdDebugMarkerBeginEXT= 0;
static PFN_vkCmdDebugMarkerEndEXT pfn_vkCmdDebugMarkerEndEXT= 0;
static PFN_vkCmdDebugMarkerInsertEXT pfn_vkCmdDebugMarkerInsertEXT= 0;
static PFN_vkDebugMarkerSetObjectNameEXT pfn_vkDebugMarkerSetObjectNameEXT= 0;
static PFN_vkDebugMarkerSetObjectTagEXT pfn_vkDebugMarkerSetObjectTagEXT= 0;
#endif /* VK_EXT_debug_marker */
#ifdef VK_EXT_debug_report
static PFN_vkCreateDebugReportCallbackEXT pfn_vkCreateDebugReportCallbackEXT= 0;
static PFN_vkDebugReportMessageEXT pfn_vkDebugReportMessageEXT= 0;
static PFN_vkDestroyDebugReportCallbackEXT pfn_vkDestroyDebugReportCallbackEXT= 0;
#endif /* VK_EXT_debug_report */
#ifdef VK_EXT_debug_utils
static PFN_vkCmdBeginDebugUtilsLabelEXT pfn_vkCmdBeginDebugUtilsLabelEXT= 0;
static PFN_vkCmdEndDebugUtilsLabelEXT pfn_vkCmdEndDebugUtilsLabelEXT= 0;
static PFN_vkCmdInsertDebugUtilsLabelEXT pfn_vkCmdInsertDebugUtilsLabelEXT= 0;
static PFN_vkCreateDebugUtilsMessengerEXT pfn_vkCreateDebugUtilsMessengerEXT= 0;
static PFN_vkDestroyDebugUtilsMessengerEXT pfn_vkDestroyDebugUtilsMessengerEXT= 0;
static PFN_vkQueueBeginDebugUtilsLabelEXT pfn_vkQueueBeginDebugUtilsLabelEXT= 0;
static PFN_vkQueueEndDebugUtilsLabelEXT pfn_vkQueueEndDebugUtilsLabelEXT= 0;
static PFN_vkQueueInsertDebugUtilsLabelEXT pfn_vkQueueInsertDebugUtilsLabelEXT= 0;
static PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT= 0;
static PFN_vkSetDebugUtilsObjectTagEXT pfn_vkSetDebugUtilsObjectTagEXT= 0;
static PFN_vkSubmitDebugUtilsMessageEXT pfn_vkSubmitDebugUtilsMessageEXT= 0;
#endif /* VK_EXT_debug_utils */
#ifdef VK_EXT_direct_mode_display
static PFN_vkReleaseDisplayEXT pfn_vkReleaseDisplayEXT= 0;
#endif /* VK_EXT_direct_mode_display */
#ifdef VK_EXT_directfb_surface
static PFN_vkCreateDirectFBSurfaceEXT pfn_vkCreateDirectFBSurfaceEXT= 0;
static PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT pfn_vkGetPhysicalDeviceDirectFBPresentationSupportEXT= 0;
#endif /* VK_EXT_directfb_surface */
#ifdef VK_EXT_discard_rectangles
static PFN_vkCmdSetDiscardRectangleEXT pfn_vkCmdSetDiscardRectangleEXT= 0;
#endif /* VK_EXT_discard_rectangles */
#ifdef VK_EXT_display_control
static PFN_vkDisplayPowerControlEXT pfn_vkDisplayPowerControlEXT= 0;
static PFN_vkGetSwapchainCounterEXT pfn_vkGetSwapchainCounterEXT= 0;
static PFN_vkRegisterDeviceEventEXT pfn_vkRegisterDeviceEventEXT= 0;
static PFN_vkRegisterDisplayEventEXT pfn_vkRegisterDisplayEventEXT= 0;
#endif /* VK_EXT_display_control */
#ifdef VK_EXT_display_surface_counter
static PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT pfn_vkGetPhysicalDeviceSurfaceCapabilities2EXT= 0;
#endif /* VK_EXT_display_surface_counter */
#ifdef VK_EXT_extended_dynamic_state
static PFN_vkCmdBindVertexBuffers2EXT pfn_vkCmdBindVertexBuffers2EXT= 0;
static PFN_vkCmdSetCullModeEXT pfn_vkCmdSetCullModeEXT= 0;
static PFN_vkCmdSetDepthBoundsTestEnableEXT pfn_vkCmdSetDepthBoundsTestEnableEXT= 0;
static PFN_vkCmdSetDepthCompareOpEXT pfn_vkCmdSetDepthCompareOpEXT= 0;
static PFN_vkCmdSetDepthTestEnableEXT pfn_vkCmdSetDepthTestEnableEXT= 0;
static PFN_vkCmdSetDepthWriteEnableEXT pfn_vkCmdSetDepthWriteEnableEXT= 0;
static PFN_vkCmdSetFrontFaceEXT pfn_vkCmdSetFrontFaceEXT= 0;
static PFN_vkCmdSetPrimitiveTopologyEXT pfn_vkCmdSetPrimitiveTopologyEXT= 0;
static PFN_vkCmdSetScissorWithCountEXT pfn_vkCmdSetScissorWithCountEXT= 0;
static PFN_vkCmdSetStencilOpEXT pfn_vkCmdSetStencilOpEXT= 0;
static PFN_vkCmdSetStencilTestEnableEXT pfn_vkCmdSetStencilTestEnableEXT= 0;
static PFN_vkCmdSetViewportWithCountEXT pfn_vkCmdSetViewportWithCountEXT= 0;
#endif /* VK_EXT_extended_dynamic_state */
#ifdef VK_EXT_extended_dynamic_state2
static PFN_vkCmdSetDepthBiasEnableEXT pfn_vkCmdSetDepthBiasEnableEXT= 0;
static PFN_vkCmdSetLogicOpEXT pfn_vkCmdSetLogicOpEXT= 0;
static PFN_vkCmdSetPatchControlPointsEXT pfn_vkCmdSetPatchControlPointsEXT= 0;
static PFN_vkCmdSetPrimitiveRestartEnableEXT pfn_vkCmdSetPrimitiveRestartEnableEXT= 0;
static PFN_vkCmdSetRasterizerDiscardEnableEXT pfn_vkCmdSetRasterizerDiscardEnableEXT= 0;
#endif /* VK_EXT_extended_dynamic_state2 */
#ifdef VK_EXT_external_memory_host
static PFN_vkGetMemoryHostPointerPropertiesEXT pfn_vkGetMemoryHostPointerPropertiesEXT= 0;
#endif /* VK_EXT_external_memory_host */
#ifdef VK_EXT_full_screen_exclusive
static PFN_vkAcquireFullScreenExclusiveModeEXT pfn_vkAcquireFullScreenExclusiveModeEXT= 0;
static PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT pfn_vkGetPhysicalDeviceSurfacePresentModes2EXT= 0;
static PFN_vkReleaseFullScreenExclusiveModeEXT pfn_vkReleaseFullScreenExclusiveModeEXT= 0;
#endif /* VK_EXT_full_screen_exclusive */
#ifdef VK_EXT_hdr_metadata
static PFN_vkSetHdrMetadataEXT pfn_vkSetHdrMetadataEXT= 0;
#endif /* VK_EXT_hdr_metadata */
#ifdef VK_EXT_headless_surface
static PFN_vkCreateHeadlessSurfaceEXT pfn_vkCreateHeadlessSurfaceEXT= 0;
#endif /* VK_EXT_headless_surface */
#ifdef VK_EXT_host_query_reset
static PFN_vkResetQueryPoolEXT pfn_vkResetQueryPoolEXT= 0;
#endif /* VK_EXT_host_query_reset */
#ifdef VK_EXT_image_drm_format_modifier
static PFN_vkGetImageDrmFormatModifierPropertiesEXT pfn_vkGetImageDrmFormatModifierPropertiesEXT= 0;
#endif /* VK_EXT_image_drm_format_modifier */
#ifdef VK_EXT_line_rasterization
static PFN_vkCmdSetLineStippleEXT pfn_vkCmdSetLineStippleEXT= 0;
#endif /* VK_EXT_line_rasterization */
#ifdef VK_EXT_metal_surface
static PFN_vkCreateMetalSurfaceEXT pfn_vkCreateMetalSurfaceEXT= 0;
#endif /* VK_EXT_metal_surface */
#ifdef VK_EXT_private_data
static PFN_vkCreatePrivateDataSlotEXT pfn_vkCreatePrivateDataSlotEXT= 0;
static PFN_vkDestroyPrivateDataSlotEXT pfn_vkDestroyPrivateDataSlotEXT= 0;
static PFN_vkGetPrivateDataEXT pfn_vkGetPrivateDataEXT= 0;
static PFN_vkSetPrivateDataEXT pfn_vkSetPrivateDataEXT= 0;
#endif /* VK_EXT_private_data */
#ifdef VK_EXT_sample_locations
static PFN_vkCmdSetSampleLocationsEXT pfn_vkCmdSetSampleLocationsEXT= 0;
static PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT= 0;
#endif /* VK_EXT_sample_locations */
#ifdef VK_EXT_tooling_info
static PFN_vkGetPhysicalDeviceToolPropertiesEXT pfn_vkGetPhysicalDeviceToolPropertiesEXT= 0;
#endif /* VK_EXT_tooling_info */
#ifdef VK_EXT_transform_feedback
static PFN_vkCmdBeginQueryIndexedEXT pfn_vkCmdBeginQueryIndexedEXT= 0;
static PFN_vkCmdBeginTransformFeedbackEXT pfn_vkCmdBeginTransformFeedbackEXT= 0;
static PFN_vkCmdBindTransformFeedbackBuffersEXT pfn_vkCmdBindTransformFeedbackBuffersEXT= 0;
static PFN_vkCmdDrawIndirectByteCountEXT pfn_vkCmdDrawIndirectByteCountEXT= 0;
static PFN_vkCmdEndQueryIndexedEXT pfn_vkCmdEndQueryIndexedEXT= 0;
static PFN_vkCmdEndTransformFeedbackEXT pfn_vkCmdEndTransformFeedbackEXT= 0;
#endif /* VK_EXT_transform_feedback */
#ifdef VK_EXT_validation_cache
static PFN_vkCreateValidationCacheEXT pfn_vkCreateValidationCacheEXT= 0;
static PFN_vkDestroyValidationCacheEXT pfn_vkDestroyValidationCacheEXT= 0;
static PFN_vkGetValidationCacheDataEXT pfn_vkGetValidationCacheDataEXT= 0;
static PFN_vkMergeValidationCachesEXT pfn_vkMergeValidationCachesEXT= 0;
#endif /* VK_EXT_validation_cache */
#ifdef VK_EXT_vertex_input_dynamic_state
static PFN_vkCmdSetVertexInputEXT pfn_vkCmdSetVertexInputEXT= 0;
#endif /* VK_EXT_vertex_input_dynamic_state */
#ifdef VK_FUCHSIA_external_memory
static PFN_vkGetMemoryZirconHandleFUCHSIA pfn_vkGetMemoryZirconHandleFUCHSIA= 0;
static PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA pfn_vkGetMemoryZirconHandlePropertiesFUCHSIA= 0;
#endif /* VK_FUCHSIA_external_memory */
#ifdef VK_FUCHSIA_external_semaphore
static PFN_vkGetSemaphoreZirconHandleFUCHSIA pfn_vkGetSemaphoreZirconHandleFUCHSIA= 0;
static PFN_vkImportSemaphoreZirconHandleFUCHSIA pfn_vkImportSemaphoreZirconHandleFUCHSIA= 0;
#endif /* VK_FUCHSIA_external_semaphore */
#ifdef VK_FUCHSIA_imagepipe_surface
static PFN_vkCreateImagePipeSurfaceFUCHSIA pfn_vkCreateImagePipeSurfaceFUCHSIA= 0;
#endif /* VK_FUCHSIA_imagepipe_surface */
#ifdef VK_GGP_stream_descriptor_surface
static PFN_vkCreateStreamDescriptorSurfaceGGP pfn_vkCreateStreamDescriptorSurfaceGGP= 0;
#endif /* VK_GGP_stream_descriptor_surface */
#ifdef VK_GOOGLE_display_timing
static PFN_vkGetPastPresentationTimingGOOGLE pfn_vkGetPastPresentationTimingGOOGLE= 0;
static PFN_vkGetRefreshCycleDurationGOOGLE pfn_vkGetRefreshCycleDurationGOOGLE= 0;
#endif /* VK_GOOGLE_display_timing */
#ifdef VK_INTEL_performance_query
static PFN_vkAcquirePerformanceConfigurationINTEL pfn_vkAcquirePerformanceConfigurationINTEL= 0;
static PFN_vkCmdSetPerformanceMarkerINTEL pfn_vkCmdSetPerformanceMarkerINTEL= 0;
static PFN_vkCmdSetPerformanceOverrideINTEL pfn_vkCmdSetPerformanceOverrideINTEL= 0;
static PFN_vkCmdSetPerformanceStreamMarkerINTEL pfn_vkCmdSetPerformanceStreamMarkerINTEL= 0;
static PFN_vkGetPerformanceParameterINTEL pfn_vkGetPerformanceParameterINTEL= 0;
static PFN_vkInitializePerformanceApiINTEL pfn_vkInitializePerformanceApiINTEL= 0;
static PFN_vkQueueSetPerformanceConfigurationINTEL pfn_vkQueueSetPerformanceConfigurationINTEL= 0;
static PFN_vkReleasePerformanceConfigurationINTEL pfn_vkReleasePerformanceConfigurationINTEL= 0;
static PFN_vkUninitializePerformanceApiINTEL pfn_vkUninitializePerformanceApiINTEL= 0;
#endif /* VK_INTEL_performance_query */
#ifdef VK_KHR_acceleration_structure
static PFN_vkBuildAccelerationStructuresKHR pfn_vkBuildAccelerationStructuresKHR= 0;
static PFN_vkCmdBuildAccelerationStructuresIndirectKHR pfn_vkCmdBuildAccelerationStructuresIndirectKHR= 0;
static PFN_vkCmdBuildAccelerationStructuresKHR pfn_vkCmdBuildAccelerationStructuresKHR= 0;
static PFN_vkCmdCopyAccelerationStructureKHR pfn_vkCmdCopyAccelerationStructureKHR= 0;
static PFN_vkCmdCopyAccelerationStructureToMemoryKHR pfn_vkCmdCopyAccelerationStructureToMemoryKHR= 0;
static PFN_vkCmdCopyMemoryToAccelerationStructureKHR pfn_vkCmdCopyMemoryToAccelerationStructureKHR= 0;
static PFN_vkCmdWriteAccelerationStructuresPropertiesKHR pfn_vkCmdWriteAccelerationStructuresPropertiesKHR= 0;
static PFN_vkCopyAccelerationStructureKHR pfn_vkCopyAccelerationStructureKHR= 0;
static PFN_vkCopyAccelerationStructureToMemoryKHR pfn_vkCopyAccelerationStructureToMemoryKHR= 0;
static PFN_vkCopyMemoryToAccelerationStructureKHR pfn_vkCopyMemoryToAccelerationStructureKHR= 0;
static PFN_vkCreateAccelerationStructureKHR pfn_vkCreateAccelerationStructureKHR= 0;
static PFN_vkDestroyAccelerationStructureKHR pfn_vkDestroyAccelerationStructureKHR= 0;
static PFN_vkGetAccelerationStructureBuildSizesKHR pfn_vkGetAccelerationStructureBuildSizesKHR= 0;
static PFN_vkGetAccelerationStructureDeviceAddressKHR pfn_vkGetAccelerationStructureDeviceAddressKHR= 0;
static PFN_vkGetDeviceAccelerationStructureCompatibilityKHR pfn_vkGetDeviceAccelerationStructureCompatibilityKHR= 0;
static PFN_vkWriteAccelerationStructuresPropertiesKHR pfn_vkWriteAccelerationStructuresPropertiesKHR= 0;
#endif /* VK_KHR_acceleration_structure */
#ifdef VK_KHR_android_surface
static PFN_vkCreateAndroidSurfaceKHR pfn_vkCreateAndroidSurfaceKHR= 0;
#endif /* VK_KHR_android_surface */
#ifdef VK_KHR_bind_memory2
static PFN_vkBindBufferMemory2KHR pfn_vkBindBufferMemory2KHR= 0;
static PFN_vkBindImageMemory2KHR pfn_vkBindImageMemory2KHR= 0;
#endif /* VK_KHR_bind_memory2 */
#ifdef VK_KHR_buffer_device_address
static PFN_vkGetBufferDeviceAddressKHR pfn_vkGetBufferDeviceAddressKHR= 0;
static PFN_vkGetBufferOpaqueCaptureAddressKHR pfn_vkGetBufferOpaqueCaptureAddressKHR= 0;
static PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR pfn_vkGetDeviceMemoryOpaqueCaptureAddressKHR= 0;
#endif /* VK_KHR_buffer_device_address */
#ifdef VK_KHR_copy_commands2
static PFN_vkCmdBlitImage2KHR pfn_vkCmdBlitImage2KHR= 0;
static PFN_vkCmdCopyBuffer2KHR pfn_vkCmdCopyBuffer2KHR= 0;
static PFN_vkCmdCopyBufferToImage2KHR pfn_vkCmdCopyBufferToImage2KHR= 0;
static PFN_vkCmdCopyImage2KHR pfn_vkCmdCopyImage2KHR= 0;
static PFN_vkCmdCopyImageToBuffer2KHR pfn_vkCmdCopyImageToBuffer2KHR= 0;
static PFN_vkCmdResolveImage2KHR pfn_vkCmdResolveImage2KHR= 0;
#endif /* VK_KHR_copy_commands2 */
#ifdef VK_KHR_create_renderpass2
static PFN_vkCmdBeginRenderPass2KHR pfn_vkCmdBeginRenderPass2KHR= 0;
static PFN_vkCmdEndRenderPass2KHR pfn_vkCmdEndRenderPass2KHR= 0;
static PFN_vkCmdNextSubpass2KHR pfn_vkCmdNextSubpass2KHR= 0;
static PFN_vkCreateRenderPass2KHR pfn_vkCreateRenderPass2KHR= 0;
#endif /* VK_KHR_create_renderpass2 */
#ifdef VK_KHR_deferred_host_operations
static PFN_vkCreateDeferredOperationKHR pfn_vkCreateDeferredOperationKHR= 0;
static PFN_vkDeferredOperationJoinKHR pfn_vkDeferredOperationJoinKHR= 0;
static PFN_vkDestroyDeferredOperationKHR pfn_vkDestroyDeferredOperationKHR= 0;
static PFN_vkGetDeferredOperationMaxConcurrencyKHR pfn_vkGetDeferredOperationMaxConcurrencyKHR= 0;
static PFN_vkGetDeferredOperationResultKHR pfn_vkGetDeferredOperationResultKHR= 0;
#endif /* VK_KHR_deferred_host_operations */
#ifdef VK_KHR_descriptor_update_template
static PFN_vkCreateDescriptorUpdateTemplateKHR pfn_vkCreateDescriptorUpdateTemplateKHR= 0;
static PFN_vkDestroyDescriptorUpdateTemplateKHR pfn_vkDestroyDescriptorUpdateTemplateKHR= 0;
static PFN_vkUpdateDescriptorSetWithTemplateKHR pfn_vkUpdateDescriptorSetWithTemplateKHR= 0;
#endif /* VK_KHR_descriptor_update_template */
#ifdef VK_KHR_device_group
static PFN_vkCmdDispatchBaseKHR pfn_vkCmdDispatchBaseKHR= 0;
static PFN_vkCmdSetDeviceMaskKHR pfn_vkCmdSetDeviceMaskKHR= 0;
static PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR pfn_vkGetDeviceGroupPeerMemoryFeaturesKHR= 0;
#endif /* VK_KHR_device_group */
#ifdef VK_KHR_device_group_creation
static PFN_vkEnumeratePhysicalDeviceGroupsKHR pfn_vkEnumeratePhysicalDeviceGroupsKHR= 0;
#endif /* VK_KHR_device_group_creation */
#ifdef VK_KHR_draw_indirect_count
static PFN_vkCmdDrawIndexedIndirectCountKHR pfn_vkCmdDrawIndexedIndirectCountKHR= 0;
static PFN_vkCmdDrawIndirectCountKHR pfn_vkCmdDrawIndirectCountKHR= 0;
#endif /* VK_KHR_draw_indirect_count */
#ifdef VK_KHR_external_fence_capabilities
static PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR pfn_vkGetPhysicalDeviceExternalFencePropertiesKHR= 0;
#endif /* VK_KHR_external_fence_capabilities */
#ifdef VK_KHR_external_fence_fd
static PFN_vkGetFenceFdKHR pfn_vkGetFenceFdKHR= 0;
static PFN_vkImportFenceFdKHR pfn_vkImportFenceFdKHR= 0;
#endif /* VK_KHR_external_fence_fd */
#ifdef VK_KHR_external_fence_win32
static PFN_vkGetFenceWin32HandleKHR pfn_vkGetFenceWin32HandleKHR= 0;
static PFN_vkImportFenceWin32HandleKHR pfn_vkImportFenceWin32HandleKHR= 0;
#endif /* VK_KHR_external_fence_win32 */
#ifdef VK_KHR_external_memory_capabilities
static PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR pfn_vkGetPhysicalDeviceExternalBufferPropertiesKHR= 0;
#endif /* VK_KHR_external_memory_capabilities */
#ifdef VK_KHR_external_memory_fd
static PFN_vkGetMemoryFdKHR pfn_vkGetMemoryFdKHR= 0;
static PFN_vkGetMemoryFdPropertiesKHR pfn_vkGetMemoryFdPropertiesKHR= 0;
#endif /* VK_KHR_external_memory_fd */
#ifdef VK_KHR_external_memory_win32
static PFN_vkGetMemoryWin32HandleKHR pfn_vkGetMemoryWin32HandleKHR= 0;
static PFN_vkGetMemoryWin32HandlePropertiesKHR pfn_vkGetMemoryWin32HandlePropertiesKHR= 0;
#endif /* VK_KHR_external_memory_win32 */
#ifdef VK_KHR_external_semaphore_capabilities
static PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR pfn_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR= 0;
#endif /* VK_KHR_external_semaphore_capabilities */
#ifdef VK_KHR_external_semaphore_fd
static PFN_vkGetSemaphoreFdKHR pfn_vkGetSemaphoreFdKHR= 0;
static PFN_vkImportSemaphoreFdKHR pfn_vkImportSemaphoreFdKHR= 0;
#endif /* VK_KHR_external_semaphore_fd */
#ifdef VK_KHR_external_semaphore_win32
static PFN_vkGetSemaphoreWin32HandleKHR pfn_vkGetSemaphoreWin32HandleKHR= 0;
static PFN_vkImportSemaphoreWin32HandleKHR pfn_vkImportSemaphoreWin32HandleKHR= 0;
#endif /* VK_KHR_external_semaphore_win32 */
#ifdef VK_KHR_fragment_shading_rate
static PFN_vkCmdSetFragmentShadingRateKHR pfn_vkCmdSetFragmentShadingRateKHR= 0;
static PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR= 0;
#endif /* VK_KHR_fragment_shading_rate */
#ifdef VK_KHR_get_memory_requirements2
static PFN_vkGetBufferMemoryRequirements2KHR pfn_vkGetBufferMemoryRequirements2KHR= 0;
static PFN_vkGetImageMemoryRequirements2KHR pfn_vkGetImageMemoryRequirements2KHR= 0;
static PFN_vkGetImageSparseMemoryRequirements2KHR pfn_vkGetImageSparseMemoryRequirements2KHR= 0;
#endif /* VK_KHR_get_memory_requirements2 */
#ifdef VK_KHR_get_physical_device_properties2
static PFN_vkGetPhysicalDeviceFeatures2KHR pfn_vkGetPhysicalDeviceFeatures2KHR= 0;
static PFN_vkGetPhysicalDeviceFormatProperties2KHR pfn_vkGetPhysicalDeviceFormatProperties2KHR= 0;
static PFN_vkGetPhysicalDeviceImageFormatProperties2KHR pfn_vkGetPhysicalDeviceImageFormatProperties2KHR= 0;
static PFN_vkGetPhysicalDeviceMemoryProperties2KHR pfn_vkGetPhysicalDeviceMemoryProperties2KHR= 0;
static PFN_vkGetPhysicalDeviceProperties2KHR pfn_vkGetPhysicalDeviceProperties2KHR= 0;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR pfn_vkGetPhysicalDeviceQueueFamilyProperties2KHR= 0;
static PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR pfn_vkGetPhysicalDeviceSparseImageFormatProperties2KHR= 0;
#endif /* VK_KHR_get_physical_device_properties2 */
#ifdef VK_KHR_maintenance1
static PFN_vkTrimCommandPoolKHR pfn_vkTrimCommandPoolKHR= 0;
#endif /* VK_KHR_maintenance1 */
#ifdef VK_KHR_maintenance3
static PFN_vkGetDescriptorSetLayoutSupportKHR pfn_vkGetDescriptorSetLayoutSupportKHR= 0;
#endif /* VK_KHR_maintenance3 */
#ifdef VK_KHR_performance_query
static PFN_vkAcquireProfilingLockKHR pfn_vkAcquireProfilingLockKHR= 0;
static PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR pfn_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR= 0;
static PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR pfn_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR= 0;
static PFN_vkReleaseProfilingLockKHR pfn_vkReleaseProfilingLockKHR= 0;
#endif /* VK_KHR_performance_query */
#ifdef VK_KHR_pipeline_executable_properties
static PFN_vkGetPipelineExecutableInternalRepresentationsKHR pfn_vkGetPipelineExecutableInternalRepresentationsKHR= 0;
static PFN_vkGetPipelineExecutablePropertiesKHR pfn_vkGetPipelineExecutablePropertiesKHR= 0;
static PFN_vkGetPipelineExecutableStatisticsKHR pfn_vkGetPipelineExecutableStatisticsKHR= 0;
#endif /* VK_KHR_pipeline_executable_properties */
#ifdef VK_KHR_push_descriptor
static PFN_vkCmdPushDescriptorSetKHR pfn_vkCmdPushDescriptorSetKHR= 0;
#endif /* VK_KHR_push_descriptor */
#ifdef VK_KHR_ray_tracing_pipeline
static PFN_vkCmdSetRayTracingPipelineStackSizeKHR pfn_vkCmdSetRayTracingPipelineStackSizeKHR= 0;
static PFN_vkCmdTraceRaysIndirectKHR pfn_vkCmdTraceRaysIndirectKHR= 0;
static PFN_vkCmdTraceRaysKHR pfn_vkCmdTraceRaysKHR= 0;
static PFN_vkCreateRayTracingPipelinesKHR pfn_vkCreateRayTracingPipelinesKHR= 0;
static PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR= 0;
static PFN_vkGetRayTracingShaderGroupHandlesKHR pfn_vkGetRayTracingShaderGroupHandlesKHR= 0;
static PFN_vkGetRayTracingShaderGroupStackSizeKHR pfn_vkGetRayTracingShaderGroupStackSizeKHR= 0;
#endif /* VK_KHR_ray_tracing_pipeline */
#ifdef VK_KHR_sampler_ycbcr_conversion
static PFN_vkCreateSamplerYcbcrConversionKHR pfn_vkCreateSamplerYcbcrConversionKHR= 0;
static PFN_vkDestroySamplerYcbcrConversionKHR pfn_vkDestroySamplerYcbcrConversionKHR= 0;
#endif /* VK_KHR_sampler_ycbcr_conversion */
#ifdef VK_KHR_shared_presentable_image
static PFN_vkGetSwapchainStatusKHR pfn_vkGetSwapchainStatusKHR= 0;
#endif /* VK_KHR_shared_presentable_image */
#ifdef VK_KHR_synchronization2
static PFN_vkCmdPipelineBarrier2KHR pfn_vkCmdPipelineBarrier2KHR= 0;
static PFN_vkCmdResetEvent2KHR pfn_vkCmdResetEvent2KHR= 0;
static PFN_vkCmdSetEvent2KHR pfn_vkCmdSetEvent2KHR= 0;
static PFN_vkCmdWaitEvents2KHR pfn_vkCmdWaitEvents2KHR= 0;
static PFN_vkCmdWriteBufferMarker2AMD pfn_vkCmdWriteBufferMarker2AMD= 0;
static PFN_vkCmdWriteTimestamp2KHR pfn_vkCmdWriteTimestamp2KHR= 0;
static PFN_vkGetQueueCheckpointData2NV pfn_vkGetQueueCheckpointData2NV= 0;
static PFN_vkQueueSubmit2KHR pfn_vkQueueSubmit2KHR= 0;
#endif /* VK_KHR_synchronization2 */
#ifdef VK_KHR_timeline_semaphore
static PFN_vkGetSemaphoreCounterValueKHR pfn_vkGetSemaphoreCounterValueKHR= 0;
static PFN_vkSignalSemaphoreKHR pfn_vkSignalSemaphoreKHR= 0;
static PFN_vkWaitSemaphoresKHR pfn_vkWaitSemaphoresKHR= 0;
#endif /* VK_KHR_timeline_semaphore */
#ifdef VK_KHR_video_decode_queue
static PFN_vkCmdDecodeVideoKHR pfn_vkCmdDecodeVideoKHR= 0;
#endif /* VK_KHR_video_decode_queue */
#ifdef VK_KHR_video_encode_queue
static PFN_vkCmdEncodeVideoKHR pfn_vkCmdEncodeVideoKHR= 0;
#endif /* VK_KHR_video_encode_queue */
#ifdef VK_KHR_video_queue
static PFN_vkBindVideoSessionMemoryKHR pfn_vkBindVideoSessionMemoryKHR= 0;
static PFN_vkCmdBeginVideoCodingKHR pfn_vkCmdBeginVideoCodingKHR= 0;
static PFN_vkCmdControlVideoCodingKHR pfn_vkCmdControlVideoCodingKHR= 0;
static PFN_vkCmdEndVideoCodingKHR pfn_vkCmdEndVideoCodingKHR= 0;
static PFN_vkCreateVideoSessionKHR pfn_vkCreateVideoSessionKHR= 0;
static PFN_vkCreateVideoSessionParametersKHR pfn_vkCreateVideoSessionParametersKHR= 0;
static PFN_vkDestroyVideoSessionKHR pfn_vkDestroyVideoSessionKHR= 0;
static PFN_vkDestroyVideoSessionParametersKHR pfn_vkDestroyVideoSessionParametersKHR= 0;
static PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR pfn_vkGetPhysicalDeviceVideoCapabilitiesKHR= 0;
static PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR pfn_vkGetPhysicalDeviceVideoFormatPropertiesKHR= 0;
static PFN_vkGetVideoSessionMemoryRequirementsKHR pfn_vkGetVideoSessionMemoryRequirementsKHR= 0;
static PFN_vkUpdateVideoSessionParametersKHR pfn_vkUpdateVideoSessionParametersKHR= 0;
#endif /* VK_KHR_video_queue */
#ifdef VK_MVK_ios_surface
static PFN_vkCreateIOSSurfaceMVK pfn_vkCreateIOSSurfaceMVK= 0;
#endif /* VK_MVK_ios_surface */
#ifdef VK_MVK_macos_surface
static PFN_vkCreateMacOSSurfaceMVK pfn_vkCreateMacOSSurfaceMVK= 0;
#endif /* VK_MVK_macos_surface */
#ifdef VK_NN_vi_surface
static PFN_vkCreateViSurfaceNN pfn_vkCreateViSurfaceNN= 0;
#endif /* VK_NN_vi_surface */
#ifdef VK_NVX_image_view_handle
static PFN_vkGetImageViewAddressNVX pfn_vkGetImageViewAddressNVX= 0;
static PFN_vkGetImageViewHandleNVX pfn_vkGetImageViewHandleNVX= 0;
#endif /* VK_NVX_image_view_handle */
#ifdef VK_NV_acquire_winrt_display
static PFN_vkAcquireWinrtDisplayNV pfn_vkAcquireWinrtDisplayNV= 0;
static PFN_vkGetWinrtDisplayNV pfn_vkGetWinrtDisplayNV= 0;
#endif /* VK_NV_acquire_winrt_display */
#ifdef VK_NV_clip_space_w_scaling
static PFN_vkCmdSetViewportWScalingNV pfn_vkCmdSetViewportWScalingNV= 0;
#endif /* VK_NV_clip_space_w_scaling */
#ifdef VK_NV_cooperative_matrix
static PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV= 0;
#endif /* VK_NV_cooperative_matrix */
#ifdef VK_NV_coverage_reduction_mode
static PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV pfn_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV= 0;
#endif /* VK_NV_coverage_reduction_mode */
#ifdef VK_NV_device_diagnostic_checkpoints
static PFN_vkCmdSetCheckpointNV pfn_vkCmdSetCheckpointNV= 0;
static PFN_vkGetQueueCheckpointDataNV pfn_vkGetQueueCheckpointDataNV= 0;
#endif /* VK_NV_device_diagnostic_checkpoints */
#ifdef VK_NV_device_generated_commands
static PFN_vkCmdBindPipelineShaderGroupNV pfn_vkCmdBindPipelineShaderGroupNV= 0;
static PFN_vkCmdExecuteGeneratedCommandsNV pfn_vkCmdExecuteGeneratedCommandsNV= 0;
static PFN_vkCmdPreprocessGeneratedCommandsNV pfn_vkCmdPreprocessGeneratedCommandsNV= 0;
static PFN_vkCreateIndirectCommandsLayoutNV pfn_vkCreateIndirectCommandsLayoutNV= 0;
static PFN_vkDestroyIndirectCommandsLayoutNV pfn_vkDestroyIndirectCommandsLayoutNV= 0;
static PFN_vkGetGeneratedCommandsMemoryRequirementsNV pfn_vkGetGeneratedCommandsMemoryRequirementsNV= 0;
#endif /* VK_NV_device_generated_commands */
#ifdef VK_NV_external_memory_capabilities
static PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV pfn_vkGetPhysicalDeviceExternalImageFormatPropertiesNV= 0;
#endif /* VK_NV_external_memory_capabilities */
#ifdef VK_NV_external_memory_win32
static PFN_vkGetMemoryWin32HandleNV pfn_vkGetMemoryWin32HandleNV= 0;
#endif /* VK_NV_external_memory_win32 */
#ifdef VK_NV_fragment_shading_rate_enums
static PFN_vkCmdSetFragmentShadingRateEnumNV pfn_vkCmdSetFragmentShadingRateEnumNV= 0;
#endif /* VK_NV_fragment_shading_rate_enums */
#ifdef VK_NV_mesh_shader
static PFN_vkCmdDrawMeshTasksIndirectCountNV pfn_vkCmdDrawMeshTasksIndirectCountNV= 0;
static PFN_vkCmdDrawMeshTasksIndirectNV pfn_vkCmdDrawMeshTasksIndirectNV= 0;
static PFN_vkCmdDrawMeshTasksNV pfn_vkCmdDrawMeshTasksNV= 0;
#endif /* VK_NV_mesh_shader */
#ifdef VK_NV_ray_tracing
static PFN_vkBindAccelerationStructureMemoryNV pfn_vkBindAccelerationStructureMemoryNV= 0;
static PFN_vkCmdBuildAccelerationStructureNV pfn_vkCmdBuildAccelerationStructureNV= 0;
static PFN_vkCmdCopyAccelerationStructureNV pfn_vkCmdCopyAccelerationStructureNV= 0;
static PFN_vkCmdTraceRaysNV pfn_vkCmdTraceRaysNV= 0;
static PFN_vkCmdWriteAccelerationStructuresPropertiesNV pfn_vkCmdWriteAccelerationStructuresPropertiesNV= 0;
static PFN_vkCompileDeferredNV pfn_vkCompileDeferredNV= 0;
static PFN_vkCreateAccelerationStructureNV pfn_vkCreateAccelerationStructureNV= 0;
static PFN_vkCreateRayTracingPipelinesNV pfn_vkCreateRayTracingPipelinesNV= 0;
static PFN_vkDestroyAccelerationStructureNV pfn_vkDestroyAccelerationStructureNV= 0;
static PFN_vkGetAccelerationStructureHandleNV pfn_vkGetAccelerationStructureHandleNV= 0;
static PFN_vkGetAccelerationStructureMemoryRequirementsNV pfn_vkGetAccelerationStructureMemoryRequirementsNV= 0;
static PFN_vkGetRayTracingShaderGroupHandlesNV pfn_vkGetRayTracingShaderGroupHandlesNV= 0;
#endif /* VK_NV_ray_tracing */
#ifdef VK_NV_scissor_exclusive
static PFN_vkCmdSetExclusiveScissorNV pfn_vkCmdSetExclusiveScissorNV= 0;
#endif /* VK_NV_scissor_exclusive */
#ifdef VK_NV_shading_rate_image
static PFN_vkCmdBindShadingRateImageNV pfn_vkCmdBindShadingRateImageNV= 0;
static PFN_vkCmdSetCoarseSampleOrderNV pfn_vkCmdSetCoarseSampleOrderNV= 0;
static PFN_vkCmdSetViewportShadingRatePaletteNV pfn_vkCmdSetViewportShadingRatePaletteNV= 0;
#endif /* VK_NV_shading_rate_image */
#ifdef VK_QNX_screen_surface
static PFN_vkCreateScreenSurfaceQNX pfn_vkCreateScreenSurfaceQNX= 0;
static PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX pfn_vkGetPhysicalDeviceScreenPresentationSupportQNX= 0;
#endif /* VK_QNX_screen_surface */
/* NVVK_GENERATE_STATIC_PFN */


/* NVVK_GENERATE_DECLARE */
#ifdef VK_AMD_buffer_marker
VKAPI_ATTR void VKAPI_CALL vkCmdWriteBufferMarkerAMD(
	VkCommandBuffer commandBuffer, 
	VkPipelineStageFlagBits pipelineStage, 
	VkBuffer dstBuffer, 
	VkDeviceSize dstOffset, 
	uint32_t marker) 
{ 
  pfn_vkCmdWriteBufferMarkerAMD(commandBuffer, pipelineStage, dstBuffer, dstOffset, marker); 
}
#endif /* VK_AMD_buffer_marker */
#ifdef VK_AMD_display_native_hdr
VKAPI_ATTR void VKAPI_CALL vkSetLocalDimmingAMD(
	VkDevice device, 
	VkSwapchainKHR swapChain, 
	VkBool32 localDimmingEnable) 
{ 
  pfn_vkSetLocalDimmingAMD(device, swapChain, localDimmingEnable); 
}
#endif /* VK_AMD_display_native_hdr */
#ifdef VK_AMD_draw_indirect_count
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCountAMD(
	VkCommandBuffer commandBuffer, 
	VkBuffer buffer, 
	VkDeviceSize offset, 
	VkBuffer countBuffer, 
	VkDeviceSize countBufferOffset, 
	uint32_t maxDrawCount, 
	uint32_t stride) 
{ 
  pfn_vkCmdDrawIndexedIndirectCountAMD(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCountAMD(
	VkCommandBuffer commandBuffer, 
	VkBuffer buffer, 
	VkDeviceSize offset, 
	VkBuffer countBuffer, 
	VkDeviceSize countBufferOffset, 
	uint32_t maxDrawCount, 
	uint32_t stride) 
{ 
  pfn_vkCmdDrawIndirectCountAMD(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); 
}
#endif /* VK_AMD_draw_indirect_count */
#ifdef VK_AMD_shader_info
VKAPI_ATTR VkResult VKAPI_CALL vkGetShaderInfoAMD(
	VkDevice device, 
	VkPipeline pipeline, 
	VkShaderStageFlagBits shaderStage, 
	VkShaderInfoTypeAMD infoType, 
	size_t* pInfoSize, 
	void* pInfo) 
{ 
  return pfn_vkGetShaderInfoAMD(device, pipeline, shaderStage, infoType, pInfoSize, pInfo); 
}
#endif /* VK_AMD_shader_info */
#ifdef VK_ANDROID_external_memory_android_hardware_buffer
VKAPI_ATTR VkResult VKAPI_CALL vkGetAndroidHardwareBufferPropertiesANDROID(
	VkDevice device, 
	const struct AHardwareBuffer* buffer, 
	VkAndroidHardwareBufferPropertiesANDROID* pProperties) 
{ 
  return pfn_vkGetAndroidHardwareBufferPropertiesANDROID(device, buffer, pProperties); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryAndroidHardwareBufferANDROID(
	VkDevice device, 
	const VkMemoryGetAndroidHardwareBufferInfoANDROID* pInfo, 
	struct AHardwareBuffer** pBuffer) 
{ 
  return pfn_vkGetMemoryAndroidHardwareBufferANDROID(device, pInfo, pBuffer); 
}
#endif /* VK_ANDROID_external_memory_android_hardware_buffer */
#ifdef VK_EXT_acquire_xlib_display
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireXlibDisplayEXT(
	VkPhysicalDevice physicalDevice, 
	Display* dpy, 
	VkDisplayKHR display) 
{ 
  return pfn_vkAcquireXlibDisplayEXT(physicalDevice, dpy, display); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetRandROutputDisplayEXT(
	VkPhysicalDevice physicalDevice, 
	Display* dpy, 
	RROutput rrOutput, 
	VkDisplayKHR* pDisplay) 
{ 
  return pfn_vkGetRandROutputDisplayEXT(physicalDevice, dpy, rrOutput, pDisplay); 
}
#endif /* VK_EXT_acquire_xlib_display */
#ifdef VK_EXT_buffer_device_address
VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddressEXT(
	VkDevice device, 
	const VkBufferDeviceAddressInfo* pInfo) 
{ 
  return pfn_vkGetBufferDeviceAddressEXT(device, pInfo); 
}
#endif /* VK_EXT_buffer_device_address */
#ifdef VK_EXT_calibrated_timestamps
VKAPI_ATTR VkResult VKAPI_CALL vkGetCalibratedTimestampsEXT(
	VkDevice device, 
	uint32_t timestampCount, 
	const VkCalibratedTimestampInfoEXT* pTimestampInfos, 
	uint64_t* pTimestamps, 
	uint64_t* pMaxDeviation) 
{ 
  return pfn_vkGetCalibratedTimestampsEXT(device, timestampCount, pTimestampInfos, pTimestamps, pMaxDeviation); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(
	VkPhysicalDevice physicalDevice, 
	uint32_t* pTimeDomainCount, 
	VkTimeDomainEXT* pTimeDomains) 
{ 
  return pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(physicalDevice, pTimeDomainCount, pTimeDomains); 
}
#endif /* VK_EXT_calibrated_timestamps */
#ifdef VK_EXT_color_write_enable
VKAPI_ATTR void VKAPI_CALL vkCmdSetColorWriteEnableEXT(
	VkCommandBuffer       commandBuffer, 
	uint32_t                                attachmentCount, 
	const VkBool32*   pColorWriteEnables) 
{ 
  pfn_vkCmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables); 
}
#endif /* VK_EXT_color_write_enable */
#ifdef VK_EXT_conditional_rendering
VKAPI_ATTR void VKAPI_CALL vkCmdBeginConditionalRenderingEXT(
	VkCommandBuffer commandBuffer, 
	const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) 
{ 
  pfn_vkCmdBeginConditionalRenderingEXT(commandBuffer, pConditionalRenderingBegin); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdEndConditionalRenderingEXT(
	VkCommandBuffer commandBuffer) 
{ 
  pfn_vkCmdEndConditionalRenderingEXT(commandBuffer); 
}
#endif /* VK_EXT_conditional_rendering */
#ifdef VK_EXT_debug_marker
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerBeginEXT(
	VkCommandBuffer commandBuffer, 
	const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) 
{ 
  pfn_vkCmdDebugMarkerBeginEXT(commandBuffer, pMarkerInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerEndEXT(
	VkCommandBuffer commandBuffer) 
{ 
  pfn_vkCmdDebugMarkerEndEXT(commandBuffer); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerInsertEXT(
	VkCommandBuffer commandBuffer, 
	const VkDebugMarkerMarkerInfoEXT* pMarkerInfo) 
{ 
  pfn_vkCmdDebugMarkerInsertEXT(commandBuffer, pMarkerInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectNameEXT(
	VkDevice device, 
	const VkDebugMarkerObjectNameInfoEXT* pNameInfo) 
{ 
  return pfn_vkDebugMarkerSetObjectNameEXT(device, pNameInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectTagEXT(
	VkDevice device, 
	const VkDebugMarkerObjectTagInfoEXT* pTagInfo) 
{ 
  return pfn_vkDebugMarkerSetObjectTagEXT(device, pTagInfo); 
}
#endif /* VK_EXT_debug_marker */
#ifdef VK_EXT_debug_report
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugReportCallbackEXT(
	VkInstance instance, 
	const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkDebugReportCallbackEXT* pCallback) 
{ 
  return pfn_vkCreateDebugReportCallbackEXT(instance, pCreateInfo, pAllocator, pCallback); 
}
VKAPI_ATTR void VKAPI_CALL vkDebugReportMessageEXT(
	VkInstance instance, 
	VkDebugReportFlagsEXT flags, 
	VkDebugReportObjectTypeEXT objectType, 
	uint64_t object, 
	size_t location, 
	int32_t messageCode, 
	const char* pLayerPrefix, 
	const char* pMessage) 
{ 
  pfn_vkDebugReportMessageEXT(instance, flags, objectType, object, location, messageCode, pLayerPrefix, pMessage); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDebugReportCallbackEXT(
	VkInstance instance, 
	VkDebugReportCallbackEXT callback, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyDebugReportCallbackEXT(instance, callback, pAllocator); 
}
#endif /* VK_EXT_debug_report */
#ifdef VK_EXT_debug_utils
VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(
	VkCommandBuffer commandBuffer, 
	const VkDebugUtilsLabelEXT* pLabelInfo) 
{ 
  pfn_vkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(
	VkCommandBuffer commandBuffer) 
{ 
  pfn_vkCmdEndDebugUtilsLabelEXT(commandBuffer); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdInsertDebugUtilsLabelEXT(
	VkCommandBuffer commandBuffer, 
	const VkDebugUtilsLabelEXT* pLabelInfo) 
{ 
  pfn_vkCmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
	VkInstance instance, 
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkDebugUtilsMessengerEXT* pMessenger) 
{ 
  return pfn_vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
	VkInstance instance, 
	VkDebugUtilsMessengerEXT messenger, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator); 
}
VKAPI_ATTR void VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(
	VkQueue queue, 
	const VkDebugUtilsLabelEXT* pLabelInfo) 
{ 
  pfn_vkQueueBeginDebugUtilsLabelEXT(queue, pLabelInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(
	VkQueue queue) 
{ 
  pfn_vkQueueEndDebugUtilsLabelEXT(queue); 
}
VKAPI_ATTR void VKAPI_CALL vkQueueInsertDebugUtilsLabelEXT(
	VkQueue queue, 
	const VkDebugUtilsLabelEXT* pLabelInfo) 
{ 
  pfn_vkQueueInsertDebugUtilsLabelEXT(queue, pLabelInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(
	VkDevice device, 
	const VkDebugUtilsObjectNameInfoEXT* pNameInfo) 
{ 
  return pfn_vkSetDebugUtilsObjectNameEXT(device, pNameInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectTagEXT(
	VkDevice device, 
	const VkDebugUtilsObjectTagInfoEXT* pTagInfo) 
{ 
  return pfn_vkSetDebugUtilsObjectTagEXT(device, pTagInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkSubmitDebugUtilsMessageEXT(
	VkInstance instance, 
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
	VkDebugUtilsMessageTypeFlagsEXT messageTypes, 
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) 
{ 
  pfn_vkSubmitDebugUtilsMessageEXT(instance, messageSeverity, messageTypes, pCallbackData); 
}
#endif /* VK_EXT_debug_utils */
#ifdef VK_EXT_direct_mode_display
VKAPI_ATTR VkResult VKAPI_CALL vkReleaseDisplayEXT(
	VkPhysicalDevice physicalDevice, 
	VkDisplayKHR display) 
{ 
  return pfn_vkReleaseDisplayEXT(physicalDevice, display); 
}
#endif /* VK_EXT_direct_mode_display */
#ifdef VK_EXT_directfb_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDirectFBSurfaceEXT(
	VkInstance instance, 
	const VkDirectFBSurfaceCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateDirectFBSurfaceEXT(instance, pCreateInfo, pAllocator, pSurface); 
}
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceDirectFBPresentationSupportEXT(
	VkPhysicalDevice physicalDevice, 
	uint32_t queueFamilyIndex, 
	IDirectFB* dfb) 
{ 
  return pfn_vkGetPhysicalDeviceDirectFBPresentationSupportEXT(physicalDevice, queueFamilyIndex, dfb); 
}
#endif /* VK_EXT_directfb_surface */
#ifdef VK_EXT_discard_rectangles
VKAPI_ATTR void VKAPI_CALL vkCmdSetDiscardRectangleEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t firstDiscardRectangle, 
	uint32_t discardRectangleCount, 
	const VkRect2D* pDiscardRectangles) 
{ 
  pfn_vkCmdSetDiscardRectangleEXT(commandBuffer, firstDiscardRectangle, discardRectangleCount, pDiscardRectangles); 
}
#endif /* VK_EXT_discard_rectangles */
#ifdef VK_EXT_display_control
VKAPI_ATTR VkResult VKAPI_CALL vkDisplayPowerControlEXT(
	VkDevice device, 
	VkDisplayKHR display, 
	const VkDisplayPowerInfoEXT* pDisplayPowerInfo) 
{ 
  return pfn_vkDisplayPowerControlEXT(device, display, pDisplayPowerInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainCounterEXT(
	VkDevice device, 
	VkSwapchainKHR swapchain, 
	VkSurfaceCounterFlagBitsEXT counter, 
	uint64_t* pCounterValue) 
{ 
  return pfn_vkGetSwapchainCounterEXT(device, swapchain, counter, pCounterValue); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkRegisterDeviceEventEXT(
	VkDevice device, 
	const VkDeviceEventInfoEXT* pDeviceEventInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkFence* pFence) 
{ 
  return pfn_vkRegisterDeviceEventEXT(device, pDeviceEventInfo, pAllocator, pFence); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkRegisterDisplayEventEXT(
	VkDevice device, 
	VkDisplayKHR display, 
	const VkDisplayEventInfoEXT* pDisplayEventInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkFence* pFence) 
{ 
  return pfn_vkRegisterDisplayEventEXT(device, display, pDisplayEventInfo, pAllocator, pFence); 
}
#endif /* VK_EXT_display_control */
#ifdef VK_EXT_display_surface_counter
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilities2EXT(
	VkPhysicalDevice physicalDevice, 
	VkSurfaceKHR surface, 
	VkSurfaceCapabilities2EXT* pSurfaceCapabilities) 
{ 
  return pfn_vkGetPhysicalDeviceSurfaceCapabilities2EXT(physicalDevice, surface, pSurfaceCapabilities); 
}
#endif /* VK_EXT_display_surface_counter */
#ifdef VK_EXT_extended_dynamic_state
VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers2EXT(
	VkCommandBuffer commandBuffer, 
	uint32_t firstBinding, 
	uint32_t bindingCount, 
	const VkBuffer* pBuffers, 
	const VkDeviceSize* pOffsets, 
	const VkDeviceSize* pSizes, 
	const VkDeviceSize* pStrides) 
{ 
  pfn_vkCmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetCullModeEXT(
	VkCommandBuffer commandBuffer, 
	VkCullModeFlags cullMode) 
{ 
  pfn_vkCmdSetCullModeEXT(commandBuffer, cullMode); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBoundsTestEnableEXT(
	VkCommandBuffer commandBuffer, 
	VkBool32 depthBoundsTestEnable) 
{ 
  pfn_vkCmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthCompareOpEXT(
	VkCommandBuffer commandBuffer, 
	VkCompareOp depthCompareOp) 
{ 
  pfn_vkCmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthTestEnableEXT(
	VkCommandBuffer commandBuffer, 
	VkBool32 depthTestEnable) 
{ 
  pfn_vkCmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthWriteEnableEXT(
	VkCommandBuffer commandBuffer, 
	VkBool32 depthWriteEnable) 
{ 
  pfn_vkCmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetFrontFaceEXT(
	VkCommandBuffer commandBuffer, 
	VkFrontFace frontFace) 
{ 
  pfn_vkCmdSetFrontFaceEXT(commandBuffer, frontFace); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetPrimitiveTopologyEXT(
	VkCommandBuffer commandBuffer, 
	VkPrimitiveTopology primitiveTopology) 
{ 
  pfn_vkCmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetScissorWithCountEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t scissorCount, 
	const VkRect2D* pScissors) 
{ 
  pfn_vkCmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilOpEXT(
	VkCommandBuffer commandBuffer, 
	VkStencilFaceFlags faceMask, 
	VkStencilOp failOp, 
	VkStencilOp passOp, 
	VkStencilOp depthFailOp, 
	VkCompareOp compareOp) 
{ 
  pfn_vkCmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilTestEnableEXT(
	VkCommandBuffer commandBuffer, 
	VkBool32 stencilTestEnable) 
{ 
  pfn_vkCmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewportWithCountEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t viewportCount, 
	const VkViewport* pViewports) 
{ 
  pfn_vkCmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports); 
}
#endif /* VK_EXT_extended_dynamic_state */
#ifdef VK_EXT_extended_dynamic_state2
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBiasEnableEXT(
	VkCommandBuffer commandBuffer, 
	VkBool32 depthBiasEnable) 
{ 
  pfn_vkCmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetLogicOpEXT(
	VkCommandBuffer commandBuffer, 
	VkLogicOp logicOp) 
{ 
  pfn_vkCmdSetLogicOpEXT(commandBuffer, logicOp); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetPatchControlPointsEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t patchControlPoints) 
{ 
  pfn_vkCmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetPrimitiveRestartEnableEXT(
	VkCommandBuffer commandBuffer, 
	VkBool32 primitiveRestartEnable) 
{ 
  pfn_vkCmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetRasterizerDiscardEnableEXT(
	VkCommandBuffer commandBuffer, 
	VkBool32 rasterizerDiscardEnable) 
{ 
  pfn_vkCmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable); 
}
#endif /* VK_EXT_extended_dynamic_state2 */
#ifdef VK_EXT_external_memory_host
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryHostPointerPropertiesEXT(
	VkDevice device, 
	VkExternalMemoryHandleTypeFlagBits handleType, 
	const void* pHostPointer, 
	VkMemoryHostPointerPropertiesEXT* pMemoryHostPointerProperties) 
{ 
  return pfn_vkGetMemoryHostPointerPropertiesEXT(device, handleType, pHostPointer, pMemoryHostPointerProperties); 
}
#endif /* VK_EXT_external_memory_host */
#ifdef VK_EXT_full_screen_exclusive
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireFullScreenExclusiveModeEXT(
	VkDevice device, 
	VkSwapchainKHR swapchain) 
{ 
  return pfn_vkAcquireFullScreenExclusiveModeEXT(device, swapchain); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModes2EXT(
	VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceSurfaceInfo2KHR* pSurfaceInfo, 
	uint32_t* pPresentModeCount, 
	VkPresentModeKHR* pPresentModes) 
{ 
  return pfn_vkGetPhysicalDeviceSurfacePresentModes2EXT(physicalDevice, pSurfaceInfo, pPresentModeCount, pPresentModes); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkReleaseFullScreenExclusiveModeEXT(
	VkDevice device, 
	VkSwapchainKHR swapchain) 
{ 
  return pfn_vkReleaseFullScreenExclusiveModeEXT(device, swapchain); 
}
#endif /* VK_EXT_full_screen_exclusive */
#ifdef VK_EXT_hdr_metadata
VKAPI_ATTR void VKAPI_CALL vkSetHdrMetadataEXT(
	VkDevice device, 
	uint32_t swapchainCount, 
	const VkSwapchainKHR* pSwapchains, 
	const VkHdrMetadataEXT* pMetadata) 
{ 
  pfn_vkSetHdrMetadataEXT(device, swapchainCount, pSwapchains, pMetadata); 
}
#endif /* VK_EXT_hdr_metadata */
#ifdef VK_EXT_headless_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateHeadlessSurfaceEXT(
	VkInstance instance, 
	const VkHeadlessSurfaceCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateHeadlessSurfaceEXT(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_EXT_headless_surface */
#ifdef VK_EXT_host_query_reset
VKAPI_ATTR void VKAPI_CALL vkResetQueryPoolEXT(
	VkDevice device, 
	VkQueryPool queryPool, 
	uint32_t firstQuery, 
	uint32_t queryCount) 
{ 
  pfn_vkResetQueryPoolEXT(device, queryPool, firstQuery, queryCount); 
}
#endif /* VK_EXT_host_query_reset */
#ifdef VK_EXT_image_drm_format_modifier
VKAPI_ATTR VkResult VKAPI_CALL vkGetImageDrmFormatModifierPropertiesEXT(
	VkDevice device, 
	VkImage image, 
	VkImageDrmFormatModifierPropertiesEXT* pProperties) 
{ 
  return pfn_vkGetImageDrmFormatModifierPropertiesEXT(device, image, pProperties); 
}
#endif /* VK_EXT_image_drm_format_modifier */
#ifdef VK_EXT_line_rasterization
VKAPI_ATTR void VKAPI_CALL vkCmdSetLineStippleEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t lineStippleFactor, 
	uint16_t lineStipplePattern) 
{ 
  pfn_vkCmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern); 
}
#endif /* VK_EXT_line_rasterization */
#ifdef VK_EXT_metal_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateMetalSurfaceEXT(
	VkInstance instance, 
	const VkMetalSurfaceCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateMetalSurfaceEXT(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_EXT_metal_surface */
#ifdef VK_EXT_private_data
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePrivateDataSlotEXT(
	VkDevice device, 
	const VkPrivateDataSlotCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkPrivateDataSlotEXT* pPrivateDataSlot) 
{ 
  return pfn_vkCreatePrivateDataSlotEXT(device, pCreateInfo, pAllocator, pPrivateDataSlot); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyPrivateDataSlotEXT(
	VkDevice device, 
	VkPrivateDataSlotEXT privateDataSlot, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyPrivateDataSlotEXT(device, privateDataSlot, pAllocator); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPrivateDataEXT(
	VkDevice device, 
	VkObjectType objectType, 
	uint64_t objectHandle, 
	VkPrivateDataSlotEXT privateDataSlot, 
	uint64_t* pData) 
{ 
  pfn_vkGetPrivateDataEXT(device, objectType, objectHandle, privateDataSlot, pData); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkSetPrivateDataEXT(
	VkDevice device, 
	VkObjectType objectType, 
	uint64_t objectHandle, 
	VkPrivateDataSlotEXT privateDataSlot, 
	uint64_t data) 
{ 
  return pfn_vkSetPrivateDataEXT(device, objectType, objectHandle, privateDataSlot, data); 
}
#endif /* VK_EXT_private_data */
#ifdef VK_EXT_sample_locations
VKAPI_ATTR void VKAPI_CALL vkCmdSetSampleLocationsEXT(
	VkCommandBuffer commandBuffer, 
	const VkSampleLocationsInfoEXT* pSampleLocationsInfo) 
{ 
  pfn_vkCmdSetSampleLocationsEXT(commandBuffer, pSampleLocationsInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMultisamplePropertiesEXT(
	VkPhysicalDevice physicalDevice, 
	VkSampleCountFlagBits samples, 
	VkMultisamplePropertiesEXT* pMultisampleProperties) 
{ 
  pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT(physicalDevice, samples, pMultisampleProperties); 
}
#endif /* VK_EXT_sample_locations */
#ifdef VK_EXT_tooling_info
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceToolPropertiesEXT(
	VkPhysicalDevice physicalDevice, 
	uint32_t* pToolCount, 
	VkPhysicalDeviceToolPropertiesEXT* pToolProperties) 
{ 
  return pfn_vkGetPhysicalDeviceToolPropertiesEXT(physicalDevice, pToolCount, pToolProperties); 
}
#endif /* VK_EXT_tooling_info */
#ifdef VK_EXT_transform_feedback
VKAPI_ATTR void VKAPI_CALL vkCmdBeginQueryIndexedEXT(
	VkCommandBuffer commandBuffer, 
	VkQueryPool queryPool, 
	uint32_t query, 
	VkQueryControlFlags flags, 
	uint32_t index) 
{ 
  pfn_vkCmdBeginQueryIndexedEXT(commandBuffer, queryPool, query, flags, index); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdBeginTransformFeedbackEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t firstCounterBuffer, 
	uint32_t counterBufferCount, 
	const VkBuffer* pCounterBuffers, 
	const VkDeviceSize* pCounterBufferOffsets) 
{ 
  pfn_vkCmdBeginTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindTransformFeedbackBuffersEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t firstBinding, 
	uint32_t bindingCount, 
	const VkBuffer* pBuffers, 
	const VkDeviceSize* pOffsets, 
	const VkDeviceSize* pSizes) 
{ 
  pfn_vkCmdBindTransformFeedbackBuffersEXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectByteCountEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t instanceCount, 
	uint32_t firstInstance, 
	VkBuffer counterBuffer, 
	VkDeviceSize counterBufferOffset, 
	uint32_t counterOffset, 
	uint32_t vertexStride) 
{ 
  pfn_vkCmdDrawIndirectByteCountEXT(commandBuffer, instanceCount, firstInstance, counterBuffer, counterBufferOffset, counterOffset, vertexStride); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdEndQueryIndexedEXT(
	VkCommandBuffer commandBuffer, 
	VkQueryPool queryPool, 
	uint32_t query, 
	uint32_t index) 
{ 
  pfn_vkCmdEndQueryIndexedEXT(commandBuffer, queryPool, query, index); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdEndTransformFeedbackEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t firstCounterBuffer, 
	uint32_t counterBufferCount, 
	const VkBuffer* pCounterBuffers, 
	const VkDeviceSize* pCounterBufferOffsets) 
{ 
  pfn_vkCmdEndTransformFeedbackEXT(commandBuffer, firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets); 
}
#endif /* VK_EXT_transform_feedback */
#ifdef VK_EXT_validation_cache
VKAPI_ATTR VkResult VKAPI_CALL vkCreateValidationCacheEXT(
	VkDevice device, 
	const VkValidationCacheCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkValidationCacheEXT* pValidationCache) 
{ 
  return pfn_vkCreateValidationCacheEXT(device, pCreateInfo, pAllocator, pValidationCache); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyValidationCacheEXT(
	VkDevice device, 
	VkValidationCacheEXT validationCache, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyValidationCacheEXT(device, validationCache, pAllocator); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetValidationCacheDataEXT(
	VkDevice device, 
	VkValidationCacheEXT validationCache, 
	size_t* pDataSize, 
	void* pData) 
{ 
  return pfn_vkGetValidationCacheDataEXT(device, validationCache, pDataSize, pData); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkMergeValidationCachesEXT(
	VkDevice device, 
	VkValidationCacheEXT dstCache, 
	uint32_t srcCacheCount, 
	const VkValidationCacheEXT* pSrcCaches) 
{ 
  return pfn_vkMergeValidationCachesEXT(device, dstCache, srcCacheCount, pSrcCaches); 
}
#endif /* VK_EXT_validation_cache */
#ifdef VK_EXT_vertex_input_dynamic_state
VKAPI_ATTR void VKAPI_CALL vkCmdSetVertexInputEXT(
	VkCommandBuffer commandBuffer, 
	uint32_t vertexBindingDescriptionCount, 
	const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, 
	uint32_t vertexAttributeDescriptionCount, 
	const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) 
{ 
  pfn_vkCmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions); 
}
#endif /* VK_EXT_vertex_input_dynamic_state */
#ifdef VK_FUCHSIA_external_memory
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryZirconHandleFUCHSIA(
	VkDevice device, 
	const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, 
	zx_handle_t* pZirconHandle) 
{ 
  return pfn_vkGetMemoryZirconHandleFUCHSIA(device, pGetZirconHandleInfo, pZirconHandle); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryZirconHandlePropertiesFUCHSIA(
	VkDevice device, 
	VkExternalMemoryHandleTypeFlagBits handleType, 
	zx_handle_t zirconHandle, 
	VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties) 
{ 
  return pfn_vkGetMemoryZirconHandlePropertiesFUCHSIA(device, handleType, zirconHandle, pMemoryZirconHandleProperties); 
}
#endif /* VK_FUCHSIA_external_memory */
#ifdef VK_FUCHSIA_external_semaphore
VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreZirconHandleFUCHSIA(
	VkDevice device, 
	const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, 
	zx_handle_t* pZirconHandle) 
{ 
  return pfn_vkGetSemaphoreZirconHandleFUCHSIA(device, pGetZirconHandleInfo, pZirconHandle); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreZirconHandleFUCHSIA(
	VkDevice device, 
	const VkImportSemaphoreZirconHandleInfoFUCHSIA* pImportSemaphoreZirconHandleInfo) 
{ 
  return pfn_vkImportSemaphoreZirconHandleFUCHSIA(device, pImportSemaphoreZirconHandleInfo); 
}
#endif /* VK_FUCHSIA_external_semaphore */
#ifdef VK_FUCHSIA_imagepipe_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImagePipeSurfaceFUCHSIA(
	VkInstance instance, 
	const VkImagePipeSurfaceCreateInfoFUCHSIA* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateImagePipeSurfaceFUCHSIA(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_FUCHSIA_imagepipe_surface */
#ifdef VK_GGP_stream_descriptor_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateStreamDescriptorSurfaceGGP(
	VkInstance instance, 
	const VkStreamDescriptorSurfaceCreateInfoGGP* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateStreamDescriptorSurfaceGGP(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_GGP_stream_descriptor_surface */
#ifdef VK_GOOGLE_display_timing
VKAPI_ATTR VkResult VKAPI_CALL vkGetPastPresentationTimingGOOGLE(
	VkDevice device, 
	VkSwapchainKHR swapchain, 
	uint32_t* pPresentationTimingCount, 
	VkPastPresentationTimingGOOGLE* pPresentationTimings) 
{ 
  return pfn_vkGetPastPresentationTimingGOOGLE(device, swapchain, pPresentationTimingCount, pPresentationTimings); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetRefreshCycleDurationGOOGLE(
	VkDevice device, 
	VkSwapchainKHR swapchain, 
	VkRefreshCycleDurationGOOGLE* pDisplayTimingProperties) 
{ 
  return pfn_vkGetRefreshCycleDurationGOOGLE(device, swapchain, pDisplayTimingProperties); 
}
#endif /* VK_GOOGLE_display_timing */
#ifdef VK_INTEL_performance_query
VKAPI_ATTR VkResult VKAPI_CALL vkAcquirePerformanceConfigurationINTEL(
	VkDevice device, 
	const VkPerformanceConfigurationAcquireInfoINTEL* pAcquireInfo, 
	VkPerformanceConfigurationINTEL* pConfiguration) 
{ 
  return pfn_vkAcquirePerformanceConfigurationINTEL(device, pAcquireInfo, pConfiguration); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCmdSetPerformanceMarkerINTEL(
	VkCommandBuffer commandBuffer, 
	const VkPerformanceMarkerInfoINTEL* pMarkerInfo) 
{ 
  return pfn_vkCmdSetPerformanceMarkerINTEL(commandBuffer, pMarkerInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCmdSetPerformanceOverrideINTEL(
	VkCommandBuffer commandBuffer, 
	const VkPerformanceOverrideInfoINTEL* pOverrideInfo) 
{ 
  return pfn_vkCmdSetPerformanceOverrideINTEL(commandBuffer, pOverrideInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCmdSetPerformanceStreamMarkerINTEL(
	VkCommandBuffer commandBuffer, 
	const VkPerformanceStreamMarkerInfoINTEL* pMarkerInfo) 
{ 
  return pfn_vkCmdSetPerformanceStreamMarkerINTEL(commandBuffer, pMarkerInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPerformanceParameterINTEL(
	VkDevice device, 
	VkPerformanceParameterTypeINTEL parameter, 
	VkPerformanceValueINTEL* pValue) 
{ 
  return pfn_vkGetPerformanceParameterINTEL(device, parameter, pValue); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkInitializePerformanceApiINTEL(
	VkDevice device, 
	const VkInitializePerformanceApiInfoINTEL* pInitializeInfo) 
{ 
  return pfn_vkInitializePerformanceApiINTEL(device, pInitializeInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSetPerformanceConfigurationINTEL(
	VkQueue queue, 
	VkPerformanceConfigurationINTEL configuration) 
{ 
  return pfn_vkQueueSetPerformanceConfigurationINTEL(queue, configuration); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkReleasePerformanceConfigurationINTEL(
	VkDevice device, 
	VkPerformanceConfigurationINTEL configuration) 
{ 
  return pfn_vkReleasePerformanceConfigurationINTEL(device, configuration); 
}
VKAPI_ATTR void VKAPI_CALL vkUninitializePerformanceApiINTEL(
	VkDevice device) 
{ 
  pfn_vkUninitializePerformanceApiINTEL(device); 
}
#endif /* VK_INTEL_performance_query */
#ifdef VK_KHR_acceleration_structure
VKAPI_ATTR VkResult VKAPI_CALL vkBuildAccelerationStructuresKHR(
	VkDevice                                           device, 
	VkDeferredOperationKHR deferredOperation, 
	uint32_t infoCount, 
	const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, 
	const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) 
{ 
  return pfn_vkBuildAccelerationStructuresKHR(device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresIndirectKHR(
	VkCommandBuffer                  commandBuffer, 
	uint32_t                                           infoCount, 
	const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, 
	const VkDeviceAddress*             pIndirectDeviceAddresses, 
	const uint32_t*                    pIndirectStrides, 
	const uint32_t* const*             ppMaxPrimitiveCounts) 
{ 
  pfn_vkCmdBuildAccelerationStructuresIndirectKHR(commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresKHR(
	VkCommandBuffer                                    commandBuffer, 
	uint32_t infoCount, 
	const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, 
	const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) 
{ 
  pfn_vkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureKHR(
	VkCommandBuffer commandBuffer, 
	const VkCopyAccelerationStructureInfoKHR* pInfo) 
{ 
  pfn_vkCmdCopyAccelerationStructureKHR(commandBuffer, pInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureToMemoryKHR(
	VkCommandBuffer commandBuffer, 
	const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) 
{ 
  pfn_vkCmdCopyAccelerationStructureToMemoryKHR(commandBuffer, pInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyMemoryToAccelerationStructureKHR(
	VkCommandBuffer commandBuffer, 
	const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) 
{ 
  pfn_vkCmdCopyMemoryToAccelerationStructureKHR(commandBuffer, pInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdWriteAccelerationStructuresPropertiesKHR(
	VkCommandBuffer commandBuffer, 
	uint32_t accelerationStructureCount, 
	const VkAccelerationStructureKHR* pAccelerationStructures, 
	VkQueryType queryType, 
	VkQueryPool queryPool, 
	uint32_t firstQuery) 
{ 
  pfn_vkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureKHR(
	VkDevice device, 
	VkDeferredOperationKHR deferredOperation, 
	const VkCopyAccelerationStructureInfoKHR* pInfo) 
{ 
  return pfn_vkCopyAccelerationStructureKHR(device, deferredOperation, pInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureToMemoryKHR(
	VkDevice device, 
	VkDeferredOperationKHR deferredOperation, 
	const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo) 
{ 
  return pfn_vkCopyAccelerationStructureToMemoryKHR(device, deferredOperation, pInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCopyMemoryToAccelerationStructureKHR(
	VkDevice device, 
	VkDeferredOperationKHR deferredOperation, 
	const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo) 
{ 
  return pfn_vkCopyMemoryToAccelerationStructureKHR(device, deferredOperation, pInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateAccelerationStructureKHR(
	VkDevice                                           device, 
	const VkAccelerationStructureCreateInfoKHR*        pCreateInfo, 
	const VkAllocationCallbacks*       pAllocator, 
	VkAccelerationStructureKHR*                        pAccelerationStructure) 
{ 
  return pfn_vkCreateAccelerationStructureKHR(device, pCreateInfo, pAllocator, pAccelerationStructure); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyAccelerationStructureKHR(
	VkDevice device, 
	VkAccelerationStructureKHR accelerationStructure, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyAccelerationStructureKHR(device, accelerationStructure, pAllocator); 
}
VKAPI_ATTR void VKAPI_CALL vkGetAccelerationStructureBuildSizesKHR(
	VkDevice                                            device, 
	VkAccelerationStructureBuildTypeKHR                 buildType, 
	const VkAccelerationStructureBuildGeometryInfoKHR*  pBuildInfo, 
	const uint32_t*  pMaxPrimitiveCounts, 
	VkAccelerationStructureBuildSizesInfoKHR*           pSizeInfo) 
{ 
  pfn_vkGetAccelerationStructureBuildSizesKHR(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo); 
}
VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetAccelerationStructureDeviceAddressKHR(
	VkDevice device, 
	const VkAccelerationStructureDeviceAddressInfoKHR* pInfo) 
{ 
  return pfn_vkGetAccelerationStructureDeviceAddressKHR(device, pInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkGetDeviceAccelerationStructureCompatibilityKHR(
	VkDevice device, 
	const VkAccelerationStructureVersionInfoKHR* pVersionInfo, 
	VkAccelerationStructureCompatibilityKHR* pCompatibility) 
{ 
  pfn_vkGetDeviceAccelerationStructureCompatibilityKHR(device, pVersionInfo, pCompatibility); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkWriteAccelerationStructuresPropertiesKHR(
	VkDevice device, 
	uint32_t accelerationStructureCount, 
	const VkAccelerationStructureKHR* pAccelerationStructures, 
	VkQueryType  queryType, 
	size_t       dataSize, 
	void* pData, 
	size_t stride) 
{ 
  return pfn_vkWriteAccelerationStructuresPropertiesKHR(device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride); 
}
#endif /* VK_KHR_acceleration_structure */
#ifdef VK_KHR_android_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateAndroidSurfaceKHR(
	VkInstance instance, 
	const VkAndroidSurfaceCreateInfoKHR* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateAndroidSurfaceKHR(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_KHR_android_surface */
#ifdef VK_KHR_bind_memory2
VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory2KHR(
	VkDevice device, 
	uint32_t bindInfoCount, 
	const VkBindBufferMemoryInfo* pBindInfos) 
{ 
  return pfn_vkBindBufferMemory2KHR(device, bindInfoCount, pBindInfos); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory2KHR(
	VkDevice device, 
	uint32_t bindInfoCount, 
	const VkBindImageMemoryInfo* pBindInfos) 
{ 
  return pfn_vkBindImageMemory2KHR(device, bindInfoCount, pBindInfos); 
}
#endif /* VK_KHR_bind_memory2 */
#ifdef VK_KHR_buffer_device_address
VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddressKHR(
	VkDevice device, 
	const VkBufferDeviceAddressInfo* pInfo) 
{ 
  return pfn_vkGetBufferDeviceAddressKHR(device, pInfo); 
}
VKAPI_ATTR uint64_t VKAPI_CALL vkGetBufferOpaqueCaptureAddressKHR(
	VkDevice device, 
	const VkBufferDeviceAddressInfo* pInfo) 
{ 
  return pfn_vkGetBufferOpaqueCaptureAddressKHR(device, pInfo); 
}
VKAPI_ATTR uint64_t VKAPI_CALL vkGetDeviceMemoryOpaqueCaptureAddressKHR(
	VkDevice device, 
	const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) 
{ 
  return pfn_vkGetDeviceMemoryOpaqueCaptureAddressKHR(device, pInfo); 
}
#endif /* VK_KHR_buffer_device_address */
#ifdef VK_KHR_copy_commands2
VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage2KHR(
	VkCommandBuffer commandBuffer, 
	const VkBlitImageInfo2KHR* pBlitImageInfo) 
{ 
  pfn_vkCmdBlitImage2KHR(commandBuffer, pBlitImageInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer2KHR(
	VkCommandBuffer commandBuffer, 
	const VkCopyBufferInfo2KHR* pCopyBufferInfo) 
{ 
  pfn_vkCmdCopyBuffer2KHR(commandBuffer, pCopyBufferInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage2KHR(
	VkCommandBuffer commandBuffer, 
	const VkCopyBufferToImageInfo2KHR* pCopyBufferToImageInfo) 
{ 
  pfn_vkCmdCopyBufferToImage2KHR(commandBuffer, pCopyBufferToImageInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage2KHR(
	VkCommandBuffer commandBuffer, 
	const VkCopyImageInfo2KHR* pCopyImageInfo) 
{ 
  pfn_vkCmdCopyImage2KHR(commandBuffer, pCopyImageInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer2KHR(
	VkCommandBuffer commandBuffer, 
	const VkCopyImageToBufferInfo2KHR* pCopyImageToBufferInfo) 
{ 
  pfn_vkCmdCopyImageToBuffer2KHR(commandBuffer, pCopyImageToBufferInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdResolveImage2KHR(
	VkCommandBuffer commandBuffer, 
	const VkResolveImageInfo2KHR* pResolveImageInfo) 
{ 
  pfn_vkCmdResolveImage2KHR(commandBuffer, pResolveImageInfo); 
}
#endif /* VK_KHR_copy_commands2 */
#ifdef VK_KHR_create_renderpass2
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2KHR(
	VkCommandBuffer commandBuffer, 
	const VkRenderPassBeginInfo*      pRenderPassBegin, 
	const VkSubpassBeginInfo*      pSubpassBeginInfo) 
{ 
  pfn_vkCmdBeginRenderPass2KHR(commandBuffer, pRenderPassBegin, pSubpassBeginInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2KHR(
	VkCommandBuffer commandBuffer, 
	const VkSubpassEndInfo*        pSubpassEndInfo) 
{ 
  pfn_vkCmdEndRenderPass2KHR(commandBuffer, pSubpassEndInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2KHR(
	VkCommandBuffer commandBuffer, 
	const VkSubpassBeginInfo*      pSubpassBeginInfo, 
	const VkSubpassEndInfo*        pSubpassEndInfo) 
{ 
  pfn_vkCmdNextSubpass2KHR(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2KHR(
	VkDevice device, 
	const VkRenderPassCreateInfo2* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkRenderPass* pRenderPass) 
{ 
  return pfn_vkCreateRenderPass2KHR(device, pCreateInfo, pAllocator, pRenderPass); 
}
#endif /* VK_KHR_create_renderpass2 */
#ifdef VK_KHR_deferred_host_operations
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDeferredOperationKHR(
	VkDevice device, 
	const VkAllocationCallbacks* pAllocator, 
	VkDeferredOperationKHR* pDeferredOperation) 
{ 
  return pfn_vkCreateDeferredOperationKHR(device, pAllocator, pDeferredOperation); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkDeferredOperationJoinKHR(
	VkDevice device, 
	VkDeferredOperationKHR operation) 
{ 
  return pfn_vkDeferredOperationJoinKHR(device, operation); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDeferredOperationKHR(
	VkDevice device, 
	VkDeferredOperationKHR operation, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyDeferredOperationKHR(device, operation, pAllocator); 
}
VKAPI_ATTR uint32_t VKAPI_CALL vkGetDeferredOperationMaxConcurrencyKHR(
	VkDevice device, 
	VkDeferredOperationKHR operation) 
{ 
  return pfn_vkGetDeferredOperationMaxConcurrencyKHR(device, operation); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetDeferredOperationResultKHR(
	VkDevice device, 
	VkDeferredOperationKHR operation) 
{ 
  return pfn_vkGetDeferredOperationResultKHR(device, operation); 
}
#endif /* VK_KHR_deferred_host_operations */
#ifdef VK_KHR_descriptor_update_template
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorUpdateTemplateKHR(
	VkDevice device, 
	const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) 
{ 
  return pfn_vkCreateDescriptorUpdateTemplateKHR(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorUpdateTemplateKHR(
	VkDevice device, 
	VkDescriptorUpdateTemplate descriptorUpdateTemplate, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyDescriptorUpdateTemplateKHR(device, descriptorUpdateTemplate, pAllocator); 
}
VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSetWithTemplateKHR(
	VkDevice device, 
	VkDescriptorSet descriptorSet, 
	VkDescriptorUpdateTemplate descriptorUpdateTemplate, 
	const void* pData) 
{ 
  pfn_vkUpdateDescriptorSetWithTemplateKHR(device, descriptorSet, descriptorUpdateTemplate, pData); 
}
#endif /* VK_KHR_descriptor_update_template */
#ifdef VK_KHR_device_group
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchBaseKHR(
	VkCommandBuffer commandBuffer, 
	uint32_t baseGroupX, 
	uint32_t baseGroupY, 
	uint32_t baseGroupZ, 
	uint32_t groupCountX, 
	uint32_t groupCountY, 
	uint32_t groupCountZ) 
{ 
  pfn_vkCmdDispatchBaseKHR(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetDeviceMaskKHR(
	VkCommandBuffer commandBuffer, 
	uint32_t deviceMask) 
{ 
  pfn_vkCmdSetDeviceMaskKHR(commandBuffer, deviceMask); 
}
VKAPI_ATTR void VKAPI_CALL vkGetDeviceGroupPeerMemoryFeaturesKHR(
	VkDevice device, 
	uint32_t heapIndex, 
	uint32_t localDeviceIndex, 
	uint32_t remoteDeviceIndex, 
	VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) 
{ 
  pfn_vkGetDeviceGroupPeerMemoryFeaturesKHR(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures); 
}
#endif /* VK_KHR_device_group */
#ifdef VK_KHR_device_group_creation
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceGroupsKHR(
	VkInstance instance, 
	uint32_t* pPhysicalDeviceGroupCount, 
	VkPhysicalDeviceGroupProperties* pPhysicalDeviceGroupProperties) 
{ 
  return pfn_vkEnumeratePhysicalDeviceGroupsKHR(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties); 
}
#endif /* VK_KHR_device_group_creation */
#ifdef VK_KHR_draw_indirect_count
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCountKHR(
	VkCommandBuffer commandBuffer, 
	VkBuffer buffer, 
	VkDeviceSize offset, 
	VkBuffer countBuffer, 
	VkDeviceSize countBufferOffset, 
	uint32_t maxDrawCount, 
	uint32_t stride) 
{ 
  pfn_vkCmdDrawIndexedIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCountKHR(
	VkCommandBuffer commandBuffer, 
	VkBuffer buffer, 
	VkDeviceSize offset, 
	VkBuffer countBuffer, 
	VkDeviceSize countBufferOffset, 
	uint32_t maxDrawCount, 
	uint32_t stride) 
{ 
  pfn_vkCmdDrawIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); 
}
#endif /* VK_KHR_draw_indirect_count */
#ifdef VK_KHR_external_fence_capabilities
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalFencePropertiesKHR(
	VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, 
	VkExternalFenceProperties* pExternalFenceProperties) 
{ 
  pfn_vkGetPhysicalDeviceExternalFencePropertiesKHR(physicalDevice, pExternalFenceInfo, pExternalFenceProperties); 
}
#endif /* VK_KHR_external_fence_capabilities */
#ifdef VK_KHR_external_fence_fd
VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceFdKHR(
	VkDevice device, 
	const VkFenceGetFdInfoKHR* pGetFdInfo, 
	int* pFd) 
{ 
  return pfn_vkGetFenceFdKHR(device, pGetFdInfo, pFd); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkImportFenceFdKHR(
	VkDevice device, 
	const VkImportFenceFdInfoKHR* pImportFenceFdInfo) 
{ 
  return pfn_vkImportFenceFdKHR(device, pImportFenceFdInfo); 
}
#endif /* VK_KHR_external_fence_fd */
#ifdef VK_KHR_external_fence_win32
VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceWin32HandleKHR(
	VkDevice device, 
	const VkFenceGetWin32HandleInfoKHR* pGetWin32HandleInfo, 
	HANDLE* pHandle) 
{ 
  return pfn_vkGetFenceWin32HandleKHR(device, pGetWin32HandleInfo, pHandle); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkImportFenceWin32HandleKHR(
	VkDevice device, 
	const VkImportFenceWin32HandleInfoKHR* pImportFenceWin32HandleInfo) 
{ 
  return pfn_vkImportFenceWin32HandleKHR(device, pImportFenceWin32HandleInfo); 
}
#endif /* VK_KHR_external_fence_win32 */
#ifdef VK_KHR_external_memory_capabilities
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalBufferPropertiesKHR(
	VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, 
	VkExternalBufferProperties* pExternalBufferProperties) 
{ 
  pfn_vkGetPhysicalDeviceExternalBufferPropertiesKHR(physicalDevice, pExternalBufferInfo, pExternalBufferProperties); 
}
#endif /* VK_KHR_external_memory_capabilities */
#ifdef VK_KHR_external_memory_fd
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdKHR(
	VkDevice device, 
	const VkMemoryGetFdInfoKHR* pGetFdInfo, 
	int* pFd) 
{ 
  return pfn_vkGetMemoryFdKHR(device, pGetFdInfo, pFd); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdPropertiesKHR(
	VkDevice device, 
	VkExternalMemoryHandleTypeFlagBits handleType, 
	int fd, 
	VkMemoryFdPropertiesKHR* pMemoryFdProperties) 
{ 
  return pfn_vkGetMemoryFdPropertiesKHR(device, handleType, fd, pMemoryFdProperties); 
}
#endif /* VK_KHR_external_memory_fd */
#ifdef VK_KHR_external_memory_win32
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryWin32HandleKHR(
	VkDevice device, 
	const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, 
	HANDLE* pHandle) 
{ 
  return pfn_vkGetMemoryWin32HandleKHR(device, pGetWin32HandleInfo, pHandle); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryWin32HandlePropertiesKHR(
	VkDevice device, 
	VkExternalMemoryHandleTypeFlagBits handleType, 
	HANDLE handle, 
	VkMemoryWin32HandlePropertiesKHR* pMemoryWin32HandleProperties) 
{ 
  return pfn_vkGetMemoryWin32HandlePropertiesKHR(device, handleType, handle, pMemoryWin32HandleProperties); 
}
#endif /* VK_KHR_external_memory_win32 */
#ifdef VK_KHR_external_semaphore_capabilities
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(
	VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, 
	VkExternalSemaphoreProperties* pExternalSemaphoreProperties) 
{ 
  pfn_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties); 
}
#endif /* VK_KHR_external_semaphore_capabilities */
#ifdef VK_KHR_external_semaphore_fd
VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreFdKHR(
	VkDevice device, 
	const VkSemaphoreGetFdInfoKHR* pGetFdInfo, 
	int* pFd) 
{ 
  return pfn_vkGetSemaphoreFdKHR(device, pGetFdInfo, pFd); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreFdKHR(
	VkDevice device, 
	const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) 
{ 
  return pfn_vkImportSemaphoreFdKHR(device, pImportSemaphoreFdInfo); 
}
#endif /* VK_KHR_external_semaphore_fd */
#ifdef VK_KHR_external_semaphore_win32
VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreWin32HandleKHR(
	VkDevice device, 
	const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, 
	HANDLE* pHandle) 
{ 
  return pfn_vkGetSemaphoreWin32HandleKHR(device, pGetWin32HandleInfo, pHandle); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreWin32HandleKHR(
	VkDevice device, 
	const VkImportSemaphoreWin32HandleInfoKHR* pImportSemaphoreWin32HandleInfo) 
{ 
  return pfn_vkImportSemaphoreWin32HandleKHR(device, pImportSemaphoreWin32HandleInfo); 
}
#endif /* VK_KHR_external_semaphore_win32 */
#ifdef VK_KHR_fragment_shading_rate
VKAPI_ATTR void VKAPI_CALL vkCmdSetFragmentShadingRateKHR(
	VkCommandBuffer           commandBuffer, 
	const VkExtent2D*                           pFragmentSize, 
	const VkFragmentShadingRateCombinerOpKHR    combinerOps[2]) 
{ 
  pfn_vkCmdSetFragmentShadingRateKHR(commandBuffer, pFragmentSize, combinerOps); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceFragmentShadingRatesKHR(
	VkPhysicalDevice physicalDevice, 
	uint32_t* pFragmentShadingRateCount, 
	VkPhysicalDeviceFragmentShadingRateKHR* pFragmentShadingRates) 
{ 
  return pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates); 
}
#endif /* VK_KHR_fragment_shading_rate */
#ifdef VK_KHR_get_memory_requirements2
VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2KHR(
	VkDevice device, 
	const VkBufferMemoryRequirementsInfo2* pInfo, 
	VkMemoryRequirements2* pMemoryRequirements) 
{ 
  pfn_vkGetBufferMemoryRequirements2KHR(device, pInfo, pMemoryRequirements); 
}
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements2KHR(
	VkDevice device, 
	const VkImageMemoryRequirementsInfo2* pInfo, 
	VkMemoryRequirements2* pMemoryRequirements) 
{ 
  pfn_vkGetImageMemoryRequirements2KHR(device, pInfo, pMemoryRequirements); 
}
VKAPI_ATTR void VKAPI_CALL vkGetImageSparseMemoryRequirements2KHR(
	VkDevice device, 
	const VkImageSparseMemoryRequirementsInfo2* pInfo, 
	uint32_t* pSparseMemoryRequirementCount, 
	VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) 
{ 
  pfn_vkGetImageSparseMemoryRequirements2KHR(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements); 
}
#endif /* VK_KHR_get_memory_requirements2 */
#ifdef VK_KHR_get_physical_device_properties2
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures2KHR(
	VkPhysicalDevice physicalDevice, 
	VkPhysicalDeviceFeatures2* pFeatures) 
{ 
  pfn_vkGetPhysicalDeviceFeatures2KHR(physicalDevice, pFeatures); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties2KHR(
	VkPhysicalDevice physicalDevice, 
	VkFormat format, 
	VkFormatProperties2* pFormatProperties) 
{ 
  pfn_vkGetPhysicalDeviceFormatProperties2KHR(physicalDevice, format, pFormatProperties); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties2KHR(
	VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, 
	VkImageFormatProperties2* pImageFormatProperties) 
{ 
  return pfn_vkGetPhysicalDeviceImageFormatProperties2KHR(physicalDevice, pImageFormatInfo, pImageFormatProperties); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2KHR(
	VkPhysicalDevice physicalDevice, 
	VkPhysicalDeviceMemoryProperties2* pMemoryProperties) 
{ 
  pfn_vkGetPhysicalDeviceMemoryProperties2KHR(physicalDevice, pMemoryProperties); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties2KHR(
	VkPhysicalDevice physicalDevice, 
	VkPhysicalDeviceProperties2* pProperties) 
{ 
  pfn_vkGetPhysicalDeviceProperties2KHR(physicalDevice, pProperties); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties2KHR(
	VkPhysicalDevice physicalDevice, 
	uint32_t* pQueueFamilyPropertyCount, 
	VkQueueFamilyProperties2* pQueueFamilyProperties) 
{ 
  pfn_vkGetPhysicalDeviceQueueFamilyProperties2KHR(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceSparseImageFormatProperties2KHR(
	VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, 
	uint32_t* pPropertyCount, 
	VkSparseImageFormatProperties2* pProperties) 
{ 
  pfn_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(physicalDevice, pFormatInfo, pPropertyCount, pProperties); 
}
#endif /* VK_KHR_get_physical_device_properties2 */
#ifdef VK_KHR_maintenance1
VKAPI_ATTR void VKAPI_CALL vkTrimCommandPoolKHR(
	VkDevice device, 
	VkCommandPool commandPool, 
	VkCommandPoolTrimFlags flags) 
{ 
  pfn_vkTrimCommandPoolKHR(device, commandPool, flags); 
}
#endif /* VK_KHR_maintenance1 */
#ifdef VK_KHR_maintenance3
VKAPI_ATTR void VKAPI_CALL vkGetDescriptorSetLayoutSupportKHR(
	VkDevice device, 
	const VkDescriptorSetLayoutCreateInfo* pCreateInfo, 
	VkDescriptorSetLayoutSupport* pSupport) 
{ 
  pfn_vkGetDescriptorSetLayoutSupportKHR(device, pCreateInfo, pSupport); 
}
#endif /* VK_KHR_maintenance3 */
#ifdef VK_KHR_performance_query
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireProfilingLockKHR(
	VkDevice device, 
	const VkAcquireProfilingLockInfoKHR* pInfo) 
{ 
  return pfn_vkAcquireProfilingLockKHR(device, pInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(
	VkPhysicalDevice physicalDevice, 
	uint32_t queueFamilyIndex, 
	uint32_t* pCounterCount, 
	VkPerformanceCounterKHR* pCounters, 
	VkPerformanceCounterDescriptionKHR* pCounterDescriptions) 
{ 
  return pfn_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueFamilyIndex, pCounterCount, pCounters, pCounterDescriptions); 
}
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(
	VkPhysicalDevice physicalDevice, 
	const VkQueryPoolPerformanceCreateInfoKHR* pPerformanceQueryCreateInfo, 
	uint32_t* pNumPasses) 
{ 
  pfn_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(physicalDevice, pPerformanceQueryCreateInfo, pNumPasses); 
}
VKAPI_ATTR void VKAPI_CALL vkReleaseProfilingLockKHR(
	VkDevice device) 
{ 
  pfn_vkReleaseProfilingLockKHR(device); 
}
#endif /* VK_KHR_performance_query */
#ifdef VK_KHR_pipeline_executable_properties
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutableInternalRepresentationsKHR(
	VkDevice                        device, 
	const VkPipelineExecutableInfoKHR*  pExecutableInfo, 
	uint32_t* pInternalRepresentationCount, 
	VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations) 
{ 
  return pfn_vkGetPipelineExecutableInternalRepresentationsKHR(device, pExecutableInfo, pInternalRepresentationCount, pInternalRepresentations); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutablePropertiesKHR(
	VkDevice                        device, 
	const VkPipelineInfoKHR*        pPipelineInfo, 
	uint32_t* pExecutableCount, 
	VkPipelineExecutablePropertiesKHR* pProperties) 
{ 
  return pfn_vkGetPipelineExecutablePropertiesKHR(device, pPipelineInfo, pExecutableCount, pProperties); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutableStatisticsKHR(
	VkDevice                        device, 
	const VkPipelineExecutableInfoKHR*  pExecutableInfo, 
	uint32_t* pStatisticCount, 
	VkPipelineExecutableStatisticKHR* pStatistics) 
{ 
  return pfn_vkGetPipelineExecutableStatisticsKHR(device, pExecutableInfo, pStatisticCount, pStatistics); 
}
#endif /* VK_KHR_pipeline_executable_properties */
#ifdef VK_KHR_push_descriptor
VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetKHR(
	VkCommandBuffer commandBuffer, 
	VkPipelineBindPoint pipelineBindPoint, 
	VkPipelineLayout layout, 
	uint32_t set, 
	uint32_t descriptorWriteCount, 
	const VkWriteDescriptorSet* pDescriptorWrites) 
{ 
  pfn_vkCmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites); 
}
#endif /* VK_KHR_push_descriptor */
#ifdef VK_KHR_ray_tracing_pipeline
VKAPI_ATTR void VKAPI_CALL vkCmdSetRayTracingPipelineStackSizeKHR(
	VkCommandBuffer commandBuffer, 
	uint32_t pipelineStackSize) 
{ 
  pfn_vkCmdSetRayTracingPipelineStackSizeKHR(commandBuffer, pipelineStackSize); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysIndirectKHR(
	VkCommandBuffer commandBuffer, 
	const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, 
	VkDeviceAddress indirectDeviceAddress) 
{ 
  pfn_vkCmdTraceRaysIndirectKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysKHR(
	VkCommandBuffer commandBuffer, 
	const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, 
	uint32_t width, 
	uint32_t height, 
	uint32_t depth) 
{ 
  pfn_vkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRayTracingPipelinesKHR(
	VkDevice device, 
	VkDeferredOperationKHR deferredOperation, 
	VkPipelineCache pipelineCache, 
	uint32_t createInfoCount, 
	const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, 
	const VkAllocationCallbacks* pAllocator, 
	VkPipeline* pPipelines) 
{ 
  return pfn_vkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(
	VkDevice device, 
	VkPipeline pipeline, 
	uint32_t firstGroup, 
	uint32_t groupCount, 
	size_t dataSize, 
	void* pData) 
{ 
  return pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingShaderGroupHandlesKHR(
	VkDevice device, 
	VkPipeline pipeline, 
	uint32_t firstGroup, 
	uint32_t groupCount, 
	size_t dataSize, 
	void* pData) 
{ 
  return pfn_vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData); 
}
VKAPI_ATTR VkDeviceSize VKAPI_CALL vkGetRayTracingShaderGroupStackSizeKHR(
	VkDevice device, 
	VkPipeline pipeline, 
	uint32_t group, 
	VkShaderGroupShaderKHR groupShader) 
{ 
  return pfn_vkGetRayTracingShaderGroupStackSizeKHR(device, pipeline, group, groupShader); 
}
#endif /* VK_KHR_ray_tracing_pipeline */
#ifdef VK_KHR_sampler_ycbcr_conversion
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSamplerYcbcrConversionKHR(
	VkDevice device, 
	const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSamplerYcbcrConversion* pYcbcrConversion) 
{ 
  return pfn_vkCreateSamplerYcbcrConversionKHR(device, pCreateInfo, pAllocator, pYcbcrConversion); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroySamplerYcbcrConversionKHR(
	VkDevice device, 
	VkSamplerYcbcrConversion ycbcrConversion, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroySamplerYcbcrConversionKHR(device, ycbcrConversion, pAllocator); 
}
#endif /* VK_KHR_sampler_ycbcr_conversion */
#ifdef VK_KHR_shared_presentable_image
VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainStatusKHR(
	VkDevice device, 
	VkSwapchainKHR swapchain) 
{ 
  return pfn_vkGetSwapchainStatusKHR(device, swapchain); 
}
#endif /* VK_KHR_shared_presentable_image */
#ifdef VK_KHR_synchronization2
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2KHR(
	VkCommandBuffer                   commandBuffer, 
	const VkDependencyInfoKHR*                                pDependencyInfo) 
{ 
  pfn_vkCmdPipelineBarrier2KHR(commandBuffer, pDependencyInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdResetEvent2KHR(
	VkCommandBuffer                   commandBuffer, 
	VkEvent                                             event, 
	VkPipelineStageFlags2KHR                            stageMask) 
{ 
  pfn_vkCmdResetEvent2KHR(commandBuffer, event, stageMask); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetEvent2KHR(
	VkCommandBuffer                   commandBuffer, 
	VkEvent                                             event, 
	const VkDependencyInfoKHR*                          pDependencyInfo) 
{ 
  pfn_vkCmdSetEvent2KHR(commandBuffer, event, pDependencyInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents2KHR(
	VkCommandBuffer                   commandBuffer, 
	uint32_t                                            eventCount, 
	const VkEvent*                     pEvents, 
	const VkDependencyInfoKHR*         pDependencyInfos) 
{ 
  pfn_vkCmdWaitEvents2KHR(commandBuffer, eventCount, pEvents, pDependencyInfos); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdWriteBufferMarker2AMD(
	VkCommandBuffer                   commandBuffer, 
	VkPipelineStageFlags2KHR                            stage, 
	VkBuffer                                            dstBuffer, 
	VkDeviceSize                                        dstOffset, 
	uint32_t                                            marker) 
{ 
  pfn_vkCmdWriteBufferMarker2AMD(commandBuffer, stage, dstBuffer, dstOffset, marker); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp2KHR(
	VkCommandBuffer                   commandBuffer, 
	VkPipelineStageFlags2KHR                            stage, 
	VkQueryPool                                         queryPool, 
	uint32_t                                            query) 
{ 
  pfn_vkCmdWriteTimestamp2KHR(commandBuffer, stage, queryPool, query); 
}
VKAPI_ATTR void VKAPI_CALL vkGetQueueCheckpointData2NV(
	VkQueue queue, 
	uint32_t* pCheckpointDataCount, 
	VkCheckpointData2NV* pCheckpointData) 
{ 
  pfn_vkGetQueueCheckpointData2NV(queue, pCheckpointDataCount, pCheckpointData); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2KHR(
	VkQueue                           queue, 
	uint32_t                            submitCount, 
	const VkSubmitInfo2KHR*           pSubmits, 
	VkFence           fence) 
{ 
  return pfn_vkQueueSubmit2KHR(queue, submitCount, pSubmits, fence); 
}
#endif /* VK_KHR_synchronization2 */
#ifdef VK_KHR_timeline_semaphore
VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreCounterValueKHR(
	VkDevice device, 
	VkSemaphore semaphore, 
	uint64_t* pValue) 
{ 
  return pfn_vkGetSemaphoreCounterValueKHR(device, semaphore, pValue); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkSignalSemaphoreKHR(
	VkDevice device, 
	const VkSemaphoreSignalInfo* pSignalInfo) 
{ 
  return pfn_vkSignalSemaphoreKHR(device, pSignalInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkWaitSemaphoresKHR(
	VkDevice device, 
	const VkSemaphoreWaitInfo* pWaitInfo, 
	uint64_t timeout) 
{ 
  return pfn_vkWaitSemaphoresKHR(device, pWaitInfo, timeout); 
}
#endif /* VK_KHR_timeline_semaphore */
#ifdef VK_KHR_video_decode_queue
VKAPI_ATTR void VKAPI_CALL vkCmdDecodeVideoKHR(
	VkCommandBuffer commandBuffer, 
	const VkVideoDecodeInfoKHR* pFrameInfo) 
{ 
  pfn_vkCmdDecodeVideoKHR(commandBuffer, pFrameInfo); 
}
#endif /* VK_KHR_video_decode_queue */
#ifdef VK_KHR_video_encode_queue
VKAPI_ATTR void VKAPI_CALL vkCmdEncodeVideoKHR(
	VkCommandBuffer commandBuffer, 
	const VkVideoEncodeInfoKHR* pEncodeInfo) 
{ 
  pfn_vkCmdEncodeVideoKHR(commandBuffer, pEncodeInfo); 
}
#endif /* VK_KHR_video_encode_queue */
#ifdef VK_KHR_video_queue
VKAPI_ATTR VkResult VKAPI_CALL vkBindVideoSessionMemoryKHR(
	VkDevice device, 
	VkVideoSessionKHR videoSession, 
	uint32_t videoSessionBindMemoryCount, 
	const VkVideoBindMemoryKHR* pVideoSessionBindMemories) 
{ 
  return pfn_vkBindVideoSessionMemoryKHR(device, videoSession, videoSessionBindMemoryCount, pVideoSessionBindMemories); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdBeginVideoCodingKHR(
	VkCommandBuffer commandBuffer, 
	const VkVideoBeginCodingInfoKHR* pBeginInfo) 
{ 
  pfn_vkCmdBeginVideoCodingKHR(commandBuffer, pBeginInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdControlVideoCodingKHR(
	VkCommandBuffer commandBuffer, 
	const VkVideoCodingControlInfoKHR* pCodingControlInfo) 
{ 
  pfn_vkCmdControlVideoCodingKHR(commandBuffer, pCodingControlInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdEndVideoCodingKHR(
	VkCommandBuffer commandBuffer, 
	const VkVideoEndCodingInfoKHR* pEndCodingInfo) 
{ 
  pfn_vkCmdEndVideoCodingKHR(commandBuffer, pEndCodingInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateVideoSessionKHR(
	VkDevice device, 
	const VkVideoSessionCreateInfoKHR* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkVideoSessionKHR* pVideoSession) 
{ 
  return pfn_vkCreateVideoSessionKHR(device, pCreateInfo, pAllocator, pVideoSession); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateVideoSessionParametersKHR(
	VkDevice device, 
	const VkVideoSessionParametersCreateInfoKHR* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkVideoSessionParametersKHR* pVideoSessionParameters) 
{ 
  return pfn_vkCreateVideoSessionParametersKHR(device, pCreateInfo, pAllocator, pVideoSessionParameters); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyVideoSessionKHR(
	VkDevice device, 
	VkVideoSessionKHR videoSession, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyVideoSessionKHR(device, videoSession, pAllocator); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyVideoSessionParametersKHR(
	VkDevice device, 
	VkVideoSessionParametersKHR videoSessionParameters, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyVideoSessionParametersKHR(device, videoSessionParameters, pAllocator); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceVideoCapabilitiesKHR(
	VkPhysicalDevice physicalDevice, 
	const VkVideoProfileKHR* pVideoProfile, 
	VkVideoCapabilitiesKHR* pCapabilities) 
{ 
  return pfn_vkGetPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, pVideoProfile, pCapabilities); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceVideoFormatPropertiesKHR(
	VkPhysicalDevice physicalDevice, 
	const VkPhysicalDeviceVideoFormatInfoKHR* pVideoFormatInfo, 
	uint32_t* pVideoFormatPropertyCount, 
	VkVideoFormatPropertiesKHR* pVideoFormatProperties) 
{ 
  return pfn_vkGetPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, pVideoFormatInfo, pVideoFormatPropertyCount, pVideoFormatProperties); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetVideoSessionMemoryRequirementsKHR(
	VkDevice device, 
	VkVideoSessionKHR videoSession, 
	uint32_t* pVideoSessionMemoryRequirementsCount, 
	VkVideoGetMemoryPropertiesKHR* pVideoSessionMemoryRequirements) 
{ 
  return pfn_vkGetVideoSessionMemoryRequirementsKHR(device, videoSession, pVideoSessionMemoryRequirementsCount, pVideoSessionMemoryRequirements); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkUpdateVideoSessionParametersKHR(
	VkDevice device, 
	VkVideoSessionParametersKHR videoSessionParameters, 
	const VkVideoSessionParametersUpdateInfoKHR* pUpdateInfo) 
{ 
  return pfn_vkUpdateVideoSessionParametersKHR(device, videoSessionParameters, pUpdateInfo); 
}
#endif /* VK_KHR_video_queue */
#ifdef VK_MVK_ios_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateIOSSurfaceMVK(
	VkInstance instance, 
	const VkIOSSurfaceCreateInfoMVK* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateIOSSurfaceMVK(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_MVK_ios_surface */
#ifdef VK_MVK_macos_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateMacOSSurfaceMVK(
	VkInstance instance, 
	const VkMacOSSurfaceCreateInfoMVK* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateMacOSSurfaceMVK(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_MVK_macos_surface */
#ifdef VK_NN_vi_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateViSurfaceNN(
	VkInstance instance, 
	const VkViSurfaceCreateInfoNN* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateViSurfaceNN(instance, pCreateInfo, pAllocator, pSurface); 
}
#endif /* VK_NN_vi_surface */
#ifdef VK_NVX_image_view_handle
VKAPI_ATTR VkResult VKAPI_CALL vkGetImageViewAddressNVX(
	VkDevice device, 
	VkImageView imageView, 
	VkImageViewAddressPropertiesNVX* pProperties) 
{ 
  return pfn_vkGetImageViewAddressNVX(device, imageView, pProperties); 
}
VKAPI_ATTR uint32_t VKAPI_CALL vkGetImageViewHandleNVX(
	VkDevice device, 
	const VkImageViewHandleInfoNVX* pInfo) 
{ 
  return pfn_vkGetImageViewHandleNVX(device, pInfo); 
}
#endif /* VK_NVX_image_view_handle */
#ifdef VK_NV_acquire_winrt_display
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireWinrtDisplayNV(
	VkPhysicalDevice physicalDevice, 
	VkDisplayKHR display) 
{ 
  return pfn_vkAcquireWinrtDisplayNV(physicalDevice, display); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetWinrtDisplayNV(
	VkPhysicalDevice physicalDevice, 
	uint32_t deviceRelativeId, 
	VkDisplayKHR* pDisplay) 
{ 
  return pfn_vkGetWinrtDisplayNV(physicalDevice, deviceRelativeId, pDisplay); 
}
#endif /* VK_NV_acquire_winrt_display */
#ifdef VK_NV_clip_space_w_scaling
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewportWScalingNV(
	VkCommandBuffer commandBuffer, 
	uint32_t firstViewport, 
	uint32_t viewportCount, 
	const VkViewportWScalingNV* pViewportWScalings) 
{ 
  pfn_vkCmdSetViewportWScalingNV(commandBuffer, firstViewport, viewportCount, pViewportWScalings); 
}
#endif /* VK_NV_clip_space_w_scaling */
#ifdef VK_NV_cooperative_matrix
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(
	VkPhysicalDevice physicalDevice, 
	uint32_t* pPropertyCount, 
	VkCooperativeMatrixPropertiesNV* pProperties) 
{ 
  return pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV(physicalDevice, pPropertyCount, pProperties); 
}
#endif /* VK_NV_cooperative_matrix */
#ifdef VK_NV_coverage_reduction_mode
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(
	VkPhysicalDevice physicalDevice, 
	uint32_t* pCombinationCount, 
	VkFramebufferMixedSamplesCombinationNV* pCombinations) 
{ 
  return pfn_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV(physicalDevice, pCombinationCount, pCombinations); 
}
#endif /* VK_NV_coverage_reduction_mode */
#ifdef VK_NV_device_diagnostic_checkpoints
VKAPI_ATTR void VKAPI_CALL vkCmdSetCheckpointNV(
	VkCommandBuffer commandBuffer, 
	const void* pCheckpointMarker) 
{ 
  pfn_vkCmdSetCheckpointNV(commandBuffer, pCheckpointMarker); 
}
VKAPI_ATTR void VKAPI_CALL vkGetQueueCheckpointDataNV(
	VkQueue queue, 
	uint32_t* pCheckpointDataCount, 
	VkCheckpointDataNV* pCheckpointData) 
{ 
  pfn_vkGetQueueCheckpointDataNV(queue, pCheckpointDataCount, pCheckpointData); 
}
#endif /* VK_NV_device_diagnostic_checkpoints */
#ifdef VK_NV_device_generated_commands
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipelineShaderGroupNV(
	VkCommandBuffer commandBuffer, 
	VkPipelineBindPoint pipelineBindPoint, 
	VkPipeline pipeline, 
	uint32_t groupIndex) 
{ 
  pfn_vkCmdBindPipelineShaderGroupNV(commandBuffer, pipelineBindPoint, pipeline, groupIndex); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdExecuteGeneratedCommandsNV(
	VkCommandBuffer commandBuffer, 
	VkBool32 isPreprocessed, 
	const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) 
{ 
  pfn_vkCmdExecuteGeneratedCommandsNV(commandBuffer, isPreprocessed, pGeneratedCommandsInfo); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdPreprocessGeneratedCommandsNV(
	VkCommandBuffer commandBuffer, 
	const VkGeneratedCommandsInfoNV* pGeneratedCommandsInfo) 
{ 
  pfn_vkCmdPreprocessGeneratedCommandsNV(commandBuffer, pGeneratedCommandsInfo); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateIndirectCommandsLayoutNV(
	VkDevice device, 
	const VkIndirectCommandsLayoutCreateInfoNV* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkIndirectCommandsLayoutNV* pIndirectCommandsLayout) 
{ 
  return pfn_vkCreateIndirectCommandsLayoutNV(device, pCreateInfo, pAllocator, pIndirectCommandsLayout); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyIndirectCommandsLayoutNV(
	VkDevice device, 
	VkIndirectCommandsLayoutNV indirectCommandsLayout, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyIndirectCommandsLayoutNV(device, indirectCommandsLayout, pAllocator); 
}
VKAPI_ATTR void VKAPI_CALL vkGetGeneratedCommandsMemoryRequirementsNV(
	VkDevice device, 
	const VkGeneratedCommandsMemoryRequirementsInfoNV* pInfo, 
	VkMemoryRequirements2* pMemoryRequirements) 
{ 
  pfn_vkGetGeneratedCommandsMemoryRequirementsNV(device, pInfo, pMemoryRequirements); 
}
#endif /* VK_NV_device_generated_commands */
#ifdef VK_NV_external_memory_capabilities
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceExternalImageFormatPropertiesNV(
	VkPhysicalDevice physicalDevice, 
	VkFormat format, 
	VkImageType type, 
	VkImageTiling tiling, 
	VkImageUsageFlags usage, 
	VkImageCreateFlags flags, 
	VkExternalMemoryHandleTypeFlagsNV externalHandleType, 
	VkExternalImageFormatPropertiesNV* pExternalImageFormatProperties) 
{ 
  return pfn_vkGetPhysicalDeviceExternalImageFormatPropertiesNV(physicalDevice, format, type, tiling, usage, flags, externalHandleType, pExternalImageFormatProperties); 
}
#endif /* VK_NV_external_memory_capabilities */
#ifdef VK_NV_external_memory_win32
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryWin32HandleNV(
	VkDevice device, 
	VkDeviceMemory memory, 
	VkExternalMemoryHandleTypeFlagsNV handleType, 
	HANDLE* pHandle) 
{ 
  return pfn_vkGetMemoryWin32HandleNV(device, memory, handleType, pHandle); 
}
#endif /* VK_NV_external_memory_win32 */
#ifdef VK_NV_fragment_shading_rate_enums
VKAPI_ATTR void VKAPI_CALL vkCmdSetFragmentShadingRateEnumNV(
	VkCommandBuffer           commandBuffer, 
	VkFragmentShadingRateNV                     shadingRate, 
	const VkFragmentShadingRateCombinerOpKHR    combinerOps[2]) 
{ 
  pfn_vkCmdSetFragmentShadingRateEnumNV(commandBuffer, shadingRate, combinerOps); 
}
#endif /* VK_NV_fragment_shading_rate_enums */
#ifdef VK_NV_mesh_shader
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectCountNV(
	VkCommandBuffer commandBuffer, 
	VkBuffer buffer, 
	VkDeviceSize offset, 
	VkBuffer countBuffer, 
	VkDeviceSize countBufferOffset, 
	uint32_t maxDrawCount, 
	uint32_t stride) 
{ 
  pfn_vkCmdDrawMeshTasksIndirectCountNV(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectNV(
	VkCommandBuffer commandBuffer, 
	VkBuffer buffer, 
	VkDeviceSize offset, 
	uint32_t drawCount, 
	uint32_t stride) 
{ 
  pfn_vkCmdDrawMeshTasksIndirectNV(commandBuffer, buffer, offset, drawCount, stride); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksNV(
	VkCommandBuffer commandBuffer, 
	uint32_t taskCount, 
	uint32_t firstTask) 
{ 
  pfn_vkCmdDrawMeshTasksNV(commandBuffer, taskCount, firstTask); 
}
#endif /* VK_NV_mesh_shader */
#ifdef VK_NV_ray_tracing
VKAPI_ATTR VkResult VKAPI_CALL vkBindAccelerationStructureMemoryNV(
	VkDevice device, 
	uint32_t bindInfoCount, 
	const VkBindAccelerationStructureMemoryInfoNV* pBindInfos) 
{ 
  return pfn_vkBindAccelerationStructureMemoryNV(device, bindInfoCount, pBindInfos); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructureNV(
	VkCommandBuffer commandBuffer, 
	const VkAccelerationStructureInfoNV* pInfo, 
	VkBuffer instanceData, 
	VkDeviceSize instanceOffset, 
	VkBool32 update, 
	VkAccelerationStructureNV dst, 
	VkAccelerationStructureNV src, 
	VkBuffer scratch, 
	VkDeviceSize scratchOffset) 
{ 
  pfn_vkCmdBuildAccelerationStructureNV(commandBuffer, pInfo, instanceData, instanceOffset, update, dst, src, scratch, scratchOffset); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureNV(
	VkCommandBuffer commandBuffer, 
	VkAccelerationStructureNV dst, 
	VkAccelerationStructureNV src, 
	VkCopyAccelerationStructureModeKHR mode) 
{ 
  pfn_vkCmdCopyAccelerationStructureNV(commandBuffer, dst, src, mode); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysNV(
	VkCommandBuffer commandBuffer, 
	VkBuffer raygenShaderBindingTableBuffer, 
	VkDeviceSize raygenShaderBindingOffset, 
	VkBuffer missShaderBindingTableBuffer, 
	VkDeviceSize missShaderBindingOffset, 
	VkDeviceSize missShaderBindingStride, 
	VkBuffer hitShaderBindingTableBuffer, 
	VkDeviceSize hitShaderBindingOffset, 
	VkDeviceSize hitShaderBindingStride, 
	VkBuffer callableShaderBindingTableBuffer, 
	VkDeviceSize callableShaderBindingOffset, 
	VkDeviceSize callableShaderBindingStride, 
	uint32_t width, 
	uint32_t height, 
	uint32_t depth) 
{ 
  pfn_vkCmdTraceRaysNV(commandBuffer, raygenShaderBindingTableBuffer, raygenShaderBindingOffset, missShaderBindingTableBuffer, missShaderBindingOffset, missShaderBindingStride, hitShaderBindingTableBuffer, hitShaderBindingOffset, hitShaderBindingStride, callableShaderBindingTableBuffer, callableShaderBindingOffset, callableShaderBindingStride, width, height, depth); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdWriteAccelerationStructuresPropertiesNV(
	VkCommandBuffer commandBuffer, 
	uint32_t accelerationStructureCount, 
	const VkAccelerationStructureNV* pAccelerationStructures, 
	VkQueryType queryType, 
	VkQueryPool queryPool, 
	uint32_t firstQuery) 
{ 
  pfn_vkCmdWriteAccelerationStructuresPropertiesNV(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCompileDeferredNV(
	VkDevice device, 
	VkPipeline pipeline, 
	uint32_t shader) 
{ 
  return pfn_vkCompileDeferredNV(device, pipeline, shader); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateAccelerationStructureNV(
	VkDevice device, 
	const VkAccelerationStructureCreateInfoNV* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkAccelerationStructureNV* pAccelerationStructure) 
{ 
  return pfn_vkCreateAccelerationStructureNV(device, pCreateInfo, pAllocator, pAccelerationStructure); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRayTracingPipelinesNV(
	VkDevice device, 
	VkPipelineCache pipelineCache, 
	uint32_t createInfoCount, 
	const VkRayTracingPipelineCreateInfoNV* pCreateInfos, 
	const VkAllocationCallbacks* pAllocator, 
	VkPipeline* pPipelines) 
{ 
  return pfn_vkCreateRayTracingPipelinesNV(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); 
}
VKAPI_ATTR void VKAPI_CALL vkDestroyAccelerationStructureNV(
	VkDevice device, 
	VkAccelerationStructureNV accelerationStructure, 
	const VkAllocationCallbacks* pAllocator) 
{ 
  pfn_vkDestroyAccelerationStructureNV(device, accelerationStructure, pAllocator); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetAccelerationStructureHandleNV(
	VkDevice device, 
	VkAccelerationStructureNV accelerationStructure, 
	size_t dataSize, 
	void* pData) 
{ 
  return pfn_vkGetAccelerationStructureHandleNV(device, accelerationStructure, dataSize, pData); 
}
VKAPI_ATTR void VKAPI_CALL vkGetAccelerationStructureMemoryRequirementsNV(
	VkDevice device, 
	const VkAccelerationStructureMemoryRequirementsInfoNV* pInfo, 
	VkMemoryRequirements2KHR* pMemoryRequirements) 
{ 
  pfn_vkGetAccelerationStructureMemoryRequirementsNV(device, pInfo, pMemoryRequirements); 
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingShaderGroupHandlesNV(
	VkDevice device, 
	VkPipeline pipeline, 
	uint32_t firstGroup, 
	uint32_t groupCount, 
	size_t dataSize, 
	void* pData) 
{ 
  return pfn_vkGetRayTracingShaderGroupHandlesNV(device, pipeline, firstGroup, groupCount, dataSize, pData); 
}
#endif /* VK_NV_ray_tracing */
#ifdef VK_NV_scissor_exclusive
VKAPI_ATTR void VKAPI_CALL vkCmdSetExclusiveScissorNV(
	VkCommandBuffer commandBuffer, 
	uint32_t firstExclusiveScissor, 
	uint32_t exclusiveScissorCount, 
	const VkRect2D* pExclusiveScissors) 
{ 
  pfn_vkCmdSetExclusiveScissorNV(commandBuffer, firstExclusiveScissor, exclusiveScissorCount, pExclusiveScissors); 
}
#endif /* VK_NV_scissor_exclusive */
#ifdef VK_NV_shading_rate_image
VKAPI_ATTR void VKAPI_CALL vkCmdBindShadingRateImageNV(
	VkCommandBuffer commandBuffer, 
	VkImageView imageView, 
	VkImageLayout imageLayout) 
{ 
  pfn_vkCmdBindShadingRateImageNV(commandBuffer, imageView, imageLayout); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetCoarseSampleOrderNV(
	VkCommandBuffer commandBuffer, 
	VkCoarseSampleOrderTypeNV sampleOrderType, 
	uint32_t customSampleOrderCount, 
	const VkCoarseSampleOrderCustomNV* pCustomSampleOrders) 
{ 
  pfn_vkCmdSetCoarseSampleOrderNV(commandBuffer, sampleOrderType, customSampleOrderCount, pCustomSampleOrders); 
}
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewportShadingRatePaletteNV(
	VkCommandBuffer commandBuffer, 
	uint32_t firstViewport, 
	uint32_t viewportCount, 
	const VkShadingRatePaletteNV* pShadingRatePalettes) 
{ 
  pfn_vkCmdSetViewportShadingRatePaletteNV(commandBuffer, firstViewport, viewportCount, pShadingRatePalettes); 
}
#endif /* VK_NV_shading_rate_image */
#ifdef VK_QNX_screen_surface
VKAPI_ATTR VkResult VKAPI_CALL vkCreateScreenSurfaceQNX(
	VkInstance instance, 
	const VkScreenSurfaceCreateInfoQNX* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkSurfaceKHR* pSurface) 
{ 
  return pfn_vkCreateScreenSurfaceQNX(instance, pCreateInfo, pAllocator, pSurface); 
}
VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceScreenPresentationSupportQNX(
	VkPhysicalDevice physicalDevice, 
	uint32_t queueFamilyIndex, 
	struct _screen_window* window) 
{ 
  return pfn_vkGetPhysicalDeviceScreenPresentationSupportQNX(physicalDevice, queueFamilyIndex, window); 
}
#endif /* VK_QNX_screen_surface */
/* NVVK_GENERATE_DECLARE */


/* super load/reset */
void load_VK_EXTENSIONS(VkInstance instance, PFN_vkGetInstanceProcAddr getInstanceProcAddr, VkDevice device, PFN_vkGetDeviceProcAddr getDeviceProcAddr) {
/* NVVK_GENERATE_LOAD_PROC */
#ifdef VK_AMD_buffer_marker
  pfn_vkCmdWriteBufferMarkerAMD = (PFN_vkCmdWriteBufferMarkerAMD)getDeviceProcAddr(device, "vkCmdWriteBufferMarkerAMD");
#endif /* VK_AMD_buffer_marker */
#ifdef VK_AMD_display_native_hdr
  pfn_vkSetLocalDimmingAMD = (PFN_vkSetLocalDimmingAMD)getDeviceProcAddr(device, "vkSetLocalDimmingAMD");
#endif /* VK_AMD_display_native_hdr */
#ifdef VK_AMD_draw_indirect_count
  pfn_vkCmdDrawIndexedIndirectCountAMD = (PFN_vkCmdDrawIndexedIndirectCountAMD)getDeviceProcAddr(device, "vkCmdDrawIndexedIndirectCountAMD");
  pfn_vkCmdDrawIndirectCountAMD = (PFN_vkCmdDrawIndirectCountAMD)getDeviceProcAddr(device, "vkCmdDrawIndirectCountAMD");
#endif /* VK_AMD_draw_indirect_count */
#ifdef VK_AMD_shader_info
  pfn_vkGetShaderInfoAMD = (PFN_vkGetShaderInfoAMD)getDeviceProcAddr(device, "vkGetShaderInfoAMD");
#endif /* VK_AMD_shader_info */
#ifdef VK_ANDROID_external_memory_android_hardware_buffer
  pfn_vkGetAndroidHardwareBufferPropertiesANDROID = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)getDeviceProcAddr(device, "vkGetAndroidHardwareBufferPropertiesANDROID");
  pfn_vkGetMemoryAndroidHardwareBufferANDROID = (PFN_vkGetMemoryAndroidHardwareBufferANDROID)getDeviceProcAddr(device, "vkGetMemoryAndroidHardwareBufferANDROID");
#endif /* VK_ANDROID_external_memory_android_hardware_buffer */
#ifdef VK_EXT_acquire_xlib_display
  pfn_vkAcquireXlibDisplayEXT = (PFN_vkAcquireXlibDisplayEXT)getInstanceProcAddr(instance, "vkAcquireXlibDisplayEXT");
  pfn_vkGetRandROutputDisplayEXT = (PFN_vkGetRandROutputDisplayEXT)getInstanceProcAddr(instance, "vkGetRandROutputDisplayEXT");
#endif /* VK_EXT_acquire_xlib_display */
#ifdef VK_EXT_buffer_device_address
  pfn_vkGetBufferDeviceAddressEXT = (PFN_vkGetBufferDeviceAddressEXT)getDeviceProcAddr(device, "vkGetBufferDeviceAddressEXT");
#endif /* VK_EXT_buffer_device_address */
#ifdef VK_EXT_calibrated_timestamps
  pfn_vkGetCalibratedTimestampsEXT = (PFN_vkGetCalibratedTimestampsEXT)getDeviceProcAddr(device, "vkGetCalibratedTimestampsEXT");
  pfn_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)getInstanceProcAddr(instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
#endif /* VK_EXT_calibrated_timestamps */
#ifdef VK_EXT_color_write_enable
  pfn_vkCmdSetColorWriteEnableEXT = (PFN_vkCmdSetColorWriteEnableEXT)getDeviceProcAddr(device, "vkCmdSetColorWriteEnableEXT");
#endif /* VK_EXT_color_write_enable */
#ifdef VK_EXT_conditional_rendering
  pfn_vkCmdBeginConditionalRenderingEXT = (PFN_vkCmdBeginConditionalRenderingEXT)getDeviceProcAddr(device, "vkCmdBeginConditionalRenderingEXT");
  pfn_vkCmdEndConditionalRenderingEXT = (PFN_vkCmdEndConditionalRenderingEXT)getDeviceProcAddr(device, "vkCmdEndConditionalRenderingEXT");
#endif /* VK_EXT_conditional_rendering */
#ifdef VK_EXT_debug_marker
  pfn_vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)getDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT");
  pfn_vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)getDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT");
  pfn_vkCmdDebugMarkerInsertEXT = (PFN_vkCmdDebugMarkerInsertEXT)getDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT");
  pfn_vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)getDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT");
  pfn_vkDebugMarkerSetObjectTagEXT = (PFN_vkDebugMarkerSetObjectTagEXT)getDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT");
#endif /* VK_EXT_debug_marker */
#ifdef VK_EXT_debug_report
  pfn_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)getInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
  pfn_vkDebugReportMessageEXT = (PFN_vkDebugReportMessageEXT)getInstanceProcAddr(instance, "vkDebugReportMessageEXT");
  pfn_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)getInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
#endif /* VK_EXT_debug_report */
#ifdef VK_EXT_debug_utils
  pfn_vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)getInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
  pfn_vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)getInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
  pfn_vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)getInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT");
  pfn_vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)getInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  pfn_vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)getInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  pfn_vkQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)getInstanceProcAddr(instance, "vkQueueBeginDebugUtilsLabelEXT");
  pfn_vkQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)getInstanceProcAddr(instance, "vkQueueEndDebugUtilsLabelEXT");
  pfn_vkQueueInsertDebugUtilsLabelEXT = (PFN_vkQueueInsertDebugUtilsLabelEXT)getInstanceProcAddr(instance, "vkQueueInsertDebugUtilsLabelEXT");
  pfn_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)getInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
  pfn_vkSetDebugUtilsObjectTagEXT = (PFN_vkSetDebugUtilsObjectTagEXT)getInstanceProcAddr(instance, "vkSetDebugUtilsObjectTagEXT");
  pfn_vkSubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)getInstanceProcAddr(instance, "vkSubmitDebugUtilsMessageEXT");
#endif /* VK_EXT_debug_utils */
#ifdef VK_EXT_direct_mode_display
  pfn_vkReleaseDisplayEXT = (PFN_vkReleaseDisplayEXT)getInstanceProcAddr(instance, "vkReleaseDisplayEXT");
#endif /* VK_EXT_direct_mode_display */
#ifdef VK_EXT_directfb_surface
  pfn_vkCreateDirectFBSurfaceEXT = (PFN_vkCreateDirectFBSurfaceEXT)getInstanceProcAddr(instance, "vkCreateDirectFBSurfaceEXT");
  pfn_vkGetPhysicalDeviceDirectFBPresentationSupportEXT = (PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT)getInstanceProcAddr(instance, "vkGetPhysicalDeviceDirectFBPresentationSupportEXT");
#endif /* VK_EXT_directfb_surface */
#ifdef VK_EXT_discard_rectangles
  pfn_vkCmdSetDiscardRectangleEXT = (PFN_vkCmdSetDiscardRectangleEXT)getDeviceProcAddr(device, "vkCmdSetDiscardRectangleEXT");
#endif /* VK_EXT_discard_rectangles */
#ifdef VK_EXT_display_control
  pfn_vkDisplayPowerControlEXT = (PFN_vkDisplayPowerControlEXT)getDeviceProcAddr(device, "vkDisplayPowerControlEXT");
  pfn_vkGetSwapchainCounterEXT = (PFN_vkGetSwapchainCounterEXT)getDeviceProcAddr(device, "vkGetSwapchainCounterEXT");
  pfn_vkRegisterDeviceEventEXT = (PFN_vkRegisterDeviceEventEXT)getDeviceProcAddr(device, "vkRegisterDeviceEventEXT");
  pfn_vkRegisterDisplayEventEXT = (PFN_vkRegisterDisplayEventEXT)getDeviceProcAddr(device, "vkRegisterDisplayEventEXT");
#endif /* VK_EXT_display_control */
#ifdef VK_EXT_display_surface_counter
  pfn_vkGetPhysicalDeviceSurfaceCapabilities2EXT = (PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilities2EXT");
#endif /* VK_EXT_display_surface_counter */
#ifdef VK_EXT_extended_dynamic_state
  pfn_vkCmdBindVertexBuffers2EXT = (PFN_vkCmdBindVertexBuffers2EXT)getDeviceProcAddr(device, "vkCmdBindVertexBuffers2EXT");
  pfn_vkCmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)getDeviceProcAddr(device, "vkCmdSetCullModeEXT");
  pfn_vkCmdSetDepthBoundsTestEnableEXT = (PFN_vkCmdSetDepthBoundsTestEnableEXT)getDeviceProcAddr(device, "vkCmdSetDepthBoundsTestEnableEXT");
  pfn_vkCmdSetDepthCompareOpEXT = (PFN_vkCmdSetDepthCompareOpEXT)getDeviceProcAddr(device, "vkCmdSetDepthCompareOpEXT");
  pfn_vkCmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT)getDeviceProcAddr(device, "vkCmdSetDepthTestEnableEXT");
  pfn_vkCmdSetDepthWriteEnableEXT = (PFN_vkCmdSetDepthWriteEnableEXT)getDeviceProcAddr(device, "vkCmdSetDepthWriteEnableEXT");
  pfn_vkCmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT)getDeviceProcAddr(device, "vkCmdSetFrontFaceEXT");
  pfn_vkCmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT)getDeviceProcAddr(device, "vkCmdSetPrimitiveTopologyEXT");
  pfn_vkCmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT)getDeviceProcAddr(device, "vkCmdSetScissorWithCountEXT");
  pfn_vkCmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT)getDeviceProcAddr(device, "vkCmdSetStencilOpEXT");
  pfn_vkCmdSetStencilTestEnableEXT = (PFN_vkCmdSetStencilTestEnableEXT)getDeviceProcAddr(device, "vkCmdSetStencilTestEnableEXT");
  pfn_vkCmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT)getDeviceProcAddr(device, "vkCmdSetViewportWithCountEXT");
#endif /* VK_EXT_extended_dynamic_state */
#ifdef VK_EXT_extended_dynamic_state2
  pfn_vkCmdSetDepthBiasEnableEXT = (PFN_vkCmdSetDepthBiasEnableEXT)getDeviceProcAddr(device, "vkCmdSetDepthBiasEnableEXT");
  pfn_vkCmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT)getDeviceProcAddr(device, "vkCmdSetLogicOpEXT");
  pfn_vkCmdSetPatchControlPointsEXT = (PFN_vkCmdSetPatchControlPointsEXT)getDeviceProcAddr(device, "vkCmdSetPatchControlPointsEXT");
  pfn_vkCmdSetPrimitiveRestartEnableEXT = (PFN_vkCmdSetPrimitiveRestartEnableEXT)getDeviceProcAddr(device, "vkCmdSetPrimitiveRestartEnableEXT");
  pfn_vkCmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT)getDeviceProcAddr(device, "vkCmdSetRasterizerDiscardEnableEXT");
#endif /* VK_EXT_extended_dynamic_state2 */
#ifdef VK_EXT_external_memory_host
  pfn_vkGetMemoryHostPointerPropertiesEXT = (PFN_vkGetMemoryHostPointerPropertiesEXT)getDeviceProcAddr(device, "vkGetMemoryHostPointerPropertiesEXT");
#endif /* VK_EXT_external_memory_host */
#ifdef VK_EXT_full_screen_exclusive
  pfn_vkAcquireFullScreenExclusiveModeEXT = (PFN_vkAcquireFullScreenExclusiveModeEXT)getDeviceProcAddr(device, "vkAcquireFullScreenExclusiveModeEXT");
  pfn_vkGetPhysicalDeviceSurfacePresentModes2EXT = (PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModes2EXT");
  pfn_vkReleaseFullScreenExclusiveModeEXT = (PFN_vkReleaseFullScreenExclusiveModeEXT)getDeviceProcAddr(device, "vkReleaseFullScreenExclusiveModeEXT");
#endif /* VK_EXT_full_screen_exclusive */
#ifdef VK_EXT_hdr_metadata
  pfn_vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT)getDeviceProcAddr(device, "vkSetHdrMetadataEXT");
#endif /* VK_EXT_hdr_metadata */
#ifdef VK_EXT_headless_surface
  pfn_vkCreateHeadlessSurfaceEXT = (PFN_vkCreateHeadlessSurfaceEXT)getInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT");
#endif /* VK_EXT_headless_surface */
#ifdef VK_EXT_host_query_reset
  pfn_vkResetQueryPoolEXT = (PFN_vkResetQueryPoolEXT)getDeviceProcAddr(device, "vkResetQueryPoolEXT");
#endif /* VK_EXT_host_query_reset */
#ifdef VK_EXT_image_drm_format_modifier
  pfn_vkGetImageDrmFormatModifierPropertiesEXT = (PFN_vkGetImageDrmFormatModifierPropertiesEXT)getDeviceProcAddr(device, "vkGetImageDrmFormatModifierPropertiesEXT");
#endif /* VK_EXT_image_drm_format_modifier */
#ifdef VK_EXT_line_rasterization
  pfn_vkCmdSetLineStippleEXT = (PFN_vkCmdSetLineStippleEXT)getDeviceProcAddr(device, "vkCmdSetLineStippleEXT");
#endif /* VK_EXT_line_rasterization */
#ifdef VK_EXT_metal_surface
  pfn_vkCreateMetalSurfaceEXT = (PFN_vkCreateMetalSurfaceEXT)getInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT");
#endif /* VK_EXT_metal_surface */
#ifdef VK_EXT_private_data
  pfn_vkCreatePrivateDataSlotEXT = (PFN_vkCreatePrivateDataSlotEXT)getDeviceProcAddr(device, "vkCreatePrivateDataSlotEXT");
  pfn_vkDestroyPrivateDataSlotEXT = (PFN_vkDestroyPrivateDataSlotEXT)getDeviceProcAddr(device, "vkDestroyPrivateDataSlotEXT");
  pfn_vkGetPrivateDataEXT = (PFN_vkGetPrivateDataEXT)getDeviceProcAddr(device, "vkGetPrivateDataEXT");
  pfn_vkSetPrivateDataEXT = (PFN_vkSetPrivateDataEXT)getDeviceProcAddr(device, "vkSetPrivateDataEXT");
#endif /* VK_EXT_private_data */
#ifdef VK_EXT_sample_locations
  pfn_vkCmdSetSampleLocationsEXT = (PFN_vkCmdSetSampleLocationsEXT)getDeviceProcAddr(device, "vkCmdSetSampleLocationsEXT");
  pfn_vkGetPhysicalDeviceMultisamplePropertiesEXT = (PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT)getInstanceProcAddr(instance, "vkGetPhysicalDeviceMultisamplePropertiesEXT");
#endif /* VK_EXT_sample_locations */
#ifdef VK_EXT_tooling_info
  pfn_vkGetPhysicalDeviceToolPropertiesEXT = (PFN_vkGetPhysicalDeviceToolPropertiesEXT)getInstanceProcAddr(instance, "vkGetPhysicalDeviceToolPropertiesEXT");
#endif /* VK_EXT_tooling_info */
#ifdef VK_EXT_transform_feedback
  pfn_vkCmdBeginQueryIndexedEXT = (PFN_vkCmdBeginQueryIndexedEXT)getDeviceProcAddr(device, "vkCmdBeginQueryIndexedEXT");
  pfn_vkCmdBeginTransformFeedbackEXT = (PFN_vkCmdBeginTransformFeedbackEXT)getDeviceProcAddr(device, "vkCmdBeginTransformFeedbackEXT");
  pfn_vkCmdBindTransformFeedbackBuffersEXT = (PFN_vkCmdBindTransformFeedbackBuffersEXT)getDeviceProcAddr(device, "vkCmdBindTransformFeedbackBuffersEXT");
  pfn_vkCmdDrawIndirectByteCountEXT = (PFN_vkCmdDrawIndirectByteCountEXT)getDeviceProcAddr(device, "vkCmdDrawIndirectByteCountEXT");
  pfn_vkCmdEndQueryIndexedEXT = (PFN_vkCmdEndQueryIndexedEXT)getDeviceProcAddr(device, "vkCmdEndQueryIndexedEXT");
  pfn_vkCmdEndTransformFeedbackEXT = (PFN_vkCmdEndTransformFeedbackEXT)getDeviceProcAddr(device, "vkCmdEndTransformFeedbackEXT");
#endif /* VK_EXT_transform_feedback */
#ifdef VK_EXT_validation_cache
  pfn_vkCreateValidationCacheEXT = (PFN_vkCreateValidationCacheEXT)getDeviceProcAddr(device, "vkCreateValidationCacheEXT");
  pfn_vkDestroyValidationCacheEXT = (PFN_vkDestroyValidationCacheEXT)getDeviceProcAddr(device, "vkDestroyValidationCacheEXT");
  pfn_vkGetValidationCacheDataEXT = (PFN_vkGetValidationCacheDataEXT)getDeviceProcAddr(device, "vkGetValidationCacheDataEXT");
  pfn_vkMergeValidationCachesEXT = (PFN_vkMergeValidationCachesEXT)getDeviceProcAddr(device, "vkMergeValidationCachesEXT");
#endif /* VK_EXT_validation_cache */
#ifdef VK_EXT_vertex_input_dynamic_state
  pfn_vkCmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)getDeviceProcAddr(device, "vkCmdSetVertexInputEXT");
#endif /* VK_EXT_vertex_input_dynamic_state */
#ifdef VK_FUCHSIA_external_memory
  pfn_vkGetMemoryZirconHandleFUCHSIA = (PFN_vkGetMemoryZirconHandleFUCHSIA)getDeviceProcAddr(device, "vkGetMemoryZirconHandleFUCHSIA");
  pfn_vkGetMemoryZirconHandlePropertiesFUCHSIA = (PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA)getDeviceProcAddr(device, "vkGetMemoryZirconHandlePropertiesFUCHSIA");
#endif /* VK_FUCHSIA_external_memory */
#ifdef VK_FUCHSIA_external_semaphore
  pfn_vkGetSemaphoreZirconHandleFUCHSIA = (PFN_vkGetSemaphoreZirconHandleFUCHSIA)getDeviceProcAddr(device, "vkGetSemaphoreZirconHandleFUCHSIA");
  pfn_vkImportSemaphoreZirconHandleFUCHSIA = (PFN_vkImportSemaphoreZirconHandleFUCHSIA)getDeviceProcAddr(device, "vkImportSemaphoreZirconHandleFUCHSIA");
#endif /* VK_FUCHSIA_external_semaphore */
#ifdef VK_FUCHSIA_imagepipe_surface
  pfn_vkCreateImagePipeSurfaceFUCHSIA = (PFN_vkCreateImagePipeSurfaceFUCHSIA)getInstanceProcAddr(instance, "vkCreateImagePipeSurfaceFUCHSIA");
#endif /* VK_FUCHSIA_imagepipe_surface */
#ifdef VK_GGP_stream_descriptor_surface
  pfn_vkCreateStreamDescriptorSurfaceGGP = (PFN_vkCreateStreamDescriptorSurfaceGGP)getInstanceProcAddr(instance, "vkCreateStreamDescriptorSurfaceGGP");
#endif /* VK_GGP_stream_descriptor_surface */
#ifdef VK_GOOGLE_display_timing
  pfn_vkGetPastPresentationTimingGOOGLE = (PFN_vkGetPastPresentationTimingGOOGLE)getDeviceProcAddr(device, "vkGetPastPresentationTimingGOOGLE");
  pfn_vkGetRefreshCycleDurationGOOGLE = (PFN_vkGetRefreshCycleDurationGOOGLE)getDeviceProcAddr(device, "vkGetRefreshCycleDurationGOOGLE");
#endif /* VK_GOOGLE_display_timing */
#ifdef VK_INTEL_performance_query
  pfn_vkAcquirePerformanceConfigurationINTEL = (PFN_vkAcquirePerformanceConfigurationINTEL)getDeviceProcAddr(device, "vkAcquirePerformanceConfigurationINTEL");
  pfn_vkCmdSetPerformanceMarkerINTEL = (PFN_vkCmdSetPerformanceMarkerINTEL)getDeviceProcAddr(device, "vkCmdSetPerformanceMarkerINTEL");
  pfn_vkCmdSetPerformanceOverrideINTEL = (PFN_vkCmdSetPerformanceOverrideINTEL)getDeviceProcAddr(device, "vkCmdSetPerformanceOverrideINTEL");
  pfn_vkCmdSetPerformanceStreamMarkerINTEL = (PFN_vkCmdSetPerformanceStreamMarkerINTEL)getDeviceProcAddr(device, "vkCmdSetPerformanceStreamMarkerINTEL");
  pfn_vkGetPerformanceParameterINTEL = (PFN_vkGetPerformanceParameterINTEL)getDeviceProcAddr(device, "vkGetPerformanceParameterINTEL");
  pfn_vkInitializePerformanceApiINTEL = (PFN_vkInitializePerformanceApiINTEL)getDeviceProcAddr(device, "vkInitializePerformanceApiINTEL");
  pfn_vkQueueSetPerformanceConfigurationINTEL = (PFN_vkQueueSetPerformanceConfigurationINTEL)getDeviceProcAddr(device, "vkQueueSetPerformanceConfigurationINTEL");
  pfn_vkReleasePerformanceConfigurationINTEL = (PFN_vkReleasePerformanceConfigurationINTEL)getDeviceProcAddr(device, "vkReleasePerformanceConfigurationINTEL");
  pfn_vkUninitializePerformanceApiINTEL = (PFN_vkUninitializePerformanceApiINTEL)getDeviceProcAddr(device, "vkUninitializePerformanceApiINTEL");
#endif /* VK_INTEL_performance_query */
#ifdef VK_KHR_acceleration_structure
  pfn_vkBuildAccelerationStructuresKHR = (PFN_vkBuildAccelerationStructuresKHR)getDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR");
  pfn_vkCmdBuildAccelerationStructuresIndirectKHR = (PFN_vkCmdBuildAccelerationStructuresIndirectKHR)getDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresIndirectKHR");
  pfn_vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)getDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
  pfn_vkCmdCopyAccelerationStructureKHR = (PFN_vkCmdCopyAccelerationStructureKHR)getDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR");
  pfn_vkCmdCopyAccelerationStructureToMemoryKHR = (PFN_vkCmdCopyAccelerationStructureToMemoryKHR)getDeviceProcAddr(device, "vkCmdCopyAccelerationStructureToMemoryKHR");
  pfn_vkCmdCopyMemoryToAccelerationStructureKHR = (PFN_vkCmdCopyMemoryToAccelerationStructureKHR)getDeviceProcAddr(device, "vkCmdCopyMemoryToAccelerationStructureKHR");
  pfn_vkCmdWriteAccelerationStructuresPropertiesKHR = (PFN_vkCmdWriteAccelerationStructuresPropertiesKHR)getDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR");
  pfn_vkCopyAccelerationStructureKHR = (PFN_vkCopyAccelerationStructureKHR)getDeviceProcAddr(device, "vkCopyAccelerationStructureKHR");
  pfn_vkCopyAccelerationStructureToMemoryKHR = (PFN_vkCopyAccelerationStructureToMemoryKHR)getDeviceProcAddr(device, "vkCopyAccelerationStructureToMemoryKHR");
  pfn_vkCopyMemoryToAccelerationStructureKHR = (PFN_vkCopyMemoryToAccelerationStructureKHR)getDeviceProcAddr(device, "vkCopyMemoryToAccelerationStructureKHR");
  pfn_vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)getDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
  pfn_vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)getDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
  pfn_vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)getDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
  pfn_vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)getDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
  pfn_vkGetDeviceAccelerationStructureCompatibilityKHR = (PFN_vkGetDeviceAccelerationStructureCompatibilityKHR)getDeviceProcAddr(device, "vkGetDeviceAccelerationStructureCompatibilityKHR");
  pfn_vkWriteAccelerationStructuresPropertiesKHR = (PFN_vkWriteAccelerationStructuresPropertiesKHR)getDeviceProcAddr(device, "vkWriteAccelerationStructuresPropertiesKHR");
#endif /* VK_KHR_acceleration_structure */
#ifdef VK_KHR_android_surface
  pfn_vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)getInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR");
#endif /* VK_KHR_android_surface */
#ifdef VK_KHR_bind_memory2
  pfn_vkBindBufferMemory2KHR = (PFN_vkBindBufferMemory2KHR)getDeviceProcAddr(device, "vkBindBufferMemory2KHR");
  pfn_vkBindImageMemory2KHR = (PFN_vkBindImageMemory2KHR)getDeviceProcAddr(device, "vkBindImageMemory2KHR");
#endif /* VK_KHR_bind_memory2 */
#ifdef VK_KHR_buffer_device_address
  pfn_vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)getDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
  pfn_vkGetBufferOpaqueCaptureAddressKHR = (PFN_vkGetBufferOpaqueCaptureAddressKHR)getDeviceProcAddr(device, "vkGetBufferOpaqueCaptureAddressKHR");
  pfn_vkGetDeviceMemoryOpaqueCaptureAddressKHR = (PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR)getDeviceProcAddr(device, "vkGetDeviceMemoryOpaqueCaptureAddressKHR");
#endif /* VK_KHR_buffer_device_address */
#ifdef VK_KHR_copy_commands2
  pfn_vkCmdBlitImage2KHR = (PFN_vkCmdBlitImage2KHR)getDeviceProcAddr(device, "vkCmdBlitImage2KHR");
  pfn_vkCmdCopyBuffer2KHR = (PFN_vkCmdCopyBuffer2KHR)getDeviceProcAddr(device, "vkCmdCopyBuffer2KHR");
  pfn_vkCmdCopyBufferToImage2KHR = (PFN_vkCmdCopyBufferToImage2KHR)getDeviceProcAddr(device, "vkCmdCopyBufferToImage2KHR");
  pfn_vkCmdCopyImage2KHR = (PFN_vkCmdCopyImage2KHR)getDeviceProcAddr(device, "vkCmdCopyImage2KHR");
  pfn_vkCmdCopyImageToBuffer2KHR = (PFN_vkCmdCopyImageToBuffer2KHR)getDeviceProcAddr(device, "vkCmdCopyImageToBuffer2KHR");
  pfn_vkCmdResolveImage2KHR = (PFN_vkCmdResolveImage2KHR)getDeviceProcAddr(device, "vkCmdResolveImage2KHR");
#endif /* VK_KHR_copy_commands2 */
#ifdef VK_KHR_create_renderpass2
  pfn_vkCmdBeginRenderPass2KHR = (PFN_vkCmdBeginRenderPass2KHR)getDeviceProcAddr(device, "vkCmdBeginRenderPass2KHR");
  pfn_vkCmdEndRenderPass2KHR = (PFN_vkCmdEndRenderPass2KHR)getDeviceProcAddr(device, "vkCmdEndRenderPass2KHR");
  pfn_vkCmdNextSubpass2KHR = (PFN_vkCmdNextSubpass2KHR)getDeviceProcAddr(device, "vkCmdNextSubpass2KHR");
  pfn_vkCreateRenderPass2KHR = (PFN_vkCreateRenderPass2KHR)getDeviceProcAddr(device, "vkCreateRenderPass2KHR");
#endif /* VK_KHR_create_renderpass2 */
#ifdef VK_KHR_deferred_host_operations
  pfn_vkCreateDeferredOperationKHR = (PFN_vkCreateDeferredOperationKHR)getDeviceProcAddr(device, "vkCreateDeferredOperationKHR");
  pfn_vkDeferredOperationJoinKHR = (PFN_vkDeferredOperationJoinKHR)getDeviceProcAddr(device, "vkDeferredOperationJoinKHR");
  pfn_vkDestroyDeferredOperationKHR = (PFN_vkDestroyDeferredOperationKHR)getDeviceProcAddr(device, "vkDestroyDeferredOperationKHR");
  pfn_vkGetDeferredOperationMaxConcurrencyKHR = (PFN_vkGetDeferredOperationMaxConcurrencyKHR)getDeviceProcAddr(device, "vkGetDeferredOperationMaxConcurrencyKHR");
  pfn_vkGetDeferredOperationResultKHR = (PFN_vkGetDeferredOperationResultKHR)getDeviceProcAddr(device, "vkGetDeferredOperationResultKHR");
#endif /* VK_KHR_deferred_host_operations */
#ifdef VK_KHR_descriptor_update_template
  pfn_vkCreateDescriptorUpdateTemplateKHR = (PFN_vkCreateDescriptorUpdateTemplateKHR)getDeviceProcAddr(device, "vkCreateDescriptorUpdateTemplateKHR");
  pfn_vkDestroyDescriptorUpdateTemplateKHR = (PFN_vkDestroyDescriptorUpdateTemplateKHR)getDeviceProcAddr(device, "vkDestroyDescriptorUpdateTemplateKHR");
  pfn_vkUpdateDescriptorSetWithTemplateKHR = (PFN_vkUpdateDescriptorSetWithTemplateKHR)getDeviceProcAddr(device, "vkUpdateDescriptorSetWithTemplateKHR");
#endif /* VK_KHR_descriptor_update_template */
#ifdef VK_KHR_device_group
  pfn_vkCmdDispatchBaseKHR = (PFN_vkCmdDispatchBaseKHR)getDeviceProcAddr(device, "vkCmdDispatchBaseKHR");
  pfn_vkCmdSetDeviceMaskKHR = (PFN_vkCmdSetDeviceMaskKHR)getDeviceProcAddr(device, "vkCmdSetDeviceMaskKHR");
  pfn_vkGetDeviceGroupPeerMemoryFeaturesKHR = (PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR)getDeviceProcAddr(device, "vkGetDeviceGroupPeerMemoryFeaturesKHR");
#endif /* VK_KHR_device_group */
#ifdef VK_KHR_device_group_creation
  pfn_vkEnumeratePhysicalDeviceGroupsKHR = (PFN_vkEnumeratePhysicalDeviceGroupsKHR)getInstanceProcAddr(instance, "vkEnumeratePhysicalDeviceGroupsKHR");
#endif /* VK_KHR_device_group_creation */
#ifdef VK_KHR_draw_indirect_count
  pfn_vkCmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR)getDeviceProcAddr(device, "vkCmdDrawIndexedIndirectCountKHR");
  pfn_vkCmdDrawIndirectCountKHR = (PFN_vkCmdDrawIndirectCountKHR)getDeviceProcAddr(device, "vkCmdDrawIndirectCountKHR");
#endif /* VK_KHR_draw_indirect_count */
#ifdef VK_KHR_external_fence_capabilities
  pfn_vkGetPhysicalDeviceExternalFencePropertiesKHR = (PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalFencePropertiesKHR");
#endif /* VK_KHR_external_fence_capabilities */
#ifdef VK_KHR_external_fence_fd
  pfn_vkGetFenceFdKHR = (PFN_vkGetFenceFdKHR)getDeviceProcAddr(device, "vkGetFenceFdKHR");
  pfn_vkImportFenceFdKHR = (PFN_vkImportFenceFdKHR)getDeviceProcAddr(device, "vkImportFenceFdKHR");
#endif /* VK_KHR_external_fence_fd */
#ifdef VK_KHR_external_fence_win32
  pfn_vkGetFenceWin32HandleKHR = (PFN_vkGetFenceWin32HandleKHR)getDeviceProcAddr(device, "vkGetFenceWin32HandleKHR");
  pfn_vkImportFenceWin32HandleKHR = (PFN_vkImportFenceWin32HandleKHR)getDeviceProcAddr(device, "vkImportFenceWin32HandleKHR");
#endif /* VK_KHR_external_fence_win32 */
#ifdef VK_KHR_external_memory_capabilities
  pfn_vkGetPhysicalDeviceExternalBufferPropertiesKHR = (PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalBufferPropertiesKHR");
#endif /* VK_KHR_external_memory_capabilities */
#ifdef VK_KHR_external_memory_fd
  pfn_vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)getDeviceProcAddr(device, "vkGetMemoryFdKHR");
  pfn_vkGetMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR)getDeviceProcAddr(device, "vkGetMemoryFdPropertiesKHR");
#endif /* VK_KHR_external_memory_fd */
#ifdef VK_KHR_external_memory_win32
  pfn_vkGetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)getDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR");
  pfn_vkGetMemoryWin32HandlePropertiesKHR = (PFN_vkGetMemoryWin32HandlePropertiesKHR)getDeviceProcAddr(device, "vkGetMemoryWin32HandlePropertiesKHR");
#endif /* VK_KHR_external_memory_win32 */
#ifdef VK_KHR_external_semaphore_capabilities
  pfn_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = (PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR");
#endif /* VK_KHR_external_semaphore_capabilities */
#ifdef VK_KHR_external_semaphore_fd
  pfn_vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)getDeviceProcAddr(device, "vkGetSemaphoreFdKHR");
  pfn_vkImportSemaphoreFdKHR = (PFN_vkImportSemaphoreFdKHR)getDeviceProcAddr(device, "vkImportSemaphoreFdKHR");
#endif /* VK_KHR_external_semaphore_fd */
#ifdef VK_KHR_external_semaphore_win32
  pfn_vkGetSemaphoreWin32HandleKHR = (PFN_vkGetSemaphoreWin32HandleKHR)getDeviceProcAddr(device, "vkGetSemaphoreWin32HandleKHR");
  pfn_vkImportSemaphoreWin32HandleKHR = (PFN_vkImportSemaphoreWin32HandleKHR)getDeviceProcAddr(device, "vkImportSemaphoreWin32HandleKHR");
#endif /* VK_KHR_external_semaphore_win32 */
#ifdef VK_KHR_fragment_shading_rate
  pfn_vkCmdSetFragmentShadingRateKHR = (PFN_vkCmdSetFragmentShadingRateKHR)getDeviceProcAddr(device, "vkCmdSetFragmentShadingRateKHR");
  pfn_vkGetPhysicalDeviceFragmentShadingRatesKHR = (PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFragmentShadingRatesKHR");
#endif /* VK_KHR_fragment_shading_rate */
#ifdef VK_KHR_get_memory_requirements2
  pfn_vkGetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)getDeviceProcAddr(device, "vkGetBufferMemoryRequirements2KHR");
  pfn_vkGetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2KHR)getDeviceProcAddr(device, "vkGetImageMemoryRequirements2KHR");
  pfn_vkGetImageSparseMemoryRequirements2KHR = (PFN_vkGetImageSparseMemoryRequirements2KHR)getDeviceProcAddr(device, "vkGetImageSparseMemoryRequirements2KHR");
#endif /* VK_KHR_get_memory_requirements2 */
#ifdef VK_KHR_get_physical_device_properties2
  pfn_vkGetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR");
  pfn_vkGetPhysicalDeviceFormatProperties2KHR = (PFN_vkGetPhysicalDeviceFormatProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2KHR");
  pfn_vkGetPhysicalDeviceImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties2KHR");
  pfn_vkGetPhysicalDeviceMemoryProperties2KHR = (PFN_vkGetPhysicalDeviceMemoryProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties2KHR");
  pfn_vkGetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
  pfn_vkGetPhysicalDeviceQueueFamilyProperties2KHR = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties2KHR");
  pfn_vkGetPhysicalDeviceSparseImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
#endif /* VK_KHR_get_physical_device_properties2 */
#ifdef VK_KHR_maintenance1
  pfn_vkTrimCommandPoolKHR = (PFN_vkTrimCommandPoolKHR)getDeviceProcAddr(device, "vkTrimCommandPoolKHR");
#endif /* VK_KHR_maintenance1 */
#ifdef VK_KHR_maintenance3
  pfn_vkGetDescriptorSetLayoutSupportKHR = (PFN_vkGetDescriptorSetLayoutSupportKHR)getDeviceProcAddr(device, "vkGetDescriptorSetLayoutSupportKHR");
#endif /* VK_KHR_maintenance3 */
#ifdef VK_KHR_performance_query
  pfn_vkAcquireProfilingLockKHR = (PFN_vkAcquireProfilingLockKHR)getDeviceProcAddr(device, "vkAcquireProfilingLockKHR");
  pfn_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR = (PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)getInstanceProcAddr(instance, "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");
  pfn_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR = (PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR");
  pfn_vkReleaseProfilingLockKHR = (PFN_vkReleaseProfilingLockKHR)getDeviceProcAddr(device, "vkReleaseProfilingLockKHR");
#endif /* VK_KHR_performance_query */
#ifdef VK_KHR_pipeline_executable_properties
  pfn_vkGetPipelineExecutableInternalRepresentationsKHR = (PFN_vkGetPipelineExecutableInternalRepresentationsKHR)getDeviceProcAddr(device, "vkGetPipelineExecutableInternalRepresentationsKHR");
  pfn_vkGetPipelineExecutablePropertiesKHR = (PFN_vkGetPipelineExecutablePropertiesKHR)getDeviceProcAddr(device, "vkGetPipelineExecutablePropertiesKHR");
  pfn_vkGetPipelineExecutableStatisticsKHR = (PFN_vkGetPipelineExecutableStatisticsKHR)getDeviceProcAddr(device, "vkGetPipelineExecutableStatisticsKHR");
#endif /* VK_KHR_pipeline_executable_properties */
#ifdef VK_KHR_push_descriptor
  pfn_vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)getDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");
#endif /* VK_KHR_push_descriptor */
#ifdef VK_KHR_ray_tracing_pipeline
  pfn_vkCmdSetRayTracingPipelineStackSizeKHR = (PFN_vkCmdSetRayTracingPipelineStackSizeKHR)getDeviceProcAddr(device, "vkCmdSetRayTracingPipelineStackSizeKHR");
  pfn_vkCmdTraceRaysIndirectKHR = (PFN_vkCmdTraceRaysIndirectKHR)getDeviceProcAddr(device, "vkCmdTraceRaysIndirectKHR");
  pfn_vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)getDeviceProcAddr(device, "vkCmdTraceRaysKHR");
  pfn_vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)getDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
  pfn_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = (PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)getDeviceProcAddr(device, "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR");
  pfn_vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)getDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
  pfn_vkGetRayTracingShaderGroupStackSizeKHR = (PFN_vkGetRayTracingShaderGroupStackSizeKHR)getDeviceProcAddr(device, "vkGetRayTracingShaderGroupStackSizeKHR");
#endif /* VK_KHR_ray_tracing_pipeline */
#ifdef VK_KHR_sampler_ycbcr_conversion
  pfn_vkCreateSamplerYcbcrConversionKHR = (PFN_vkCreateSamplerYcbcrConversionKHR)getDeviceProcAddr(device, "vkCreateSamplerYcbcrConversionKHR");
  pfn_vkDestroySamplerYcbcrConversionKHR = (PFN_vkDestroySamplerYcbcrConversionKHR)getDeviceProcAddr(device, "vkDestroySamplerYcbcrConversionKHR");
#endif /* VK_KHR_sampler_ycbcr_conversion */
#ifdef VK_KHR_shared_presentable_image
  pfn_vkGetSwapchainStatusKHR = (PFN_vkGetSwapchainStatusKHR)getDeviceProcAddr(device, "vkGetSwapchainStatusKHR");
#endif /* VK_KHR_shared_presentable_image */
#ifdef VK_KHR_synchronization2
  pfn_vkCmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)getDeviceProcAddr(device, "vkCmdPipelineBarrier2KHR");
  pfn_vkCmdResetEvent2KHR = (PFN_vkCmdResetEvent2KHR)getDeviceProcAddr(device, "vkCmdResetEvent2KHR");
  pfn_vkCmdSetEvent2KHR = (PFN_vkCmdSetEvent2KHR)getDeviceProcAddr(device, "vkCmdSetEvent2KHR");
  pfn_vkCmdWaitEvents2KHR = (PFN_vkCmdWaitEvents2KHR)getDeviceProcAddr(device, "vkCmdWaitEvents2KHR");
  pfn_vkCmdWriteBufferMarker2AMD = (PFN_vkCmdWriteBufferMarker2AMD)getDeviceProcAddr(device, "vkCmdWriteBufferMarker2AMD");
  pfn_vkCmdWriteTimestamp2KHR = (PFN_vkCmdWriteTimestamp2KHR)getDeviceProcAddr(device, "vkCmdWriteTimestamp2KHR");
  pfn_vkGetQueueCheckpointData2NV = (PFN_vkGetQueueCheckpointData2NV)getDeviceProcAddr(device, "vkGetQueueCheckpointData2NV");
  pfn_vkQueueSubmit2KHR = (PFN_vkQueueSubmit2KHR)getDeviceProcAddr(device, "vkQueueSubmit2KHR");
#endif /* VK_KHR_synchronization2 */
#ifdef VK_KHR_timeline_semaphore
  pfn_vkGetSemaphoreCounterValueKHR = (PFN_vkGetSemaphoreCounterValueKHR)getDeviceProcAddr(device, "vkGetSemaphoreCounterValueKHR");
  pfn_vkSignalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR)getDeviceProcAddr(device, "vkSignalSemaphoreKHR");
  pfn_vkWaitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR)getDeviceProcAddr(device, "vkWaitSemaphoresKHR");
#endif /* VK_KHR_timeline_semaphore */
#ifdef VK_KHR_video_decode_queue
  pfn_vkCmdDecodeVideoKHR = (PFN_vkCmdDecodeVideoKHR)getDeviceProcAddr(device, "vkCmdDecodeVideoKHR");
#endif /* VK_KHR_video_decode_queue */
#ifdef VK_KHR_video_encode_queue
  pfn_vkCmdEncodeVideoKHR = (PFN_vkCmdEncodeVideoKHR)getDeviceProcAddr(device, "vkCmdEncodeVideoKHR");
#endif /* VK_KHR_video_encode_queue */
#ifdef VK_KHR_video_queue
  pfn_vkBindVideoSessionMemoryKHR = (PFN_vkBindVideoSessionMemoryKHR)getDeviceProcAddr(device, "vkBindVideoSessionMemoryKHR");
  pfn_vkCmdBeginVideoCodingKHR = (PFN_vkCmdBeginVideoCodingKHR)getDeviceProcAddr(device, "vkCmdBeginVideoCodingKHR");
  pfn_vkCmdControlVideoCodingKHR = (PFN_vkCmdControlVideoCodingKHR)getDeviceProcAddr(device, "vkCmdControlVideoCodingKHR");
  pfn_vkCmdEndVideoCodingKHR = (PFN_vkCmdEndVideoCodingKHR)getDeviceProcAddr(device, "vkCmdEndVideoCodingKHR");
  pfn_vkCreateVideoSessionKHR = (PFN_vkCreateVideoSessionKHR)getDeviceProcAddr(device, "vkCreateVideoSessionKHR");
  pfn_vkCreateVideoSessionParametersKHR = (PFN_vkCreateVideoSessionParametersKHR)getDeviceProcAddr(device, "vkCreateVideoSessionParametersKHR");
  pfn_vkDestroyVideoSessionKHR = (PFN_vkDestroyVideoSessionKHR)getDeviceProcAddr(device, "vkDestroyVideoSessionKHR");
  pfn_vkDestroyVideoSessionParametersKHR = (PFN_vkDestroyVideoSessionParametersKHR)getDeviceProcAddr(device, "vkDestroyVideoSessionParametersKHR");
  pfn_vkGetPhysicalDeviceVideoCapabilitiesKHR = (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceVideoCapabilitiesKHR");
  pfn_vkGetPhysicalDeviceVideoFormatPropertiesKHR = (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceVideoFormatPropertiesKHR");
  pfn_vkGetVideoSessionMemoryRequirementsKHR = (PFN_vkGetVideoSessionMemoryRequirementsKHR)getDeviceProcAddr(device, "vkGetVideoSessionMemoryRequirementsKHR");
  pfn_vkUpdateVideoSessionParametersKHR = (PFN_vkUpdateVideoSessionParametersKHR)getDeviceProcAddr(device, "vkUpdateVideoSessionParametersKHR");
#endif /* VK_KHR_video_queue */
#ifdef VK_MVK_ios_surface
  pfn_vkCreateIOSSurfaceMVK = (PFN_vkCreateIOSSurfaceMVK)getInstanceProcAddr(instance, "vkCreateIOSSurfaceMVK");
#endif /* VK_MVK_ios_surface */
#ifdef VK_MVK_macos_surface
  pfn_vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)getInstanceProcAddr(instance, "vkCreateMacOSSurfaceMVK");
#endif /* VK_MVK_macos_surface */
#ifdef VK_NN_vi_surface
  pfn_vkCreateViSurfaceNN = (PFN_vkCreateViSurfaceNN)getInstanceProcAddr(instance, "vkCreateViSurfaceNN");
#endif /* VK_NN_vi_surface */
#ifdef VK_NVX_image_view_handle
  pfn_vkGetImageViewAddressNVX = (PFN_vkGetImageViewAddressNVX)getDeviceProcAddr(device, "vkGetImageViewAddressNVX");
  pfn_vkGetImageViewHandleNVX = (PFN_vkGetImageViewHandleNVX)getDeviceProcAddr(device, "vkGetImageViewHandleNVX");
#endif /* VK_NVX_image_view_handle */
#ifdef VK_NV_acquire_winrt_display
  pfn_vkAcquireWinrtDisplayNV = (PFN_vkAcquireWinrtDisplayNV)getInstanceProcAddr(instance, "vkAcquireWinrtDisplayNV");
  pfn_vkGetWinrtDisplayNV = (PFN_vkGetWinrtDisplayNV)getInstanceProcAddr(instance, "vkGetWinrtDisplayNV");
#endif /* VK_NV_acquire_winrt_display */
#ifdef VK_NV_clip_space_w_scaling
  pfn_vkCmdSetViewportWScalingNV = (PFN_vkCmdSetViewportWScalingNV)getDeviceProcAddr(device, "vkCmdSetViewportWScalingNV");
#endif /* VK_NV_clip_space_w_scaling */
#ifdef VK_NV_cooperative_matrix
  pfn_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV = (PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV)getInstanceProcAddr(instance, "vkGetPhysicalDeviceCooperativeMatrixPropertiesNV");
#endif /* VK_NV_cooperative_matrix */
#ifdef VK_NV_coverage_reduction_mode
  pfn_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV = (PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV");
#endif /* VK_NV_coverage_reduction_mode */
#ifdef VK_NV_device_diagnostic_checkpoints
  pfn_vkCmdSetCheckpointNV = (PFN_vkCmdSetCheckpointNV)getDeviceProcAddr(device, "vkCmdSetCheckpointNV");
  pfn_vkGetQueueCheckpointDataNV = (PFN_vkGetQueueCheckpointDataNV)getDeviceProcAddr(device, "vkGetQueueCheckpointDataNV");
#endif /* VK_NV_device_diagnostic_checkpoints */
#ifdef VK_NV_device_generated_commands
  pfn_vkCmdBindPipelineShaderGroupNV = (PFN_vkCmdBindPipelineShaderGroupNV)getDeviceProcAddr(device, "vkCmdBindPipelineShaderGroupNV");
  pfn_vkCmdExecuteGeneratedCommandsNV = (PFN_vkCmdExecuteGeneratedCommandsNV)getDeviceProcAddr(device, "vkCmdExecuteGeneratedCommandsNV");
  pfn_vkCmdPreprocessGeneratedCommandsNV = (PFN_vkCmdPreprocessGeneratedCommandsNV)getDeviceProcAddr(device, "vkCmdPreprocessGeneratedCommandsNV");
  pfn_vkCreateIndirectCommandsLayoutNV = (PFN_vkCreateIndirectCommandsLayoutNV)getDeviceProcAddr(device, "vkCreateIndirectCommandsLayoutNV");
  pfn_vkDestroyIndirectCommandsLayoutNV = (PFN_vkDestroyIndirectCommandsLayoutNV)getDeviceProcAddr(device, "vkDestroyIndirectCommandsLayoutNV");
  pfn_vkGetGeneratedCommandsMemoryRequirementsNV = (PFN_vkGetGeneratedCommandsMemoryRequirementsNV)getDeviceProcAddr(device, "vkGetGeneratedCommandsMemoryRequirementsNV");
#endif /* VK_NV_device_generated_commands */
#ifdef VK_NV_external_memory_capabilities
  pfn_vkGetPhysicalDeviceExternalImageFormatPropertiesNV = (PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV)getInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalImageFormatPropertiesNV");
#endif /* VK_NV_external_memory_capabilities */
#ifdef VK_NV_external_memory_win32
  pfn_vkGetMemoryWin32HandleNV = (PFN_vkGetMemoryWin32HandleNV)getDeviceProcAddr(device, "vkGetMemoryWin32HandleNV");
#endif /* VK_NV_external_memory_win32 */
#ifdef VK_NV_fragment_shading_rate_enums
  pfn_vkCmdSetFragmentShadingRateEnumNV = (PFN_vkCmdSetFragmentShadingRateEnumNV)getDeviceProcAddr(device, "vkCmdSetFragmentShadingRateEnumNV");
#endif /* VK_NV_fragment_shading_rate_enums */
#ifdef VK_NV_mesh_shader
  pfn_vkCmdDrawMeshTasksIndirectCountNV = (PFN_vkCmdDrawMeshTasksIndirectCountNV)getDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectCountNV");
  pfn_vkCmdDrawMeshTasksIndirectNV = (PFN_vkCmdDrawMeshTasksIndirectNV)getDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectNV");
  pfn_vkCmdDrawMeshTasksNV = (PFN_vkCmdDrawMeshTasksNV)getDeviceProcAddr(device, "vkCmdDrawMeshTasksNV");
#endif /* VK_NV_mesh_shader */
#ifdef VK_NV_ray_tracing
  pfn_vkBindAccelerationStructureMemoryNV = (PFN_vkBindAccelerationStructureMemoryNV)getDeviceProcAddr(device, "vkBindAccelerationStructureMemoryNV");
  pfn_vkCmdBuildAccelerationStructureNV = (PFN_vkCmdBuildAccelerationStructureNV)getDeviceProcAddr(device, "vkCmdBuildAccelerationStructureNV");
  pfn_vkCmdCopyAccelerationStructureNV = (PFN_vkCmdCopyAccelerationStructureNV)getDeviceProcAddr(device, "vkCmdCopyAccelerationStructureNV");
  pfn_vkCmdTraceRaysNV = (PFN_vkCmdTraceRaysNV)getDeviceProcAddr(device, "vkCmdTraceRaysNV");
  pfn_vkCmdWriteAccelerationStructuresPropertiesNV = (PFN_vkCmdWriteAccelerationStructuresPropertiesNV)getDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesNV");
  pfn_vkCompileDeferredNV = (PFN_vkCompileDeferredNV)getDeviceProcAddr(device, "vkCompileDeferredNV");
  pfn_vkCreateAccelerationStructureNV = (PFN_vkCreateAccelerationStructureNV)getDeviceProcAddr(device, "vkCreateAccelerationStructureNV");
  pfn_vkCreateRayTracingPipelinesNV = (PFN_vkCreateRayTracingPipelinesNV)getDeviceProcAddr(device, "vkCreateRayTracingPipelinesNV");
  pfn_vkDestroyAccelerationStructureNV = (PFN_vkDestroyAccelerationStructureNV)getDeviceProcAddr(device, "vkDestroyAccelerationStructureNV");
  pfn_vkGetAccelerationStructureHandleNV = (PFN_vkGetAccelerationStructureHandleNV)getDeviceProcAddr(device, "vkGetAccelerationStructureHandleNV");
  pfn_vkGetAccelerationStructureMemoryRequirementsNV = (PFN_vkGetAccelerationStructureMemoryRequirementsNV)getDeviceProcAddr(device, "vkGetAccelerationStructureMemoryRequirementsNV");
  pfn_vkGetRayTracingShaderGroupHandlesNV = (PFN_vkGetRayTracingShaderGroupHandlesNV)getDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesNV");
#endif /* VK_NV_ray_tracing */
#ifdef VK_NV_scissor_exclusive
  pfn_vkCmdSetExclusiveScissorNV = (PFN_vkCmdSetExclusiveScissorNV)getDeviceProcAddr(device, "vkCmdSetExclusiveScissorNV");
#endif /* VK_NV_scissor_exclusive */
#ifdef VK_NV_shading_rate_image
  pfn_vkCmdBindShadingRateImageNV = (PFN_vkCmdBindShadingRateImageNV)getDeviceProcAddr(device, "vkCmdBindShadingRateImageNV");
  pfn_vkCmdSetCoarseSampleOrderNV = (PFN_vkCmdSetCoarseSampleOrderNV)getDeviceProcAddr(device, "vkCmdSetCoarseSampleOrderNV");
  pfn_vkCmdSetViewportShadingRatePaletteNV = (PFN_vkCmdSetViewportShadingRatePaletteNV)getDeviceProcAddr(device, "vkCmdSetViewportShadingRatePaletteNV");
#endif /* VK_NV_shading_rate_image */
#ifdef VK_QNX_screen_surface
  pfn_vkCreateScreenSurfaceQNX = (PFN_vkCreateScreenSurfaceQNX)getInstanceProcAddr(instance, "vkCreateScreenSurfaceQNX");
  pfn_vkGetPhysicalDeviceScreenPresentationSupportQNX = (PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX)getInstanceProcAddr(instance, "vkGetPhysicalDeviceScreenPresentationSupportQNX");
#endif /* VK_QNX_screen_surface */
/* NVVK_GENERATE_LOAD_PROC */
}


/* clang-format on */

