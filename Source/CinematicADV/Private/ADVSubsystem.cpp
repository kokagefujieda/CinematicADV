// Copyright 2026 kokage. All Rights Reserved.

#include "ADVSubsystem.h"
#include "CinematicADVConfig.h"
#include "SSkipGaugeWidget.h"
#include "LevelSequencePlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/SOverlay.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UADVSubsystem::Deinitialize()
{
	HideSkipGauge();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void UADVSubsystem::RegisterSequencePlayer(ULevelSequencePlayer* Player)
{
	if (ULevelSequencePlayer* OldPlayer = ActivePlayer.Get())
	{
		OldPlayer->OnStop.RemoveDynamic(this, &UADVSubsystem::OnPlayerStopped);
	}

	// Cancel any in-progress skip
	OnSkipReleased();

	ActivePlayer      = Player;
	bSectionActive    = false;
	bAdvanceRequested = false;

	if (Player)
	{
		Player->OnStop.AddDynamic(this, &UADVSubsystem::OnPlayerStopped);
	}

	TryBindInput();
}

void UADVSubsystem::TryAutoRegisterPlayer()
{
	if (ActivePlayer.IsValid()) { return; }

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) { return; }

	for (TObjectIterator<ULevelSequencePlayer> It; It; ++It)
	{
		ULevelSequencePlayer* Player = *It;
		if (Player && Player->IsPlaying() && Player->GetWorld() == World)
		{
			RegisterSequencePlayer(Player);
			return;
		}
	}
}

void UADVSubsystem::TryBindInput()
{
	if (bInputBound) { return; }

	// Auto-discover UCinematicADVConfig:
	//   /Game/ paths take priority over plugin Content paths.
	//   Multiple /Game/ configs → warn and abort.
	UCinematicADVConfig* Config = nullptr;

	if (FAssetRegistryModule* ARModule =
		FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		TArray<FAssetData> AllAssets;
		ARModule->Get().GetAssetsByClass(
			UCinematicADVConfig::StaticClass()->GetClassPathName(), AllAssets);

		TArray<FAssetData> UserAssets;
		TArray<FAssetData> PluginAssets;
		for (const FAssetData& Asset : AllAssets)
		{
			const FString Path = Asset.PackagePath.ToString();
			if (Path.StartsWith(TEXT("/Game/")))
				UserAssets.Add(Asset);
			else
				PluginAssets.Add(Asset);
		}

		if (UserAssets.Num() == 1)
		{
			Config = Cast<UCinematicADVConfig>(UserAssets[0].GetAsset());
		}
		else if (UserAssets.Num() > 1)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CinematicADV] %d UCinematicADVConfig assets found under /Game/. "
				     "Cannot auto-select. Please ensure only one exists."),
				UserAssets.Num());
			return;
		}
		else if (PluginAssets.Num() > 0)
		{
			Config = Cast<UCinematicADVConfig>(PluginAssets[0].GetAsset());
		}
	}
	if (!Config) { return; }

	APlayerController* PC = GetGameInstance()
		? GetGameInstance()->GetFirstLocalPlayerController()
		: nullptr;
	if (!PC) { return; }

	// Activate the Input Mapping Context
	if (ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (auto* InputSub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			if (Config->InputMappingContext)
			{
				InputSub->AddMappingContext(Config->InputMappingContext, 90);
			}
		}
	}

	// Cache config values
	ConfigFadeDuration = Config->FadeOutDuration;
	ConfigHoldDuration = FMath::Max(Config->HoldDuration, 0.1f);
	ConfigGaugeSize    = Config->GaugeSize;
	ConfigGaugeColor   = Config->GaugeColor;
	ConfigGaugeBgColor = Config->GaugeBackgroundColor;

	// Bind actions
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		if (Config->AdvanceAction)
		{
			EIC->BindAction(Config->AdvanceAction, ETriggerEvent::Started, this, &UADVSubsystem::Advance);
			bInputBound = true;
		}
		if (Config->SkipAction)
		{
			EIC->BindAction(Config->SkipAction, ETriggerEvent::Started,   this, &UADVSubsystem::OnSkipPressed);
			EIC->BindAction(Config->SkipAction, ETriggerEvent::Completed, this, &UADVSubsystem::OnSkipReleased);
			EIC->BindAction(Config->SkipAction, ETriggerEvent::Canceled,  this, &UADVSubsystem::OnSkipReleased);
		}
	}
}

// ---------------------------------------------------------------------------
// Section enter (called by FClickWaitEvalTemplate every frame)
// ---------------------------------------------------------------------------

void UADVSubsystem::OnSectionEntered(EClickWaitMode Mode, FFrameTime Start, FFrameTime End, FFrameRate Rate)
{
	TryAutoRegisterPlayer();

	if (bSectionActive
		&& ActiveMode         == Mode
		&& ActiveSectionStart == Start
		&& ActiveSectionEnd   == End)
	{
		return;
	}

	bSectionActive     = true;
	ActiveMode         = Mode;
	ActiveSectionStart = Start;
	ActiveSectionEnd   = End;
	ActiveDisplayRate  = Rate;
	bAdvanceRequested  = false;
}

// ---------------------------------------------------------------------------
// Input — Advance
// ---------------------------------------------------------------------------

void UADVSubsystem::Advance()
{
	if (!bSectionActive) { return; }
	bAdvanceRequested = true;
}

// ---------------------------------------------------------------------------
// Input — Skip (hold)
// ---------------------------------------------------------------------------

