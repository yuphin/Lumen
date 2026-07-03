#include "LumenPCH.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#endif	// defined(_WIN32) || defined(_WIN64)
#include "OS.h"
#include "Framework/Base/String.h"
namespace os {

#if defined(_WIN32) || defined(_WIN64)
static i32 win32_key_code(WPARAM key, LPARAM lparam) {
	if (key >= '0' && key <= '9') return (i32)key;
	if (key >= 'A' && key <= 'Z') return (i32)key;
	if (key >= VK_F1 && key <= VK_F24) return 290 + (i32)(key - VK_F1);
	if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return 320 + (i32)(key - VK_NUMPAD0);
	switch (key) {
		case VK_SPACE:
			return 32;
		case VK_OEM_7:
			return 39;
		case VK_OEM_COMMA:
			return 44;
		case VK_OEM_MINUS:
			return 45;
		case VK_OEM_PERIOD:
			return 46;
		case VK_OEM_2:
			return 47;
		case VK_OEM_1:
			return 59;
		case VK_OEM_PLUS:
			return 61;
		case VK_OEM_4:
			return 91;
		case VK_OEM_5:
			return 92;
		case VK_OEM_6:
			return 93;
		case VK_OEM_3:
			return 96;
		case VK_ESCAPE:
			return 256;
		case VK_RETURN:
			return (lparam & (1 << 24)) ? 335 : 257;
		case VK_TAB:
			return 258;
		case VK_BACK:
			return 259;
		case VK_INSERT:
			return 260;
		case VK_DELETE:
			return 261;
		case VK_RIGHT:
			return 262;
		case VK_LEFT:
			return 263;
		case VK_DOWN:
			return 264;
		case VK_UP:
			return 265;
		case VK_PRIOR:
			return 266;
		case VK_NEXT:
			return 267;
		case VK_HOME:
			return 268;
		case VK_END:
			return 269;
		case VK_CAPITAL:
			return 280;
		case VK_SCROLL:
			return 281;
		case VK_NUMLOCK:
			return 282;
		case VK_SNAPSHOT:
			return 283;
		case VK_PAUSE:
			return 284;
		case VK_DECIMAL:
			return 330;
		case VK_DIVIDE:
			return 331;
		case VK_MULTIPLY:
			return 332;
		case VK_SUBTRACT:
			return 333;
		case VK_ADD:
			return 334;
		case VK_SHIFT:
			return MapVirtualKeyA((u32)((lparam >> 16) & 0xff), MAPVK_VSC_TO_VK_EX) == VK_RSHIFT ? 344 : 340;
		case VK_CONTROL:
			return (lparam & (1 << 24)) ? 345 : 341;
		case VK_MENU:
			return (lparam & (1 << 24)) ? 346 : 342;
		case VK_LWIN:
			return 343;
		case VK_RWIN:
			return 347;
		case VK_APPS:
			return 348;
		default:
			return -1;
	}
}

static LRESULT CALLBACK window_proc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam) {
	Window* window = (Window*)GetWindowLongPtrA(handle, GWLP_USERDATA);
	if (!window && message == WM_NCCREATE) {
		CREATESTRUCTA* create = (CREATESTRUCTA*)lparam;
		window = (Window*)create->lpCreateParams;
		SetWindowLongPtrA(handle, GWLP_USERDATA, (LONG_PTR)window);
	}
	if (!window) return DefWindowProcA(handle, message, wparam, lparam);

	switch (message) {
		case WM_CLOSE:
			window->close_requested = true;
			return 0;
		case WM_SIZE: {
			RECT rect = {};
			GetClientRect(handle, &rect);
			window->width = window->framebuffer_width = (u32)(rect.right - rect.left);
			window->height = window->framebuffer_height = (u32)(rect.bottom - rect.top);
			if (window->resize_callback)
				window->resize_callback(window->user_data, window->framebuffer_width, window->framebuffer_height);
			return 0;
		}
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP: {
			i32 key = win32_key_code(wparam, lparam);
			bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
			bool repeat = pressed && (lparam & (1ll << 30));
			if (key >= 0 && window->key_callback) {
				window->key_callback(window->user_data, key, pressed, repeat, (GetKeyState(VK_CONTROL) & 0x8000) != 0,
									 (GetKeyState(VK_SHIFT) & 0x8000) != 0, (GetKeyState(VK_MENU) & 0x8000) != 0,
									 (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0);
			}
			return message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ? DefWindowProcA(handle, message, wparam, lparam)
																	  : 0;
		}
		case WM_SETFOCUS:
			if (window->focus_callback) window->focus_callback(window->user_data, true);
			return 0;
		case WM_KILLFOCUS:
			if (window->focus_callback) window->focus_callback(window->user_data, false);
			return 0;
		case WM_CHAR:
			if (window->text_callback) window->text_callback(window->user_data, (u32)wparam);
			return 0;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			SetCapture(handle);
			[[fallthrough]];
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
			if (window->mouse_button_callback) {
				i32 button = (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)	  ? 0
							 : (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) ? 1
																					  : 2;
				bool pressed = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
				window->mouse_button_callback(window->user_data, button, pressed);
			}
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) ReleaseCapture();
			return 0;
		case WM_MOUSEMOVE:
			if (window->mouse_move_callback)
				window->mouse_move_callback(window->user_data, (i16)(lparam & 0xffff), (i16)((lparam >> 16) & 0xffff));
			return 0;
		case WM_MOUSEWHEEL:
			if (window->mouse_scroll_callback)
				window->mouse_scroll_callback(window->user_data, 0.0, (i16)((wparam >> 16) & 0xffff) / 120.0);
			return 0;
		case WM_MOUSEHWHEEL:
			if (window->mouse_scroll_callback)
				window->mouse_scroll_callback(window->user_data, (i16)((wparam >> 16) & 0xffff) / 120.0, 0.0);
			return 0;
	}
	return DefWindowProcA(handle, message, wparam, lparam);
}

