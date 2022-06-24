#include <ntifs.h>
#include "data.h"
#include "common.h"
#include "autolock.h"


Globals _globals;


extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING)
{
	InitializeListHead(&_globals.ItemsHead);
	_globals.Mutex.Init();

	auto status = STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject = nullptr;
	bool symLink = false, loadImage = false, pNotify = false;

	do
	{
		UNICODE_STRING dev = RTL_CONSTANT_STRING(L"\\Device\\MyDevice");
		status = IoCreateDevice(pDriverObject, 0, &dev, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed IoCreateDevice -> Status=(%x)\n", status);
			break;
		}

		DeviceObject->Flags |= DO_DIRECT_IO;
		DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

		status = IoCreateSymbolicLink(&dos, &dev);
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed IoCreateSymbolicLink -> Status=(%x)\n", status);
			break;
		}
		symLink = true;

		status = PsSetLoadImageNotifyRoutine(ImageLoadCallback);
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed PsSetLoadImageNotifyRoutine -> Status=(%x)\n", status);
			break;
		}
		loadImage = true;

		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed PsSetCreateProcessNotifyRoutineEx -> Status=(%x)\n", status);
			break;
		}
		pNotify = true;

		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status))
		{
			DbgMsg("failed PsSetCreateThreadNotifyRoutine -> Status=(%x)\n", status);
			break;
		}

	} while (false);

	if (!NT_SUCCESS(status))
	{
		if (pNotify)
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
		if (loadImage)
			PsRemoveLoadImageNotifyRoutine(ImageLoadCallback);
		if (symLink)
			IoDeleteSymbolicLink(&dos);
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
		return status;
	}

	pDriverObject->DriverUnload = UnloadDriver;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = \
		pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateClose;
	pDriverObject->MajorFunction[IRP_MJ_READ] = Read;

	DbgMsg("Driver loaded\n");
	return status;
}



void UnloadDriver(PDRIVER_OBJECT pDriverObject)
{
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	PsRemoveLoadImageNotifyRoutine(ImageLoadCallback);
	IoDeleteSymbolicLink(&dos);
	IoDeleteDevice(pDriverObject->DeviceObject);

	while (!IsListEmpty(&_globals.ItemsHead))
	{
		auto entry = RemoveHeadList(&_globals.ItemsHead);
		ExFreePool(CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry));
	}

	DbgMsg("Driver unloaded\n");
}

NTSTATUS IrpComplete(PIRP Irp, NTSTATUS Status, ULONG_PTR Info)
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}

NTSTATUS CreateClose(PDEVICE_OBJECT, PIRP Irp)
{
	auto stack = IoGetCurrentIrpStackLocation(Irp);

	switch (stack->MajorFunction)
	{
	case IRP_MJ_CREATE:
		DbgMsg("Handle to SymbolicLink %wZ opened\n", dos);
		break;

	case IRP_MJ_CLOSE:
		DbgMsg("Handle to SymbolicLink %wZ closed\n", dos);
		break;

	default:
		break;
	}

	return IrpComplete(Irp);
}

NTSTATUS Read(PDEVICE_OBJECT, PIRP Irp)
{
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Read.Length;
	auto status = STATUS_SUCCESS;
	ULONG count = 0;
	NT_ASSERT(Irp->MdlAddress);

	auto buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);
	if (!buffer)
		status = STATUS_INSUFFICIENT_RESOURCES;
	else
	{
		AutoLock lock(_globals.Mutex);
		while (true)
		{
			if (IsListEmpty(&_globals.ItemsHead))
				break;

			auto entry = RemoveHeadList(&_globals.ItemsHead);
			auto item = CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry);
			auto size = item->Data.Size;

			if (len < size)
			{
				InsertHeadList(&_globals.ItemsHead, entry);
				break;
			}
			--_globals.ItemsCount;

			memcpy(buffer, &item->Data, size);
			len -= size;
			buffer += size;
			count += size;
			ExFreePool(item);
		}
	}

	return IrpComplete(Irp, status, count);
}

