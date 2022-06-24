#pragma once



#define IO__PROCESS_NOTIFY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_THREAD_NOTIFY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_IMAGE_CALLBACK CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_RANDOM_SHIT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)


enum class ItemType : short
{
	None,
	ProcessCreate,
	ProcessExit,
	ThreadCreate,
	ThreadExit,
	ImageLoad
};

struct ItemHeader
{
	ItemType Type;
	ULONG Size;
	LARGE_INTEGER SystemTime;
	LARGE_INTEGER LocalTime;
};

struct ProcessCreateInfo : ItemHeader
{
	ULONG ParentProcessId;
	ULONG ProcessId;
	USHORT CommandLineLength;
	USHORT CommandLineOffset;
};

struct ProcessExitInfo : ItemHeader
{
	ULONG ProcessId;
};

struct ThreadCreateExitInfo : ItemHeader
{
	ULONG ProcessId;
	ULONG ThreadId;
};

struct ImageLoadInfo : ItemHeader
{
	ULONG ProcessId;
	USHORT ImageNameLength;
	USHORT ImageNameOffset;
	USHORT DllNameLength;
	USHORT DllNameOffset;
};