void UADVSubsystem::OnSkipPressed()
{
	if (!ActivePlayer.IsValid() || bFadeInProgress) { return; }
	bSkipHeld        = true;
	SkipHoldElapsed  = 0.0f;
	ShowSkipGauge();
}

void UADVSubsystem::OnSkipReleased()
{
	if (!bSkipHeld) { return; }
	bSkipHeld       = false;
	SkipHoldElapsed = 0.0f;
	HideSkipGauge();
}

void UADVSubsystem::Skip()
{
	// Blueprint-callable direct skip (no hold required)
	DoSkip();
}

void UADVSubsystem::DoSkip()
{
	if (!ActivePlayer.IsValid() || bFadeInProgress) { return; }

	APlayerController* PC = GetGameInstance()
		? GetGameInstance()->GetFirstLocalPlayerController()
		: nullptr;
	if (!PC) { return; }

	bFadeInProgress = true;
	bSectionActive  = false;

	// Fade to black
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(
			0.0f, 1.0f,
			FMath::Max(ConfigFadeDuration, KINDA_SMALL_NUMBER),
			FLinearColor::Black,
			/*bShouldFadeAudio*/ false,
			/*bHoldWhenFinished*/ true);
	}

	// Stop sequencer after fade — fires OnStop → external bindings notified
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World) { return; }

	World->GetTimerManager().SetTimer(
		SkipFadeTimerHandle,
		[this]()
		{
			bFadeInProgress = false;
			if (ULevelSequencePlayer* Player = ActivePlayer.Get())
			{
				Player->Stop(); // OnStop delegate fires here
			}
		},
		FMath::Max(ConfigFadeDuration, KINDA_SMALL_NUMBER),
		/*bLoop*/ false);
}

// ---------------------------------------------------------------------------
// Gauge widget
// ---------------------------------------------------------------------------

void UADVSubsystem::ShowSkipGauge()
{
	if (!GEngine || !GEngine->GameViewport) { return; }
	if (SkipGaugeContainer.IsValid()) { return; }

	TWeakObjectPtr<UADVSubsystem> WeakSelf(this);

	SAssignNew(SkipGaugeContainer, SOverlay)
	+ SOverlay::Slot()
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(0.f, 0.f, 50.f, 50.f))
	[
		SAssignNew(SkipGaugeSlate, SSkipGaugeWidget)
		.Progress_Lambda([WeakSelf]() -> float
		{
			if (!WeakSelf.IsValid()) { return 0.0f; }
			return FMath::Clamp(
				WeakSelf->SkipHoldElapsed / WeakSelf->ConfigHoldDuration,
				0.0f, 1.0f);
		})
		.GaugeColor(ConfigGaugeColor)
		.BackgroundColor(ConfigGaugeBgColor)
		.WidgetSize(ConfigGaugeSize)
	];

	GEngine->GameViewport->AddViewportWidgetContent(
		SkipGaugeContainer.ToSharedRef(), 100);
}

void UADVSubsystem::HideSkipGauge()
{
	if (!SkipGaugeContainer.IsValid()) { return; }

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(
			SkipGaugeContainer.ToSharedRef());
	}
	SkipGaugeContainer.Reset();
	SkipGaugeSlate.Reset();
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UADVSubsystem::Tick(float DeltaTime)
{
	// Handle hold-to-skip progress
	if (bSkipHeld)
	{
		SkipHoldElapsed += DeltaTime;
		if (SkipHoldElapsed >= ConfigHoldDuration)
		{
			bSkipHeld = false;
			HideSkipGauge();
			DoSkip();
		}
		// Gauge updates via TAttribute lambda — no explicit update needed
	}

	if (!bSectionActive) { return; }

	ULevelSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	// Process advance (works even when paused)
	if (bAdvanceRequested)
	{
		bAdvanceRequested = false;
		bSectionActive    = false;
		JumpPastSection();
		return;
	}

	if (!Player->IsPlaying()) { return; }

	// Convert current time to display rate for comparison
	const FQualifiedFrameTime CurrentQFT    = Player->GetCurrentTime();
	const FFrameTime           CurrentInDisp =
		FFrameRate::TransformTime(CurrentQFT.Time, CurrentQFT.Rate, ActiveDisplayRate);

	if (CurrentInDisp.FrameNumber >= ActiveSectionEnd.FrameNumber)
	{
		if (ActiveMode == EClickWaitMode::Loop)
		{
			LoopToStart();
		}
		else // Stop
		{
			Player->Pause();
			// bSectionActive stays true — waiting for Advance()
		}
	}
}

bool UADVSubsystem::IsTickable() const
{
	return bSkipHeld || (bSectionActive && ActivePlayer.IsValid());
}

TStatId UADVSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UADVSubsystem, STATGROUP_Tickables);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UADVSubsystem::JumpPastSection()
{
	ULevelSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = FFrameTime(ActiveSectionEnd.FrameNumber + FFrameNumber(1));
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);
	Player->Play();
}

void UADVSubsystem::LoopToStart()
{
	ULevelSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionStart;
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);
	Player->Play();
}

void UADVSubsystem::OnPlayerStopped()
{
	bSectionActive    = false;
	bAdvanceRequested = false;

	// Cancel gauge and any pending fade timer
	if (bSkipHeld)
	{
		bSkipHeld       = false;
		SkipHoldElapsed = 0.0f;
		HideSkipGauge();
	}

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(SkipFadeTimerHandle);
	}
	bFadeInProgress = false;
}
