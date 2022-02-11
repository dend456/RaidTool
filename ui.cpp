#include <iostream>
#include <queue>
#include <fstream>
#include <string>
#include <thread>
#include <charconv>

#include "IconsFontAwesome5.h"
#include "fontawesome-webfont.h"
#include "ui.h"
#include "detours.h"
#include "settings.h"
#include <fmt/core.h>
#include "icon.h"

#include <game.h>
#include <classes.h>
#include <raid.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include <guild.h>

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
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        return true;
    }
    return CallWindowProc(oldWndProc, hWnd, uMsg, wParam, lParam);
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
    std::ifstream input(settings::itemIconsPath);
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
}

UI::UI()
{
    //settings::load();
}

UI::~UI()
{
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

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    context = nullptr;
    (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)oldWndProc);
}

void UI::update(float dt) noexcept
{

}

static ImVector<ImRect> s_GroupPanelLabelStack;

void BeginGroupPanel(const char* name, const ImVec2& size, float maxWidth, const char* droppableID, void* payload, size_t payloadSize)
{
    ImGui::BeginGroup();

    auto cursorPos = ImGui::GetCursorScreenPos();
    auto itemSpacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    auto frameHeight = ImGui::GetFrameHeight();
    ImGui::BeginGroup();

    ImVec2 effectiveSize = size;
    if (size.x < 0.0f)
        effectiveSize.x = ImGui::GetContentRegionAvailWidth();
    else
        effectiveSize.x = size.x;
    ImGui::Dummy(ImVec2(effectiveSize.x, 0.0f));

    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::SameLine(0.0f, 0.0f);
    if (droppableID)
    {
        ImGui::Selectable(name, false, 0, {55, 15});
        if (ImGui::BeginDragDropSource())
        {
            ImGui::Text(name);
            ImGui::SetDragDropPayload(droppableID, payload, payloadSize);
            ImGui::EndDragDropSource();
        }
    }
    else
    {
        ImGui::TextUnformatted(name);
    }
    auto labelMin = ImGui::GetItemRectMin();
    auto labelMax = ImGui::GetItemRectMax();
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::Dummy(ImVec2(0.0, frameHeight + itemSpacing.y));
    ImGui::BeginGroup();

    //ImGui::GetWindowDrawList()->AddRect(labelMin, labelMax, IM_COL32(255, 0, 255, 255));

    ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
    ImGui::GetCurrentWindow()->ContentRegionRect.Max.x -= frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->WorkRect.Max.x -= frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->InnerRect.Max.x -= frameHeight * 0.5f;
#else
    ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x -= frameHeight * 0.5f;
#endif
    ImGui::GetCurrentWindow()->Size.x -= frameHeight;

    auto itemWidth = ImGui::CalcItemWidth();
    ImGui::PushItemWidth(ImMax(0.0f, maxWidth));

    s_GroupPanelLabelStack.push_back(ImRect(labelMin, labelMax));
}

