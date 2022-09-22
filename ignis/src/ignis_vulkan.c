#include "ignis_vulkan.h"

#include <vulkan/vulkan_core.h>

#include <stdio.h>

/**
 * @brief Creates a buffer and allocates the necessary memory to it.
 * @param[in] physicalDevice Physical device
 * @param[in] logicalDevice Logical device
 * @param[in] bufferSize Buffer size in bytes
 * @param[in] usageFlags Flags indicating what this buffer will be used for
 * @param[in] memoryPropertyFlags Flags indicating the properties of the memory to be allocated to this buffer
 * @param[out] buffer Pointer to the buffer handle
 * @param[out] memory Pointer to the memory handle
 * @return True if the buffer creation was successful
 */
bool CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize bufferSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer *buffer, VkDeviceMemory *memory)
{
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = bufferSize;
    vertexBufferInfo.usage = usageFlags;
    vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: Expose as a parameter?
    if (vkCreateBuffer(logicalDevice, &vertexBufferInfo, NULL, buffer) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create buffer!\n");
        return false;
    }

    // --- Allocate memory for the buffer ---

    // Check memory requirements for the buffer
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memoryRequirements);

    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

    VkMemoryAllocateInfo memoryAllocateInfo = {};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;

    uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;
    uint32_t memoryTypeIndex;
    bool memoryTypeIndexFound = false;
    // Go through each bit in the memoryTypeBits and check if the bit is set
    // and the memory type at that particular index supports the required properties.
    // For example, we check if bit 5 in the memoryTypeBits is set (meaning it is supported) and
    // if the memory type at index 5 supports all the flags in the requiredProperties flag
    for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((memoryTypeBits & (1 << i)) 
                && ((deviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags))
        {
            memoryTypeIndex = i;
            memoryTypeIndexFound = true;
            break;
        }
    }
    if (!memoryTypeIndexFound)
    {
        printf("[Engine] Failed to find a suitable memory type for the buffer!\n");
        return false;
    }
    memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(logicalDevice, &memoryAllocateInfo, NULL, memory) != VK_SUCCESS)
    {
        printf("[Engine] Failed to allocate memory for the default render batch's vertex buffer!\n");
        return false;
    }

    // --- Bind the buffer to the memory ---
    vkBindBufferMemory(logicalDevice, *buffer, *memory, 0);

    return true;
}

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
bool CreateImage2D(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, const uint32_t width, const uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryProperties, VkImage *image, VkDeviceMemory *memory)
{
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usageFlags;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(logicalDevice, &imageInfo, NULL, image) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create image!\n");
        return false;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(logicalDevice, *image, &memoryRequirements);

    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

    uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;
    uint32_t memoryTypeIndex;
    bool memoryTypeIndexFound = false;
    // Go through each bit in the memoryTypeBits and check if the bit is set
    // and the memory type at that particular index supports the required properties.
    // For example, we check if bit 5 in the memoryTypeBits is set (meaning it is supported) and
    // if the memory type at index 5 supports all the flags in the requiredProperties flag
    for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((memoryTypeBits & (1 << i)) 
                && ((deviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties))
        {
            memoryTypeIndex = i;
            memoryTypeIndexFound = true;
            break;
        }
    }

    if (!memoryTypeIndexFound)
    {
        printf("[Engine] Failed to find a suitable memory type for the buffer!\n");
        vkDestroyImage(logicalDevice, *image, NULL);
        *image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(logicalDevice, &allocInfo, NULL, memory) != VK_SUCCESS)
    {
        printf("[Engine] Failed to allocate memory for the image!\n");
        vkDestroyImage(logicalDevice, *image, NULL);
        *image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(logicalDevice, *image, *memory, 0);

    return true;
}

/**
 * @brief Creates an image view for the specified image.
 * @param[in] logicalDevice Logical device
 * @param[in] image Image to create the image view for
 * @param[in] format Image read format
 * @param[in] imageAspectFlags Flags indicating how the image is supposed to be used
 * @param[in] imageView Pointer to the image view handle
 * @return True if the image view creation was successful
 */
bool CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageView *imageView)
{
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    imageViewCreateInfo.subresourceRange.aspectMask = imageAspectFlags;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(logicalDevice, &imageViewCreateInfo, NULL, imageView);

    return true;
}
