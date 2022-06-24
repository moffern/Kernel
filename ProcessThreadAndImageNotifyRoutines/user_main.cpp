#include <iostream>
#include <Windows.h>
#include "..\KMDF Driver20\common.h"

bool thread = false;
// Declarations
//----------------------------------------------------------
int Error(const char* message);
void DisplayTime(const LARGE_INTEGER& time);
void DisplayInfo(BYTE* buffer, DWORD size);
//----------------------------------------------------------

int main()
{
	auto hFile = ::CreateFile(L"\\\\.\\MyDeviceLink", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return Error("failed to open file\n");

	DWORD size{ 1 << 16 };
	BYTE* buffer = new BYTE[size];

	while (true)
	{
		DWORD bytes;
		if (!::ReadFile(hFile, buffer, size, &bytes, nullptr))
			return Error("failed to read file\n");

		if (bytes > 0)
			DisplayInfo(buffer, bytes);

		::Sleep(200);
	}

	delete[] buffer;
	CloseHandle(hFile);

	std::cin.get();
	return 0;
}


// Definitions
//----------------------------------------------------------
int Error(const char* message)
{
	printf("%s, error=(%d)", message, GetLastError());
	return 1;
}

void DisplayTime(const LARGE_INTEGER& time)
{
	SYSTEMTIME st;
	FileTimeToSystemTime((FILETIME*)&time, &st);
	printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void DisplayInfo(BYTE* buffer, DWORD size)
{
	auto count = size;
	while (GetAsyncKeyState(VK_DELETE) & 1)
	{
		thread = !thread;
		if (thread)
			printf("Thread notification ON\n");
		else
			printf("Thread notification OFF\n");
	}
	while (count > 0)
	{
		auto header = (ItemHeader*)buffer;
		switch (header->Type)
		{
		case ItemType::ProcessCreate:
		{
			DisplayTime(header->LocalTime);
			auto item = (ProcessCreateInfo*)buffer;
			std::string commandline((CHAR*)buffer + item->CommandLineOffset, item->CommandLineLength);
			printf("PID(%u) created from ParentPID(%u) [%s]\n", item->ProcessId,
				item->ParentProcessId, commandline.c_str());
			break;
		}

		case ItemType::ProcessExit:
		{
			DisplayTime(header->LocalTime);
			auto item = (ProcessExitInfo*)buffer;
			printf("Process %u exited\n", item->ProcessId);
			break;
		}

		case ItemType::ThreadCreate:
		{
			if (thread)
			{
				DisplayTime(header->LocalTime);
				auto item = (ThreadCreateExitInfo*)buffer;
				printf("Thread %u created from Process %u\n", item->ThreadId, item->ProcessId);
			}
			break;
		}

		case ItemType::ThreadExit:
		{
			if (thread)
			{
				DisplayTime(header->LocalTime);
				auto item = (ThreadCreateExitInfo*)buffer;
				printf("Thread %u exited from Process %u\n", item->ThreadId, item->ProcessId);
			}
			break;
		}

		case ItemType::ImageLoad:
		{
			auto item = (ImageLoadInfo*)buffer;
			std::string imagename((CHAR*)buffer + item->ImageNameOffset, item->ImageNameLength);
			std::string dll((CHAR*)buffer + item->DllNameOffset, item->DllNameLength);
			if (strstr(dll.c_str(), ".exe"))
				break;
			DisplayTime(header->LocalTime);
			printf("PID(%u)\t[%s]: (%s)\n", item->ProcessId, imagename.c_str(), dll.c_str());
			break;
		}

		default:
			break;
		}
		buffer += header->Size;
		count -= header->Size;
	}
}
//----------------------------------------------------------