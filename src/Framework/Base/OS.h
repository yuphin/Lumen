#pragma once
namespace lm {
struct String;
}
namespace os {
typedef u32 AccessFlags;
typedef u64 FileHandle;
enum {
	AccessFlag_Read = (1 << 0),
	AccessFlag_Write = (1 << 1),
	AccessFlag_Execute = (1 << 2),
	AccessFlag_Append = (1 << 3),
	AccessFlag_ShareRead = (1 << 4),
	AccessFlag_ShareWrite = (1 << 5),
	AccessFlag_Inherited = (1 << 6),
};

struct FileProperties {
	u64 size;
	u64 created;
	u64 modified;
};
u64 get_page_size();
void* reserve(u64 reserve_size);
bool commit(void* ptr, u64 commit_size);
u64 file_read(FileHandle handle, void* out_data, u64 size = U64_MAX);
FileHandle file_open(const lm::String& path, AccessFlags access_flags);
FileProperties file_properties(FileHandle handle);


}  // namespace os