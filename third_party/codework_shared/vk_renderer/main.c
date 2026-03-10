// main.c
#include <stdio.h>
#include <vulkan/vulkan.h>

int main(void) {
    uint32_t count = 0;
    VkResult r = vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    if (r != VK_SUCCESS) { fprintf(stderr, "Vulkan not OK (%d)\n", r); return 1; }
    printf("Vulkan is alive. Extensions available: %u\n", count);
    return 0;
}
