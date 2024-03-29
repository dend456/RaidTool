#include <Windows.h>
#include <iostream>
#include <queue>
#include <fstream>
#include <string>
#include <thread>
#include <charconv>

#include "ui.h"
#include <detours/detours.h>
#include "settings.h"
#include <fmt/core.h>
#include "icon.h"

#include <game.h>
#include <classes.h>
#include <MinHook.h>

#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"
#include <guild.h>
#include <set>
#include "imgui_ext.h"

#include <boost/xpressive/xpressive.hpp>

using namespace std::literals;


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
typedef HRESULT(WINAPI* IDirectInputDevice_GetDeviceData_t)(IDirectInputDevice8* pThis, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
typedef HRESULT(WINAPI* IDirectInputDevice_GetDeviceState_t)(IDirectInputDevice8* pThis, DWORD cbData, LPVOID lpvData);
typedef HRESULT(WINAPI* IDirectInputDevice_SetDeviceFormat_t)(IDirectInputDevice8* pThis, LPCDIDATAFORMAT lpdf);
typedef SHORT(WINAPI* GetAsyncKeyState_t)(int vKey);
typedef SHORT(WINAPI* GetKeyState_t)(int vKey);

IDirectInputDevice_GetDeviceData_t fnGetDeviceData = nullptr;
IDirectInputDevice_GetDeviceState_t fnGetDeviceState = nullptr;
IDirectInputDevice_SetDeviceFormat_t fnSetDeviceFormat = nullptr;
GetAsyncKeyState_t fnGetAsyncKeyState = nullptr;
GetKeyState_t fnGetKeyState = nullptr;

uint64_t getDeviceDataAddr = 0;
uint64_t getDeviceStateAddr = 0;
uint64_t setDeviceFormatAddr = 0;

LPDIRECT3DDEVICE9 dxDevice = nullptr;

bool UI::LoadTextureFromFile(const char* filename, IDirect3DDevice9* device, PDIRECT3DTEXTURE9* out_texture, int* out_width, int* out_height)
{
    PDIRECT3DTEXTURE9 texture;
    HRESULT hr = D3DXCreateTextureFromFile(device, filename, &texture);
    if (hr != S_OK)
        return false;
    D3DSURFACE_DESC my_image_desc;
    texture->GetLevelDesc(0, &my_image_desc);
    *out_texture = texture;
    *out_width = (int)my_image_desc.Width;
    *out_height = (int)my_image_desc.Height;
    return true;
}

SHORT WINAPI UI::HookedGetKeyState(int vKey)
{
    return GetKeyState(vKey);
}

SHORT WINAPI UI::HookedGetAsyncKeyState(int vKey)
{
    return GetAsyncKeyState(vKey);
}

HRESULT WINAPI UI::HookedSetDeviceFormat(IDirectInputDevice8* pThis, LPCDIDATAFORMAT lpdf)
{
    return fnSetDeviceFormat(pThis, lpdf);
}

HRESULT WINAPI UI::HookedGetDeviceState(IDirectInputDevice8* pThis, DWORD cbData, LPVOID lpvData)
{
    HRESULT ret = fnGetDeviceState(pThis, cbData, lpvData);
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        memset(lpvData, 0, cbData);
        return DIERR_INPUTLOST;
    }
    return ret;
}

HRESULT WINAPI UI::HookedGetDeviceData(IDirectInputDevice8* pThis, DWORD acbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
{
    HRESULT ret = fnGetDeviceData(pThis, acbObjectData, rgdod, pdwInOut, dwFlags);
    if (ret == DI_OK)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard)
        {
            *pdwInOut = 0;
            return ret;
        }
    }
    return ret;
}

LRESULT __stdcall UI::wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))// || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        return true;
    }
    auto ret = CallWindowProc(oldWndProc, hWnd, uMsg, wParam, lParam);
    return ret;
}

BOOL CALLBACK UI::enumWindowCallback(HWND handle, LPARAM lParam)
{
    char name[256];
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (process_id != GetCurrentProcessId())
        return TRUE;
    GetWindowText(handle, name, 256);
    if (strcmp(name, "EverQuest") == 0)
    {
        *((HWND*)lParam) = handle;
        return FALSE;
    }
    return TRUE;
}