struct MonitorSelection {
	HMONITOR primary;
	HMONITOR secondary;
};

static BOOL CALLBACK find_secondary_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
	MonitorSelection* selection = (MonitorSelection*)data;
	if (monitor != selection->primary && !selection->secondary) selection->secondary = monitor;
	return TRUE;
}
#else
static i32 x11_key_code(KeySym key) {
	if (key >= XK_0 && key <= XK_9) return (i32)key;
	if (key >= XK_a && key <= XK_z) return (i32)(key - XK_a + 'A');
	if (key >= XK_A && key <= XK_Z) return (i32)key;
	if (key >= XK_F1 && key <= XK_F24) return 290 + (i32)(key - XK_F1);
	switch (key) {
		case XK_space:
			return 32;
		case XK_apostrophe:
			return 39;
		case XK_comma:
			return 44;
		case XK_minus:
			return 45;
		case XK_period:
			return 46;
		case XK_slash:
			return 47;
		case XK_semicolon:
			return 59;
		case XK_equal:
			return 61;
		case XK_bracketleft:
			return 91;
		case XK_backslash:
			return 92;
		case XK_bracketright:
			return 93;
		case XK_grave:
			return 96;
		case XK_Escape:
			return 256;
		case XK_Return:
			return 257;
		case XK_Tab:
			return 258;
		case XK_BackSpace:
			return 259;
		case XK_Insert:
			return 260;
		case XK_Delete:
			return 261;
		case XK_Right:
			return 262;
		case XK_Left:
			return 263;
		case XK_Down:
			return 264;
		case XK_Up:
			return 265;
		case XK_Page_Up:
			return 266;
		case XK_Page_Down:
			return 267;
		case XK_Home:
			return 268;
		case XK_End:
			return 269;
		case XK_Shift_L:
			return 340;
		case XK_Control_L:
			return 341;
		case XK_Alt_L:
			return 342;
		case XK_Super_L:
			return 343;
		case XK_Shift_R:
			return 344;
		case XK_Control_R:
			return 345;
		case XK_Alt_R:
			return 346;
		case XK_Super_R:
			return 347;
		default:
			return -1;
	}
}
#endif

