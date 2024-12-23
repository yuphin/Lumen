#include "../LumenPCH.h"
#include "Window.h"

namespace Window {
Window _window;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	auto ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	ptr->key_map[static_cast<KeyInput>(key)] = static_cast<KeyAction>(action);
	for (auto& cb : ptr->key_callbacks) {
		cb(static_cast<KeyInput>(key), static_cast<KeyAction>(action));
	}
}

static void window_size_callback(GLFWwindow* window, int width, int height) {}

static void char_callback(GLFWwindow* window, uint32_t codepoint) {}

static void mouse_click_callback(GLFWwindow* window, int button, int action, int mods) {
	KeyAction callback_action;

	switch (action) {
		case GLFW_RELEASE:
			callback_action = KeyAction::RELEASE;
			break;

		case GLFW_REPEAT:
			callback_action = KeyAction::REPEAT;
			break;
		case GLFW_PRESS:
			callback_action = KeyAction::PRESS;
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
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	window_ptr->mouse_map[mouse_button] = {callback_action, xpos, ypos};
	for (auto& cb : window_ptr->mouse_click_callbacks) {
		cb(mouse_button, callback_action, xpos, ypos);
	}
}

static void mouse_move_callback(GLFWwindow* window, double x, double y) {
	const auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	window_ptr->mouse_prev_x = window_ptr->mouse_pos_x;
	window_ptr->mouse_prev_y = window_ptr->mouse_pos_y;
	window_ptr->mouse_pos_x = x;
	window_ptr->mouse_pos_y = y;

	window_ptr->mouse_delta_prev_x = window_ptr->mouse_prev_x - window_ptr->mouse_pos_x;
	window_ptr->mouse_delta_prev_y = window_ptr->mouse_prev_y - window_ptr->mouse_pos_y;
	window_ptr->mouse_delta_prev_x *= -1;

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	for (auto& [k, v] : window_ptr->mouse_map) {
		v.x = xpos;
		v.y = ypos;
	}

	for (auto& cb : window_ptr->mouse_move_callbacks) {
		cb(window_ptr->mouse_delta_prev_x, window_ptr->mouse_delta_prev_y, xpos, ypos);
	}
}

static void scroll_callback(GLFWwindow* window, double x, double y) {
	const auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	for (auto& cb : window_ptr->mouse_scroll_callbacks) cb(x, y);
}

void init(int width, int height, bool fullscreen) {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	_window.window_handle =
		glfwCreateWindow(width, height, "Lumen", fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
	LUMEN_ASSERT(_window.window_handle, "Failed to create a window!");
	glfwSetWindowUserPointer(_window.window_handle, &_window);
	glfwSetKeyCallback(_window.window_handle, key_callback);
	glfwSetWindowSizeCallback(_window.window_handle, window_size_callback);
	glfwSetCharCallback(_window.window_handle, char_callback);
	glfwSetMouseButtonCallback(_window.window_handle, mouse_click_callback);
	glfwSetCursorPosCallback(_window.window_handle, mouse_move_callback);
	glfwSetScrollCallback(_window.window_handle, scroll_callback);
	_window.window_width = width;
	_window.window_height = height;
	// Viewport and window sizes are the same for now
	_window.viewport_width = width;
	_window.viewport_height = height;
}

void poll() { glfwPollEvents(); }

void destroy() {
	glfwDestroyWindow(_window.window_handle);
	glfwTerminate();
}

void add_mouse_click_callback(MouseClickCallback callback) { _window.mouse_click_callbacks.push_back(callback); }

void add_mouse_move_callback(MouseMoveCallback callback) { _window.mouse_move_callbacks.push_back(callback); }

void add_scroll_callback(MouseScrollCallback callback) { _window.mouse_scroll_callbacks.push_back(callback); }

void add_key_callback(KeyCallback callback) { _window.key_callbacks.push_back(callback); }

bool is_mouse_held(MouseAction mb, glm::ivec2& pos) {
	auto res = _window.mouse_map.find(mb);
	if (res == _window.mouse_map.end() ||
		(res->second.action != KeyAction::PRESS && res->second.action != KeyAction::REPEAT)) {
		return false;
	}
	pos = glm::ivec2(res->second.x, res->second.y);
	return true;
}

bool is_mouse_up(MouseAction mb, glm::ivec2& pos) {
	auto res = _window.mouse_map.find(mb);
	if (res == _window.mouse_map.end() || res->second.action != KeyAction::RELEASE) {
		return false;
	}
	pos = glm::ivec2(res->second.x, res->second.y);
	return true;
}

bool should_close() { return glfwWindowShouldClose(_window.window_handle); }
bool is_key_down(KeyInput input) { return _window.key_map[input] == KeyAction::PRESS; }
bool is_key_up(KeyInput input) { return _window.key_map[input] == KeyAction::RELEASE; }
bool is_key_held(KeyInput input) {
	return _window.key_map[input] == KeyAction::PRESS || _window.key_map[input] == KeyAction::REPEAT;
}
bool is_mouse_held(MouseAction mb) {
	auto res = _window.mouse_map.find(mb);
	return res != _window.mouse_map.end() &&
		   (res->second.action == KeyAction::PRESS || res->second.action == KeyAction::REPEAT);
}
bool is_mouse_down(MouseAction mb) {
	auto res = _window.mouse_map.find(mb);
	return res != _window.mouse_map.end() && res->second.action == KeyAction::PRESS;
}
bool is_mouse_up(MouseAction mb) {
	auto res = _window.mouse_map.find(mb);
	return res != _window.mouse_map.end() && res->second.action == KeyAction::RELEASE;
}

Window* get() { return &_window; }
void update_window_size() {
	int width, height;
	glfwGetWindowSize(_window.window_handle, &width, &height);
	_window.window_width = width;
	_window.window_height = height;
	_window.viewport_width = width;
	_window.viewport_height = height;
}

uint32_t width() { return _window.viewport_width; }
uint32_t height() { return _window.viewport_height; }
}  // namespace Window
