#include <SokuLib.hpp>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include "main.hpp"

HMODULE hDllModule;
std::filesystem::path basePath;

//credit enebe/shady-loader
static bool GetModulePath(HMODULE handle, std::filesystem::path& result) {
	std::wstring buffer;
	int len = MAX_PATH + 1;
	do {
		buffer.resize(len);
		len = GetModuleFileNameW(handle, buffer.data(), buffer.size());
	} while (len > buffer.size());

	if (len) result = std::filesystem::path(buffer.begin(), buffer.end()).parent_path();
	return len;
}

static inline std::filesystem::path toAbsPath(const std::filesystem::path& path, const std::filesystem::path& base = basePath) {
	return std::filesystem::weakly_canonical(path.is_absolute() ? path : base / path);
}

namespace {
	using SokuLib::SpriteEx, SokuLib::DrawUtils::DxSokuColor, SokuLib::TamperNearCall;
	using SokuLib::v2::AnimationObject, SokuLib::v2::Player;
	using TrailImage = struct __TrailImage {
		struct SpriteTrail : public SpriteEx {
			byte blendMode;
			char align[3];
		};
		SokuLib::List<SpriteTrail> sprites;
		byte step;
		char align0d;
		short maxSize;
		DWORD color;
		byte unknown14;
		char align15[3];
	};
#define ADDR_TRAIL_COPIER 0x4634ce
	//void __thiscall CSpriteEx_copy(CSpriteEx*, CSpriteEx*)
	using OrgFunc1 = void __thiscall(SpriteEx*, SpriteEx*);
	using OrgFunc1Ptr = OrgFunc1*;
	OrgFunc1Ptr orgRenderFixer = nullptr;
	static void __fastcall RenderFixer(SpriteEx* This, int _, SpriteEx* character) {
		orgRenderFixer(This, character);
		if (iniProxy["RenderFix"_l]["Enabled"_l]) {
			auto& parent = *reinterpret_cast<Player*>((DWORD)character - 4);
			DxSokuColor color(iniProxy["RenderFix"_l]["Color"_l]);
			color *= DxSokuColor(parent.renderInfos.color);
			This->setColor(color);
			if (iniProxy["Others"_l]["Debug"_l]) {
				auto& trail = *reinterpret_cast<TrailImage*>(&parent.unknown714);
				trail.color = parent.teamId ? 0x0000ffff : 0x00ff00ff;
				trail.step = 1;
			}
		}
	}

#define ADDR_TRAIL_APPEND 0x46a71c
	//void __thiscall PlayerTrail_addNode(TrailImage*, AnimationState*, byte mode)
	using OrgFunc2 = void __thiscall(TrailImage*, SpriteEx*, byte);
	using OrgFunc2Ptr = OrgFunc2*;
	static int lastFrame = -1;//hasty fit
	const int& gCounter = *reinterpret_cast<int*>(0x8985d8);
	OrgFunc2Ptr orgAutoTrailer = nullptr;
	static void __fastcall AutoTrailer(TrailImage* This, int _, SpriteEx* character, byte mode) {
		if (iniProxy["Others"_l]["TryFitRivDecerlerate"_l]) {
			if (lastFrame == gCounter) {
				std::cout << "jumped" <<std::endl;
				return;
			}
		}
		if (iniProxy["AutoTrail"_l]["Enabled"_l]) {
			int cmode = iniProxy["AutoTrail"_l]["BlendMode"_l];
			mode = cmode>=0 ? cmode : mode;
			orgAutoTrailer(This, character, mode);
		}
	}

#define ADDR_TRAIL_SETTER 0x56d649
	//void __thiscall Player_SetTrail(Player *this, int timer, int step, uint color)
	using OrgFunc3 = void __thiscall(Player*, int, int, UINT32);
	using OrgFunc3Ptr = OrgFunc3*;
	OrgFunc3Ptr orgYoumuTrailer = nullptr;
	static void __fastcall YoumuTrailer(Player* This, int _, int timer, int step, UINT32 color) {
		if (iniProxy["Youmu6DTrail"_l]["Enabled"_l]) {
			step = iniProxy["Youmu6DTrail"_l]["Step"_l];
			color = iniProxy["Youmu6DTrail"_l]["Color"_l];
			orgYoumuTrailer(This, timer, step, color);
		}
	}

#define ADDR_TOTAL_RENDER 0x47a9bf
	//void __thiscall CBattleManager_RenderBattle(CBattleManager *this)
	using OrgFunc4 = void __thiscall(SokuLib::BattleManager*);
	using OrgFunc4Ptr = OrgFunc4*;
	OrgFunc4Ptr orgTotalRenderer = nullptr;
	static void __fastcall TotalRenderer(SokuLib::BattleManager* This) {
		orgTotalRenderer(This);
		std::cout << lastFrame << "->" << gCounter <<"\n" << std::endl;
		lastFrame = gCounter;
	}

#define ADDR_TRAIL_CLEANER_JUDGE 0x46a728
}


// Called when the mod loader is ready to initialize this module.
// All hooks should be placed here. It's also a good moment to load settings from the ini.
extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule)
{
	DWORD old;

#ifdef _DEBUG
	FILE *_;

	AllocConsole();
	freopen_s(&_, "CONOUT$", "w", stdout);
	freopen_s(&_, "CONOUT$", "w", stderr);
#endif
	hDllModule = hMyModule;

	GetModulePath(hMyModule, basePath);
	std::cout << "Absolute Folder Path[" << basePath << "]\n";
	iniProxy.setPath(toAbsPath(iniProxy.getPath()));
	iniProxy.load();//load .ini settings

	puts("Hello, world!");
	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	orgRenderFixer = TamperNearCall(ADDR_TRAIL_COPIER, SokuLib::union_cast<decltype(orgRenderFixer)>(&RenderFixer));
	orgAutoTrailer = TamperNearCall(ADDR_TRAIL_APPEND, SokuLib::union_cast<decltype(orgAutoTrailer)>(&AutoTrailer));
	orgYoumuTrailer = TamperNearCall(ADDR_TRAIL_SETTER, SokuLib::union_cast<decltype(orgYoumuTrailer)>(&YoumuTrailer));
	if (iniProxy["Others"_l]["TryFitRivDecerlerate"_l]) {
		orgTotalRenderer = TamperNearCall(ADDR_TOTAL_RENDER, SokuLib::union_cast<decltype(orgTotalRenderer)>(&TotalRenderer));
	}
	if (iniProxy["Others"_l]["Debug"_l]) {
		std::cout << "Debug mode enabled" << std::endl;
		*(byte*)ADDR_TRAIL_CLEANER_JUDGE = 0xEB;//jg -> jmp
	}
	VirtualProtect((PVOID)TEXT_SECTION_OFFSET, TEXT_SECTION_SIZE, old, &old);

	FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	return true;
}

// We check if the game version is what we target (in our case, Soku 1.10a).
extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16])
{
	return memcmp(hash, SokuLib::targetHash, sizeof(SokuLib::targetHash)) == 0;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	return TRUE;
}

// New mod loader functions
// Loading priority. Mods are loaded in order by ascending level of priority (the highest first).
// When 2 mods define the same loading priority the loading order is undefined.
extern "C" __declspec(dllexport) int getPriority()
{
	return 0;
}

// Not yet implemented in the mod loader, subject to change
// SokuModLoader::IValue **getConfig();
// void freeConfig(SokuModLoader::IValue **v);
// bool commitConfig(SokuModLoader::IValue *);
// const char *getFailureReason();
// bool hasChainedHooks();
// void unHook();