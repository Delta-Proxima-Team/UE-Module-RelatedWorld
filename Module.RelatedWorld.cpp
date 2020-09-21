// Copyright Delta-Proxima Team (c) 2007-2020

#include "Modules/ModuleManager.h"
#include "RelatedWorldModuleInterface.h"
#include "WorldDirector.h"

#include "GameFramework/Character.h"

void HOOK_AActor_OnRep_ReplicatedMovement(UObject* Context, FFrame& Stack, RESULT_DECL);

void HOOK_ACharacter_ClientAdjustPosition(UObject* Context, FFrame& Stack, RESULT_DECL);

class FRelatedWorldModule : public IRelatedWorldModule
{
public:
	void StartupModule()
	{
		WorldDirector = NewObject<UWorldDirector>();
		check(WorldDirector);
		WorldDirector->AddToRoot();

		//Setup some hooks on RepNotify events
		UFunction* FUNC_AActor_OnRep_ReplicatedMovement = AActor::StaticClass()->FindFunctionByName(TEXT("OnRep_ReplicatedMovement"));
		FUNC_AActor_OnRep_ReplicatedMovement->SetNativeFunc(&HOOK_AActor_OnRep_ReplicatedMovement);

		//Character Hooks
		UFunction* FUNC_ACharacter_ClientAdjustPosition = ACharacter::StaticClass()->FindFunctionByName(TEXT("ClientAdjustPosition"));
		FUNC_ACharacter_ClientAdjustPosition->SetNativeFunc(&HOOK_ACharacter_ClientAdjustPosition);
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
