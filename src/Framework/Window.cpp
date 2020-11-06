#include "LumenPCH.h"
#include "Window.h"

Window::Window(int width, int height, bool fullscreen) : height(height), width(width) {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	window_handle = glfwCreateWindow(width, height, "Vulkan",
									 fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
	LUMEN_ASSERT(window_handle, "Failed to create a window!");
	glfwSetWindowUserPointer(window_handle, this);
	glfwSetKeyCallback(window_handle, key_callback);
	glfwSetWindowSizeCallback(window_handle, window_size_callback);
}

void Window::poll() {
	glfwPollEvents();
}

Window::~Window() {
	glfwDestroyWindow(window_handle);
	glfwTerminate();
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	auto ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	ptr->key_map[static_cast<KeyInput>(key)] = static_cast<KeyAction>(action);
}

void Window::window_size_callback(GLFWwindow* window, int width, int height) {
	auto ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	ptr->resized = true;
}
