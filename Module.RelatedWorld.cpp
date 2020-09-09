// Copyright Delta-Proxima Team (c) 2007-2020

#include "Modules/ModuleManager.h"
#include "RelatedWorldModuleInterface.h"
#include "WorldDirector.h"

void OnRep_ReplicatedMovement_Hook(UObject* Context, FFrame& TheStack, RESULT_DECL);

class FRelatedWorldModule : public IRelatedWorldModule
{
public:
	void StartupModule()
	{
		WorldDirector = NewObject<UWorldDirector>();
		check(WorldDirector);
		WorldDirector->AddToRoot();

		//Setup some hooks on RepNotify events
		UFunction* FUNC_OnRep_ReplicatedMovement = AActor::StaticClass()->FindFunctionByName(TEXT("OnRep_ReplicatedMovement"));
		FUNC_OnRep_ReplicatedMovement->SetNativeFunc(&OnRep_ReplicatedMovement_Hook);
	}

	void ShutdownModule()
	{
		WorldDirector->RemoveFromRoot();
	}

	virtual FORCEINLINE UWorldDirector* GetWorldDirector() const
	{
		return WorldDirector;
	}

private:
	UWorldDirector* WorldDirector;
};

IMPLEMENT_MODULE(FRelatedWorldModule, RelatedWorld);