bool window_create(Window& window, const WindowDesc& desc) {
	window.user_data = desc.user_data;
	window.key_callback = desc.key_callback;
	window.text_callback = desc.text_callback;
	window.mouse_button_callback = desc.mouse_button_callback;
	window.mouse_move_callback = desc.mouse_move_callback;
	window.mouse_scroll_callback = desc.mouse_scroll_callback;
	window.resize_callback = desc.resize_callback;
	window.focus_callback = desc.focus_callback;
	window.width = window.framebuffer_width = desc.width;
	window.height = window.framebuffer_height = desc.height;
#if defined(_WIN32) || defined(_WIN64)
	static bool _class_registered = false;
	static const char* _class_name = "LumenWindowClass";
	HINSTANCE instance = GetModuleHandleA(nullptr);
	if (!_class_registered) {
		WNDCLASSA window_class = {};
		window_class.lpfnWndProc = window_proc;
		window_class.hInstance = instance;
		window_class.lpszClassName = _class_name;
		window_class.hCursor = LoadCursorA(nullptr, (LPCSTR)IDC_ARROW);
		if (!RegisterClassA(&window_class)) return false;
		_class_registered = true;
	}
	HMONITOR primary = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
	MonitorSelection selection = {primary, nullptr};
	EnumDisplayMonitors(nullptr, nullptr, find_secondary_monitor, (LPARAM)&selection);
	HMONITOR monitor = desc.on_second_monitor && selection.secondary ? selection.secondary : primary;
	MONITORINFO monitor_info = {sizeof(MONITORINFO)};
	GetMonitorInfoA(monitor, &monitor_info);
	DWORD style = desc.fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
	RECT rect = desc.fullscreen ? monitor_info.rcMonitor : RECT{0, 0, (LONG)desc.width, (LONG)desc.height};
	if (!desc.fullscreen) AdjustWindowRect(&rect, style, FALSE);
	i32 width = rect.right - rect.left;
	i32 height = rect.bottom - rect.top;
	i32 x = desc.fullscreen
				? rect.left
				: monitor_info.rcWork.left + (monitor_info.rcWork.right - monitor_info.rcWork.left - width) / 2;
	i32 y = desc.fullscreen
				? rect.top
				: monitor_info.rcWork.top + (monitor_info.rcWork.bottom - monitor_info.rcWork.top - height) / 2;
	HWND handle =
		CreateWindowExA(0, _class_name, desc.title, style, x, y, width, height, nullptr, nullptr, instance, &window);
	if (!handle) return false;
	window.handle = handle;
	ShowWindow(handle, SW_SHOW);
	UpdateWindow(handle);
	RECT client = {};
	GetClientRect(handle, &client);
	window.width = window.framebuffer_width = (u32)(client.right - client.left);
	window.height = window.framebuffer_height = (u32)(client.bottom - client.top);
#else
	Display* display = XOpenDisplay(nullptr);
	if (!display) return false;
	i32 screen = DefaultScreen(display);
	::Window root = RootWindow(display, screen);
	::Window handle = XCreateSimpleWindow(display, root, 0, 0, desc.width, desc.height, 0, 0, 0);
	XStoreName(display, handle, desc.title);
	XSelectInput(display, handle,
				 KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
					 StructureNotifyMask | FocusChangeMask);
	Atom delete_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, handle, &delete_atom, 1);
	XMapWindow(display, handle);
	window.display = display;
	window.handle = (void*)(uintptr_t)handle;
	window.platform_data[0] = (u64)delete_atom;
#endif
	return true;
}

void window_destroy(Window& window) {
#if defined(_WIN32) || defined(_WIN64)
	if (window.handle) DestroyWindow((HWND)window.handle);
#else
	if (window.display && window.handle) XDestroyWindow((Display*)window.display, (::Window)(uintptr_t)window.handle);
	if (window.display) XCloseDisplay((Display*)window.display);
#endif
	window = {};
}

void window_poll_events(Window& window) {
#if defined(_WIN32) || defined(_WIN64)
	MSG message;
	while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}
