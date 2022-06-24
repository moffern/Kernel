#include "data.h"



// Declarations
//------------------------------------------------------------------------------------------------------------
int Error(const char* message);
void Menu();
void Execute();
//------------------------------------------------------------------------------------------------------------



// Main
//------------------------------------------------------------------------------------------------------------
int main()
{
	Execute();

	std::cin.get();
	return 0;
}
//------------------------------------------------------------------------------------------------------------




// Definitions
//------------------------------------------------------------------------------------------------------------
int Error(const char* message)
{
	printf("%s, error=(%u)", message, GetLastError());
	return 1;
}


void Menu()
{
	std::cout << "OPTIONS:\n"
		<< "1) Add process\n"
		<< "2) Remove process\n"
		<< "3) Show process list\n"
		<< "4) Show all active processes\n"
		<< "5) Hide process\n"
		<< "6) Show menu\n"
		<< std::endl;
}


void Execute()
{
	KCom Driver{ L"\\\\.\\random" };
	bool first = true;

	while (true)
	{
		if (first)
		{
			Menu();
			first = false;
		}

		std::string s;
		char ch;
		std::cin >> ch;
		printf("\n");

		switch (ch)
		{
		case '1':
			printf("Enter process name to add: ");
			std::cin >> s;
			if (!Driver.AddProcessByName(s.c_str()))
				printf("%s already added\n", s.c_str());
			break;

		case '2':
			printf("Enter process name to remove: ");
			std::cin >> s;
			if (!Driver.RemoveProcessByName(s.c_str()))
				printf("%s not found\n", s.c_str());
			break;

		case '3':
		{
			DWORD size{ 10 };
			auto pInfo = Driver.FindProcessByName(size);
			if (pInfo == nullptr)
			{
				printf("%s not found\n", s.c_str());
				break;
			}

			if (size == 0)
				printf("Process list empty...\n");
			else
			{
				printf("Process list:\n------------------------------------\n\n");
				for (int i = 0; i < (size / sizeof(ProcessInfo)); ++i)
				{
					printf("%d) [%s]:\n", i, pInfo[i].Name);
					if (pInfo[i].PidCount == 0)
						printf("PID list empty...\n");
					else
					{
						printf("[PIDS](%u) -> { ", pInfo[i].PidCount);
						for (int j = 0; j < pInfo[i].PidCount; ++j)
						{
							if (pInfo[i].Pid[j] != 0)
								printf("(%u) ", pInfo[i].Pid[j]);
						}
						printf("}\n\n");
					}
				}
				printf("------------------------------------\n\n");
			}

			delete[] pInfo;
			break;
		}

		case '4':
		{
			DWORD size{ 100 };
			auto pInfo = Driver.FindActiveProcesses(size);
			if (pInfo == nullptr)
			{
				printf("%s not found\n", s.c_str());
				break;
			}

			if (size == 0)
				printf("Process list empty...\n");
			else
			{
				printf("Active processes:\n------------------------------------\n\n");
				for (int i = 0; i < (size / sizeof(ProcessInfo)); ++i)
				{
					printf("%d) [%s]:\n", i, pInfo[i].Name);
					if (pInfo[i].PidCount == 0)
						printf("PID list empty...\n\n");
					else
					{
						printf("[PIDS](%u) -> { ", pInfo[i].PidCount);
						for (int j = 0; j < pInfo[i].PidCount; ++j)
						{
							//if (pInfo[i].Pid[j] != 0)
								printf("(%u) ", pInfo[i].Pid[j]);
						}
						printf("}\n\n");
					}
				}
				printf("------------------------------------\n\n");
			}

			delete[] pInfo;
			break;
		}

		case '5':
		{
			printf("Enter process name to hide: ");
			std::cin >> s;
			if (!Driver.HideProcess(s.c_str()))
			{
				printf("failed to hide %s\n", s.c_str());
			}
			break;
		}

		case '6':
			Menu();
			break;
		}
	}
}
//------------------------------------------------------------------------------------------------------------