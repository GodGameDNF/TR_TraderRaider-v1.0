#include <filesystem>
#include <fstream>
#include <iostream>
#include <windows.h>
#include <string>
#include <vector>

#include <chrono>
#include <ctime>
#include <iomanip>

using namespace RE;

class FactionManager;
FactionManager* factionManager = nullptr;

PlayerCharacter* p = nullptr;
BSScript::IVirtualMachine* vm = nullptr;
TESDataHandler* dataHandler = nullptr;
TESFaction* caravanLeader = nullptr;
BGSKeyword* kcheck = nullptr;


bool isVendorDeathHandlerRegistered = false;

//126570;
//BGSObjectInstanceExtra* (*_BGSObjectInstanceExtra_ctor)(BGSObjectInstanceExtra*);

void AddItem(BGSInventoryList* list, TESBoundObject* bound, uint32_t count, ExtraDataList* extra, uint16_t c)
{
	using func_t = decltype(&AddItem);
	REL::Relocation<func_t> func{ REL::ID(98986) };
	return func(list, bound, count, extra, c);
}

void RemoveAllItems(BSScript::IVirtualMachine * vm, uint32_t i, TESObjectREFR * del, TESObjectREFR * send, bool bMove) {
	using func_t = decltype(&RemoveAllItems);
	REL::Relocation<func_t> func{ REL::ID(534058) };
	return func(vm, i, del, send, bMove);
}

void CallGlobalFunctionNoWait(BSScript::IVirtualMachine* vm, uint32_t i, void* acb, BSFixedString* scriptname, BSFixedString* functionname, BSScript::ArrayWrapper<BSScript::Variable>* args)
{
	using func_t = decltype(&CallGlobalFunctionNoWait);
	REL::Relocation<func_t> func{ REL::ID(710927) };
	return func(vm, i, acb, scriptname, functionname, args);
}

class FactionManager
{
	private:
		FactionManager()
		{
		// Private constructor to prevent instantiation
		}

		std::vector<RE::TESFaction*> vendorFactions;
		static FactionManager* instance;
	public:
		static FactionManager* GetInstance()
		{	
			if (!instance) {
				instance = new FactionManager();
			}
			return instance;
		}

	void ProcessFactionForms(){
		auto& AllFactionForms = dataHandler->GetFormArray<RE::TESFaction>();

		for (RE::TESFaction* faction : AllFactionForms) {
			if (faction) {
				TESObjectREFR* vendorChest = faction->vendorData.merchantContainer;
				if (vendorChest) {
					uint32_t flags = faction->data.flags;
					bool hasFlag = (flags & 16384) != 0;
					if (hasFlag) {
						vendorFactions.push_back(faction);
					}
				}
			}
		}
	}

	std::vector<TESFaction*> GetFactions() const
	{
		return vendorFactions;
	}
};

FactionManager* FactionManager::instance = nullptr;


void CheckVendor(Actor* a)
{
	TESFaction* vendorfaction = nullptr;
	std::string vendorEditorID;

	std::vector<RE::TESFaction*> vendorFactions = factionManager->GetFactions();
	for (RE::TESFaction* faction : vendorFactions) {
		if (a->IsInFaction(faction)) {
			vendorfaction = faction;
			const char* name = (const char*)vendorfaction->GetFormEditorID();
			if (!name) {
				return;
			}
			vendorEditorID = name;
			if (vendorEditorID.find("WorkshopVendor") == std::string::npos) {
				break;
			}
		}
	}

	if (!vendorfaction) {
		return;
	}

	if (vendorEditorID.find("aravan") != std::string::npos && !(a->IsInFaction(caravanLeader))) {
		//logger::info("상인인데 리더가 아님");
		return;
	} else if (a->IsInFaction(caravanLeader)) {
		//logger::info("카라반 리더");
	} else if (vendorEditorID.find("WorkshopVendor") != std::string::npos) {
		//logger::info("정착지 상인");
		TESForm* note = dataHandler->LookupForm(0x80F, "TR_TraderRaider.esp");
		TESBoundObject* bound = (TESBoundObject*)note;

		ExtraDataList* tempData = new ExtraDataList;
		AddItem(a->inventoryList, bound, 1, tempData, 0);
		//delete tempData;  // ctd 나는데
		return;
	}

	

	TESObjectREFR* vendorChest = vendorfaction->vendorData.merchantContainer;
	if (!vendorChest) {
		//logger::info("상인인데 상자 발견 못함");
		return;
	}

	RemoveAllItems(vm, 0, vendorChest, a, true);

	TESObjectREFR* dChest01 = a->GetLinkedRef(kcheck);
	if (dChest01) {
		//logger::info("추가 상자 발견");
		RemoveAllItems(vm, 0, dChest01, a, true);
	} else {
		//logger::info("추가 상자 없으니 함수 호출");
		std::vector<Actor*> sender;
		sender.push_back(a);

		BSScrapArray<BSScript::Variable> variables = BSScript::detail::PackVariables(sender); // 포인터는 배열에 감싸서 보내야함
		BSScript::ArrayWrapper<BSScript::Variable>* args = new BSScript::ArrayWrapper<BSScript::Variable>(variables, *vm);

		CallGlobalFunctionNoWait(vm, 0, nullptr, new BSFixedString("TR_TraderRaider_F4SE"), new BSFixedString("otherChest"), args);
	}
}

class VendorDeathHandler : public BSTEventSink<TESDeathEvent>
{
public:
	// override
	virtual RE::BSEventNotifyControl ProcessEvent(const TESDeathEvent& a_event, BSTEventSource<TESDeathEvent>* a_eventSource) override
	{
		if (!a_event.dead) {
			Actor* b = (Actor*)a_event.actorDying.get();
			CheckVendor(b);
		}

		return RE::BSEventNotifyControl::kContinue;
	}
};

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case F4SE::MessagingInterface::kGameLoaded:

        if (!isVendorDeathHandlerRegistered) {
			logger::info("등록");

			dataHandler = RE::TESDataHandler::GetSingleton();
			p = PlayerCharacter::GetSingleton();
			caravanLeader = (TESFaction*)dataHandler->LookupForm(0x1328B9, "Fallout4.esm");
			kcheck = (BGSKeyword*)dataHandler->LookupForm(0x0010FCD1, "Fallout4.esm");

			factionManager = FactionManager::GetInstance();
			factionManager->ProcessFactionForms();

			uint32_t tete = (factionManager->GetFactions()).size();
			logger::info("팩션수 {}", tete);

			//FactionRegister();

			VendorDeathHandler* myHandler = new VendorDeathHandler();

			if (auto* eventSource = TESDeathEvent::GetEventSource()) {
				eventSource->RegisterSink(myHandler);
				isVendorDeathHandlerRegistered = true;  // 핸들러가 등록되었음을 표시
			}
		}

		break;
	}
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
{
	vm = a_vm;

	REL::IDDatabase::Offset2ID o2i;
	logger::info("0x0x80750: {}", o2i(0x80750));

	//a_vm->BindNativeMethod("TR_TraderRaider_F4SE"sv, "SendFormID"sv, SendFormID);

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format("{}.log", Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("Global Log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%^%l%$] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	if (papyrus)
		papyrus->Register(RegisterPapyrusFunctions);

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	if (message)
		message->RegisterListener(OnF4SEMessage);

	return true;
}
