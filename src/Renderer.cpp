#include "Renderer.h"
#include <cstdio>

Renderer::Renderer()
{
	this->setupShaders();
}

Renderer::~Renderer()
{
#ifndef __EMSCRIPTEN__
	wgpuBindGroupRelease(bindGroup);
	wgpuBufferRelease(uRotBuf);
	wgpuBufferRelease(indxBuf);
	wgpuBufferRelease(vertBuf);
	wgpuRenderPipelineRelease(pipeline);
	wgpuSwapChainRelease(swapchain);
	wgpuQueueRelease(queue);
	wgpuDeviceRelease(device);
#endif
}

void Renderer::setupShaders()
{
	triangle_vert_wgsl = R"(
	const PI : f32 = 3.141592653589793;
	fn radians(degs : f32) -> f32 {
		return (degs * PI) / 180.0;
	}
	[[block]] struct Rotation {
		[[offset(0)]] degs : f32;
	};
	[[set(0), binding(0)]] var<uniform> uRot : Rotation;
	[[location(0)]] var<in>  aPos : vec2<f32>;
	[[location(1)]] var<in>  aCol : vec3<f32>;	
	[[location(0)]] var<out> vCol : vec3<f32>;
	[[builtin(position)]] var<out> Position : vec4<f32>;
	[[stage(vertex)]] fn main() -> void {
		var rads : f32 = radians(uRot.degs);
		var cosA : f32 = cos(rads);
		var sinA : f32 = sin(rads);
		var rot : mat3x3<f32> = mat3x3<f32>(
			vec3<f32>( cosA, sinA, 0.0),
			vec3<f32>(-sinA, cosA, 0.0),
			vec3<f32>( 0.0,  0.0,  1.0));
		Position = vec4<f32>(rot * vec3<f32>(aPos, 1.0), 1.0);
		vCol = aCol;
	}
)";

	triangle_frag_wgsl = R"(
	[[location(0)]] var<in> vCol : vec3<f32>;
	[[location(0)]] var<out> fragColor : vec4<f32>;
	[[stage(fragment)]] fn main() -> void {
		fragColor = vec4<f32>(vCol, 1.0);
	}
)";
}

/**
 * Helper to create a shader from WGSL source.
 *
 * \param[in] code WGSL shader source
 * \param[in] label optional shader name
 */
WGPUShaderModule Renderer::createShader(const char* const code, const char* label) {
	WGPUShaderModuleWGSLDescriptor wgsl = {};
	wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	wgsl.source = code;
	WGPUShaderModuleDescriptor desc = {};
	desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl);
	desc.label = label;
	return wgpuDeviceCreateShaderModule(device, &desc);
}

/**
 * Helper to create a buffer.
 *
 * \param[in] data pointer to the start of the raw data
 * \param[in] size number of bytes in \a data
 * \param[in] usage type of buffer
 */
WGPUBuffer Renderer::createBuffer(const void* data, size_t size, WGPUBufferUsage usage) {
	WGPUBufferDescriptor desc = {};
	desc.usage = WGPUBufferUsage_CopyDst | usage;
	desc.size = size;	
	WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
	wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
	return buffer;
}

/**
 * Bare minimum pipeline to draw a triangle using the above shaders.
 */