void EndGroupPanel()
{
    ImGui::PopItemWidth();

    auto itemSpacing = ImGui::GetStyle().ItemSpacing;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    auto frameHeight = ImGui::GetFrameHeight();

    ImGui::EndGroup();

    //ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 255, 0, 64), 4.0f);

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::Dummy(ImVec2(frameHeight * 0.5f, 0.0f));
    ImGui::Dummy(ImVec2(0.0, frameHeight - frameHeight * 0.5f - itemSpacing.y));

    ImGui::EndGroup();

    auto itemMin = ImGui::GetItemRectMin();
    auto itemMax = ImGui::GetItemRectMax();
    //ImGui::GetWindowDrawList()->AddRectFilled(itemMin, itemMax, IM_COL32(255, 0, 0, 64), 4.0f);

    auto labelRect = s_GroupPanelLabelStack.back();
    s_GroupPanelLabelStack.pop_back();

    ImVec2 halfFrame = ImVec2(frameHeight * 0.25f, frameHeight) * 0.5f;
    ImRect frameRect = ImRect(itemMin + halfFrame, itemMax - ImVec2(halfFrame.x, 0.0f));
    labelRect.Min.x -= itemSpacing.x;
    labelRect.Max.x += itemSpacing.x;
    for (int i = 0; i < 4; ++i)
    {
        switch (i)
        {
            // left half-plane
        case 0: ImGui::PushClipRect(ImVec2(-FLT_MAX, -FLT_MAX), ImVec2(labelRect.Min.x, FLT_MAX), true); break;
            // right half-plane
        case 1: ImGui::PushClipRect(ImVec2(labelRect.Max.x, -FLT_MAX), ImVec2(FLT_MAX, FLT_MAX), true); break;
            // top
        case 2: ImGui::PushClipRect(ImVec2(labelRect.Min.x, -FLT_MAX), ImVec2(labelRect.Max.x, labelRect.Min.y), true); break;
            // bottom
        case 3: ImGui::PushClipRect(ImVec2(labelRect.Min.x, labelRect.Max.y), ImVec2(labelRect.Max.x, FLT_MAX), true); break;
        }

        ImGui::GetWindowDrawList()->AddRect(
            frameRect.Min, frameRect.Max,
            ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)),
            halfFrame.x);

        ImGui::PopClipRect();
    }

    ImGui::PopStyleVar(2);

#if IMGUI_VERSION_NUM >= 17301
    ImGui::GetCurrentWindow()->ContentRegionRect.Max.x += frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->WorkRect.Max.x += frameHeight * 0.5f;
    ImGui::GetCurrentWindow()->InnerRect.Max.x += frameHeight * 0.5f;
#else
    ImGui::GetCurrentWindow()->ContentsRegionRect.Max.x += frameHeight * 0.5f;
#endif
    ImGui::GetCurrentWindow()->Size.x += frameHeight;

    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    ImGui::EndGroup();
}

