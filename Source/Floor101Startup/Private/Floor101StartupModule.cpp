#include "Floor101Startup.h"
#include "Modules/ModuleManager.h"
#include "PreLoadScreenBase.h"
#include "PreLoadScreenManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFloor101Startup, Log, All);

class FFloor101PreLoadScreen final : public FPreLoadScreenBase
{
public:
	virtual void Init() override { Widget = Floor101::Startup::MakeLoadingWidget(); }
	virtual TSharedPtr<SWidget> GetWidget() override { return Widget; }
	virtual const TSharedPtr<const SWidget> GetWidget() const override { return Widget; }
	virtual void OnPlay(TWeakPtr<SWindow> Window) override
	{
		FPreLoadScreenBase::OnPlay(Window);
		UE_LOG(LogFloor101Startup, Display, TEXT("F101: branded loading screen shown"));
	}
	virtual void OnStop() override { UE_LOG(LogFloor101Startup, Display, TEXT("F101: branded loading screen finished")); }
	virtual void CleanUp() override { Widget.Reset(); }
private:
	TSharedPtr<SWidget> Widget;
};

class FFloor101StartupModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (auto* Manager = FPreLoadScreenManager::Get(); Manager && !GIsEditor && !IsRunningCommandlet())
		{
			Screen = MakeShared<FFloor101PreLoadScreen>();
			Screen->Init();
			Manager->RegisterPreLoadScreen(Screen);
		}
	}
	virtual void ShutdownModule() override
	{
		if (auto* Manager = FPreLoadScreenManager::Get(); Manager && Screen) { Manager->UnRegisterPreLoadScreen(Screen); }
		Screen.Reset();
	}
private:
	TSharedPtr<FFloor101PreLoadScreen> Screen;
};

IMPLEMENT_MODULE(FFloor101StartupModule, Floor101Startup);
