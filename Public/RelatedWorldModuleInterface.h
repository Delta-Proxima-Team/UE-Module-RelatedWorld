// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "Modules/ModuleInterface.h"

class UWorldDirector;

class IRelatedWorldModule : public IModuleInterface
{
public:
	virtual FORCEINLINE UWorldDirector* GetWorldDirector() const = 0;
};