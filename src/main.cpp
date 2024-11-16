#include <iostream>
#include <init.h>

void record_command_buffer(std::shared_ptr<VkContext> context, int image_index, int frame, int vbuffer_id, int obuffer_id) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(context->command_buffers[frame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = context->graphics_pipeline.render_pass;
    renderPassInfo.framebuffer = context->swapchain_framebuffers[image_index];

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = context->swapchain_extent;

    VkClearValue clearColor = {{{1.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(context->command_buffers[frame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(context->command_buffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, context->graphics_pipeline.graphics_pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(context->swapchain_extent.width);
    viewport.height = static_cast<float>(context->swapchain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(context->command_buffers[frame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = context->swapchain_extent;
    vkCmdSetScissor(context->command_buffers[frame], 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {context->vertex_buffers[vbuffer_id].buffer, context->object_position_buffers[obuffer_id].buffer};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(context->command_buffers[frame], 0, 2, vertexBuffers, offsets);

    vkCmdDraw(context->command_buffers[frame], context->vertex_buffers[vbuffer_id].length, context->object_position_buffers[vbuffer_id].length, 0, 0);

    vkCmdEndRenderPass(context->command_buffers[frame]);

    if (VkResult result = vkEndCommandBuffer(context->command_buffers[frame]); result != VK_SUCCESS) {
        throw std::runtime_error("Could not record command buffer: " + std::string(string_VkResult(result)));
    }
}

bool framebuffer_resized_flag = false;
int current_frame = 0;

void draw_frame(std::shared_ptr<VkContext> context, int vbuffer_id, int obuffer_id) {
    vkWaitForFences(context->logical_device, 1, &context->command_buffer_fences[current_frame], VK_TRUE, UINT64_MAX);

    // Get the next image;
    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(context->logical_device, context->swapchain, UINT64_MAX, context->image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        context->rebuild_swapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Could not aquire swapchain image: " + std::string(string_VkResult(result)));
    }

    // Only reset command_buffer fence if we are sure that it will be submitted on this frame.
    vkResetFences(context->logical_device, 1, &context->command_buffer_fences[current_frame]);

    // Record command buffer.
    vkResetCommandBuffer(context->command_buffers[current_frame], 0);
    record_command_buffer(context, image_index, current_frame, vbuffer_id, obuffer_id);

    // Submit graphics queue.
    VkSubmitInfo info {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &context->command_buffers[current_frame];
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &context->image_available_semaphores[current_frame];
    VkPipelineStageFlags stages_to_wait_on_semaphores = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    info.pWaitDstStageMask = &stages_to_wait_on_semaphores;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &context->image_done_rendering_semaphores[current_frame];
    vkQueueSubmit(context->get_graphics_queue(), 1, &info, context->command_buffer_fences[current_frame]);

    // Submit presentation queue.
    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pImageIndices = &image_index;
    presentInfo.pSwapchains = &context->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &context->image_done_rendering_semaphores[current_frame];
    result = vkQueuePresentKHR(context->get_presentation_queue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized_flag) {
        context->rebuild_swapchain();
        framebuffer_resized_flag = false;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not present swapchain image: " + std::string(string_VkResult(result)));
    }

    current_frame = (current_frame + 1) % context->MAX_FRAMES_IN_FLIGHT;
}

int main() {
    std::shared_ptr<VkContext> vk_context = std::make_shared<VkContext>();

    std::vector<Vertex> vertex_data = {
        Vertex(0, 0, 0, 255, 0), Vertex(10, 10, 0, 255, 0), Vertex(0, 10, 0, 255, 0),
        Vertex(0, 0, 0, 255, 0), Vertex(10, 0, 0, 255, 0), Vertex(10, 10, 0, 255, 0),
    };

    std::vector<ObjectData> object_data = {
        ObjectData(0, 0), ObjectData(20, 20),
        ObjectData(40, 40), ObjectData(60, 60),
        ObjectData(80, 80),
    };

    int vertex_buffer_id = vk_context->create_vertex_buffer(vertex_data);
    int object_buffer_id = vk_context->create_object_position_buffer(object_data);

    bool running = true;

    while(running) {
        SDL_UpdateWindowSurface(vk_context->window);
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_WINDOWEVENT:
                    switch(event.window.event) {
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                            framebuffer_resized_flag = true;
                            break;
                    }
                    break;
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }
        draw_frame(vk_context, vertex_buffer_id, object_buffer_id);
    }
    
    return 0;
}