void Renderer::createPipelineAndBuffers() {
	// compile shaders
	// NOTE: these are now the WGSL shaders (tested with Dawn and Chrome Canary)
	WGPUShaderModule vertMod = createShader(triangle_vert_wgsl.c_str());
	WGPUShaderModule fragMod = createShader(triangle_frag_wgsl.c_str());

	// bind group layout (used by both the pipeline layout and uniform bind group, released at the end of this function)
	WGPUBindGroupLayoutEntry bglEntry = {};
	bglEntry.binding = 0;
	bglEntry.visibility = WGPUShaderStage_Vertex;
	bglEntry.type = WGPUBindingType_UniformBuffer;

	WGPUBindGroupLayoutDescriptor bglDesc = {};
	bglDesc.entryCount = 1;
	bglDesc.entries = &bglEntry;
	WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

	// pipeline layout (used by the render pipeline, released after its creation)
	WGPUPipelineLayoutDescriptor layoutDesc = {};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = &bindGroupLayout;
	WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

	// begin pipeline set-up
	WGPURenderPipelineDescriptor desc = {};

	desc.layout = pipelineLayout;

	desc.vertexStage.module = vertMod;
	desc.vertexStage.entryPoint = "main";

	WGPUProgrammableStageDescriptor fragStage = {};
	fragStage.module = fragMod;
	fragStage.entryPoint = "main";
	desc.fragmentStage = &fragStage;

	// describe buffer layouts
	WGPUVertexAttributeDescriptor vertAttrs[2] = {};
	vertAttrs[0].format = WGPUVertexFormat_Float2;
	vertAttrs[0].offset = 0;
	vertAttrs[0].shaderLocation = 0;
	vertAttrs[1].format = WGPUVertexFormat_Float3;
	vertAttrs[1].offset = 2 * sizeof(float);
	vertAttrs[1].shaderLocation = 1;
	WGPUVertexBufferLayoutDescriptor vertDesc = {};
	vertDesc.arrayStride = 5 * sizeof(float);
	vertDesc.attributeCount = 2;
	vertDesc.attributes = vertAttrs;
	WGPUVertexStateDescriptor vertState = {};
	vertState.vertexBufferCount = 1;
	vertState.vertexBuffers = &vertDesc;

	desc.vertexState = &vertState;
	desc.primitiveTopology = WGPUPrimitiveTopology_TriangleList;

	desc.sampleCount = 1;

	// describe blend
	WGPUBlendDescriptor blendDesc = {};
	blendDesc.operation = WGPUBlendOperation_Add;
	blendDesc.srcFactor = WGPUBlendFactor_SrcAlpha;
	blendDesc.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	WGPUColorStateDescriptor colorDesc = {};
	colorDesc.format = WGPUTextureFormat_RGBA8Unorm; //webgpu::getSwapChainFormat(device);
	colorDesc.alphaBlend = blendDesc;
	colorDesc.colorBlend = blendDesc;
	colorDesc.writeMask = WGPUColorWriteMask_All;

	desc.colorStateCount = 1;
	desc.colorStates = &colorDesc;

	desc.sampleMask = 0xFFFFFFFF; // <-- Note: this currently causes Emscripten to fail (sampleMask ends up as -1, which trips an assert)

	pipeline = wgpuDeviceCreateRenderPipeline(device, &desc);

	// partial clean-up (just move to the end, no?)
	wgpuPipelineLayoutRelease(pipelineLayout);

	wgpuShaderModuleRelease(fragMod);
	wgpuShaderModuleRelease(vertMod);

	// create the buffers (x, y, r, g, b)
	float const vertData[] = {
		-0.8f, -0.8f, 0.0f, 0.0f, 1.0f, // BL
		 0.8f, -0.8f, 0.0f, 1.0f, 0.0f, // BR
		-0.0f,  0.8f, 1.0f, 0.0f, 0.0f, // top
	};
	uint16_t const indxData[] = {
		0, 1, 2,
		0 // padding (better way of doing this?)
	};	
	vertBuf = createBuffer(vertData, sizeof(vertData), WGPUBufferUsage_Vertex);
	indxBuf = createBuffer(indxData, sizeof(indxData), WGPUBufferUsage_Index);

	// create the uniform bind group (note 'rotDeg' is copied here, not bound in any way)
	uRotBuf = createBuffer(&rotDeg, sizeof(rotDeg), WGPUBufferUsage_Uniform);

	WGPUBindGroupEntry bgEntry = {};
	bgEntry.binding = 0;
	bgEntry.buffer = uRotBuf;
	bgEntry.offset = 0;
	bgEntry.size = sizeof(rotDeg);

	WGPUBindGroupDescriptor bgDesc = {};
	bgDesc.layout = bindGroupLayout;
	bgDesc.entryCount = 1;
	bgDesc.entries = &bgEntry;

	bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

	// last bit of clean-up
	wgpuBindGroupLayoutRelease(bindGroupLayout);
}


/**
 * ImGui setup function that is called only once.
 * 
 * NOTICE: This is the only place where GLFW is needed in the Renderer. 
 * If you do not want to use ImGui, you can get rid of GLFW too.
 */
void Renderer::setupImGui(GLFWwindow* window) {
	
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
	// You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
	io.IniFilename = NULL;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer backends				
	ImGui_ImplGlfw_InitForOther(window, true);
	ImGui_ImplWGPU_Init(device, 3, WGPUTextureFormat_RGBA8Unorm);

	// Load Fonts
		// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
		// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
		// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
		// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
		// - Read 'docs/FONTS.md' for more instructions and details.
		// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
		// - Emscripten allows preloading a file or folder to be accessible at runtime. See Makefile for details.
		//io.Fonts->AddFontDefault();
#ifndef IMGUI_DISABLE_FILE_FUNCTIONS
				//io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);
				//io.Fonts->AddFontFromFileTTF("fonts/Cousine-Regular.ttf", 15.0f);
				//io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf", 16.0f);
				//io.Fonts->AddFontFromFileTTF("fonts/ProggyTiny.ttf", 10.0f);
				//ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
				//IM_ASSERT(font != NULL);
#endif
}

/**
 * Rendering of ImGui. Various menu definitions should be here.
 */
void Renderer::renderImGui()
{
	if (!this->_showImGui) {
		return;
	}

	// Start the Dear ImGui frame
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
	ImGui::Begin("WebRenderer");                                // Create a window called "Hello, world!" and append into it.

	ImGui::SliderFloat("Rotation speed", &speed, 0.0f, 2.0f);                  // Edit 1 float using a slider from 0.0f to 1.0f
	ImGui::ColorEdit3("Clear color", (float*)&clear_color);       // Edit 3 floats representing a color
	ImGui::ColorEdit3("Vertex 1", (float*)&vertex1);       // Edit 3 floats representing a color
	ImGui::ColorEdit3("Vertex 2", (float*)&vertex2);       // Edit 3 floats representing a color
	ImGui::ColorEdit3("Vertex 3", (float*)&vertex3);       // Edit 3 floats representing a color

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();

	// Rendering
	ImGui::Render();
}

