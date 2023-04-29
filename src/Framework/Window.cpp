#include "../LumenPCH.h"
#include "Window.h"

Window::Window(int width, int height, bool fullscreen) {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window_handle = glfwCreateWindow(width, height, "Lumen", fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
	LUMEN_ASSERT(window_handle, "Failed to create a window!");
	glfwSetWindowUserPointer(window_handle, this);
	glfwSetKeyCallback(window_handle, key_callback);
	glfwSetWindowSizeCallback(window_handle, window_size_callback);
	glfwSetCharCallback(window_handle, char_callback);
	glfwSetMouseButtonCallback(window_handle, mouse_click_callback);
	glfwSetCursorPosCallback(window_handle, mouse_move_callback);
	glfwSetScrollCallback(window_handle, scroll_callback);
}

void Window::poll() { glfwPollEvents(); }

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

void Window::char_callback(GLFWwindow* window, uint32_t codepoint) {
	auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
}

void Window::mouse_click_callback(GLFWwindow* window, int button, int action, int mods) {
	KeyAction callback_action;
	switch (action) {
		case GLFW_RELEASE:
			callback_action = KeyAction::RELEASE;
			break;
		case GLFW_PRESS:
			callback_action = KeyAction::PRESS;
			break;
		case GLFW_REPEAT:
			callback_action = KeyAction::REPEAT;
			break;
		default:
			callback_action = KeyAction::UNKNOWN;
			break;
	}

	MouseAction mouse_button;
	switch (button) {
		case GLFW_MOUSE_BUTTON_1:
			mouse_button = MouseAction::LEFT;
			break;
		case GLFW_MOUSE_BUTTON_2:
			mouse_button = MouseAction::RIGHT;
			break;
		case GLFW_MOUSE_BUTTON_3:
			mouse_button = MouseAction::MIDDLE;
			break;
		default:
			mouse_button = MouseAction::UNKNOWN;
			break;
	}

	auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	window_ptr->mouse_map[mouse_button] = callback_action;
	for (auto& cb : window_ptr->mouse_click_callbacks) {
		cb(mouse_button, callback_action);
	}
}

void Window::mouse_move_callback(GLFWwindow* window, double x, double y) {
	const auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	window_ptr->mouse_prev_x = window_ptr->mouse_pos_x;
	window_ptr->mouse_prev_y = window_ptr->mouse_pos_y;
	window_ptr->mouse_pos_x = x;
	window_ptr->mouse_pos_y = y;

	window_ptr->mouse_delta_prev_x = window_ptr->mouse_prev_x - window_ptr->mouse_pos_x;
	window_ptr->mouse_delta_prev_y = window_ptr->mouse_prev_y - window_ptr->mouse_pos_y;
	window_ptr->mouse_delta_prev_x *= -1;

	for (auto& cb : window_ptr->mouse_move_callbacks) {
		cb(window_ptr->mouse_delta_prev_x, window_ptr->mouse_delta_prev_y);
	}
}

void Window::scroll_callback(GLFWwindow* window, double x, double y) {
	const auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	for (auto& cb : window_ptr->mouse_scroll_callbacks) cb(x, y);
}

void Window::add_mouse_click_callback(MouseClickCallback callback) { mouse_click_callbacks.push_back(callback); }

void Window::add_mouse_move_callback(MouseMoveCallback callback) { mouse_move_callbacks.push_back(callback); }

void Window::add_scroll_callback(MouseScrollCallback callback) { mouse_scroll_callbacks.push_back(callback); }