#else
	Display* display = (Display*)window.display;
	while (XPending(display)) {
		XEvent event;
		XNextEvent(display, &event);
		switch (event.type) {
			case ClientMessage:
				if ((u64)event.xclient.data.l[0] == window.platform_data[0]) window.close_requested = true;
				break;
			case ConfigureNotify:
				window.width = window.framebuffer_width = (u32)event.xconfigure.width;
				window.height = window.framebuffer_height = (u32)event.xconfigure.height;
				if (window.resize_callback) window.resize_callback(window.user_data, window.width, window.height);
				break;
			case FocusIn:
			case FocusOut:
				if (window.focus_callback) window.focus_callback(window.user_data, event.type == FocusIn);
				break;
			case MotionNotify:
				if (window.mouse_move_callback)
					window.mouse_move_callback(window.user_data, event.xmotion.x, event.xmotion.y);
				break;
			case ButtonPress:
			case ButtonRelease:
				if (event.xbutton.button == Button4 || event.xbutton.button == Button5) {
					if (event.type == ButtonPress && window.mouse_scroll_callback)
						window.mouse_scroll_callback(window.user_data, 0, event.xbutton.button == Button4 ? 1 : -1);
				} else if (window.mouse_button_callback) {
					i32 button = event.xbutton.button == Button1 ? 0 : event.xbutton.button == Button3 ? 1 : 2;
					window.mouse_button_callback(window.user_data, button, event.type == ButtonPress);
				}
				break;
			case KeyPress:
			case KeyRelease: {
				KeySym symbol = XLookupKeysym(&event.xkey, 0);
				i32 key = x11_key_code(symbol);
				if (key >= 0 && window.key_callback) {
					u32 state = event.xkey.state;
					window.key_callback(window.user_data, key, event.type == KeyPress, false, state & ControlMask,
										state & ShiftMask, state & Mod1Mask, state & Mod4Mask);
				}
				if (event.type == KeyPress && window.text_callback) {
					char text[8] = {};
					KeySym unused;
					i32 count = XLookupString(&event.xkey, text, sizeof(text), &unused, nullptr);
					if (count == 1) window.text_callback(window.user_data, (u8)text[0]);
				}
			} break;
		}
	}
#endif
}

void window_wait_events(Window& window) {
#if defined(_WIN32) || defined(_WIN64)
	WaitMessage();
	window_poll_events(window);
#else
	XEvent event;
	XNextEvent((Display*)window.display, &event);
	XPutBackEvent((Display*)window.display, &event);
	window_poll_events(window);
#endif
}

void window_framebuffer_size(Window& window, u32& width, u32& height) {
	width = window.framebuffer_width;
	height = window.framebuffer_height;
}

void window_set_clipboard_text(Window& window, const char* text) {
#if defined(_WIN32) || defined(_WIN64)
	if (!OpenClipboard((HWND)window.handle)) return;
	EmptyClipboard();
	u64 size = strlen(text) + 1;
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
	if (memory) {
		void* data = GlobalLock(memory);
		memcpy(data, text, size);
		GlobalUnlock(memory);
		SetClipboardData(CF_TEXT, memory);
	}
	CloseClipboard();
#else
	XStoreBytes((Display*)window.display, text, (i32)strlen(text));
#endif
}

const char* window_get_clipboard_text(Window& window) {
	static char _buffer[4096];
	_buffer[0] = 0;
#if defined(_WIN32) || defined(_WIN64)
	if (!OpenClipboard((HWND)window.handle)) return _buffer;
	HANDLE memory = GetClipboardData(CF_TEXT);
	if (memory) {
		const char* text = (const char*)GlobalLock(memory);
		if (text) {
			strncpy_s(_buffer, sizeof(_buffer), text, _TRUNCATE);
			GlobalUnlock(memory);
		}
	}
	CloseClipboard();
#else
	i32 size = 0;
	char* text = XFetchBytes((Display*)window.display, &size);
	if (text) {
		if (size >= (i32)sizeof(_buffer)) size = (i32)sizeof(_buffer) - 1;
		memcpy(_buffer, text, size);
		_buffer[size] = 0;
		XFree(text);
	}
#endif
	return _buffer;
}

u32 window_required_vulkan_extensions(const char** extensions, u32 capacity) {
	LUMEN_ASSERT(capacity >= 2, "Window Vulkan extension array is too small");
	if (capacity < 2) return 0;
	extensions[0] = "VK_KHR_surface";
#if defined(_WIN32) || defined(_WIN64)
	extensions[1] = "VK_KHR_win32_surface";
#else
	extensions[1] = "VK_KHR_xlib_surface";
#endif
	return 2;
}