/**
 * Handling function when the rendering target is resized.
 */
WGPUSwapChain Renderer::resize(int width, int height)
{
#ifdef __EMSCRIPTEN__
	/*ImGui_ImplWGPU_InvalidateDeviceObjects();

	auto wgpu_swap_chain = this->swapchain;
	if (wgpu_swap_chain)
		wgpuSwapChainRelease(wgpu_swap_chain);

	WGPUSwapChainDescriptor swap_chain_desc = {};
	swap_chain_desc.usage = WGPUTextureUsage_OutputAttachment;
	swap_chain_desc.format = WGPUTextureFormat_RGBA8Unorm;
	swap_chain_desc.width = width;
	swap_chain_desc.height = height;
	swap_chain_desc.presentMode = WGPUPresentMode_Fifo;
	this->swapchain = wgpuDeviceCreateSwapChain(device, NULL, &swap_chain_desc);
	
	ImGui_ImplWGPU_CreateDeviceObjects();*/
#endif

	return this->swapchain;
}

/**
 * Main rendering loop.
 */
bool Renderer::render(double /*time*/) 
{	
	// ImGui rendering 
	this->renderImGui();	
	
	// rendering of a triangle
	WGPUTextureView backBufView = wgpuSwapChainGetCurrentTextureView(swapchain);			// create textureView;

	WGPURenderPassColorAttachmentDescriptor colorDesc = {};
	colorDesc.attachment = backBufView;
	colorDesc.loadOp = WGPULoadOp_Clear;
	colorDesc.storeOp = WGPUStoreOp_Store;
	colorDesc.clearColor.r = clear_color.x;
	colorDesc.clearColor.g = clear_color.y;
	colorDesc.clearColor.b = clear_color.z;
	colorDesc.clearColor.a = clear_color.w;

	WGPURenderPassDescriptor renderPass = {};
	renderPass.colorAttachmentCount = 1;
	renderPass.colorAttachments = &colorDesc;
	//renderPass.depthStencilAttachment = NULL;

	// create encoder
	WGPUCommandEncoderDescriptor enc_desc = {};
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

	WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPass);	// create pass
	
	// update the rotation
	rotDeg += 0.1f * speed * dir;
	wgpuQueueWriteBuffer(queue, uRotBuf, 0, &rotDeg, sizeof(rotDeg));

	// update the colors
	float const vertData[] = {
		-0.8f, -0.8f, vertex1.x, vertex1.y, vertex1.z, // BL
		 0.8f, -0.8f, vertex2.x, vertex2.y, vertex2.z, // BR
		-0.0f,  0.8f, vertex3.x, vertex3.y, vertex3.z, // top
	};
	wgpuQueueWriteBuffer(queue, vertBuf, 0, vertData, sizeof(vertData));

	// draw the triangle (comment these five lines to simply clear the screen)
	wgpuRenderPassEncoderSetPipeline(pass, pipeline);
	wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, 0);
	wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertBuf, 0, 0);
	wgpuRenderPassEncoderSetIndexBuffer(pass, indxBuf, WGPUIndexFormat_Uint16, 0, 0);
	wgpuRenderPassEncoderDrawIndexed(pass, 3, 1, 0, 0, 0);

	if (_showImGui) {
		ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
	}

	wgpuRenderPassEncoderEndPass(pass);
	wgpuRenderPassEncoderRelease(pass);														// release pass

	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);				// create commands
	wgpuCommandEncoderRelease(encoder);														// release encoder

	wgpuQueueSubmit(queue, 1, &commands);
	wgpuCommandBufferRelease(commands);														// release commands

#ifndef __EMSCRIPTEN__
	wgpuSwapChainPresent(swapchain);
#endif
	wgpuTextureViewRelease(backBufView);													// release textureView


	return true;
}

/**
 * Mouse handling function.
 */
void Renderer::mouseClicked(int button, int action, int x, int y)
{
	if (button == MOUSE_LEFT_BUTTON) {
		if (action == ACTION_PRESSED) {
			dir *= -1;
		}		
	}

	printf("button:%d action:%d x:%d y:%d\n", button, action, x, y);
}

/**
 * Keyboard handling function.
 */
void Renderer::keyPressed(int button, int action)
{	
	printf("key:%d action:%d\n", button, action);
}

/**
 * Setter for rendering of ImGui.
 */
void Renderer::showImGui(bool state)
{
	this->_showImGui = state;
}

/**
 * Setter for a color. Just an example now! Sets the vertex1's color.
 */
void Renderer::setColor(float r, float g, float b)
{
	this->vertex1.x = r;
	this->vertex1.y = g;
	this->vertex1.z = b;
}
