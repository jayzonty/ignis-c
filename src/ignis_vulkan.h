#ifndef IGNIS_VULKAN_H
#define IGNIS_VULKAN_H

#include <vulkan/vulkan_core.h>

#include <stdbool.h>

/**
 * @brief Creates a buffer and allocates the necessary memory to it.
 * @param[in] physicalDevice Physical device
 * @param[in] logicalDevice Logical device
 * @param[in] bufferSize Buffer size in bytes
 * @param[in] usageFlags Flags indicating how this buffer will be used
 * @param[in] memoryPropertyFlags Flags indicating the properties of the memory to be allocated to this buffer
 * @param[out] buffer Pointer to the buffer handle
 * @param[out] memory Pointer to the memory handle
 * @return True if the buffer creation was successful
 */
bool CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer *buffer, VkDeviceMemory *memory);

/**
 * @brief Creates an image and allocates the necessary memory to it.
 * @param[in] physicalDevice Physical device
 * @param[in] logicalDevice Logical device
 * @param[in] width Image width
 * @param[in] height Image height
 * @param[in] format Image format
 * @param[in] tiling Image tiling
 * @param[in] usageFlags Flags indicating how this image will be used
 * @param[in] memoryPropertyFlags Flags indicating the properties of the memory to be allocated to this image
 * @param[out] image Pointer to the image handle
 * @param[out] memory Pointer to the memory handle
 * @return True if the image creation was successful
 */
bool CreateImage2D(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, const uint32_t width, const uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryProperties, VkImage *image, VkDeviceMemory *memory);

/**
 * @brief Creates an image view for the specified image.
 * @param[in] logicalDevice Logical device
 * @param[in] image Image to create the image view for
 * @param[in] format Image read format
 * @param[in] imageAspectFlags Flags indicating how the image is supposed to be used
 * @param[in] imageView Pointer to the image view handle
 * @return True if the image view creation was successful
 */
bool CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageView *imageView);

#endif // IGNIS_VULKAN_H
