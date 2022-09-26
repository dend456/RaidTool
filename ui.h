#pragma once
#include <Windows.h>
#include <vector>
#include <d3d9.h>
#include <d3dx9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <map>
#include <vector>
#include <regex>
#include <filesystem>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#include <imgui.h>
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "icon.h"
#include <raid.h>
#include "chaction.h"

namespace fs = std::filesystem;

class UI
{
private:
	static inline WNDPROC oldWndProc = nullptr;
	static inline HWND hwnd = nullptr;
	static inline HINSTANCE hin = nullptr;

	static inline bool init = true;
	static inline bool inputHooked = false;
	static inline ImFont* iconFont = nullptr;

	static constexpr inline int BUFFER_SIZE = 256;

	static bool LoadTextureFromFile(const char* filename, IDirect3DDevice9* device, PDIRECT3DTEXTURE9* out_texture, int* out_width, int* out_height);

	static LRESULT __stdcall wndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static HRESULT WINAPI HookedGetDeviceData(IDirectInputDevice8* pThis, DWORD acbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
	static HRESULT WINAPI HookedGetDeviceState(IDirectInputDevice8* pThis, DWORD cbData, LPVOID lpvData);
	static HRESULT WINAPI HookedSetDeviceFormat(IDirectInputDevice8* pThis, LPCDIDATAFORMAT lpdf);
	static SHORT WINAPI HookedGetAsyncKeyState(int vKey);
	static SHORT WINAPI HookedGetKeyState(int vKey);
	static BOOL CALLBACK enumWindowCallback(HWND handle, LPARAM lParam);

	void loadItems() noexcept;
	std::vector<std::filesystem::path> getSavedRaids() const noexcept;

	std::string commandFuncCallback(int eq, int* p, const char* s) noexcept;
	void onLogMessageCallback(const char* msg) noexcept;
	bool refreshCmd() noexcept;
	std::string getPasteString() const noexcept;

	IDirect3DDevice9* device = nullptr;
	std::map<std::string, int> itemIcons;
	std::map<std::string, Icon> icons;
	std::vector<std::string> systems;
	const char* selectedSystem = nullptr;
	bool menuOpen = true;
	bool windowOpen = true;
	bool chactionsWindowOpen = false;
	bool exit = false;
	bool dkpError = false;
	bool moveMenuButton = false;
	Icon* settingsIcon = nullptr;
	char characterBuff[BUFFER_SIZE] = { 0 };
	char serverBuff[BUFFER_SIZE] = { 0 };
	int playerDkp = -1;
	ImGuiContext* context = nullptr;

	std::vector<ChactionGroup> chactions;

	Icon* getIcon(const std::string& item) noexcept;

public:
	UI();
	~UI();
	void shutdown() noexcept;

	void update(float dt) noexcept;
	void render(IDirect3DDevice9* device) noexcept;
	void hookInput() noexcept;
	void unhookInput() noexcept;
	bool isWindowOpen() const noexcept { return windowOpen; }
	bool exiting() const noexcept { return exit; }
};
