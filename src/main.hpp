#pragma once
#include <iostream>

#include "config.hpp"
using namespace cfg::ex;
inline auto iniProxy = cfg::Config{
#ifdef INI_FILENAME
	INI_FILENAME,
#else
	L"YoumuTrailFix.ini",
#endif // INI_FILENAME
	addSection<"AutoTrail">(addBool<"Enabled">(true)
		,addInteger<"BlendMode">(2)//0, 1, 2
	),
	addSection<"RenderFix">(addBool<"Enabled">(true)
		,addInteger<"Color">(0x40ffffff)
	),
	addSection<"Youmu6DTrail">(addBool<"Enabled">(true)
		,addInteger<"Step">(4)
		,addInteger<"Color">(0x00ffff00)
	),
	addSection<"Others">(

	)
};

#include <filesystem>
extern HMODULE hDllModule;
extern std::filesystem::path basePath;

#include <Tamper.hpp>
template <int oprSize>
class TrampTamper {//credit enebe shady/memory.cpp
	static_assert(oprSize >= 5);
	static constexpr int size = oprSize + 12;
	//std::array<byte, size> shim;
	byte* shim = nullptr;
	DWORD addr = NULL;

	TrampTamper(const TrampTamper&) = delete;
	TrampTamper& operator=(const TrampTamper&) = delete;
public:
	inline auto alloc(int sz = size) {//TODO: pool allocator in a single page
		return reinterpret_cast<decltype(shim)>(
			VirtualAlloc(nullptr, sz, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)
			);
	}
	inline void hook(void* target) {
		if (!addr) return;
		shim = alloc(size);
		if (!shim) return;
		int i = -1;
		shim[++i] = 0x60;// 0;pushad
		//shim[++i] = 0x8B; shim[++i] = 0xC8;// 1;mov ecx, eax
		auto& calla = shim[++i]; calla = 0xE8; //	1;call hook
		i += 4;//	2~5;to target
		shim[++i] = 0x61;// 6;popad
		auto& opra = shim[++i];// 7~size-6;org opr
		auto& jmpa = shim[i += oprSize]; jmpa = 0xE9; // size-5;jmp org
		i += 4;// size-4~size-1; 
#ifdef _DEBUG
		assert(i == size - 1);
#endif // _DEBUG

		DWORD old;
		if (VirtualProtect((PVOID)addr, oprSize, PAGE_EXECUTE_READWRITE, &old)) {
			memcpy((void*)&opra, (const void*)addr, oprSize);
			memset((void*)addr, 0x90, oprSize);
			SokuLib::TamperNearJmp(addr, shim);
			VirtualProtect((PVOID)addr, oprSize, old, &old);

			SokuLib::TamperNearCall((DWORD)&calla, target);
			SokuLib::TamperNearJmp((DWORD)&jmpa, addr + oprSize);
		}
		else {
			VirtualFree(shim, 0, MEM_RELEASE); shim = nullptr;
		}
	}
	inline void restore() {
		if (!addr || !shim) return;
		auto& opra = shim[7];
		DWORD old;
		VirtualProtect((PVOID)addr, oprSize, PAGE_EXECUTE_READWRITE, &old);
		memcpy((void*)addr, &opra, oprSize);
		VirtualProtect((PVOID)addr, oprSize, old, &old);

		VirtualFree(shim, 0, MEM_RELEASE); shim = nullptr;
	}
	TrampTamper(DWORD source) : addr(source) {}
	~TrampTamper() {
		restore();
	}
};
