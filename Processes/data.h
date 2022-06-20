#pragma once
#include "fastmutex.h"


#define DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)
#define SIZE(a) sizeof(a)/sizeof(*a)
#define DRIVER_TAG 'lifo'

UNICODE_STRING dos = RTL_CONSTANT_STRING(L"\\??\\random");

NTSTATUS IrpComplete(PIRP Irp, NTSTATUS Status = STATUS_SUCCESS, ULONG_PTR Info = 0);
DRIVER_UNLOAD UnloadDriver;
DRIVER_DISPATCH CreateClose, IoControl;

int ref_index{};
ProcessInfo* RetProcByName(const char* name, vector<ProcessInfo*>& vec, int& index = ref_index);
void FindProcess(const char* name, vector<ProcessInfo*>& vec);
bool FindPid(ULONG pid, ProcessInfo* process = nullptr);
bool RemovePid(ULONG pid, ProcessInfo* proc);
void HideByPid(ULONG pid);

void OnProcessNotify(PEPROCESS, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);

constexpr auto SizeOf = []<size_t size>(auto(&)[size]) { return size; };
#define SIZEOF(a) SizeOf(a)

template <typename T>
class vector;

template <typename T>
struct Globals
{
	FastMutex Mutex{};
	vector<T> vec{};
};