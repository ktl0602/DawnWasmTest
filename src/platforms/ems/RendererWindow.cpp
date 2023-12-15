#include "RendererWindow.h"

#include <GLFW/glfw3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

/**
 * Temporary dummy window handle.
 */ 
struct HandleImpl {} DUMMY;

// initialization of static members
MouseHandler RendererWindow::mouseClickHandlerClb = NULLPTR;
ResizeHandler RendererWindow::resizeHandlerClb = NULLPTR;
KeyHandler RendererWindow::keyHandlerClb = NULLPTR;

//******************************** Public API ********************************/
/**
 * Function that creates a new window. For this GLFW3 library is currently used.
 */
Handle RendererWindow::create(unsigned /*winW*/, unsigned /*winH*/, const char* /*name*/) {
	glfwSetErrorCallback(print_glfw_error);
	if (!glfwInit())
		return NULLPTR;

	// Make sure GLFW does not initialize any graphics context.
	// This needs to be done explicitly later
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	_window = glfwCreateWindow(WINDOW_W, WINDOW_H, WINDOW_TITLE, NULL, NULL);
	if (!_window)
	{
		glfwTerminate();
		return NULLPTR;
	}

	Handle res = &DUMMY;

	return res;
}

/**
 * Obtaining a WebGPU device based on the available system's backend.
 */
WGPUDevice RendererWindow::createDevice(Handle window, WGPUBackendType type) {
	wgpu_device = emscripten_webgpu_get_device();

	if (!wgpu_device)
		return NULLPTR;

	wgpuDeviceSetUncapturedErrorCallback(wgpu_device, print_wgpu_error, NULL);

	// Use C++ wrapper due to misbehavior in Emscripten.
	// Some offset computation for wgpuInstanceCreateSurface in JavaScript
	// seem to be inline with struct alignments in the C++ structure
	wgpu::SurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};
	html_surface_desc.selector = "#canvas";

	wgpu::SurfaceDescriptor surface_desc = {};
	surface_desc.nextInChain = &html_surface_desc;

	// Use 'null' instance
	wgpu::Instance instance = {};
	wgpu_surface = instance.CreateSurface(&surface_desc).Release();

	return wgpu_device;
}

WGPUSwapChain RendererWindow::createSwapChain(WGPUDevice device)
{
	WGPUSwapChainDescriptor swap_chain_desc = {};
	swap_chain_desc.usage = WGPUTextureUsage_OutputAttachment;
	swap_chain_desc.format = WGPUTextureFormat_RGBA8Unorm;
	swap_chain_desc.width = WINDOW_W;
	swap_chain_desc.height = WINDOW_H;
	swap_chain_desc.presentMode = WGPUPresentMode_Fifo;
	wgpu_swap_chain = wgpuDeviceCreateSwapChain(wgpu_device, wgpu_surface, &swap_chain_desc);
	
	return wgpu_swap_chain;
}

/**
 * Destroys the window.
 */
void RendererWindow::destroy(Handle /*wHnd*/) 
{
	//glfwDestroyWindow(_window);
}

/**
 * Opens the window.
 */
void RendererWindow::show(Handle /*wHnd*/, bool /*show*/)
{
	glfwShowWindow(_window);
}

/**
 * Main application/window loop.
 */
void RendererWindow::loop(Handle /*wHnd*/, RenderFunc func)
{
	emscripten_request_animation_frame_loop([](double time, void* userData) {
		//glfwPollEvents();

		//// React to changes in screen size
		//if (resizeHandlerClb != NULLPTR) {
		//	int width, height;
		//	glfwGetFramebufferSize(_window, &width, &height);

		//	if (width != wgpu_swap_chain_width && height != wgpu_swap_chain_height)
		//	{
		//		resizeHandlerClb(width, height);
		//	}
		//}

		RenderFunc redraw = (RenderFunc)userData;
		return (EM_BOOL)redraw(time);
		}, (void*)func);
}

/**
 * Converts mouse button constants from GLFW3 library into our library constants.
 */
int RendererWindow::convertMouseButton(int button)
{
	if (button == 0) {
		return MOUSE_LEFT_BUTTON;
	}
	else if (button == 1) {
		return MOUSE_MIDDLE_BUTTON;
	}
	else if (button == 2) {
		return MOUSE_RIGHT_BUTTON;
	}

	return -1;
}

/**
 * Converts mouse action constants from GLFW3 library into our library constants.
 */
int RendererWindow::convertMouseAction(int action)
{
	if (action == GLFW_PRESS) {
		return ACTION_PRESSED;
	}
	else if (action == GLFW_RELEASE) {
		return ACTION_RELEASED;
	}

	return -1;
}

/**
 * Binds mouse click inside the GLFW3 window to the Renderer mouse click handler.
 */
void RendererWindow::mouseClicked(MouseHandler func)
{
	mouseClickHandlerClb = func;

	// this is an alternative way if we do not want to use GLFW
#ifdef __EMSCRIPTEN__	
	//emscripten_set_mousedown_callback("#canvas", (void*)0, true, [](int eventType, const EmscriptenMouseEvent* mouseEvent, void* /*userData*/) {
	//	if (mouseClickHandler != NULLPTR) {
	//		mouseClickHandler(RendererWindow::convertMouseButton(mouseEvent->button), RendererWindow::convertMouseAction(eventType), mouseEvent->targetX, mouseEvent->targetY);
	//	}

	//	return (EM_BOOL)0;
	//	});

	//emscripten_set_mouseup_callback("#canvas", (void*)0, true, [](int eventType, const EmscriptenMouseEvent* mouseEvent, void* /*userData*/) {
	//	if (mouseClickHandler != NULLPTR) {
	//		mouseClickHandler(RendererWindow::convertMouseButton(mouseEvent->button), RendererWindow::convertMouseAction(eventType), mouseEvent->targetX, mouseEvent->targetY);
	//	}

	//	return (EM_BOOL)0;
	//	});
#endif

	glfwSetMouseButtonCallback(_window, [](GLFWwindow* window, int button, int action, int mods) {
		if (mouseClickHandlerClb != nullptr) {
			double xpos, ypos;
			//getting cursor position
			glfwGetCursorPos(window, &xpos, &ypos);

			mouseClickHandlerClb(RendererWindow::convertMouseButton(button),
				RendererWindow::convertMouseAction(action), 
				xpos, 
				ypos);
		}
		});
//#endif // __EMSCRIPTEN__

}

/**
 * Binds key press inside the GLFW3 window to the Renderer key press handler.
 */
void RendererWindow::keyPressed(KeyHandler func)
{
	keyHandlerClb = func;

	glfwSetKeyCallback(_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		if (keyHandlerClb != NULLPTR) {
			keyHandlerClb(key, RendererWindow::convertMouseAction(action));
		}
		});
}

/**
 * Stores the resize callback.
 */
void RendererWindow::resized(ResizeHandler func)
{
	resizeHandlerClb = func;
}
