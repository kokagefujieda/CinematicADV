// Copyright 2026 kokage. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CinematicADVSettings.generated.h"

class UCinematicADVConfig;

/**
 * Project-wide settings for CinematicADV.
 * Accessible via Project Settings → Plugins → CinematicADV.
 */
UCLASS(Config=Game, DefaultConfig, DisplayName="CinematicADV")
class CINEMATICADV_API UCinematicADVSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCinematicADVSettings();

	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName()  const override { return FName("CinematicADV"); }

	/** DataAsset that defines the Input Mapping Context and Input Actions for CinematicADV. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Input",
		meta=(DisplayName="CinematicADV Config Asset"))
	TSoftObjectPtr<UCinematicADVConfig> ConfigAsset;

	static const UCinematicADVSettings* Get() { return GetDefault<UCinematicADVSettings>(); }
};
