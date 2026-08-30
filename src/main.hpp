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
		addBool<"Debug">(false)
		,addBool<"TryFitRivDecerlerate">(false)
		,addBool<"Sandevistan">(false)
	)
};

#include "utils/tramphook.hpp"
using utils::TrampTamper, utils::CPUState;

#include <filesystem>
extern HMODULE hDllModule;
extern std::filesystem::path basePath;
