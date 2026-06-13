#include "Window.h"

namespace Window {
Window _window;

static bool valid_key_index(i32 key) { return key >= 0 && key < static_cast<i32>(MAX_KEY_INPUTS); }

static KeyAction key_action(KeyInput input) {
	i32 idx = static_cast<i32>(input);
	return valid_key_index(idx) ? _window.key_map[idx] : KeyAction::UNKNOWN;
}

static MouseInput* get_mouse_input(Window* window, MouseAction action) {
	i32 idx = static_cast<i32>(action);
	if (idx < 0 || idx >= static_cast<i32>(MOUSE_ACTION_COUNT)) {
		return nullptr;
	}
	return &window->mouse_map[idx];
}

template <typename Callback, u64 N>
static void add_callback(lm::SmallArray<Callback, N>& callbacks, Callback callback) {
	LUMEN_ASSERT(callback.procedure, "Cannot add an empty Window callback");
	LUMEN_ASSERT(callbacks.size < callbacks.capacity(), "Window callback capacity exceeded");
	if (callbacks.size >= callbacks.capacity()) {
		return;
	}
	callbacks.push_back(callback);
}

static void key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods) {
	if (!valid_key_index(key)) {
		return;
	}
	auto ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	ptr->key_map[key] = static_cast<KeyAction>(action);
	for (auto& cb : ptr->key_callbacks) {
		cb.procedure(cb.user_data, static_cast<KeyInput>(key), static_cast<KeyAction>(action));
	}
}

static void window_size_callback(GLFWwindow* window, i32 width, i32 height) {}

static void char_callback(GLFWwindow* window, u32 codepoint) {}

static void mouse_click_callback(GLFWwindow* window, i32 button, i32 action, i32 mods) {
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
	*get_mouse_input(window_ptr, mouse_button) = {callback_action, xpos, ypos};
	for (auto& cb : window_ptr->mouse_click_callbacks) {
		cb.procedure(cb.user_data, mouse_button, callback_action, xpos, ypos);
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
	for (MouseInput& input : window_ptr->mouse_map) {
		input.x = xpos;
		input.y = ypos;
	}

	for (auto& cb : window_ptr->mouse_move_callbacks) {
		cb.procedure(cb.user_data, window_ptr->mouse_delta_prev_x, window_ptr->mouse_delta_prev_y, xpos, ypos);
	}
}

static void scroll_callback(GLFWwindow* window, double x, double y) {
	const auto window_ptr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	for (auto& cb : window_ptr->mouse_scroll_callbacks) cb.procedure(cb.user_data, x, y);
}

void init(i32 width, i32 height, bool fullscreen, bool on_second_monitor) {
	_window.key_map.resize(MAX_KEY_INPUTS);
	for (KeyAction& action : _window.key_map) {
		action = KeyAction::RELEASE;
	}
	_window.mouse_map.resize(MOUSE_ACTION_COUNT);
	for (MouseInput& input : _window.mouse_map) {
		input = {};
	}

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

	if (on_second_monitor) {
		int count;
		GLFWmonitor** monitors = glfwGetMonitors(&count);
		int target_monitor = (count > 1) ? 1 : 0;
		int mx, my;
		glfwGetMonitorPos(monitors[target_monitor], &mx, &my);
		const GLFWvidmode* mode = glfwGetVideoMode(monitors[target_monitor]);
		glfwSetWindowPos(_window.window_handle, mx + (mode->width - width) / 2, my + (mode->height - height) / 2);
	}
}

void poll() { glfwPollEvents(); }

void destroy() {
	glfwDestroyWindow(_window.window_handle);
	glfwTerminate();
}

void add_mouse_click_callback(void (*procedure)(void*, MouseAction, KeyAction, double, double), void* user_data) {
	add_callback(_window.mouse_click_callbacks, MouseClickCallback{procedure, user_data});
}

void add_mouse_move_callback(void (*procedure)(void*, double, double, double, double), void* user_data) {
	add_callback(_window.mouse_move_callbacks, MouseMoveCallback{procedure, user_data});
}

void add_scroll_callback(void (*procedure)(void*, double, double), void* user_data) {
	add_callback(_window.mouse_scroll_callbacks, MouseScrollCallback{procedure, user_data});
}

void add_key_callback(void (*procedure)(void*, KeyInput, KeyAction), void* user_data) {
	add_callback(_window.key_callbacks, KeyCallback{procedure, user_data});
}

bool is_mouse_held(MouseAction mb, glm::ivec2& pos) {
	const MouseInput* input = get_mouse_input(&_window, mb);
	if (!input || (input->action != KeyAction::PRESS && input->action != KeyAction::REPEAT)) {
		return false;
	}
	pos = glm::ivec2(input->x, input->y);
	return true;
}

bool is_mouse_up(MouseAction mb, glm::ivec2& pos) {
	const MouseInput* input = get_mouse_input(&_window, mb);
	if (!input || input->action != KeyAction::RELEASE) {
		return false;
	}
	pos = glm::ivec2(input->x, input->y);
	return true;
}

bool should_close() { return glfwWindowShouldClose(_window.window_handle); }
bool is_key_down(KeyInput input) { return key_action(input) == KeyAction::PRESS; }
bool is_key_up(KeyInput input) { return key_action(input) == KeyAction::RELEASE; }
bool is_key_held(KeyInput input) {
	KeyAction action = key_action(input);
	return action == KeyAction::PRESS || action == KeyAction::REPEAT;
}
bool is_mouse_held(MouseAction mb) {
	const MouseInput* input = get_mouse_input(&_window, mb);
	return input && (input->action == KeyAction::PRESS || input->action == KeyAction::REPEAT);
}
bool is_mouse_down(MouseAction mb) {
	const MouseInput* input = get_mouse_input(&_window, mb);
	return input && input->action == KeyAction::PRESS;
}
bool is_mouse_up(MouseAction mb) {
	const MouseInput* input = get_mouse_input(&_window, mb);
	return input && input->action == KeyAction::RELEASE;
}

Window* get() { return &_window; }
void update_window_size() {
	i32 width, height;
	glfwGetWindowSize(_window.window_handle, &width, &height);
	_window.window_width = width;
	_window.window_height = height;
	_window.viewport_width = width;
	_window.viewport_height = height;
}

u32 width() { return _window.viewport_width; }
u32 height() { return _window.viewport_height; }
f32 aspect_ratio() { return (f32)_window.viewport_width / _window.viewport_height; }
}  // namespace Window