i32 window_create_vulkan_surface(Window& window, void* instance, void* surface) {
#if defined(_WIN32) || defined(_WIN64)
	VkWin32SurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
	create_info.hinstance = GetModuleHandleA(nullptr);
	create_info.hwnd = (HWND)window.handle;
	return (i32)vkCreateWin32SurfaceKHR((VkInstance)instance, &create_info, nullptr, (VkSurfaceKHR*)surface);
#else
	VkXlibSurfaceCreateInfoKHR create_info = {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
	create_info.dpy = (Display*)window.display;
	create_info.window = (::Window)(uintptr_t)window.handle;
	return (i32)vkCreateXlibSurfaceKHR((VkInstance)instance, &create_info, nullptr, (VkSurfaceKHR*)surface);
#endif
}

f64 time_seconds() {
#if defined(_WIN32) || defined(_WIN64)
	LARGE_INTEGER counter;
	static LARGE_INTEGER _frequency = [] {
		LARGE_INTEGER value;
		QueryPerformanceFrequency(&value);
		return value;
	}();
	QueryPerformanceCounter(&counter);
	return (f64)counter.QuadPart / (f64)_frequency.QuadPart;
#else
	timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (f64)time.tv_sec + (f64)time.tv_nsec / 1000000000.0;
#endif
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI thread_entry(void* raw_thread) {
	Thread* thread = (Thread*)raw_thread;
	thread->procedure(thread->data);
	return 0;
}
#else
static void* thread_entry(void* raw_thread) {
	Thread* thread = (Thread*)raw_thread;
	thread->procedure(thread->data);
	return nullptr;
}
#endif

u32 processor_count() {
#if defined(_WIN32) || defined(_WIN64)
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwNumberOfProcessors;
#else
	long count = sysconf(_SC_NPROCESSORS_ONLN);
	return count > 0 ? (u32)count : 1;
#endif
}

bool thread_start(Thread& thread, ThreadProcedure procedure, void* data) {
	thread.procedure = procedure;
	thread.data = data;
#if defined(_WIN32) || defined(_WIN64)
	thread.handle = CreateThread(nullptr, 0, thread_entry, &thread, 0, nullptr);
	if (!thread.handle) {
		thread.procedure = nullptr;
		thread.data = nullptr;
		return false;
	}
#else
	if (pthread_create(&thread.native, nullptr, thread_entry, &thread) != 0) {
		thread.procedure = nullptr;
		thread.data = nullptr;
		return false;
	}
	thread.started = true;
#endif
	return true;
}

void thread_join(Thread& thread) {
#if defined(_WIN32) || defined(_WIN64)
	if (!thread.handle) return;
	WaitForSingleObject((HANDLE)thread.handle, INFINITE);
	CloseHandle((HANDLE)thread.handle);
	thread.handle = nullptr;
#else
	if (!thread.started) return;
	pthread_join(thread.native, nullptr);
	thread.started = false;
#endif
	thread.procedure = nullptr;
	thread.data = nullptr;
}

void thread_set_name(Thread& thread, const char* name) {
#if defined(_WIN32) || defined(_WIN64)
	if (!thread.handle) return;
	wchar_t wide_name[64] = {};
	u32 i = 0;
	for (; name[i] && i + 1 < ARRAY_SIZE(wide_name); ++i) {
		wide_name[i] = (wchar_t)(u8)name[i];
	}
	wide_name[i] = L'\0';
	SetThreadDescription((HANDLE)thread.handle, wide_name);
#elif defined(__linux__)
	if (thread.started) {
		pthread_setname_np(thread.native, name);
	}
#endif
}

#if defined(_WIN32) || defined(_WIN64)
void Mutex::lock() { AcquireSRWLockExclusive((PSRWLOCK)&native); }
void Mutex::unlock() { ReleaseSRWLockExclusive((PSRWLOCK)&native); }

void ConditionVariable::wait(Mutex& mutex) {
	SleepConditionVariableSRW((PCONDITION_VARIABLE)&native, (PSRWLOCK)&mutex.native, INFINITE, 0);
}
void ConditionVariable::notify_one() { WakeConditionVariable((PCONDITION_VARIABLE)&native); }
void ConditionVariable::notify_all() { WakeAllConditionVariable((PCONDITION_VARIABLE)&native); }

Semaphore::Semaphore(u32 initial_count, u32 max_count) {
	handle = CreateSemaphoreA(nullptr, (LONG)initial_count, (LONG)max_count, nullptr);
	LUMEN_ASSERT(handle, "Failed to create semaphore");
}
Semaphore::~Semaphore() {
	if (handle) CloseHandle((HANDLE)handle);
}
void Semaphore::acquire() { WaitForSingleObject((HANDLE)handle, INFINITE); }
void Semaphore::release() { ReleaseSemaphore((HANDLE)handle, 1, nullptr); }
#else
Mutex::Mutex() { pthread_mutex_init(&native, nullptr); }
Mutex::~Mutex() { pthread_mutex_destroy(&native); }
void Mutex::lock() { pthread_mutex_lock(&native); }
void Mutex::unlock() { pthread_mutex_unlock(&native); }

ConditionVariable::ConditionVariable() { pthread_cond_init(&native, nullptr); }
ConditionVariable::~ConditionVariable() { pthread_cond_destroy(&native); }
void ConditionVariable::wait(Mutex& mutex) { pthread_cond_wait(&native, &mutex.native); }
void ConditionVariable::notify_one() { pthread_cond_signal(&native); }
void ConditionVariable::notify_all() { pthread_cond_broadcast(&native); }

Semaphore::Semaphore(u32 initial_count, u32 max_count) {
	(void)max_count;
	i32 result = sem_init(&native, 0, initial_count);
	LUMEN_ASSERT(result == 0, "Failed to create semaphore");
}
Semaphore::~Semaphore() { sem_destroy(&native); }
void Semaphore::acquire() {
	while (sem_wait(&native) != 0 && errno == EINTR) {
	}
}
void Semaphore::release() { sem_post(&native); }
#endif

u64 get_page_size() {
#if defined(_WIN32) || defined(_WIN64)
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwPageSize;
#else
	return sysconf(_SC_PAGE_SIZE);
#endif	// defined(_WIN32) || defined(_WIN64)
}

void reserve(void* ptr, u64 size) {
#if defined(_WIN32) || defined(_WIN64)
	void* res = VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_NOACCESS);
	assert(res == ptr);
#else
	void* result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(result == ptr);
#endif
}

void* reserve(u64 reserve_size) {
	void* data_base;
#if defined(_WIN32) || defined(_WIN64)
	data_base = VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_NOACCESS);
#else
	data_base = mmap(0, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (data_base == MAP_FAILED) {
		data_base = nullptr;
	}
#endif	// defined(_WIN32) || defined(_WIN64)
	return data_base;
}
bool commit(void* ptr, u64 commit_size) {
#if defined(_WIN32) || defined(_WIN64)
	return (VirtualAlloc(ptr, commit_size, MEM_COMMIT, PAGE_READWRITE) != 0);
#else
	return mprotect(ptr, commit_size, PROT_READ | PROT_WRITE) == 0;
#endif	// defined(_WIN32) || defined(_WIN64)
}

u64 file_read(FileHandle handle, void* out_data, u64 size) {
	if (handle == 0) {
		return 0;
	}
#if defined(_WIN32) || defined(_WIN64)
	HANDLE os_handle = (HANDLE)handle;
	if (size == U64_MAX) {
		GetFileSizeEx(os_handle, (PLARGE_INTEGER)&size);
	}
	u64 total_bytes_read = 0;

	for (u64 offset = 0; offset < size; offset += total_bytes_read) {
		DWORD bytes_read;
		OVERLAPPED overlapped = {0};
		overlapped.Offset = (offset & 0x00000000ffffffffull);
		overlapped.OffsetHigh = (offset & 0xffffffff00000000ull) >> 32;
		u64 remaining = size - offset;
		if (remaining > U32_MAX) {
			remaining = U32_MAX;
		}
		if (!ReadFile(os_handle, (u8*)out_data + offset, (DWORD)remaining, &bytes_read, &overlapped)) {
			return 0;
		}
		total_bytes_read += bytes_read;
	}
	return total_bytes_read;

#else
	int fd = (int)handle;
	if (size == U64_MAX) {
		struct stat file_stat;
		if (fstat(fd, &file_stat) == 0) {
			size = file_stat.st_size;
		} else {
			return 0;
		}
	}
	u64 total_bytes_read = 0;
	u64 bytes_left = size;
	while (bytes_left > 0) {
		ssize_t result = pread(fd, (u8*)out_data + total_bytes_read, bytes_left, total_bytes_read);
		if (result > 0) {
			total_bytes_read += (u64)result;
			bytes_left -= (u64)result;
		} else if (result == 0) {
			break;
		} else if (errno != EINTR) {
			return 0;
		}
	}
	return total_bytes_read;
#endif	// defined(_WIN32) || defined(_WIN64)
}

u64 file_write(FileHandle handle, void* in_data, u64 size) {
	if (handle == 0) {
		return 0;
	}
#if defined(_WIN32) || defined(_WIN64)
	HANDLE win_handle = (HANDLE)handle;
	u64 src_offset = 0;
	u64 dst_offset = 0;
	for (;;) {
		DWORD bytes_left = DWORD(size - src_offset);
		if (bytes_left == 0) {
			break;
		}
		DWORD bytes_written = 0;
		void* data = (u8*)in_data + src_offset;
		OVERLAPPED overlapped = {0};
		overlapped.Offset = (dst_offset & 0x00000000ffffffffull);
		overlapped.OffsetHigh = (dst_offset & 0xffffffff00000000ull) >> 32;
		BOOL success = WriteFile(win_handle, data, bytes_left, &bytes_written, &overlapped);
		if (success == 0) {
			break;
		}
		src_offset += bytes_written;
		dst_offset += bytes_written;
	}
	return src_offset;
#else
	int fd = (int)handle;
	u64 src_offset = 0;
	u64 dst_offset = 0;
	for (;;) {
		u64 bytes_left = size - src_offset;
		if (bytes_left == 0) break;
		ssize_t n = pwrite(fd, (u8*)in_data + src_offset, bytes_left, (off_t)dst_offset);
		if (n < 0) {
			if (errno == EINTR) continue;
			break;
		}
		src_offset += (u64)n;
		dst_offset += (u64)n;
	}
	return src_offset;
#endif
}

FileHandle file_open(const lm::String& path, AccessFlags access_flags) {
	assert(path.is_cstr());
#if defined(_WIN32) || defined(_WIN64)
	DWORD access = 0;
	DWORD creation_disposition = OPEN_EXISTING;
	if (access_flags & AccessFlag_Read) {
		access |= GENERIC_READ;
	}
	if (access_flags & AccessFlag_Write) {
		access |= GENERIC_WRITE;
		creation_disposition = CREATE_ALWAYS;
	};
	if (access_flags & AccessFlag_Execute) {
		access |= GENERIC_EXECUTE;
	}

	HANDLE file_handle = CreateFileA(path.data, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, creation_disposition,
									 FILE_ATTRIBUTE_NORMAL, NULL);

	if (file_handle == INVALID_HANDLE_VALUE) {
		return 0;
	}
	return (FileHandle)file_handle;
#else
	int flags = 0;
	if ((access_flags & AccessFlag_Read) && (access_flags & AccessFlag_Write)) {
		flags |= O_RDWR | O_CREAT | O_TRUNC;
	} else if (access_flags & AccessFlag_Read) {
		flags |= O_RDONLY;
	} else if (access_flags & AccessFlag_Write) {
		flags |= O_WRONLY | O_CREAT | O_TRUNC;
	}
	if (access_flags & AccessFlag_Append) {
		flags |= O_APPEND;
	}

	int fd = open(path.data, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		return 0;
	}
	return (FileHandle)fd;

#endif	// defined(_WIN32) || defined(_WIN64)
}

void file_close(FileHandle handle) {
	if (handle == 0) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	CloseHandle((HANDLE)handle);
#else
	close((int)handle);
#endif
}

bool directory_create(const lm::String& path) {
	assert(path.is_cstr());
#if defined(_WIN32) || defined(_WIN64)
	if (CreateDirectoryA(path.data, NULL)) {
		return true;
	}
	return GetLastError() == ERROR_ALREADY_EXISTS;
#else
	if (mkdir(path.data, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0) {
		return true;
	}
	return errno == EEXIST;
#endif
}

FileProperties file_properties(FileHandle handle) {
	if (handle == 0) {
		return {};
	}

#if defined(_WIN32) || defined(_WIN64)
	HANDLE file_handle = (HANDLE)handle;
	BY_HANDLE_FILE_INFORMATION info;
	BOOL success = GetFileInformationByHandle(file_handle, &info);
	FileProperties properties = {};
	if (success) {
		u32 size_low = info.nFileSizeLow;
		u32 size_high = info.nFileSizeHigh;
		u64 size = ((u64)size_high << 32) | size_low;
		properties = {
			.size = size,
			.created = ((u64)info.ftCreationTime.dwHighDateTime << 32) | info.ftCreationTime.dwLowDateTime,
			.modified = ((u64)info.ftLastWriteTime.dwHighDateTime << 32) | info.ftLastWriteTime.dwLowDateTime};
	}
	return properties;
#else
	int fd = (int)handle;
	struct stat file_stat;
	if (fstat(fd, &file_stat) == 0) {
		FileProperties properties = {
			.size = (u64)file_stat.st_size,
			.created = (u64)file_stat.st_ctime,
			.modified = (u64)file_stat.st_mtime};
		return properties;
	}
	return {};
#endif
}

}  // namespace os
