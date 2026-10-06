#include "vk_renderer.h"
#include <assert.h>
#include <stdio.h>
static VkResult acquire_result,record_result;
static int resets,submits,signaled=1;
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice d,uint32_t n,const VkFence *f,VkBool32 all,uint64_t timeout){(void)d;(void)n;(void)f;(void)all;(void)timeout;assert(signaled);return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice d,uint32_t n,const VkFence *f){(void)d;(void)n;(void)f;assert(signaled);signaled=0;++resets;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice d,VkSwapchainKHR s,uint64_t t,VkSemaphore a,VkFence f,uint32_t *i){(void)d;(void)s;(void)t;(void)a;(void)f;*i=0;return acquire_result;}
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer c,VkCommandBufferResetFlags f){(void)c;(void)f;return record_result;}
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer c,const VkCommandBufferBeginInfo *i){(void)c;(void)i;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer c){(void)c;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue q,uint32_t n,const VkSubmitInfo *s,VkFence f){(void)q;(void)n;(void)s;(void)f;assert(!signaled);signaled=1;++submits;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue q,const VkPresentInfoKHR *i){(void)q;(void)i;return VK_SUCCESS;}
int main(void){
 VkRenderer r={0};VkRendererDevice device={0};VkRendererFrameState frame={0};VkSemaphore finished[1]={0};uint32_t index=99;VkCommandBuffer command=0;
 r.context.device=&device;r.frames=&frame;r.frame_count=1;r.command_pool.render_finished=finished;r.command_pool.render_finished_count=1;
 acquire_result=VK_ERROR_OUT_OF_DATE_KHR;
 assert(vk_renderer_commands_begin_frame(&r,&index,&command)==VK_ERROR_OUT_OF_DATE_KHR && resets==0 && signaled);
 acquire_result=VK_SUCCESS;record_result=VK_ERROR_DEVICE_LOST;
 assert(vk_renderer_commands_begin_frame(&r,&index,&command)==VK_ERROR_DEVICE_LOST && resets==0 && signaled);
 record_result=VK_SUCCESS;acquire_result=VK_SUBOPTIMAL_KHR;
 assert(vk_renderer_commands_begin_frame(&r,&index,&command)==VK_SUCCESS && resets==0 && signaled);
 assert(vk_renderer_commands_end_frame(&r,index,command)==VK_SUCCESS && resets==1 && submits==1 && signaled);
 acquire_result=VK_SUCCESS;
 assert(vk_renderer_commands_begin_frame(&r,&index,&command)==VK_SUCCESS);
 assert(vk_renderer_commands_end_frame(&r,index,command)==VK_SUCCESS && resets==2 && submits==2);
 puts("frame submission: out-of-date and recording failures leave fence signaled; suboptimal acquired image submits; next frame progresses");
}
