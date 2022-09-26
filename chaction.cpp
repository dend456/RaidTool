#include <Windows.h>
#include <Mmsystem.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "chaction.h"
#include <game.h>
#include <fmt/core.h>
#include "settings.h"
#include "imgui_ext.h"
#include <boost/xpressive/xpressive.hpp>
#pragma comment(lib, "winmm.lib")

void CommandAction::fire(const boost::xpressive::cmatch& match) noexcept
{
	if (command[0])
	{
		std::string repCommand = command;
		for (int i = 1; i < match.size(); ++i)
		{
			boost::xpressive::sregex rep = boost::xpressive::as_xpr(fmt::format("{{{}}}", i - 1));
			repCommand = boost::xpressive::regex_replace(repCommand, rep, match[i]);
		}
		if (!repCommand.empty())
		{
			//fmt::print(settings::logFile, "fire - {}\n", repCommand);
			//fflush(settings::logFile);
			Game::hookedCommandFunc(0, 0, repCommand.c_str());
		}
	}
}

void CommandAction::renderCreator() noexcept
{
	ImGui::SetNextItemWidth(60);
	ImGui::LabelText("##commandactioncommand", "Command:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(300);
	ImGui::InputText("##commandactioncommandtext", &command, sizeof(command));
}
void PauseAction::fire(const boost::xpressive::cmatch& match) noexcept
{

}
void PauseAction::renderCreator() noexcept
{

}
void AudioAction::fire(const boost::xpressive::cmatch& match) noexcept
{
	PlaySound((LPCSTR)SND_ALIAS_SYSTEMSTART, NULL, SND_ALIAS_ID | SND_ASYNC);
}
void AudioAction::renderCreator() noexcept
{
	ImGui::SetNextItemWidth(60);
	ImGui::LabelText("##audioaction", "wav:");
}

bool Chaction::renderCreator() noexcept
{
	bool ret = false;
	ImGui::PushID(this);

	ImGui::SetNextItemWidth(60);
	ImGui::Text("   Name:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(300);
	ImGui::InputText("##chactionnameinput", &name, sizeof(name));
	ImGui::Text("  Regex:");
	ImGui::SameLine();
	bool needsColorPop = false;
	if (regexError)
	{
		needsColorPop = true;
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
	}
	ImGui::SetNextItemWidth(300);
	if (ImGui::InputText("##chactionregexinput", &regexBuff, sizeof(regexBuff)))
	{
		try
		{
			regex = boost::xpressive::cregex::compile(regexBuff);
			regexError = false;
		}
		catch (const std::exception&)
		{
			regexError = true;
		}
	}
	if (needsColorPop)
	{
		ImGui::PopStyleColor();
	}

	ImGui::BeginGroup();
	int id = 0;
	int removeId = -1;
	for (auto& a : actions)
	{
		ImGui::PushID(id);
		a->renderCreator();
		ImGui::SameLine();
		if (ImGui::Button("X"))
		{
			removeId = id;
		}
		++id;
		ImGui::PopID();
	}
	ImGui::EndGroup();
	
	if (removeId != -1)
	{
		actions.erase(actions.begin() + removeId);
	}

	if (ImGui::Button("Add Command"))
	{
		actions.push_back(std::make_unique<CommandAction>());
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Audio"))
	{
		actions.push_back(std::make_unique<AudioAction>());
	}
	ImGui::PopID();
	return ret;
}

bool Chaction::render() noexcept
{
	bool ret = false;
	ImGui::PushID(this);
	if (creatorOpen)
	{
		if (BeginGroupPanel(name.c_str(), {300, 300}, 300, nullptr, nullptr, 0))
		{
			creatorOpen = !creatorOpen;
		}
		ret = renderCreator();
		EndGroupPanel();
	}
	else
	{
		ImGui::Text("%s", name);
		if (ImGui::IsItemClicked(0))
		{
			creatorOpen = !creatorOpen;
		}
		else if (ImGui::IsItemClicked(1))
		{
			ImGui::OpenPopup("chactionmenu");
		}
		if (ImGui::BeginPopup("chactionmenu"))
		{
			if (ImGui::MenuItem("Delete"))
			{
				ret = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
	ImGui::PopID();
	return ret;
}

bool ChactionGroup::render() noexcept
{
	static constexpr float INDENT_WIDTH = 10.0f;
	bool ret = false;
	ImGui::PushID(this);
	ImGui::Checkbox("##enabled", &enabled);
	ImGui::SameLine();
	bool treeOpen = ImGui::TreeNode(name.c_str());
	if (ImGui::IsItemClicked(1))
	{
	ImGui::OpenPopup("chactiongroupmenu");
	}
	if (ImGui::BeginPopup("chactiongroupmenu"))
	{
		if (ImGui::MenuItem("New Group"))
		{
			subgroups.emplace_back();
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem("New Chaction"))
		{
			chactions.emplace_back();
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem("Delete"))
		{
			ret = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if(treeOpen)
	{
		ImGui::Indent(INDENT_WIDTH);
		std::size_t toDel = subgroups.size();
		for (std::size_t ind = 0; ind < subgroups.size(); ++ind)
		{
			subgroups[ind].render();
		}
		if (toDel < subgroups.size())
		{

		}

		ImGui::Indent(INDENT_WIDTH * 2);

		toDel = chactions.size();
		for (std::size_t ind = 0; ind < chactions.size(); ++ind)
		{
			if (chactions[ind].render())
			{
				toDel = ind;
			}
		}
		if (toDel < chactions.size())
		{
			chactions.erase(chactions.begin() + toDel);
		}

		ImGui::Unindent(INDENT_WIDTH * 3);
		ImGui::TreePop();
	}
	ImGui::PopID();
	return false;
}

void ChactionGroup::onMessage(const char* msg) noexcept
{
	if (enabled)
	{
		boost::xpressive::cmatch match;
		for (auto& chaction : chactions)
		{
			if (boost::xpressive::regex_match(msg, match, chaction.regex))
			{
				for (auto& action : chaction.actions)
				{
					action->fire(match);
				}
			}
		}

		for (auto& sub : subgroups)
		{
			sub.onMessage(msg);
		}
	}
}

void to_json(json& js, const Action& act)
{
}

void to_json(json& js, const CommandAction& act)
{
	js["command"] = act.command;
}

void to_json(json& js, const PauseAction& act)
{
	js["time"] = act.time;
}


void from_json(const json& js, Action& act)
{

}

void from_json(const json& js, CommandAction& act)
{
	act.command = js.at("command").get<std::string>();
}

void from_json(const json& js, PauseAction& act)
{
	act.time = js.at("time").get<int>();
}

void to_json(json& js, const Chaction& chaction)
{
	js["name"] = chaction.name;
	js["regex"] = chaction.regexBuff;
	js["actions"] = chaction.actions;
}

void to_json(json& js, const ChactionGroup& chactionGroup)
{
	js["name"] = chactionGroup.name;
	js["enabled"] = chactionGroup.enabled;
	js["chactions"] = chactionGroup.chactions;
	js["subgroups"] = chactionGroup.subgroups;
}

void from_json(const json& js, Chaction& chaction)
{
	chaction.name = js.at("name").get<std::string>();
	chaction.regexBuff = js.at("regex").get<std::string>();
	chaction.actions = js.at("actions").get<decltype(chaction.actions)>();

	try
	{
		chaction.regex = boost::xpressive::cregex::compile(chaction.regexBuff);
		chaction.regexError = false;
	}
	catch (const std::exception&)
	{
		chaction.regexError = true;
	}
}

void from_json(const json& js, ChactionGroup& chactionGroup)
{
	chactionGroup.name = js.at("name").get<std::string>();
	chactionGroup.enabled = js.at("enabled").get<bool>();
	chactionGroup.chactions = js.at("chactions").get<decltype(chactionGroup.chactions)>();
	chactionGroup.subgroups = js.at("subgroups").get<decltype(chactionGroup.subgroups)>();
}