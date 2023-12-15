/**
 * \file window.h
 * Abstraction for creating and managing windows.
 */
#pragma once

#include "defines.h"
#include <stdio.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_wgpu.h"
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include <GLFW/glfw3.h>

#define WINDOW_W 1280
#define WINDOW_H 720
#define WINDOW_TITLE "WebRenderer"

 /**
  * \typedef Handle
  * Opaque window handle.
  */
typedef struct HandleImpl* Handle;

/**
 * Function prototype for the redraw callback. See \c #loop().
 */
typedef bool (*RenderFunc) (double);
typedef void (*MouseHandler) (int, int, int, int);
typedef void (*KeyHandler) (int, int);
typedef void (*ResizeHandler) (int, int);

class RendererWindow {
private:
	
	int wgpu_swap_chain_width, wgpu_swap_chain_height;

	WGPUSwapChain _NULLABLE wgpu_swap_chain = NULLPTR;
	WGPUDevice _NULLABLE wgpu_device = NULLPTR;
	WGPUSurface _NULLABLE wgpu_surface = NULLPTR;

	GLFWwindow* _NULLABLE _window;

	static MouseHandler _NULLABLE mouseClickHandlerClb;
	static ResizeHandler _NULLABLE resizeHandlerClb;
	static KeyHandler _NULLABLE keyHandlerClb;

public:
	/**
	 * Creates a new window.
	 *
	 * \param[in] winW optional internal width (or zero to use the default width)
	 * \param[in] winH optional internal height (or zero to use the default height)
	 * \param[in] name optional window title (alternatively repurposed as the element ID for web-based implementations)
	 */
	Handle _NULLABLE create(unsigned winW = 0, unsigned winH = 0, const char* _NULLABLE name = NULLPTR);	
	
	/**
	 * Destroys a window, freeing any resources.
	 *
	 * \param[in] wHnd window to destroy
	 */
	void destroy(Handle _NONNULL wHnd);

	/**
	 * Shows or hides a window.
	 *
	 * \param[in] wHnd window to show or hide
	 * \param[in] show \c true to show, \c false to hide
	 */
	void show(Handle _NONNULL wHnd, bool show = true);

	/**
	 * Registers the redraw function to be called each frame.
	 *
	 * \note Currently this blocks, returning only when the redraw function returns.
	 *
	 * \todo rethink this - what do we do for multiple windows?
	 *
	 * \param[in] wHnd window to synchronise the redraw with
	 * \param[in] func function to be called each \e frame (or \c null to do nothing)
	 */
	void loop(Handle _NONNULL wHnd, RenderFunc _NULLABLE func = NULLPTR);	

	WGPUDevice _NULLABLE createDevice(Handle _NONNULL window, WGPUBackendType type = WGPUBackendType_Force32);
	WGPUSwapChain _NULLABLE createSwapChain(WGPUDevice _NONNULL device);
	
	void mouseClicked(MouseHandler _NULLABLE func = NULLPTR);
	void keyPressed(KeyHandler _NULLABLE func = NULLPTR);	
	void resized(ResizeHandler _NULLABLE func = NULLPTR);

	inline GLFWwindow* _NULLABLE getGLFWWindow() { return _window; }

	// These are methods for converting library-specific constants into our constants.	
	static int convertMouseButton(int button);
	static int convertMouseAction(int action);

	inline static void print_wgpu_error(WGPUErrorType error_type, const char* _NULLABLE message, void* _NULLABLE)
	{
		const char* error_type_lbl = "";
		switch (error_type)
		{
		case WGPUErrorType_Validation:  error_type_lbl = "Validation"; break;
		case WGPUErrorType_OutOfMemory: error_type_lbl = "Out of memory"; break;
		case WGPUErrorType_Unknown:     error_type_lbl = "Unknown"; break;
		case WGPUErrorType_DeviceLost:  error_type_lbl = "Device lost"; break;
		default:                        error_type_lbl = "Unknown";
		}
		printf("%s error: %s\n", error_type_lbl, message);
	}

	inline static void print_glfw_error(int code, const char* _NULLABLE desc)
	{
		printf("GLFW [%d]: %s", code, desc);
	}
};
