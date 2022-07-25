#pragma once

#define IO_ADD_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x567, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_REMOVE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x568, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_UPDATE_PROCESS_LIST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x569, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_ACTIVE_PROCESSES CTL_CODE(FILE_DEVICE_UNKNOWN, 0x570, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_HIDE_PROCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x571, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)


struct ProcessInfo
{
	operator bool() const { return this != nullptr; }
	ULONG Pid[30]{};
	CHAR Name[15]{};
	UCHAR PidCount{};
};