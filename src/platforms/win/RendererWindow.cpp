#include "RendererWindow.h"

#if __has_include("d3d12.h") || (_MSC_VER >= 1900)
#define DAWN_ENABLE_BACKEND_D3D12
#endif
#if __has_include("vulkan/vulkan.h") && (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64))
#define DAWN_ENABLE_BACKEND_VULKAN
#endif

//****************************************************************************/

#include <dawn/dawn_proc.h>
//#include <dawn/webgpu.h>
//#include <dawn/webgpu_cpp.h>
#include <dawn_native/NullBackend.h>
#ifdef DAWN_ENABLE_BACKEND_D3D12
#include <dawn_native/D3D12Backend.h>
#endif
#ifdef DAWN_ENABLE_BACKEND_VULKAN
#include <dawn_native/VulkanBackend.h>
#include <vulkan/vulkan_win32.h>
#endif

#pragma comment(lib, "dawn_native.dll.lib")
#pragma comment(lib, "dawn_proc.dll.lib")
#ifdef DAWN_ENABLE_BACKEND_VULKAN
#pragma comment(lib, "vulkan-1.lib")
#endif

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

// initialization of static members
MouseHandler RendererWindow::mouseClickHandlerClb = NULLPTR;
ResizeHandler RendererWindow::resizeHandlerClb = NULLPTR;
KeyHandler RendererWindow::keyHandlerClb = NULLPTR;

/*
 * Chosen backend type for \c #device.
 */
WGPUBackendType backend;

/*
 * Something needs to hold onto this since the address is passed to the WebGPU
 * native API, exposing the type-specific swap chain implementation. The struct
 * gets filled out on calling the respective XXX::CreateNativeSwapChainImpl(),
 * binding the WebGPU device and native window, then its raw pointer is passed
 * into WebGPU as a 64-bit int. The browser API doesn't have an equivalent
 * (since the swap chain is created from the canvas directly).
 *
 * Is the struct copied or does it need holding for the lifecycle of the swap
 * chain, i.e. can it just be a temporary?
 *
 * After calling wgpuSwapChainRelease() does it also call swapImpl::Destroy()
 * to delete the underlying NativeSwapChainImpl(), invalidating this struct?
 */
static DawnSwapChainImplementation swapImpl;

/*
* Preferred swap chain format, obtained in the browser via a promise to
* GPUCanvasContext::getSwapChainPreferredFormat(). In Dawn we can call this
* directly in NativeSwapChainImpl::GetPreferredFormat() (which is hard-coded
* with D3D, for example, to RGBA8Unorm, but queried for others). For the D3D
* back-end calling wgpuSwapChainConfigure ignores the passed preference and
* asserts if it's not the preferred choice.
*/
static WGPUTextureFormat swapPref;

/**
 * Analogous to the browser's \c GPU.requestAdapter().
 * \n
 * The returned \c Adapter is a wrapper around the underlying Dawn adapter (and
 * owned by the single Dawn instance).
 *
 * \todo we might be interested in whether the \c AdapterType is discrete or integrated for power-management reasons
 *
 * \param[in] type1st first choice of \e backend type (e.g. \c WGPUBackendType_D3D12)
 * \param[in] type2nd optional fallback \e backend type (or \c WGPUBackendType_Null to pick the first choice or nothing)
 * \return the best choice adapter or an empty adapter wrapper
 */
static dawn_native::Adapter requestAdapter(WGPUBackendType type1st, WGPUBackendType type2nd = WGPUBackendType_Null) {
	static dawn_native::Instance instance;
	instance.DiscoverDefaultAdapters();
	wgpu::AdapterProperties properties;
	std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
	for (auto it = adapters.begin(); it != adapters.end(); ++it) {
		it->GetProperties(&properties);
		if (static_cast<WGPUBackendType>(properties.backendType) == type1st) {
			return *it;
		}
	}	
	if (type2nd) {
		for (auto it = adapters.begin(); it != adapters.end(); ++it) {
			it->GetProperties(&properties);
			if (static_cast<WGPUBackendType>(properties.backendType) == type2nd) {
				return *it;
			}
		}
	}
	return dawn_native::Adapter();
}

#ifdef DAWN_ENABLE_BACKEND_VULKAN
/**
 * Helper to obtain a Vulkan surface from the supplied window.
 *
 * \todo what's the lifecycle of this?
 *
 * \param[in] device WebGPU device
 * \param[in] window window on which the device will be bound
 * \return window surface (or \c VK_NULL_HANDLE if creation failed)
 */
static VkSurfaceKHR createVkSurface(WGPUDevice device, Handle window) {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
#ifdef WIN32
	VkWin32SurfaceCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hinstance = GetModuleHandle(NULL);
	info.hwnd = reinterpret_cast<HWND>(window);
	vkCreateWin32SurfaceKHR(
		dawn_native::vulkan::GetInstance(device),
		&info, nullptr, &surface);
#endif
	return surface;
}
#endif

/**
 * Creates an API-specific swap chain implementation in \c #swapImpl and stores
 * the \c #swapPref.
 */
