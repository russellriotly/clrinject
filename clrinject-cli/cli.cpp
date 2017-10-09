#include <Windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <atlbase.h>
#pragma comment(lib, "clrinject.lib")
#include <clrinject.h>

DWORD GetProcessIdByName(const char * processName) {
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (_stricmp(entry.szExeFile, processName) == 0)
			{
				CloseHandle(snapshot);
				return entry.th32ProcessID;
			}
		}
	}

	CloseHandle(snapshot);
	return 0;
}

void usage() {
	fprintf(stderr,
		"Usage:\n"
		"\tclrinject-cli.exe -p <processId/processName> -a <assemblyFile>\n\n"
		"Addition options:\n"
		"\t-e\tEnumerate Runtimes and AppDomains"
	);
	exit(1);
}

int main(int argc, char** argv) {
	InjectionOptions options;
	options.enumerate = false;
	char * processName = NULL;
	char * assemblyFile = NULL;

	//handle command line arguments
	int argi = 0;
	while (++argi < argc) {
		if (argv[argi][0] == '-') {
			switch (argv[argi][1])
			{
			case 'p':
				if (argi + 1 < argc){
					argi++;
					processName = argv[argi];
					continue;
				}
				break;
			case 'a':
				if (argi + 1 < argc) {
					argi++;
					assemblyFile = argv[argi];
					continue;
				}
				break;
			case 'e':
				options.enumerate = true;
				continue;
			}
		}
		fprintf(stderr, "Unexpected argument '%s'!\n\n", argv[argi]);
		usage();
	}

	//check arguments validity
	if (!processName || (!assemblyFile && !options.enumerate)) {
		fprintf(stderr, "Process or assembly file was not specified!\n\n");
		usage();
	}
	
	//find processId
	DWORD processId = GetProcessIdByName(processName);
	if (!processId)
		processId = strtol(processName, NULL, 10);
	
	if (!processId) {
		fprintf(stderr, "Failed to get id for process name '%s'!\n", processName);
		return 1;
	}

	options.processId = processId;
	lstrcpynW(options.assemblyFile, CA2W(assemblyFile), MAX_PATH);

	InjectionResult result;
	int retVal = Inject(&options, &result);
	if (options.enumerate) {
		printf("Enumeration:\n");
		int index = 0;
		for (int i = 0; i < result.numRuntimes; i++) {
			const Runtime& runtime = result.runtimes[i];
			printf("#%d\tRuntime Version: '%ls'; %s\n", index++, runtime.version, runtime.started ? "Started" : "Not started");
			for (int j = 0; j < runtime.numAppDomains; j++) {
				const AppDomain& appDomain = runtime.appDomains[j];
				printf("#%d\t\tAppDomain Name: '%ls'\n", index++, appDomain.friendlyName);
			}
		}
	}
	if (retVal) {
		if(result.status || result.statusMessage[0])
			fprintf(stderr, "Injection failed, return value: 0x%08X, code: 0x%08X, reason: '%s'!\n", retVal, result.status, result.statusMessage);
		else
			fprintf(stderr, "Injection failed, return value: 0x%08X!\n", retVal);
		return 1;
	}
	printf("Injection successful, return value: %d\n", result.retVal);
	return 0;
}
