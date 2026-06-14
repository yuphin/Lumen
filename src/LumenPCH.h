#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOCRYPT
#define NOVIRTUALKEYCODES
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NORASTEROPS
#define NOSYSCOMMANDS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOGDI
#define NOKERNEL
#define NOUSER
#define NONLS
#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#undef NOWINMESSAGES
#undef NOWINSTYLES
#undef NOVIRTUALKEYCODES
#undef NOKEYSTATES
#undef NOCLIPBOARD
#undef NOUSER
#undef NOSHOWWINDOW
#undef NOSYSMETRICS
#undef NOMEMMGR
#undef NOMSG
#undef NOWINOFFSETS
#endif

#define local_persist static

typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long int s64;
typedef float f32;
typedef double f64;

#define I8_MIN         (-127i8 - 1)
#define I16_MIN        (-32767i16 - 1)
#define I32_MIN        (-2147483647i32 - 1)
#define I64_MIN        (-9223372036854775807i64 - 1)
#define I8_MAX         127i8
#define I16_MAX        32767i16
#define I32_MAX        2147483647i32
#define I64_MAX        9223372036854775807i64
#define U8_MAX        0xffui8
#define U16_MAX       0xffffui16
#define U32_MAX       0xffffffffui32
#define U64_MAX       0xffffffffffffffffui64
#define F32_MAX       3.40282347E+38F
#define F32_MIN       -F32_MAX

#define KB(n) (((u64)(n)) << 10)
#define MB(n) (((u64)(n)) << 20)
#define GB(n) (((u64)(n)) << 30)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#include <assert.h>

#include "Framework/Logger.h"
#include "Framework/Base/Math.h"
#include <volk/volk.h>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>
#include <imgui/imgui.h>
#include "imgui/imgui_impl_vulkan.h"
#include <stb/stb_sprintf.h>
#include "Framework/VulkanStructs.h"
#include <initializer_list>