void drawRaider(const EQRaider* r, int color) noexcept
{
    ImGui::SetNextItemWidth(100);
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
    ImGui::LabelText("##Group", "% -15s", r->name);
    if (colorPop) ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(20);
    ImGui::LabelText("##Group", "% 3d", r->level);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SetNextItemWidth(30);
    ImGui::LabelText("##Group", "% 4s", Classes::classShortStrings[r->cls].c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(45);
    ImGui::LabelText("##Group", " %c%c%c%c%c",
        " R"[r->raidLead], " G"[r->groupLead], " L"[r->masterLooter], " A"[r->assist], " M"[r->marker]);
}

void UI::render(IDirect3DDevice9* device) noexcept
{
    static Raid raid;
    static int my_image_width = 0;
    static int my_image_height = 0;
    static char selectedRaider[16] = {0};
    static std::array<std::vector<EQRaider*>, 12> groups{};
    static std::vector<EQRaider*> ungrouped;

    if (!device || exit)
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
            return;
        }

        if (!ImGui_ImplDX9_Init(device))
        {
            return;
        }

        io.Fonts->AddFontDefault();
        ImFontConfig config;
        config.MergeMode = true;
        const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        iconFont = io.Fonts->AddFontFromMemoryTTF(fontawesome_webfont_ttf, fontawesome_webfont_ttf_len, 16.0f, &config, icon_ranges);
        hin = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
        hookInput();

        settingsIcon = getIcon("Tomato");
        if(true) raid.init();
    }

    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

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
        if (ImGui::Button(ICON_FA_COG))
        {
            menuOpen = !menuOpen;
        }
    }
    ImGui::End();

    if ((menuOpen && !settings::openWithRaidWindow) || (settings::openWithRaidWindow && raid.raidWindowOpen()))
    {
        ImGui::Begin("Raid");
        if (true)
        {
            for (auto& v : groups)
            {
                v.clear();
            }
            ungrouped.clear();
            const auto& raiders = raid.read();
            bool isRaidLead = raid.amIRaidLead();
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
                BeginGroupPanel(label.c_str(), ImVec2(150, 0), 220.0f, "MOVE_GROUP", (void*)&i, sizeof(i));
                ImGui::BeginGroup();
                std::string id = fmt::format("##Groupid{}", i);
                ImGui::PushID(id.c_str());
                for (auto* r : groups[i])
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
                            Game::hookedCommandFunc(0, 0, 0, targetStr.c_str());
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
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 235);
                    drawRaider(r, color);
                    ImGui::PopID();
                }
                int groupSize = groups[i].size();
                if (groupSize < 6)
                {
                    ImGui::PushID(i);
                    ImGui::BeginChild("##EmptyGroup", { 230.0f, 22.5f * (6 - groupSize) }, false, ImGuiWindowFlags_NoScrollbar);
                    for (int j = groupSize; j < 6; ++j)
                    {
                        ImGui::SetNextItemWidth(230);
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
                        Game::hookedCommandFunc(0, 0, 0, targetStr.c_str());
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
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 235);
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
            if (ImGui::Button("ML", { 70,25 }) && (isRaidLead || strcmp("Teach", raid.myName()) == 0))
            {
                raid.clickButton(RaidButton::masterlooter);
            }
            if (ImGui::Button("Assist", { 70,25 }) && isRaidLead)
            {
                raid.clickButton(RaidButton::assist);
            }
            if (ImGui::Button("MarkNPC", { 70,25 }) && isRaidLead)
            {
                raid.clickButton(RaidButton::mark);
            }
            ImGui::EndGroup();
            EndGroupPanel();

            ImGui::SameLine();
            BeginGroupPanel("Grouping", { 95, 200 }, 95, 0, 0, 0);
            if (ImGui::Button("Group Alts", { 85, 25 }) && isRaidLead)
            {
                raid.groupAlts();
            }
            if (ImGui::Button("Make Groups", { 85,25 }) && isRaidLead)
            {
                raid.makeGroups();
            }
            if (ImGui::Button("Kill Groups", { 85,25 }) && isRaidLead)
            {
                raid.killGroups();
            }
            EndGroupPanel();
            ImGui::SameLine();

            BeginGroupPanel("Guild Invites", { 200, 200 }, 200, 0, 0, 0);
            static bool inviteAlts = true;
            static int inviteMinLevel = 1;
            ImGui::BeginGroup();
            ImGui::Checkbox("Alts", &inviteAlts);
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragInt("Min Level", &inviteMinLevel, .8f, 1, 130);
            ImGui::EndGroup();
            ImGui::SameLine();
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

            ImGui::Dummy(ImVec2(0, 10));
            ImGui::Checkbox("Move button", &moveMenuButton);
            ImGui::Checkbox("Open with raid window", &settings::openWithRaidWindow);
            
            ImGui::Dummy(ImVec2(140, 0));
            ImGui::SameLine();
            if (ImGui::Button("Exit##e2"))
            {
                //Game::hookedBazaarFindFunc(0, 0, 0);
                //settings::save();
                exit = true;
            }
            ImGui::End();
        }
    }

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
    fnGetDeviceData = (IDirectInputDevice_GetDeviceData_t)DetourFunction((PBYTE)vTable[10], (PBYTE)HookedGetDeviceData);
    fnGetDeviceState = (IDirectInputDevice_GetDeviceState_t)DetourFunction((PBYTE)vTable[9], (PBYTE)HookedGetDeviceState);
    fnSetDeviceFormat = (IDirectInputDevice_SetDeviceFormat_t)DetourFunction((PBYTE)vTable[11], (PBYTE)HookedSetDeviceFormat);

    Game::hook({ "RaidGroupFunc", "CommandFunc"/*, "BazaarFindFunc"*/});
    //Game::hook({ "RaidGroupFunc"});
    inputHooked = true;
}

void UI::unhookInput() noexcept
{
    if (fnGetDeviceData) DetourRemove((PBYTE)fnGetDeviceData, (PBYTE)HookedGetDeviceData);
    if (fnGetDeviceState) DetourRemove((PBYTE)fnGetDeviceState, (PBYTE)HookedGetDeviceState);
    if (fnSetDeviceFormat) DetourRemove((PBYTE)fnSetDeviceFormat, (PBYTE)HookedSetDeviceFormat);
    Game::unhook();
    inputHooked = false;
}