void UI::loadItems() noexcept
{
    itemIcons["Tomato"] = 6346 - 500;
    /*std::ifstream input(settings::itemIconsPath);
    if (!input.good())
    {
        //fmt::print(settings::logFile, "Failed to open {}\n", settings::itemIconsPath.generic_string());
        return;
    }
    std::string item;
    std::string iconId;
    while (std::getline(input, item))
    {
        std::getline(input, iconId);
        itemIcons[item] = std::stoi(iconId) - 500;
    }
    */
}

UI::UI()
{
    settings::load();
}

UI::~UI()
{
}

void UI::unloadIcons() noexcept
{
    icons.clear();
}

Icon* UI::getIcon(const std::string& item) noexcept
{
    auto it = itemIcons.find(item);
    if (it != itemIcons.end())
    {
        auto iconIt = icons.find(item);
        Icon* icon = nullptr;
        if (iconIt != icons.end())
        {
            return &iconIt->second;
        }

        IconInfo i = Icon::getIconInfo(it->second);
        std::string iconFile = fmt::format("uifiles/default/dragitem{}.dds", i.fileId);
        PDIRECT3DTEXTURE9 tex = nullptr;
        int width;
        int height;
        bool ret = LoadTextureFromFile(iconFile.c_str(), device, &tex, &width, &height);
        if (ret)
        {
            auto newIt = icons.emplace(item, Icon(tex, 40, 40, width, height, i.x, i.y));
            tex->Release();
            return &(newIt.first->second);
        }
    }
    return nullptr;
}

void UI::shutdown() noexcept
{
    if (inputHooked)
    {
        unhookInput();
    }
    unloadIcons();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    context = nullptr;
    (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)oldWndProc);
}

void UI::update(float dt) noexcept
{

}


std::string UI::getPasteString() const noexcept
{
    if (!OpenClipboard(nullptr)) return "";
        
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) return "";
    
    char* pszText = static_cast<char*>(GlobalLock(hData));
    if (pszText == nullptr) return "";

    std::string text(pszText);

    GlobalUnlock(hData);
    CloseClipboard();

    return text;
}


void drawRaider(const EQRaider* r, int color) noexcept
{
    ImGui::SetNextItemWidth(115);
    bool colorPop = false;
    if (r->dead)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 242, 0, 255));
        colorPop = true;
    }
    else if (settings::openWithRaidWindow && !r->inZone)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(155, 155, 64, 255));
        colorPop = true;
    }
    else if (r->afk)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 255, 255));
        colorPop = true;
    }
    ImGui::LabelText("##Group", "%s", r->name);
    if (colorPop) ImGui::PopStyleColor();
    ImGui::SameLine(115);
    ImGui::SetNextItemWidth(35);
    ImGui::LabelText("##Group", "% 3d", r->level);
    ImGui::SameLine(150);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SetNextItemWidth(40);
    ImGui::LabelText("##Group", "% 4s", Classes::classShortStrings[r->cls].c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(190);
    ImGui::SetNextItemWidth(40); 
    ImGui::LabelText("##Group", "%c%c%c%c%c",
        " R"[r->raidLead], " G"[r->groupLead], " L"[r->masterLooter], " A"[r->assist], " M"[r->marker]);
}


std::vector<std::filesystem::path> UI::getSavedRaids() const noexcept
{
    std::vector<std::filesystem::path> files;
    const std::filesystem::path dir = ".\\raidtool\\saved raids";
    for (auto const& file : std::filesystem::directory_iterator{ dir })
    {
        files.push_back(file.path().filename());
    }

    return files;
}

void UI::onLogMessageCallback(const char* msg) noexcept
{/*
    for (auto& chactiongroup : chactions)
    {
        chactiongroup.onMessage(msg);
    }*/
}

void UI::reset(LPDIRECT3DDEVICE9 pD3D9) noexcept
{
    loadItems();
    D3DDEVICE_CREATION_PARAMETERS params;
    device->GetCreationParameters(&params);
    hwnd = params.hFocusWindow;
    if (hwnd == nullptr)
    {
        return;
    }
    unloadIcons();
    settingsIcon = nullptr;
    ImGui_ImplDX9_Shutdown();
    ImGui::SetCurrentContext(nullptr);
    ImGui::DestroyContext(context);
    context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "raidtool/raidtool.ini";
    if (!ImGui_ImplWin32_Init(hwnd))
    {
        Game::logger->error("Error in ImGui_ImplWin32_Init");
        return;
    }

    if (!ImGui_ImplDX9_Init(device))
    {
        Game::logger->error("Error in ImGui_ImplDX9_Init");
        return;
    }
    settingsIcon = getIcon("Tomato");
    this->device = pD3D9;
}

