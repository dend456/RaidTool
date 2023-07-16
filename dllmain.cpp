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
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

HINSTANCE DllHandle;

typedef HRESULT(__stdcall* EndSceneType)(IDirect3DDevice9* device);
typedef HRESULT(__stdcall* ResetType)(LPDIRECT3DDEVICE9 pD3D9, D3DPRESENT_PARAMETERS* params);
EndSceneType endScene;
ResetType resetFunc;

LPD3DXFONT font;
UI ui;
std::chrono::time_point<std::chrono::high_resolution_clock> pauseTime = std::chrono::high_resolution_clock::now();
bool paused = false;
volatile bool exiting = false;
uint64_t endSceneAddr = 0;
uint64_t resetAddr = 0;

HRESULT __stdcall hookedReset(LPDIRECT3DDEVICE9 pD3D9, D3DPRESENT_PARAMETERS* params)
{
    auto ret = resetFunc(pD3D9, params);
    ui.reset(pD3D9);
    return ret;
}

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


bool hookEndScene()
{
    Game::logger->info("Hooking EndScene");
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
    {
        fprintf(settings::logFile, "error Direct3DCreate9\n");
        fflush(settings::logFile);
        return false;
    }

    D3DPRESENT_PARAMETERS d3dparams = { 0 };
    d3dparams.BackBufferCount = 1;
    d3dparams.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dparams.MultiSampleQuality = 0;
    d3dparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dparams.hDeviceWindow = GetForegroundWindow();
    d3dparams.Windowed = true;
    d3dparams.Flags = 0;
    d3dparams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dparams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    d3dparams.BackBufferFormat = D3DFMT_R5G6B5;
    d3dparams.EnableAutoDepthStencil = 0;
    d3dparams.BackBufferWidth = 1920;
    d3dparams.BackBufferHeight = 1080;

    IDirect3DDevice9* pDevice = nullptr;
    HRESULT result = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dparams.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dparams, &pDevice);
    if (FAILED(result) || !pDevice)
    {
        fprintf(settings::logFile, "error CreateDevice %lu\n", result);
        fflush(settings::logFile);
        pD3D->Release();
        return false;
    }

    void** vTable = *reinterpret_cast<void***>(pDevice);
    EndSceneType es = (EndSceneType)vTable[42];
    ResetType r = (ResetType)vTable[16];
    endSceneAddr = (uint64_t)es;
    resetAddr = (uint64_t)r;
    MH_CreateHook((LPVOID)es, hookedEndScene, (LPVOID*)&endScene);
    MH_CreateHook((LPVOID)r, hookedReset, (LPVOID*)&resetFunc);
    MH_EnableHook(MH_ALL_HOOKS);

    pDevice->Release();
    pD3D->Release();

    Game::logger->info("EndScene hooked");
    return true;
}


DWORD __stdcall EjectThread(LPVOID lpParameter)
{
    Sleep(100);
    FreeLibraryAndExitThread(DllHandle, 0);
    return 0;
}

DWORD WINAPI Menu(HINSTANCE hModule)
{
    HANDLE hFile = CreateFile("raidtool\\log.txt", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    uint64_t nHandle = _open_osfhandle((uint64_t)hFile, _O_APPEND);
    settings::logFile = _fdopen((int)nHandle, "a");
    try
    {
        Game::logger = spdlog::basic_logger_mt("basic_logger", "raidtool\\log.txt");
        Game::logger->set_pattern("[%l][%H:%M:%S.%f%z][%05t] %v");
        //Game::logger->set_level(spdlog::level::err);
        Game::logger->set_level(spdlog::level::info);
        Game::logger->flush_on(spdlog::level::err);
        Game::logger->flush_on(spdlog::level::debug);
        spdlog::flush_every(std::chrono::seconds(5));
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        fprintf(settings::logFile, "error starting logger %s\n", ex.what());
        fflush(settings::logFile);
    }
    settings::load();
    Game::logFile = settings::logFile;
    Offsets::load("raidtool\\offsets.ini");

    MH_Initialize();
    if (!hookEndScene())
    {
        fprintf(settings::logFile, "error hooking end scene\n");
        fflush(settings::logFile);    
        if (settings::logFile) fclose(settings::logFile);
        CreateThread(0, 0, EjectThread, 0, 0, 0);
        return 0;
    }

    Game::logger->info("Starting main loop");
    while (true)
    {
        Sleep(25);
        if(!ui.isWindowOpen() || ui.exiting())
        {
            exiting = true;
            break;
        }
    }

    Game::logger = nullptr;
    spdlog::shutdown();
    ui.shutdown();
    Game::unhook();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();

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

