// Copyright Delta-Proxima Team (c) 2007-2020

#include "Modules/ModuleManager.h"
#include "RelatedWorldModuleInterface.h"
#include "WorldDirector.h"

class FRelatedWorldModule : public IRelatedWorldModule
{
public:
	void StartupModule()
	{
		WorldDirector = NewObject<UWorldDirector>();
		check(WorldDirector);
		WorldDirector->AddToRoot();
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
