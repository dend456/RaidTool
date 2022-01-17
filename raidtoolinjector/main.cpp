#include <Windows.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <vector>
#include <VersionHelpers.h>
#include <atlstr.h>

struct EQProc
{
    int pid;
    SYSTEMTIME time;
    EQProc(int pid, SYSTEMTIME time) : pid(pid), time(time) {}
};

BOOL InjectDLL(DWORD ProcessID)
{
    LPCSTR DLL_PATH = "raidtool\\raidtool.dll";
    LPVOID loadLibAddr;
    LPVOID remoteString;

    if (!ProcessID)
        return false;

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, ProcessID);

    if (!proc)
    {
        std::cout << "OpenProcess() failed: " << GetLastError() << '\n';
        return false;
    }

    loadLibAddr = (LPVOID)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
    remoteString = (LPVOID)VirtualAllocEx(proc, NULL, strlen(DLL_PATH) + 1, MEM_COMMIT, PAGE_READWRITE);

    if (!WriteProcessMemory(proc, remoteString, (LPVOID)DLL_PATH, strlen(DLL_PATH) + 1, NULL))
    {
        std::cout << "WriteProcessMemory() failed: " << GetLastError() << std::endl;
    }
    if (!CreateRemoteThread(proc, NULL, NULL, (LPTHREAD_START_ROUTINE)loadLibAddr, remoteString, NULL, NULL))
    {
        std::cout << "CreateRemoteThread() failed: " << GetLastError() << std::endl;
    }

    CloseHandle(proc);

    return true;
}

std::vector<EQProc> procs;
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    DWORD dwThreadId, dwProcessId;
    HINSTANCE hInstance;
    char name[255];
    if (!hWnd)
    {
        return true;
    }
    if (!IsWindowVisible(hWnd))
    {
        return true;
    }
    if (!GetWindowTextA(hWnd, name, 255))
    {
        return true;
    }
    hInstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
    dwThreadId = GetWindowThreadProcessId(hWnd, &dwProcessId);
    if (strcmp(name, "EverQuest") == 0)
    {
        FILETIME creation, exit, kernal, user;
        SYSTEMTIME sysTime;
        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessId);
        if (GetProcessTimes(handle, &creation, &exit, &kernal, &user) == 0)
        {
            printf("Error getting times for pid %d\n", dwProcessId);
            CloseHandle(handle);
            return true;
        }
        CloseHandle(handle);
        FileTimeToSystemTime(&creation, &sysTime);
        procs.emplace_back(dwProcessId, sysTime);
    }
    return true;
}

int main() {
    EnumWindows(EnumWindowsProc, NULL);
    DWORD pid = 0;
    if (procs.size() == 0)
    {
        std::cout << "No EQ Windows found.\n";
    }
    else if (procs.size() == 1)
    {
        pid = procs[0].pid;
    }
    else
    {
        for (const auto& p : procs)
        {
            printf("pid: %d\tstarted: %02d:%02d:%02d\n", p.pid, p.time.wHour, p.time.wMinute, p.time.wSecond);
        }
        std::cout << "Pick Target ProcessID\n";
        std::cin >> pid;
    }
    if (pid)
    {
        if (InjectDLL(pid))
        {
            std::cout << "Injected.\n";
        }
    }
    return 0;
}