void OnProcessNotify(PEPROCESS, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	if (CreateInfo)
	{
		USHORT allocSize = sizeof(FullItem<ProcessCreateInfo>);
		USHORT CommandLineSize = 0, index = 0;
		ANSI_STRING str{};

		if (CreateInfo->CommandLine)
		{
			UNICODE_STRING tmp{};
			tmp.Buffer = (PWCH)ExAllocatePoolWithTag(PagedPool, CreateInfo->CommandLine->Length, DRIVER_TAG);
			if (!tmp.Buffer)
				return;

			tmp.MaximumLength = CreateInfo->CommandLine->Length;
			RtlCopyUnicodeString(&tmp, CreateInfo->CommandLine);
			if (!NT_SUCCESS(RtlUnicodeStringToAnsiString(&str, &tmp, TRUE)))
			{
				ExFreePool(tmp.Buffer);
				return;
			}
			ExFreePool(tmp.Buffer);

			USHORT len{};
			if (!FindDllExePos(&str, index, len))
			{
				RtlFreeAnsiString(&str);
				return;
			}

			CommandLineSize = len;
			allocSize += CommandLineSize + 1;
		}

		auto info = (FullItem<ProcessCreateInfo>*)ExAllocatePoolWithTag(PagedPool,
			allocSize, DRIVER_TAG);
		if (!info)
		{
			DbgMsg("(OnProcessNotify) -> failed allocation\n");
			return;
		}

		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.SystemTime);
		ExSystemTimeToLocalTime(&item.SystemTime, &item.LocalTime);
		item.Type = ItemType::ProcessCreate;
		item.ProcessId = HandleToULong(ProcessId);
		item.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
		item.Size = sizeof(ProcessCreateInfo) + CommandLineSize + 1;

		if (!str.Buffer || !(CommandLineSize > 0))
		{
			RtlFreeAnsiString(&str);
			return;
		}

		memcpy((UCHAR*)&item + sizeof(ProcessCreateInfo), &str.Buffer[index], CommandLineSize);
		item.CommandLineLength = CommandLineSize;
		item.CommandLineOffset = sizeof(item);

		RtlFreeAnsiString(&str);
		PushItem(&info->Entry);
	}
	else
	{
		auto info = (FullItem<ProcessExitInfo>*)ExAllocatePoolWithTag(PagedPool,
			sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
		if (!info)
		{
			DbgMsg("(OnProcessNotify) -> failed allocation\n");
			return;
		}

		auto& item = info->Data;
		KeQuerySystemTimePrecise(&item.SystemTime);
		ExSystemTimeToLocalTime(&item.SystemTime, &item.LocalTime);
		item.Type = ItemType::ProcessExit;
		item.ProcessId = HandleToULong(ProcessId);
		item.Size = sizeof(ProcessExitInfo);

		PushItem(&info->Entry);
	}
}

void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
	auto info = (FullItem<ThreadCreateExitInfo>*)ExAllocatePoolWithTag(PagedPool,
		sizeof(FullItem<ThreadCreateExitInfo>), DRIVER_TAG);
	if (!info)
	{
		DbgMsg("(OnThreadNotify) -> failed allocation\n");
		return;
	}

	auto& item = info->Data;
	KeQuerySystemTimePrecise(&item.SystemTime);
	ExSystemTimeToLocalTime(&item.SystemTime, &item.LocalTime);
	item.ProcessId = HandleToULong(ProcessId);
	item.ThreadId = HandleToULong(ThreadId);
	item.Size = sizeof(item);
	item.Type = Create ? ItemType::ThreadCreate : ItemType::ThreadExit;

	PushItem(&info->Entry);
}

void ImageLoadCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo)
{
	UNREFERENCED_PARAMETER(FullImageName);
	USHORT allocSize = sizeof(FullItem<ImageLoadInfo>);
	USHORT imageNameSize = 0, dllNameSize = 0, nameIndex = 0, dllIndex = 0;
	ANSI_STRING dll{}, name{};
	//if (FullImageName)
	{
		PUNICODE_STRING tmp{ nullptr };
		PEPROCESS process{ nullptr };
		IMAGE_INFO_EX* exInfo{ nullptr };		UNICODE_STRING fileName{};

		if (!NT_SUCCESS(PsLookupProcessByProcessId(ProcessId, &process)))
			return;
		if (!NT_SUCCESS(SeLocateProcessImageName(process, &tmp)))
		{
			ObDereferenceObject(process);
			return;
		}
		ObDereferenceObject(process);

		auto size{ 0 };
		if (ImageInfo->ExtendedInfoPresent)		{
			exInfo = CONTAINING_RECORD(ImageInfo, IMAGE_INFO_EX, ImageInfo);
			size = tmp->Length > exInfo->FileObject->FileName.Length ?
				tmp->Length : exInfo->FileObject->FileName.Length;
			fileName.Buffer = (PWCH)ExAllocatePoolWithTag(PagedPool,
				size, DRIVER_TAG);
			if (!fileName.Buffer)
				return;

			fileName.MaximumLength = tmp->Length;
			RtlCopyUnicodeString(&fileName, tmp);
			if (!NT_SUCCESS(RtlUnicodeStringToAnsiString(&name, &fileName, TRUE)))
			{
				ExFreePool(fileName.Buffer);
				return;
			}

			fileName.MaximumLength = exInfo->FileObject->FileName.Length;
			RtlCopyUnicodeString(&fileName, &exInfo->FileObject->FileName);
			if (!NT_SUCCESS(RtlUnicodeStringToAnsiString(&dll, &fileName, TRUE)))
			{
				RtlFreeAnsiString(&name);
				ExFreePool(fileName.Buffer);
				return;
			}
			ExFreePool(fileName.Buffer);

			USHORT nameLen = 0, dllLen = 0;
			if (!FindDllExePos(&name, nameIndex, nameLen)
				|| !FindDllExePos(&dll, dllIndex, dllLen))
			{
				RtlFreeAnsiString(&name);
				RtlFreeAnsiString(&dll);
				return;
			}

			imageNameSize = nameLen;
			dllNameSize = dllLen;
			allocSize += imageNameSize + dllNameSize + 2;
		}
	}

	auto info = (FullItem<ImageLoadInfo>*)ExAllocatePoolWithTag(PagedPool,
		allocSize, DRIVER_TAG);
	if (!info)
	{
		DbgMsg("(ImageLoadCallback) -> failed allocation\n");
		return;
	}

	auto& item = info->Data;
	KeQuerySystemTimePrecise(&item.SystemTime);
	ExSystemTimeToLocalTime(&item.SystemTime, &item.LocalTime);
	item.ProcessId = HandleToULong(ProcessId);
	item.Type = ItemType::ImageLoad;
	item.Size = sizeof(ImageLoadInfo) + imageNameSize + dllNameSize + 2;

	if (!name.Buffer || !dll.Buffer || !(imageNameSize > 0) || !(dllNameSize > 0))
	{
		RtlFreeAnsiString(&name);
		RtlFreeAnsiString(&dll);
		return;
	}

	memcpy((UCHAR*)&item + sizeof(ImageLoadInfo), &name.Buffer[nameIndex], imageNameSize);
	memcpy((UCHAR*)&item + sizeof(ImageLoadInfo) + imageNameSize + 1, &dll.Buffer[dllIndex], dllNameSize);
	item.ImageNameLength = imageNameSize;
	item.ImageNameOffset = sizeof(ImageLoadInfo);
	item.DllNameLength = dllNameSize;
	item.DllNameOffset = sizeof(ImageLoadInfo) + imageNameSize + 1;

	RtlFreeAnsiString(&name);
	RtlFreeAnsiString(&dll);
		
	PushItem(&info->Entry);
}

void PushItem(LIST_ENTRY* entry)
{
	AutoLock lock(_globals.Mutex);
	if (_globals.ItemsCount > 1024)
	{
		auto item = RemoveHeadList(&_globals.ItemsHead);
		--_globals.ItemsCount;
		ExFreePool(CONTAINING_RECORD(item, FullItem<ItemHeader>, Entry));
	}
	InsertTailList(&_globals.ItemsHead, entry);
	++_globals.ItemsCount;
}

bool FindDllExePos(PANSI_STRING str, USHORT& index, USHORT& len)
{
	if (str->Buffer)
	{
		for (USHORT i = 0; i < str->Length; ++i)
		{
			if (str->Buffer[i] == '.' && str->Buffer[i + 1] == 'd'
				&& str->Buffer[i + 2] == 'l' && str->Buffer[i + 3] == 'l'
				|| str->Buffer[i] == '.' && str->Buffer[i + 1] == 'e'
				&& str->Buffer[i + 2] == 'x' && str->Buffer[i + 3] == 'e')
			{
				index = i;
				len = 3;
				for (;index > 0; --index)
				{
					++len;
					if (str->Buffer[index - 1] == '\\')
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}