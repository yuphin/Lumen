#include "Window.h"

namespace Window {
static Window _window;
static f64 _imgui_previous_time = 0.0;

static bool valid_key_index(i32 key) { return key >= 0 && key < static_cast<i32>(MAX_KEY_INPUTS); }

static KeyAction key_action(KeyInput input) {
	i32 idx = static_cast<i32>(input);
	return valid_key_index(idx) ? _window.key_map[idx] : KeyAction::UNKNOWN;
}

static MouseInput* get_mouse_input(Window* window, MouseAction action) {
	i32 idx = static_cast<i32>(action);
	if (idx < 0 || idx >= static_cast<i32>(MOUSE_ACTION_COUNT)) return nullptr;
	return &window->mouse_map[idx];
}

template <typename Callback, u64 N>
static void add_callback(lm::SmallArray<Callback, N>& callbacks, Callback callback) {
	LUMEN_ASSERT(callback.procedure, "Cannot add an empty Window callback");
	LUMEN_ASSERT(callbacks.size < callbacks.capacity(), "Window callback capacity exceeded");
	if (!callback.procedure || callbacks.size >= callbacks.capacity()) return;
	callbacks.push_back(callback);
}

static ImGuiKey imgui_key(i32 key) {
	if (key >= (i32)KeyInput::KEY_0 && key <= (i32)KeyInput::KEY_9)
		return (ImGuiKey)(ImGuiKey_0 + key - (i32)KeyInput::KEY_0);
	if (key >= (i32)KeyInput::KEY_A && key <= (i32)KeyInput::KEY_Z)
		return (ImGuiKey)(ImGuiKey_A + key - (i32)KeyInput::KEY_A);
	if (key >= (i32)KeyInput::KEY_F1 && key <= (i32)KeyInput::KEY_F12)
		return (ImGuiKey)(ImGuiKey_F1 + key - (i32)KeyInput::KEY_F1);
	if (key >= (i32)KeyInput::KEY_KP_0 && key <= (i32)KeyInput::KEY_KP_9)
		return (ImGuiKey)(ImGuiKey_Keypad0 + key - (i32)KeyInput::KEY_KP_0);
	switch ((KeyInput)key) {
		case KeyInput::KEY_TAB: return ImGuiKey_Tab;
		case KeyInput::KEY_LEFT: return ImGuiKey_LeftArrow;
		case KeyInput::KEY_RIGHT: return ImGuiKey_RightArrow;
		case KeyInput::KEY_UP: return ImGuiKey_UpArrow;
		case KeyInput::KEY_DOWN: return ImGuiKey_DownArrow;
		case KeyInput::KEY_PAGE_UP: return ImGuiKey_PageUp;
		case KeyInput::KEY_PAGE_DOWN: return ImGuiKey_PageDown;
		case KeyInput::KEY_HOME: return ImGuiKey_Home;
		case KeyInput::KEY_END: return ImGuiKey_End;
		case KeyInput::KEY_INSERT: return ImGuiKey_Insert;
		case KeyInput::KEY_DELETE: return ImGuiKey_Delete;
		case KeyInput::KEY_BACKSPACE: return ImGuiKey_Backspace;
		case KeyInput::SPACE: return ImGuiKey_Space;
		case KeyInput::KEY_ENTER: return ImGuiKey_Enter;
		case KeyInput::KEY_ESCAPE: return ImGuiKey_Escape;
		case KeyInput::KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
		case KeyInput::KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
		case KeyInput::KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
		case KeyInput::KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
		case KeyInput::KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
		case KeyInput::KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
		case KeyInput::KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
		case KeyInput::KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
		case KeyInput::KEY_MENU: return ImGuiKey_Menu;
		case KeyInput::APOSTROPHE: return ImGuiKey_Apostrophe;
		case KeyInput::COMMA: return ImGuiKey_Comma;
		case KeyInput::MINUS: return ImGuiKey_Minus;
		case KeyInput::PERIOD: return ImGuiKey_Period;
		case KeyInput::SLASH: return ImGuiKey_Slash;
		case KeyInput::SEMICOLON: return ImGuiKey_Semicolon;
		case KeyInput::EQUAL: return ImGuiKey_Equal;
		case KeyInput::KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
		case KeyInput::KEY_BACKSLASH: return ImGuiKey_Backslash;
		case KeyInput::KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
		case KeyInput::KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
		case KeyInput::KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
		case KeyInput::KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
		case KeyInput::KEY_NUM_LOCK: return ImGuiKey_NumLock;
		case KeyInput::KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
		case KeyInput::KEY_PAUSE: return ImGuiKey_Pause;
		case KeyInput::KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
		case KeyInput::KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
		case KeyInput::KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case KeyInput::KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
		case KeyInput::KEY_KP_ADD: return ImGuiKey_KeypadAdd;
		case KeyInput::KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
		case KeyInput::KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
		default: return ImGuiKey_None;
	}
}

static void key_callback(void* user_data, i32 key, bool pressed, bool repeat, bool control, bool shift, bool alt,
						 bool super) {
	if (!valid_key_index(key)) return;
	Window* window = (Window*)user_data;
	KeyAction action = pressed ? (repeat ? KeyAction::REPEAT : KeyAction::PRESS) : KeyAction::RELEASE;
	window->key_map[key] = action;
	if (ImGui::GetCurrentContext()) {
		ImGuiIO& io = ImGui::GetIO();
		io.AddKeyEvent(ImGuiKey_ModCtrl, control);
		io.AddKeyEvent(ImGuiKey_ModShift, shift);
		io.AddKeyEvent(ImGuiKey_ModAlt, alt);
		io.AddKeyEvent(ImGuiKey_ModSuper, super);
		ImGuiKey mapped = imgui_key(key);
		if (mapped != ImGuiKey_None) io.AddKeyEvent(mapped, pressed);
	}
	for (KeyCallback& callback : window->key_callbacks)
		callback.procedure(callback.user_data, (KeyInput)key, action);
}

static void text_callback(void*, u32 codepoint) {
	if (ImGui::GetCurrentContext()) ImGui::GetIO().AddInputCharacter(codepoint);
}

static void mouse_button_callback(void* user_data, i32 button, bool pressed) {
	Window* window = (Window*)user_data;
	MouseAction mouse_button = button == 0 ? MouseAction::LEFT : button == 1 ? MouseAction::RIGHT : MouseAction::MIDDLE;
	KeyAction action = pressed ? KeyAction::PRESS : KeyAction::RELEASE;
	MouseInput* input = get_mouse_input(window, mouse_button);
	if (input) *input = {action, window->mouse_pos_x, window->mouse_pos_y};
	if (ImGui::GetCurrentContext()) ImGui::GetIO().AddMouseButtonEvent(button, pressed);
	for (MouseClickCallback& callback : window->mouse_click_callbacks)
		callback.procedure(callback.user_data, mouse_button, action, window->mouse_pos_x, window->mouse_pos_y);
}

static void mouse_move_callback(void* user_data, f64 x, f64 y) {
	Window* window = (Window*)user_data;
	window->mouse_prev_x = window->mouse_pos_x;
	window->mouse_prev_y = window->mouse_pos_y;
	window->mouse_pos_x = x;
	window->mouse_pos_y = y;
	window->mouse_delta_prev_x = window->mouse_pos_x - window->mouse_prev_x;
	window->mouse_delta_prev_y = window->mouse_prev_y - window->mouse_pos_y;
	for (MouseInput& input : window->mouse_map) {
		input.x = x;
		input.y = y;
	}
	if (ImGui::GetCurrentContext()) ImGui::GetIO().AddMousePosEvent((f32)x, (f32)y);
	for (MouseMoveCallback& callback : window->mouse_move_callbacks)
		callback.procedure(callback.user_data, window->mouse_delta_prev_x, window->mouse_delta_prev_y, x, y);
}

static void scroll_callback(void* user_data, f64 x, f64 y) {
	Window* window = (Window*)user_data;
	if (ImGui::GetCurrentContext()) ImGui::GetIO().AddMouseWheelEvent((f32)x, (f32)y);
	for (MouseScrollCallback& callback : window->mouse_scroll_callbacks) callback.procedure(callback.user_data, x, y);
}

static void resize_callback(void* user_data, u32 width, u32 height) {
	Window* window = (Window*)user_data;
	window->window_width = window->viewport_width = width;
	window->window_height = window->viewport_height = height;
}

static void focus_callback(void* user_data, bool focused) {
	Window* window = (Window*)user_data;
	if (!focused) {
		for (KeyAction& action : window->key_map) action = KeyAction::RELEASE;
		for (MouseInput& input : window->mouse_map) input.action = KeyAction::RELEASE;
	}
	if (ImGui::GetCurrentContext()) ImGui::GetIO().AddFocusEvent(focused);
}

void init(i32 width, i32 height, bool fullscreen, bool on_second_monitor) {
	_window.key_map.resize(MAX_KEY_INPUTS);
	for (KeyAction& action : _window.key_map) action = KeyAction::RELEASE;
	_window.mouse_map.resize(MOUSE_ACTION_COUNT);
	for (MouseInput& input : _window.mouse_map) input = {};
	os::WindowDesc desc = {
		.title = "Lumen",
		.width = (u32)width,
		.height = (u32)height,
		.fullscreen = fullscreen,
		.on_second_monitor = on_second_monitor,
		.user_data = &_window,
		.key_callback = key_callback,
		.text_callback = text_callback,
		.mouse_button_callback = mouse_button_callback,
		.mouse_move_callback = mouse_move_callback,
		.mouse_scroll_callback = scroll_callback,
		.resize_callback = resize_callback,
		.focus_callback = focus_callback,
	};
	bool created = os::window_create(_window.os_window, desc);
	LUMEN_ASSERT(created, "Failed to create a window");
	if (!created) LUMEN_ERROR("Failed to create a window");
	update_window_size();
}

void poll() { os::window_poll_events(_window.os_window); }
void destroy() { os::window_destroy(_window.os_window); }

static const char* imgui_get_clipboard(void*) { return os::window_get_clipboard_text(_window.os_window); }
static void imgui_set_clipboard(void*, const char* text) { os::window_set_clipboard_text(_window.os_window, text); }

void imgui_init() {
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = "lumen_window";
	io.GetClipboardTextFn = imgui_get_clipboard;
	io.SetClipboardTextFn = imgui_set_clipboard;
	ImGui::GetMainViewport()->PlatformHandle = _window.os_window.handle;
	_imgui_previous_time = os::time_seconds();
}

void imgui_shutdown() {
	ImGuiIO& io = ImGui::GetIO();
	io.BackendPlatformName = nullptr;
	io.GetClipboardTextFn = nullptr;
	io.SetClipboardTextFn = nullptr;
}

void imgui_new_frame() {
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((f32)_window.window_width, (f32)_window.window_height);
	io.DisplayFramebufferScale =
		ImVec2(_window.window_width ? (f32)_window.viewport_width / _window.window_width : 1.0f,
			   _window.window_height ? (f32)_window.viewport_height / _window.window_height : 1.0f);
	f64 now = os::time_seconds();
	io.DeltaTime = _imgui_previous_time > 0.0 ? (f32)(now - _imgui_previous_time) : 1.0f / 60.0f;
	_imgui_previous_time = now;
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

bool is_mouse_held(MouseAction mb, lm::ivec2& pos) {
	const MouseInput* input = get_mouse_input(&_window, mb);
	if (!input || (input->action != KeyAction::PRESS && input->action != KeyAction::REPEAT)) return false;
	pos = lm::ivec2((i32)input->x, (i32)input->y);
	return true;
}
bool is_mouse_up(MouseAction mb, lm::ivec2& pos) {
	const MouseInput* input = get_mouse_input(&_window, mb);
	if (!input || input->action != KeyAction::RELEASE) return false;
	pos = lm::ivec2((i32)input->x, (i32)input->y);
	return true;
}
bool should_close() { return _window.os_window.close_requested; }
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
	os::window_framebuffer_size(_window.os_window, _window.viewport_width, _window.viewport_height);
	_window.window_width = _window.os_window.width;
	_window.window_height = _window.os_window.height;
}
void set_clipboard_text(const char* text) { os::window_set_clipboard_text(_window.os_window, text); }
f64 time_seconds() { return os::time_seconds(); }
u32 width() { return _window.viewport_width; }
u32 height() { return _window.viewport_height; }
f32 aspect_ratio() { return (f32)_window.viewport_width / _window.viewport_height; }
}  // namespace Window
