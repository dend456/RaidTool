#include <Windows.h>
#include <iostream>
#include <string>
#include <psapi.h>
#include <vector>
#include <VersionHelpers.h>
#include <atlstr.h>
#include <filesystem>
#include <fmt/core.h>
#include <tlhelp32.h>

uint64_t charAddr = 0;
uint64_t nameOffset = 0;

struct EQProc
{
    int pid;
    SYSTEMTIME time;
    std::string character;
    EQProc(int pid, SYSTEMTIME time, std::string character) : pid(pid), time(time), character(character) {}
};

bool loadOffsets(const std::filesystem::path& path) noexcept
{
    if (!std::filesystem::exists(path)) return false;

    try
    {
        char str[255] = { 0 };
        std::string pathStrtmp = path.string();
        const char* pathStr = pathStrtmp.c_str();

        GetPrivateProfileStringA("EQ", "charAddr", TEXT(""), str, sizeof(str), pathStr);
        charAddr = std::stoi(std::string(str), 0, 0);
        GetPrivateProfileStringA("SpawnInfo", "nameOffset", TEXT(""), str, sizeof(str), pathStr);
        nameOffset = std::stoi(std::string(str), 0, 0);
    }
    catch (const std::exception& e)
    {
        fmt::print("Error loading ini: {}\n", std::string(e.what()));
        return false;
    }
    return true;
}

uint64_t getBaseAddress(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    auto e = GetLastError();
    MODULEENTRY32 me = { 0 };
    me.dwSize = sizeof(me);
    auto ret = Module32First(snapshot, &me);
    if (!ret) return 0;
    while (ret)
    {
        if (strcmp(me.szModule, "eqgame.exe") == 0)
        {
            CloseHandle(snapshot);
            return (uint64_t)me.hModule;
        }
        ret = Module32Next(snapshot, &me);
    }
    CloseHandle(snapshot);
    return 0;
}

std::string getCharacter(HANDLE handle, DWORD pid)
{
    SIZE_T read;
    uint64_t baseAddr = getBaseAddress(pid);
    if (!baseAddr) return "";

    uint64_t nameAddr = 0;
    if (ReadProcessMemory(handle, (LPCVOID)(baseAddr + charAddr), &nameAddr, sizeof(nameAddr), &read))
    {
        nameAddr += nameOffset;
        char name[16] = { 0 };
        if (ReadProcessMemory(handle, (LPCVOID)(nameAddr), name, sizeof(name), &read))
        {
            return name;
        }
    }
    return "";
}


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
        HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, 1, dwProcessId);
        if (GetProcessTimes(handle, &creation, &exit, &kernal, &user) == 0)
        {
            printf("Error getting times for pid %d\n", dwProcessId);
            CloseHandle(handle);
            return true;
        }

        std::string character = getCharacter(handle, dwProcessId);

        CloseHandle(handle);
        FileTimeToSystemTime(&creation, &sysTime);
        procs.emplace_back(dwProcessId, sysTime, character);
    }
    return true;
}

int main() 
{
    loadOffsets(std::filesystem::current_path() / "offsets.ini");
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
        /*for (const auto& p : procs)
        {
            printf("pid: %d\tstarted: %02d:%02d:%02d\t%s\n", p.pid, p.time.wHour, p.time.wMinute, p.time.wSecond, p.character.c_str());
        }*/
        for (int i = 0; i < procs.size(); ++i)
        {
            fmt::print("{}: {}\n", i, procs[i].character);
        }
        std::cout << "Select EQ instance: ";
        int selection = -1;
        std::cin >> selection;
        if (selection >= 0 && selection < procs.size())
        {
            pid = procs[selection].pid;
        }
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