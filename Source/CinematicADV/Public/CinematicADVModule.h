// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FCinematicADVModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
