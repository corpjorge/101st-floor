#include "Floor101.h"
#include "Modules/ModuleManager.h"
#include "RenderCheck.h"
#include "HdrCheck.h"

DEFINE_LOG_CATEGORY(LogFloor101);

class FFloor101Module : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		Floor101::RenderCheck::StartIfRequested();
		Floor101::HDRCheck::StartIfRequested();
	}

	virtual void ShutdownModule() override
	{
		Floor101::RenderCheck::Stop();
		Floor101::HDRCheck::Stop();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FFloor101Module, Floor101, "Floor101");