void UI::render(IDirect3DDevice9* device) noexcept
{
    static Raid raid;
    static int my_image_width = 0;
    static int my_image_height = 0;
    static char selectedRaider[16] = {0};
    static std::array<std::vector<EQRaider*>, 12> groups{};
    static std::vector<EQRaider*> ungrouped;
    static int loadingDump = 0;
    static int loadingString = 0;
    static std::vector<std::filesystem::path> raidDumps;
    static int currentRaidDump = -1;
    static std::set<std::string> mod_names = { "Teach", "Shanks", "Mabiktenu", "Jolksh", "Gaskor"};
    static std::set<std::string> skip_zones = { "Default", "clz" };
    

    if (!device || exit || skip_zones.count(Game::getZone()))
    {
        return;
    }
    if (init)
    {
        init = false;
        loadItems();
        this->device = device;
        D3DDEVICE_CREATION_PARAMETERS params;
        device->GetCreationParameters(&params);
        hwnd = params.hFocusWindow;
        if (hwnd == nullptr)
        {
            return;
        }

        oldWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProc);

        context = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = "raidtool/raidtool.ini";
        if (!ImGui_ImplWin32_Init(hwnd))
        {
            Game::logger->error("Error in ImGui_ImplWin32_Init");
            return;
        }

        if (!ImGui_ImplDX9_Init(device))
        {
            Game::logger->error("Error in ImGui_ImplDX9_Init");
            return;
        }
        /*
        io.Fonts->AddFontDefault();
        ImFontConfig config;
        config.MergeMode = true;
        const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        iconFont = io.Fonts->AddFontFromMemoryTTF(fontawesome_webfont_ttf, fontawesome_webfont_ttf_len, 16.0f, &config, icon_ranges);
        */ 
        hin = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        hookInput();

        settingsIcon = getIcon("Tomato");
        Game::onLogMessageCallback = std::bind(&UI::onLogMessageCallback, this, std::placeholders::_1);;
        /*
        try
        {
            std::ifstream file("raidtool\\chactions.txt");
            json js = json::parse(file);
            chactions = js;
        }
        catch(const std::exception&)
        {

        }*/
        if(true) raid.init();
        Game::logger->info("UI initialized");
    }
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    //ImGui::EndFrame();
    //ImGui::Render();
    //ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    //return;
    if (moveMenuButton)
    {
        ImGui::Begin("##menuButton", &windowOpen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize);
    }
    else
    {
        ImGui::Begin("##menuButton", &windowOpen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
    }

    if (settingsIcon)
    {
        if (ImGui::ImageButton(settingsIcon->getTexture(), ImVec2(20.0f, 20.0f), settingsIcon->getTopLeft(), settingsIcon->getBottomRight()))
        {
            menuOpen = !menuOpen;
        }
    }
    else
    {
        if (ImGui::Button("Raid"))
        {
            menuOpen = !menuOpen;
        }
    }
    ImGui::End();

    if (loadingDump)
    {
        ImGui::Begin("Choose Raid Dump");

        std::string currentPath = "";
        if (currentRaidDump >= 0 && currentRaidDump < raidDumps.size())
        {
            currentPath = raidDumps[currentRaidDump].string();
        }

        ImGui::SetNextItemWidth(350);
        if (ImGui::BeginCombo("##raidDumpCombo", currentPath.c_str()))
        {
            for (int i = 0; i < raidDumps.size(); ++i)
            {
                bool selected = currentRaidDump == i;
                std::string strpath = raidDumps[i].string();
                if (ImGui::Selectable(strpath.c_str(), selected))
                {
                    currentRaidDump = i;
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load", { 80, 20 }))
        {
            if (currentRaidDump >= 0 && currentRaidDump < raidDumps.size())
            {
                std::filesystem::path root = ".\\raidtool\\saved raids";
                if (loadingDump == 1)
                {
                    raid.loadDump(root / raidDumps[currentRaidDump]);
                }
                else if (loadingDump == 2)
                {
                    raid.inviteDump(root / raidDumps[currentRaidDump]);
                }
            }
            loadingDump = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", { 80, 20 }))
        {
            loadingDump = 0;
        }

        ImGui::End();
    }
    else if ((menuOpen && !settings::openWithRaidWindow) || (settings::openWithRaidWindow && raid.raidWindowOpen()))
    {
        ImGui::Begin("Raid");
        if (true)
        {
            const auto& raiders = raid.read();
            bool isRaidLead = raid.amIRaidLead();
            bool isMod = mod_names.find(raid.myName()) != mod_names.end();
            for (auto& v : groups)
            {
                v.clear();
            }
            ungrouped.clear();
            for (const auto& r : raiders)
            {
                if (r.exists)
                {
                    if (r.group == -1)
                    {
                        ungrouped.push_back(r.eqraider);
                    }
                    else
                    {
                        groups[r.group].push_back(r.eqraider);
                    }
                }
            }
            auto sorter = [](const auto* r1, const auto* r2)
            {
                return strcmp(r1->name, r2->name) < 0;
            };

            for (auto& g : groups)
            {
                std::sort(g.begin(), g.end(), sorter);
            }
            std::sort(ungrouped.begin(), ungrouped.end(), sorter);
            
            
            ImGui::BeginGroup();
            for (int i = 0; i < 12; ++i)
            {
                std::string label = fmt::format("Group {}", i + 1);
                BeginGroupPanel(label.c_str(), ImVec2(150, 0), 200.0f, "MOVE_GROUP", (void*)&i, sizeof(i));
                ImGui::BeginGroup();
                std::string id = fmt::format("##Groupid{}", i);
                ImGui::PushID(id.c_str());
                for (auto* r : groups[i])
                {
                    int color = raid.colorForClass(r->cls);
                    ImGui::PushID(r->name);
                    bool selected = strcmp(r->name, selectedRaider) == 0;
                    if (ImGui::Selectable("##selectgroup", selected, 0, { 240, 15 }))
                    {
                        strncpy_s(selectedRaider, r->name, 16);
                        if (raid.setSelectedRaider(r->name))
                        {
                            std::string targetStr = fmt::format("/target {}", std::string(r->name));
                            Game::hookedCommandFunc(0, 0, targetStr.c_str());
                        }
                    }

                    if (ImGui::BeginDragDropSource())
                    {
                        drawRaider(r, color);
                        ImGui::SetDragDropPayload("MOVE_PLAYER", &r, sizeof(&r));
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOVE_PLAYER"))
                        {
                            const EQRaider* eqraider = *(EQRaider**)payload->Data;
                            raid.swapRaiders(r, eqraider);
                        }
                        ImGui::EndDragDropTarget();
                        memset(selectedRaider, 0, sizeof(selectedRaider));
                    }
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 240);
                    drawRaider(r, color);
                    ImGui::PopID();
                }
                int groupSize = (int)groups[i].size();
                if (groupSize < 6)
                {
                    ImGui::PushID(i);
                    ImGui::BeginChild("##EmptyGroup", { 240.0f, 22.5f * (6 - groupSize) }, false, ImGuiWindowFlags_NoScrollbar);
                    for (int j = groupSize; j < 6; ++j)
                    {
                        ImGui::SetNextItemWidth(240);
                        ImGui::LabelText("##Group2", "-");
                    }
                    ImGui::EndChild();
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOVE_PLAYER"))
                        {
                            const EQRaider* eqraider = *(EQRaider**)payload->Data;
                            raid.moveToGroup(eqraider->name, i);
                        }
                        else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOVE_GROUP"))
                        {
                            const int group = *(int*)payload->Data;
                            raid.moveGroupToGroup(group, i);
                        }
                        ImGui::EndDragDropTarget();
                        memset(selectedRaider, 0, sizeof(selectedRaider));
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
                ImGui::EndGroup();
                EndGroupPanel();
                if (i % 3 != 2)
                {
                    ImGui::SameLine();
                }
            }
            ImGui::EndGroup();
            ImGui::SameLine();

            BeginGroupPanel("Ungrouped", ImVec2(0, 609), 230, nullptr, nullptr, 0);
            ImGui::BeginChild("##ungrouped2", { 230, 609 });
            for (const auto* r : ungrouped)
            {
                int color = raid.colorForClass(r->cls);
                ImGui::PushID(r->name);
                bool selected = strcmp(r->name, selectedRaider) == 0;
                if (ImGui::Selectable("##selectgroup", selected, 0, { 230, 15 }))
                {
                    strncpy_s(selectedRaider, r->name, 16);
                    if (raid.setSelectedRaider(r->name))
                    {
                        std::string targetStr = fmt::format("/target {}", std::string(r->name));
                        Game::hookedCommandFunc(0, 0, targetStr.c_str());
                    }
                }
                if (ImGui::BeginDragDropSource())
                {
                    drawRaider(r, color);
                    ImGui::SetDragDropPayload("MOVE_PLAYER", &r, sizeof(&r));
                    ImGui::EndDragDropSource();
                    memset(selectedRaider, 0, sizeof(selectedRaider));
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 240);
                drawRaider(r, color);
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOVE_PLAYER"))
                {
                    const EQRaider* eqraider = *(EQRaider**)payload->Data;
                    raid.removeFromGroup(eqraider->name);
                }
                else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MOVE_GROUP"))
                {
                    const int group = *(int*)payload->Data;
                    raid.moveGroupToGroup(group, -1);
                }
                ImGui::EndDragDropTarget();
            }
            EndGroupPanel();

            ImGui::Dummy({ 400, 0 });
            ImGui::SameLine(0, 290);
            ImGui::Text("Players in raid: % 3d", raid.raidSize());
            ImGui::SameLine(0, 20);
            ImGui::Text("  Average level: % 3d", raid.averageLevel());

            ImGui::BeginGroup();
            BeginGroupPanel("Raid Shit", { 390, 200 }, 390, 0, 0, 0);
            ImGui::BeginGroup();

            if (raid.locked())
            {
                if (ImGui::Button("Unlock", { 70,25 }) && isRaidLead)
                {
                    raid.clickButton(RaidButton::unlock);
                }
            }
            else
            {
                if (ImGui::Button("Lock", { 70,25 }) && isRaidLead)
                {
                    raid.clickButton(RaidButton::lock);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("RemLead", { 70,25 }) && isRaidLead)
            {
                raid.clickButton(RaidButton::remleader);
            }
            if (ImGui::Button("Invite", { 70,25 }))
            {
                raid.clickButton(RaidButton::invite);
            }
            ImGui::SameLine();
            if (ImGui::Button("Disband", { 70,25 }))
            {
                raid.clickButton(RaidButton::disband);
            }
            ImGui::SameLine();
            if (ImGui::Button("MakeLead", { 70,25 }) && isRaidLead)
            {
                raid.clickButton(RaidButton::makeleader);
            }
            ImGui::SameLine();
            if (ImGui::Button("Save Raid", { 70,25 }))
            {
                std::string s = raid.dumpString();
                auto time = std::time(nullptr);
                std::string path = fmt::format("raidtool/saved raids/{}_{}.txt", std::string(raid.myName()), time);
                try
                {
                    FILE* f = nullptr;
                    auto e = fopen_s(&f, path.c_str(), "w");
                    if (f)
                    {
                        fmt::print(f, s);
                        std::fclose(f);
                    }
                    else
                    {
                        if (Game::logger)
                        {
                            Game::logger->error("Error saving raid {}", e);
                        }
                    }
                }
                catch(const std::exception & e)
                {
                    if (Game::logger)
                    {
                        Game::logger->error("Error saving raid {}", e.what());
                    }
                }
            }

            if (ImGui::Button("AddLooter", { 70,25 }) && isRaidLead)
            {
                raid.clickButton(RaidButton::addlooter);
            }
            ImGui::SameLine();
            if (ImGui::Button("RemLooter", { 70,25 }) && isRaidLead)
            {
                raid.clickButton(RaidButton::removelooter);
            }
            ImGui::SameLine();
            if (ImGui::Button("Options", { 70,25 }))
            {
                raid.clickButton(RaidButton::options);
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy Dump", { 70, 25 }))
            {
                std::string s = raid.dumpString();
                auto size = s.length() + 1;
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
                memcpy_s(GlobalLock(hMem), size, s.c_str(), size);
                GlobalUnlock(hMem);
                OpenClipboard(NULL);
                EmptyClipboard();
                SetClipboardData(CF_TEXT, hMem);
                CloseClipboard();
            }

            if (ImGui::Button("Inv Exp", { 70,25 }))
            {
                raid.clickButton(RaidButton::inviteexped);
            }
            ImGui::SameLine();
            if (ImGui::Button("Inv Task", { 70,25 }))
            {
                raid.clickButton(RaidButton::invitetask);
            }
            ImGui::SameLine();
            if (ImGui::Button("Find PC", { 70,25 }))
            {
                raid.clickButton(RaidButton::findpc);
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump", { 70,25 }))
            {
                raid.clickButton(RaidButton::dump);
            }
            ImGui::EndGroup();

            ImGui::SameLine();
            ImGui::BeginGroup();
            if (ImGui::Button("ML", { 70,25 }) && (isRaidLead || isMod))
            {
                raid.clickButton(RaidButton::masterlooter);
            }
            if (ImGui::Button("Assist", { 70,25 }) && (isRaidLead || isMod))
            {
                raid.clickButton(RaidButton::assist);
            }
            if (ImGui::Button("MarkNPC", { 70,25 }) && (isRaidLead || isMod))
            {
                raid.clickButton(RaidButton::mark);
            }
            ImGui::EndGroup();
            EndGroupPanel();

            BeginGroupPanel("Expedition", { 200, 100 }, 200, 0, 0, 0);
            ImGui::BeginGroup();

            if (ImGui::Button("Invite##expedinvite") && (isRaidLead || isMod))
            {
                raid.inviteToExpedition(selectedRaider);
            }
            ImGui::SameLine();
            if (ImGui::Button("Invite raid") && (isRaidLead || isMod))
            {
                raid.inviteRaidToExpedition();
            }
            ImGui::SameLine();
            if (ImGui::Button("Kick exp") && (isRaidLead || isMod))
            {
                raid.kickExpedition();
            }

            ImGui::EndGroup();
            EndGroupPanel();
            ImGui::EndGroup();

            ImGui::SameLine();
            BeginGroupPanel("Grouping", { 95, 200 }, 95, 0, 0, 0);
            if (ImGui::Button("Group Alts", { 85, 25 }) && isRaidLead)
            {
                raid.groupAlts();
            }
            if (ImGui::Button("Auto Group", { 85,25 }) && isRaidLead)
            {
                raid.makeGroups();
            }
            if (ImGui::Button("Kill Groups", { 85,25 }) && isRaidLead)
            {
                raid.killGroups();
            }
            if (ImGui::Button("From Dump##grouping", { 85,25 }))
            {
                raidDumps = getSavedRaids();
                loadingDump = 1;
            }
            if (ImGui::Button("From Paste##grouping", { 85,25 }))
            {
                std::string paste = getPasteString();
                raid.groupFromString(paste);
            }
            EndGroupPanel();
            ImGui::SameLine();

            BeginGroupPanel("Invites", { 200, 200 }, 200, 0, 0, 0);
            static bool inviteAlts = true;
            static int inviteMinLevel = 1;
            BeginGroupPanel("Group", { 90, 200 }, 90, 0, 0, 0);
            ImGui::BeginGroup();
            if (ImGui::Button("From Paste##group", { 85,25 }))
            {
                std::string paste = getPasteString();
                raid.inviteString(paste, false);
            }
            ImGui::EndGroup();
            EndGroupPanel();
            ImGui::SameLine();
            BeginGroupPanel("Raid", { 90, 200 }, 90, 0, 0, 0);
            ImGui::BeginGroup();
            ImGui::BeginGroup();
            ImGui::Checkbox("Alts", &inviteAlts);
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragInt("Min Level", &inviteMinLevel, .8f, 1, 130);
            ImGui::EndGroup();
            ImGui::BeginGroup();
            if (ImGui::Button("Invite All", { 85, 25 }))
            {
                std::bitset<17> classes;
                classes.set();
                raid.inviteGuild(classes, inviteMinLevel, inviteAlts);
            }
            if (ImGui::Button("Invite Util", { 85, 25 }))
            {
                std::bitset<17> classes = Classes::healerBits | Classes::tankBits;
                classes.set(Classes::bard);
                classes.set(Classes::enchanter);
                raid.inviteGuild(classes, inviteMinLevel, inviteAlts);
            }
            if (ImGui::Button("Invite DPS", { 85, 25 }))
            {
                std::bitset<17> classes = Classes::dpsBits;
                classes.reset(Classes::bard);
                classes.reset(Classes::enchanter);
                raid.inviteGuild(classes, inviteMinLevel, inviteAlts);
            }
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            if (ImGui::Button("From Dump##raid", { 85,25 }))
            {
                raidDumps = getSavedRaids();
                loadingDump = 2;
            }
            if (ImGui::Button("From Paste", { 85,25 }))
            {
                std::string paste = getPasteString();
                raid.inviteString(paste, true);
            }
            ImGui::EndGroup();
            ImGui::EndGroup();
            EndGroupPanel();
            EndGroupPanel();
            ImGui::SameLine();

            BeginGroupPanel("Disband", { 97, 200 }, 97, 0, 0, 0);
            if(ImGui::Button("/kickp", { 90, 25 }) && isRaidLead)
            {
                raid.kickp();
            }
            if (ImGui::Button("Group+/kickp", { 90, 25 }) && isRaidLead)
            {
                raid.groupAlts();
                raid.kickp();
            }
            EndGroupPanel();
            /*
            if (ImGui::Button("Chactions\nmore prealpha than pantheon"))
            {
                chactionsWindowOpen = !chactionsWindowOpen;
            }*/
            //if (isMod)
            {
                //ImGui::SameLine();
                static int selectedLogLevel = spdlog::level::info;
                int beforeSel = selectedLogLevel;
                ImGui::SetNextItemWidth(100);
                ImGui::Combo("Log Level", &selectedLogLevel, "Trace\0Debug\0Info\0Warn\0Error\0Critical\0Off");
                Game::logger->set_level((spdlog::level::level_enum)(selectedLogLevel));
            }
            ImGui::Dummy(ImVec2(0, 10));
            ImGui::Checkbox("Move button", &moveMenuButton);
            ImGui::Checkbox("Open with raid window", &settings::openWithRaidWindow);

            ImGui::SameLine();
            ImGui::Dummy(ImVec2(750, 0));
            ImGui::SameLine();
            if (ImGui::Button("Exit##e2"))
            {
                settings::save();
                /*json js = chactions;
                std::ofstream file("raidtool\\chactions.txt");
                file << std::setw(4) << js;
                */
                exit = true;
            }
            ImGui::End();
        }
    }
    /*
    if (chactionsWindowOpen)
    {
        ImGui::Begin("Chactions");
        for (auto& group : chactions)
        {
            group.render();
        }
        ImGui::End();
    }
    */
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void UI::hookInput() noexcept
{
    IDirectInput8* input = nullptr;
    if (DirectInput8Create(hin, DIRECTINPUT_VERSION, IID_IDirectInput8, (LPVOID*)&input, 0) != DI_OK)
    {
        return;
    }

    LPDIRECTINPUTDEVICE8 keyboard;
    if (input->CreateDevice(GUID_SysKeyboard, &keyboard, 0) != DI_OK)
    {
        input->Release();
        return;
    }

    void** vTable = *reinterpret_cast<void***>(keyboard);

    getDeviceDataAddr = (uint64_t)vTable[10];
    getDeviceStateAddr = (uint64_t)vTable[9];
    setDeviceFormatAddr = (uint64_t)vTable[11];

    MH_CreateHook((LPVOID)vTable[10], HookedGetDeviceData, (LPVOID*)&fnGetDeviceData);
    MH_CreateHook((LPVOID)vTable[9], HookedGetDeviceState, (LPVOID*)&fnGetDeviceState);
    MH_CreateHook((LPVOID)vTable[11], HookedSetDeviceFormat, (LPVOID*)&fnSetDeviceFormat);
     
    Game::hook({ "RaidGroupFunc", "CommandFunc" });
    //Game::hook({ "RaidGroupFunc"});
    inputHooked = true;
}

void UI::unhookInput() noexcept
{
    MH_DisableHook(MH_ALL_HOOKS);
    inputHooked = false;
}