#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <map>
#include <thread>
#include <fcntl.h>
#include <io.h>

#include <d3d9.h>
#include <d3dx9.h>
#include <MinHook.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "winmm.lib")

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "version.lib")

#include <guiddef.h>
#include <detours/detours.h>
#include "ui.h"
#include "settings.h"
#include <game.h>

using namespace std::chrono_literals;

HINSTANCE DllHandle;

typedef HRESULT(__stdcall* EndSceneType)(IDirect3DDevice9* device);
EndSceneType endScene;

LPD3DXFONT font;
UI ui;
std::chrono::time_point<std::chrono::high_resolution_clock> pauseTime = std::chrono::high_resolution_clock::now();
bool paused = false;
volatile bool exiting = false;

HRESULT __stdcall hookedEndScene(IDirect3DDevice9* device)
{
    if (exiting)
    {
        return endScene(device);
    }

    ui.render(device);

    static auto lastUpdate = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> timeDiff = now - lastUpdate;
    float dt = timeDiff.count();
    lastUpdate = now;
    return endScene(device);
}


void hookEndScene()
{
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
    {
        return;
    }

    D3DPRESENT_PARAMETERS d3dparams = { 0 };
    d3dparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dparams.hDeviceWindow = GetForegroundWindow();
    d3dparams.Windowed = true;
    IDirect3DDevice9* pDevice = nullptr;
    HRESULT result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dparams.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dparams, &pDevice);
    if (FAILED(result) || !pDevice)
    {
        pD3D->Release();
        return;
    }

    void** vTable = *reinterpret_cast<void***>(pDevice);
    EndSceneType es = (EndSceneType)vTable[42];

    MH_CreateHook((LPVOID)es, hookedEndScene, (LPVOID*)&endScene);
    MH_EnableHook(MH_ALL_HOOKS);

    pDevice->Release();
    pD3D->Release();
}


DWORD __stdcall EjectThread(LPVOID lpParameter)
{
    Sleep(100);
    FreeLibraryAndExitThread(DllHandle, 0);
    return 0;
}

DWORD WINAPI Menu(HINSTANCE hModule)
{
    HANDLE hFile = CreateFile("raidtool/log.txt", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    uint64_t nHandle = _open_osfhandle((uint64_t)hFile, _O_APPEND);
    settings::logFile = _fdopen(nHandle, "a");
    settings::load();
    Game::logFile = settings::logFile;

    //fopen_s(&settings::logFile, "raidtool/log.txt", "a");
    //FILE* fp = nullptr;
    //FILE* fperr = nullptr;

    AllocConsole();
    //freopen_s(&fp, "CONOUT$", "w", stdout);
    //freopen_s(&fperr, "CONERR$", "w", stderr);
    //freopen_s(&settings::logFile, "raidtool/log.txt", "a", stdout);
    //freopen_s(&fperr, "g:/b.txt", "w", stderr);
    

    FreeConsole();
    MH_Initialize();
    //hookEndScene();

    Game::hook({ "RaidGroupFunc", "CommandFunc" });
    while (true)
    {
        Sleep(25);
        if(GetAsyncKeyState(VK_F7) & 1 || !ui.isWindowOpen() || ui.exiting())
        //if(GetAsyncKeyState(VK_F7) & 1)
        {
            exiting = true;
            break;
        }
    }

    //ui.shutdown();
    Game::unhook();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    //if (fp) fclose(fp);
    //if (fperr) fclose(fperr);
    if (settings::logFile) fclose(settings::logFile);
    CreateThread(0, 0, EjectThread, 0, 0, 0);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Menu, NULL, 0, NULL);
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

