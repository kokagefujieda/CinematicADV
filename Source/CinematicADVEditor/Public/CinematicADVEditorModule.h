// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FCinematicADVEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Content Browser: right-click sounds → Set as Voice. */
	void RegisterMenus();

	FDelegateHandle TrackEditorBindingHandle;
};
