#pragma once
#include "fastmutex.h"


#define DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)
#define DRIVER_TAG 'lifo'

UNICODE_STRING dos = RTL_CONSTANT_STRING(L"\\??\\MyDeviceLink");


DRIVER_UNLOAD UnloadDriver;
DRIVER_DISPATCH CreateClose, Read;
NTSTATUS IrpComplete(PIRP Irp, NTSTATUS Status = STATUS_SUCCESS, ULONG_PTR Info = 0);

void OnProcessNotify(PEPROCESS, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
void ImageLoadCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);
void PushItem(LIST_ENTRY* entry);
bool FindDllExePos(PANSI_STRING str, USHORT& index, USHORT& len);

template <typename T>
struct FullItem
{
	LIST_ENTRY Entry;
	T Data;
};

struct Globals
{
	LIST_ENTRY ItemsHead;
	USHORT ItemsCount;
	FastMutex Mutex;
};