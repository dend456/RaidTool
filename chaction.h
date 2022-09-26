#pragma once
#include <boost/xpressive/xpressive.hpp>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Action;
class CommandAction;
class PauseAction;
class AudioAction;

namespace PolymorphicJsonSerializer_impl {
	template <class Base>
	struct Serializer {
		void (*to_json)(json& j, Base const& o);
		void (*from_json)(json const& j, Base& o);
	};

	template <class Base, class Derived>
	Serializer<Base> serializerFor() {
		return {
			[](json& j, Base const& o) {
				return to_json(j, static_cast<Derived const&>(o));
			},
			[](json const& j, Base& o) {
				return from_json(j, static_cast<Derived&>(o));
			}
		};
	}
}

template <class Base>
struct PolymorphicJsonSerializer 
{
	static inline std::unordered_map<std::string, PolymorphicJsonSerializer_impl::Serializer<Base>> _serializers
	{
		{ typeid(CommandAction).name(), PolymorphicJsonSerializer_impl::serializerFor<Base, CommandAction>()},
		{ typeid(PauseAction).name(), PolymorphicJsonSerializer_impl::serializerFor<Base, PauseAction>()},
		{ typeid(AudioAction).name(), PolymorphicJsonSerializer_impl::serializerFor<Base, AudioAction>()}
	};
	

	template <class... Derived>
	static void register_types() {
		(_serializers.emplace(std::string(typeid(Derived).name()), PolymorphicJsonSerializer_impl::serializerFor<Base, Derived>()), ...);
	}

	static void to_json(json& j, Base const& o) {
		char const* typeName = typeid(o).name();
		_serializers.at(typeName).to_json(j, o);
		j["_type"] = typeName;
	}

	static void from_json(json const& j, Base& o) {
		_serializers.at(j.at("_type").get<std::string>()).from_json(j, o);
	}
};

namespace nlohmann {
	template <typename T>
	struct adl_serializer<std::unique_ptr<T>> {
		static void to_json(json& j, const std::unique_ptr<T>& opt) {
			if (opt) {
				j = *opt.get();
			}
			else {
				j = nullptr;
			}
		}
		static void from_json(const json& j, std::unique_ptr<T>& opt)
		{
			if (j.is_null())
			{
				opt = nullptr;
			}
			else
			{
				std::string t = j.at("_type");
				if (t == typeid(CommandAction).name())
				{
					opt = std::make_unique<CommandAction>(j.get<CommandAction>());
				}
				else if (t == typeid(PauseAction).name())
				{
					opt = std::make_unique<PauseAction>(j.get<PauseAction>());
				}
				else if (t == typeid(AudioAction).name())
				{
					opt = std::make_unique<AudioAction>(j.get<AudioAction>());
				}
			}
		}
	};
}


class Action
{
public:
	bool enabled = true;

	virtual ~Action() = default;

	virtual void fire(const boost::xpressive::cmatch& match) noexcept = 0;
	virtual void renderCreator() noexcept = 0;
};

class CommandAction : public Action
{
public:
	std::string command;

	void fire(const boost::xpressive::cmatch& match) noexcept override;
	void renderCreator() noexcept override;
};

class PauseAction : public Action
{
public:
	int time = 0;
	void fire(const boost::xpressive::cmatch& match) noexcept override;
	void renderCreator() noexcept override;
};

class AudioAction : public Action
{
public:
	void fire(const boost::xpressive::cmatch& match) noexcept override;
	void renderCreator() noexcept override;
};

namespace nlohmann 
{
	template <>
	struct adl_serializer<Action> : PolymorphicJsonSerializer<Action> { };
}

class Chaction
{
public:
	std::string name = "New Trigger";
	std::string regexBuff;
	bool creatorOpen = false;
	bool regexError = false;

	Chaction() = default;
	Chaction(Chaction&&) = default;
	Chaction& operator= (Chaction&&) = default;
	Chaction(const Chaction&) = delete;
	Chaction& operator= (const Chaction&) = delete;

	std::vector<std::unique_ptr<Action>> actions;
	boost::xpressive::cregex regex; 

	bool renderCreator() noexcept;
	bool render() noexcept;
};


class ChactionGroup
{
public:
	std::string name = "New Group";
	bool enabled = false;

	ChactionGroup() = default;
	ChactionGroup(ChactionGroup&&) = default;
	ChactionGroup& operator= (ChactionGroup&&) = default;
	ChactionGroup(const ChactionGroup&) = delete;
	ChactionGroup& operator= (const ChactionGroup&) = delete;

	std::vector<ChactionGroup> subgroups;
	std::vector<Chaction> chactions;

	bool render() noexcept;
	void onMessage(const char* msg) noexcept;
};


void to_json(json& js, const Action& act);
void to_json(json& js, const CommandAction& act);
void to_json(json& js, const PauseAction& act);
void from_json(const json& js, Action& act);
void from_json(const json& js, CommandAction& act);
void from_json(const json& js, PauseAction& act);

void to_json(json& js, const Chaction& chaction);
void to_json(json& js, const ChactionGroup& chactionGroup);
void from_json(const json& js, Chaction& chaction);
void from_json(const json& js, ChactionGroup& chactionGroup);