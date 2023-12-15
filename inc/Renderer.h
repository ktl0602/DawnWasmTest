#pragma once

#include <string>

#include <webgpu/webgpu.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_wgpu.h"
#include "defines.h"

#include <GLFW/glfw3.h>

class Renderer
{
private:
	
	/**
	 * Current rotation angle (in degrees, updated per frame).
	 */
	float rotDeg = 0.0f;
	int dir = 1;
	bool _showImGui = true;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	ImVec4 vertex1 = ImVec4(1.0f, 0.0f, 0.00f, 1.00f);
	ImVec4 vertex2 = ImVec4(0.0f, 1.0f, 0.00f, 1.00f);
	ImVec4 vertex3 = ImVec4(0.0f, 0.0f, 1.00f, 1.00f);
	float speed = 0.0f;

	WGPURenderPipeline pipeline;
	WGPUBuffer vertBuf; // vertex buffer with triangle position and colours
	WGPUBuffer indxBuf; // index buffer
	WGPUBuffer uRotBuf; // uniform buffer (containing the rotation angle)
	WGPUBindGroup bindGroup;

	WGPUDevice device;
	WGPUQueue queue;
	WGPUSwapChain swapchain;

	std::string triangle_vert_wgsl;
	std::string triangle_frag_wgsl;

	void setupShaders();

public:
	Renderer();
	~Renderer();

	inline void setDevice(WGPUDevice device) { this->device = device; }
	inline void setQueue(WGPUQueue queue) { this->queue = queue; }
	inline void setSwapChain(WGPUSwapChain swapchain) { this->swapchain = swapchain; }
	
	inline WGPUDevice getDevice() { return device; }
	inline WGPUQueue getQueue() { return queue; }
	inline WGPUSwapChain getSwapChain() { return swapchain; }

	void setupImGui(GLFWwindow* window);
	void renderImGui();	
	void showImGui(bool state);
	void setColor(float r, float g, float b);

	WGPUSwapChain resize(int width, int height);
	bool render(double time);
	
	void mouseClicked(int button, int action, int x, int y);
	void keyPressed(int keyCode, int action);
	
	WGPUShaderModule createShader(const char* const code, const char* label = nullptr);
	WGPUBuffer createBuffer(const void* data, size_t size, WGPUBufferUsage usage);
	void createPipelineAndBuffers();	
};

