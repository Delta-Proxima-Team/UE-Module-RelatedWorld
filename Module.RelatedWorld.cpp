// Copyright Delta-Proxima Team (c) 2007-2020

#include "Modules/ModuleManager.h"
#include "RelatedWorldModuleInterface.h"
#include "WorldDirector.h"
#include "FunctionHook.h"

#include "GameFramework/Character.h"

DECLARE_UFUNCTION_HOOK(AActor, OnRep_ReplicatedMovement);

DECLARE_UFUNCTION_HOOK(ACharacter, ClientAdjustPosition);
#if ENGINE_MINOR_VERSION == 26
DECLARE_UFUNCTION_HOOK(ACharacter, ClientMoveResponsePacked);
#endif

DECLARE_UFUNCTION_HOOK(APlayerController, ServerUpdateCamera);

class FRelatedWorldModule : public IRelatedWorldModule
{
public:
	void StartupModule()
	{
		WorldDirector = NewObject<UWorldDirector>();
		check(WorldDirector);
		WorldDirector->AddToRoot();

		ENABLE_UFUNCTION_HOOK(AActor, OnRep_ReplicatedMovement);

		ENABLE_UFUNCTION_HOOK(ACharacter, ClientAdjustPosition);
#if ENGINE_MINOR_VERSION == 26
		ENABLE_UFUNCTION_HOOK(ACharacter, ClientMoveResponsePacked);
#endif

		FLAGS_UFUNCTION_HOOK(APlayerController, ServerUpdateCamera, FUNC_Static);
		ENABLE_UFUNCTION_HOOK(APlayerController, ServerUpdateCamera);
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
	DEFINE_UFUNCTION_HOOK(AActor, OnRep_ReplicatedMovement);
	DEFINE_UFUNCTION_HOOK(ACharacter, ClientAdjustPosition);
#if ENGINE_MINOR_VERSION == 26
	DEFINE_UFUNCTION_HOOK(ACharacter, ClientMoveResponsePacked);
#endif
	DEFINE_UFUNCTION_HOOK(APlayerController, ServerUpdateCamera);
	UWorldDirector* WorldDirector;
};

IMPLEMENT_MODULE(FRelatedWorldModule, RelatedWorld);
