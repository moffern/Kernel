#pragma once
#include <iostream>
#include <Windows.h>
#include "..\KMDF Driver19\common.h"



class KCom
{
	HANDLE hDriver{ nullptr };
public:
	KCom(LPCWSTR RegistryPath)
	{
		hDriver = CreateFile(RegistryPath, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	}

	bool AddProcessByName(const char* name)
	{
		if (hDriver == INVALID_HANDLE_VALUE)
			return false;

		DWORD bytes;
		if (DeviceIoControl(hDriver, IO_ADD_PROCESS, (PVOID)name, std::strlen(name) + 1,
			nullptr, 0, &bytes, nullptr))
		{
			return true;
		}
		return false;
	}

	bool RemoveProcessByName(const char* name)
	{
		if (hDriver == INVALID_HANDLE_VALUE)
			return false;

		DWORD bytes;
		if (DeviceIoControl(hDriver, IO_REMOVE_PROCESS, (PVOID)name, std::strlen(name) + 1,
			nullptr, 0, &bytes, nullptr))
		{
			return true;
		}
		return false;
	}

	ProcessInfo* FindProcessByName(DWORD& size)
	{
		if (hDriver == INVALID_HANDLE_VALUE)
			return nullptr;

		DWORD bytes;
		auto pInfo = new ProcessInfo[size];
		memset(pInfo, 0, sizeof(*pInfo) * size);
		if (DeviceIoControl(hDriver, IO_UPDATE_PROCESS_LIST, pInfo, (sizeof(*pInfo) * size),
			pInfo, (sizeof(*pInfo) * size), &bytes, nullptr))
		{
			size = bytes;
			return pInfo;
		}
		return nullptr;
	}

	ProcessInfo* FindActiveProcesses(DWORD& size)
	{
		if (hDriver == INVALID_HANDLE_VALUE)
			return nullptr;

		DWORD bytes;
		auto pInfo = new ProcessInfo[size];
		memset(pInfo, 0, sizeof(*pInfo) * size);
		if (DeviceIoControl(hDriver, IO_ACTIVE_PROCESSES, pInfo, (sizeof(*pInfo) * size),
			pInfo, (sizeof(*pInfo) * size), &bytes, nullptr))
		{
			size = bytes;
			return pInfo;
		}
		return nullptr;
	}

	bool HideProcess(const char* name)
	{
		if (hDriver == INVALID_HANDLE_VALUE)
			return false;

		DWORD bytes;
		if (DeviceIoControl(hDriver, IO_HIDE_PROCESS, (PVOID)name, strlen(name) + 1,
			nullptr, 0, &bytes, nullptr))
		{
			return true;
		}
		return false;
	}
};