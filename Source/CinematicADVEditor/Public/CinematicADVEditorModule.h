// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FCinematicADVEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FDelegateHandle TrackEditorBindingHandle;
};
