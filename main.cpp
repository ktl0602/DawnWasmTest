//#include "glue.h"
#include "RendererWindow.h"
#include "Renderer.h"
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>

using namespace emscripten;
#endif

Renderer* renderer;
RendererWindow* window;

// =================== "Binding Window to Renderer" =====================
/**
  * Callback render function passed to the window.
  */
bool render(double time)
{	
	return renderer->render(time);
}	

/**
  * Callback resize function passed to the window.
  */
void resizeHandler(int width, int height)
{
	renderer->resize(width, height);	
}

/**
  * Callback mouse click handle function passed to the window.
  */
void mouseClickHandler(int button, int action, int x, int y)
{
	renderer->mouseClicked(button, action, x, y);
}

/**
  * Callback key press handle function passed to the window.
  */
void keyPressHandler(int keyCode, int action)
{
	renderer->keyPressed(keyCode, action);
}
// =================== "Binding Window to Renderer" END =====================

/**
  * Main application entry point.
  */
int main(int /*argc*/, char* /*argv*/[]) {

	window = new RendererWindow();

	auto wHnd = window->create();

	if (wHnd) {

		auto win = window->getGLFWWindow();
		auto wgpu_device = window->createDevice(wHnd);

		if (wgpu_device == NULLPTR)
		{
			return 1;
		}

		// create WGPU rendering related items
		auto wgpu_swap_chain = window->createSwapChain(wgpu_device);
		auto queue = wgpuDeviceGetDefaultQueue(wgpu_device);

		// bind the user interaction
		window->mouseClicked(mouseClickHandler);
		window->keyPressed(keyPressHandler);
		window->resized(resizeHandler);

		// initialize the renderer
		renderer = new Renderer();
		renderer->setDevice(wgpu_device);
		renderer->setQueue(queue);
		renderer->setSwapChain(wgpu_swap_chain);
		renderer->setupImGui(win);
		renderer->createPipelineAndBuffers();

		// show the window & run the main loop
		window->show(wHnd);
		window->loop(wHnd, render);

		// destroy the window
		window->destroy(wHnd);
	}
	return 0;
}


// =================== "API to JS" =====================
#ifdef __EMSCRIPTEN__
/**
  * JS exposed handle function that allows to call "Module.showImGui(bool)" from the web app.
  */
void showImGui(bool state) {
	renderer->showImGui(state);
}

/**
  * JS exposed handle function that allows to call "Module.setColor(r,g,b)" from the web app.
  */
void setColor(float r, float g, float b) {
	renderer->setColor(r, g, b);
}

EMSCRIPTEN_BINDINGS(my_module) {
	function("showImGui", &showImGui);
	function("setColor", &setColor);
}
#endif // __EMSCRIPTEN__
// =================== "API to JS" END =====================