static void initSwapChain(WGPUBackendType backend, WGPUDevice device, Handle window) {
	switch (backend) {
#ifdef DAWN_ENABLE_BACKEND_D3D12
	case WGPUBackendType_D3D12:
		if (swapImpl.userData == nullptr) {
			swapImpl = dawn_native::d3d12::CreateNativeSwapChainImpl(
				device, reinterpret_cast<HWND>(window));
			swapPref = dawn_native::d3d12::GetNativeSwapChainPreferredFormat(&swapImpl);
		}
		break;
#endif
#ifdef DAWN_ENABLE_BACKEND_VULKAN
	case WGPUBackendType_Vulkan:
		if (swapImpl.userData == nullptr) {
			swapImpl = dawn_native::vulkan::CreateNativeSwapChainImpl(
				device, createVkSurface(device, window));
			swapPref = dawn_native::vulkan::GetNativeSwapChainPreferredFormat(&swapImpl);
		}
		break;
#endif
	default:
		if (swapImpl.userData == nullptr) {
			swapImpl = dawn_native::null::CreateNativeSwapChainImpl();
			swapPref = WGPUTextureFormat_Undefined;
		}
		break;
	}
}

/**
 * Dawn error handling callback (adheres to \c WGPUErrorCallback).
 *
 * \param[in] message error string
 */
static void printError(WGPUErrorType /*type*/, const char* message, void*) {
	puts(message);
}

/**
 * Function that creates the swapchain for the current (Win) platform.
 */
WGPUSwapChain RendererWindow::createSwapChain(WGPUDevice device) {
	WGPUSwapChainDescriptor swapDesc = {};
	/*
	 * Currently failing (probably because the nextInChain needs setting up, and
	 * also with the correct WGPUSType_* for the platform).
	 *
	swapDesc.usage  = WGPUTextureUsage_OutputAttachment;
	swapDesc.format = WGPUTextureFormat_BGRA8Unorm;
	swapDesc.width  = WINDOW_W;
	swapDesc.height = WINDOW_H;
	swapDesc.presentMode = WGPUPresentMode_Mailbox;
	 */

	swapDesc.implementation = reinterpret_cast<uintptr_t>(&swapImpl);
	wgpu_swap_chain = wgpuDeviceCreateSwapChain(device, nullptr, &swapDesc);
	/*
	 * Currently failing on hi-DPI (with Vulkan).
	 */
	wgpuSwapChainConfigure(wgpu_swap_chain, swapPref, WGPUTextureUsage_OutputAttachment, WINDOW_W, WINDOW_H);

	return wgpu_swap_chain;
}

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
	
	HWND hwnd = glfwGetWin32Window(_window);
	return reinterpret_cast<Handle>(hwnd);
}

/**
 * Obtaining a WebGPU device based on the available system's backend.
 */
WGPUDevice RendererWindow::createDevice(Handle window, WGPUBackendType type) {
	if (type > WGPUBackendType_OpenGLES) {
#ifdef DAWN_ENABLE_BACKEND_D3D12
		type = WGPUBackendType_D3D12;
#else
#ifdef DAWN_ENABLE_BACKEND_VULKAN
		type = WGPUBackendType_Vulkan;
#endif
#endif
	}
	/*
	 * First go at this. We're only creating one global device/swap chain so far.
	 */
	wgpu_device = NULL;
	if (dawn_native::Adapter adapter = requestAdapter(type)) {
		wgpu::AdapterProperties properties;
		adapter.GetProperties(&properties);
		backend = static_cast<WGPUBackendType>(properties.backendType);
		wgpu_device = adapter.CreateDevice();
		initSwapChain(backend, wgpu_device, window);
		DawnProcTable procs(dawn_native::GetProcs());
		procs.deviceSetUncapturedErrorCallback(wgpu_device, printError, nullptr);
		dawnProcSetProcs(&procs);
	}

	if (wgpu_device == NULL) {
		if (_window)
			glfwDestroyWindow(_window);
		glfwTerminate();
	}

	return wgpu_device;
}

/**
 * Destroys the window.
 */
void RendererWindow::destroy(Handle /*wHnd*/) 
{
	glfwDestroyWindow(_window);
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
	while (!glfwWindowShouldClose(_window)) {
		glfwPollEvents();

		// React to changes in screen size
		if (resizeHandlerClb != NULLPTR) {
			int width, height;
			glfwGetFramebufferSize(_window, &width, &height);

			if (width != wgpu_swap_chain_width && height != wgpu_swap_chain_height)
			{
				resizeHandlerClb(width, height);
			}
		}

		// render function callback
		func(0);
	}
}

/**
 * Converts mouse button constants from GLFW3 library into our library constants.
 */
int RendererWindow::convertMouseButton(int button)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		return MOUSE_LEFT_BUTTON;
	}
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
		return MOUSE_MIDDLE_BUTTON;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
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
	else if(action == GLFW_RELEASE) {
		return ACTION_RELEASED;
	}
	else if (action == GLFW_REPEAT) {
		return ACTION_REPEAT;
	}

	return -1;
}

/**
 * Binds mouse click inside the GLFW3 window to the Renderer mouse click handler.
 */
void RendererWindow::mouseClicked(MouseHandler func)
{
	mouseClickHandlerClb = func;

	glfwSetMouseButtonCallback(_window, [](GLFWwindow* window, int button, int action, int mods) {
		if (mouseClickHandlerClb != NULLPTR) {
			double xpos, ypos;
			//getting cursor position
			glfwGetCursorPos(window, &xpos, &ypos);

			mouseClickHandlerClb(RendererWindow::convertMouseButton(button), 								 
				RendererWindow::convertMouseAction(action), 
				(int)xpos, 
				(int)ypos);
		}
		});
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

