// Copyright 2026 kokage. All Rights Reserved.

#include "ADVSubsystem.h"
#include "CinematicADVConfig.h"
#include "CinematicADVSettings.h"
#include "SSkipGaugeWidget.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/SOverlay.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UADVSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UADVSubsystem::HandleWorldCleanup);
}

void UADVSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	HideSkipGauge();
	ClearActivePlayer();
	Super::Deinitialize();
}

void UADVSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// The player may be destroyed without OnStop (level travel)
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player || Player->GetWorld() == World)
	{
		ClearActivePlayer();
	}
}

// ---------------------------------------------------------------------------
// Player registration
// ---------------------------------------------------------------------------

void UADVSubsystem::RegisterSequencePlayer(ULevelSequencePlayer* Player)
{
	SetActivePlayer(Player);
}

void UADVSubsystem::SetActivePlayer(UMovieSceneSequencePlayer* Player)
{
	if (Player == ActivePlayer.Get()) { return; }

	ClearActivePlayer();
	if (!Player) { return; }

	ActivePlayer = Player;
	Player->OnStop.AddUniqueDynamic(this, &UADVSubsystem::OnPlayerStopped);
	Player->OnFinished.AddUniqueDynamic(this, &UADVSubsystem::OnPlayerStopped);

	// Input is active only while an ADV sequence plays
	EnsureInputBound();
	AddInputContext();
}

void UADVSubsystem::ClearActivePlayer()
{
	if (UMovieSceneSequencePlayer* OldPlayer = ActivePlayer.Get())
	{
		OldPlayer->OnStop.RemoveDynamic(this, &UADVSubsystem::OnPlayerStopped);
		OldPlayer->OnFinished.RemoveDynamic(this, &UADVSubsystem::OnPlayerStopped);
	}
	ActivePlayer.Reset();

	// Cancel any in-progress skip hold
	OnSkipReleased();

	bSectionActive    = false;
	bAdvanceRequested = false;
	bPendingPlayTo    = false;
	ActiveSectionKey  = 0;

	RemoveInputContext();
}

// ---------------------------------------------------------------------------
// Config / input
// ---------------------------------------------------------------------------

UCinematicADVConfig* UADVSubsystem::ResolveConfig()
{
	if (Config) { return Config; }

	// 1. Project Settings (this reference is what gets the asset into packaged builds)
	if (const UCinematicADVSettings* Settings = UCinematicADVSettings::Get())
	{
		Config = Settings->ConfigAsset.LoadSynchronous();
	}

	// 2. Fallback: search the Asset Registry.
	//    /Game/ paths take priority over plugin Content paths; multiple /Game/ configs → warn and abort.
	if (!Config)
	{
		if (FAssetRegistryModule* ARModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
		{
			TArray<FAssetData> AllAssets;
			ARModule->Get().GetAssetsByClass(UCinematicADVConfig::StaticClass()->GetClassPathName(), AllAssets);

			TArray<FAssetData> UserAssets;
			TArray<FAssetData> PluginAssets;
			for (const FAssetData& Asset : AllAssets)
			{
				if (Asset.PackagePath.ToString().StartsWith(TEXT("/Game/")))
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
					     "Set one in Project Settings → Plugins → CinematicADV → Config Asset."),
					UserAssets.Num());
			}
			else if (PluginAssets.Num() > 0)
			{
				Config = Cast<UCinematicADVConfig>(PluginAssets[0].GetAsset());
			}
		}
	}

	if (Config)
	{
		ConfigFadeDuration     = Config->FadeOutDuration;
		bConfigFadeInAfterSkip = Config->bFadeInAfterSkip;
		ConfigFadeInDuration   = Config->FadeInDuration;
		ConfigHoldDuration     = FMath::Max(Config->HoldDuration, 0.1f);
		ConfigGaugeSize        = Config->GaugeSize;
		ConfigGaugeColor       = Config->GaugeColor;
		ConfigGaugeBgColor     = Config->GaugeBackgroundColor;
	}
	return Config;
}

APlayerController* UADVSubsystem::GetLocalController() const
{
	return GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
}

void UADVSubsystem::EnsureInputBound()
{
	// Bind once per controller: a new controller (e.g. after level travel) needs its own binding
	APlayerController* PC = GetLocalController();
	if (!PC || PC == BoundController.Get()) { return; }

	UCinematicADVConfig* Cfg = ResolveConfig();
	if (!Cfg) { return; }

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!EIC) { return; }

	if (Cfg->AdvanceAction)
	{
		EIC->BindAction(Cfg->AdvanceAction, ETriggerEvent::Started, this, &UADVSubsystem::Advance);
	}
	if (Cfg->SkipAction)
	{
		EIC->BindAction(Cfg->SkipAction, ETriggerEvent::Started,   this, &UADVSubsystem::OnSkipPressed);
		EIC->BindAction(Cfg->SkipAction, ETriggerEvent::Completed, this, &UADVSubsystem::OnSkipReleased);
		EIC->BindAction(Cfg->SkipAction, ETriggerEvent::Canceled,  this, &UADVSubsystem::OnSkipReleased);
	}
	BoundController = PC;
}

void UADVSubsystem::AddInputContext()
{
	if (bContextAdded) { return; }

	UCinematicADVConfig* Cfg = ResolveConfig();
	APlayerController* PC = GetLocalController();
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	if (!Cfg || !Cfg->InputMappingContext || !LP) { return; }

	if (UEnhancedInputLocalPlayerSubsystem* InputSub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		InputSub->AddMappingContext(Cfg->InputMappingContext, 90);
		ContextLocalPlayer = LP;
		bContextAdded = true;
	}
}

void UADVSubsystem::RemoveInputContext()
{
	if (!bContextAdded) { return; }
	bContextAdded = false;

	// Outside ADV sequences the keys go back to the game
	ULocalPlayer* LP = ContextLocalPlayer.Get();
	if (!LP || !Config || !Config->InputMappingContext) { return; }

	if (UEnhancedInputLocalPlayerSubsystem* InputSub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
	{
		InputSub->RemoveMappingContext(Config->InputMappingContext);
	}
	ContextLocalPlayer.Reset();
}

// ---------------------------------------------------------------------------
// Section evaluated (called by FClickWaitEvalTemplate every frame inside a section)
// ---------------------------------------------------------------------------

void UADVSubsystem::OnSectionEvaluated(UMovieSceneSequencePlayer* Player, uint32 SectionKey, EClickWaitMode Mode,
	double LocalNow, double LocalStart, double LocalEnd)
{
	if (!Player) { return; }

	// The player evaluating the section is the one to control
	if (Player != ActivePlayer.Get())
	{
		SetActivePlayer(Player);
	}
	EnsureInputBound();
	AddInputContext();

	if (bSectionActive && ActiveSectionKey == SectionKey) { return; }

	// Convert the section range to the player's time: offset from the time evaluated right now
	const FQualifiedFrameTime Now = Player->GetCurrentTime();
	const double PlayerNow = Now.AsSeconds();

	ActiveDisplayRate  = Now.Rate;
	ActiveSectionStart = ActiveDisplayRate.AsFrameTime(PlayerNow + (LocalStart - LocalNow));
	ActiveSectionEnd   = ActiveDisplayRate.AsFrameTime(PlayerNow + (LocalEnd - LocalNow));
	ActiveSectionKey   = SectionKey;
	ActiveMode         = Mode;
	bSectionActive     = true;
	bAdvanceRequested  = false;

	// Stop exactly at the section end (set after this evaluation, in Tick)
	bPendingPlayTo = true;
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

	APlayerController* PC = GetLocalController();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!PC || !World) { return; }

	ResolveConfig();
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
	TWeakObjectPtr<UADVSubsystem> WeakThis(this);
	World->GetTimerManager().SetTimer(
		SkipFadeTimerHandle,
		[WeakThis]()
		{
			UADVSubsystem* Self = WeakThis.Get();
			if (!Self) { return; }

			Self->bFadeInProgress = false;
			if (UMovieSceneSequencePlayer* Player = Self->ActivePlayer.Get())
			{
				Player->Stop(); // OnStop delegate fires here
			}

			// Fade back in (the held black would otherwise stay on screen)
			APlayerController* Controller = Self->GetLocalController();
			if (Self->bConfigFadeInAfterSkip && Controller && Controller->PlayerCameraManager)
			{
				if (Self->ConfigFadeInDuration > 0.0f)
				{
					Controller->PlayerCameraManager->StartCameraFade(
						1.0f, 0.0f, Self->ConfigFadeInDuration, FLinearColor::Black,
						/*bShouldFadeAudio*/ false, /*bHoldWhenFinished*/ false);
				}
				else
				{
					Controller->PlayerCameraManager->StopCameraFade();
				}
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

	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player)
	{
		bSectionActive = false;
		return;
	}

	// Process advance (works even when paused)
	if (bAdvanceRequested)
	{
		bAdvanceRequested = false;
		bSectionActive    = false;
		bPendingPlayTo    = false;
		JumpPastSection();
		return;
	}

	// New section: let the player run up to the section end and pause there by itself
	if (bPendingPlayTo)
	{
		bPendingPlayTo = false;
		PlayToSectionEnd();
		return;
	}

	// Paused at the section end: loop, or keep waiting for Advance()
	if (Player->IsPaused() && IsAtSectionEnd(Player))
	{
		if (ActiveMode == EClickWaitMode::Loop)
		{
			LoopToStart();
		}
		// Stop: stay paused — waiting for Advance()
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
// Playback control
// ---------------------------------------------------------------------------

void UADVSubsystem::PlayToSectionEnd()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	// The player pauses exactly at the section end, before any frame after it is evaluated
	// (checking in Tick would let the next line / audio start for a frame first)
	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionEnd;
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Play;

	FMovieSceneSequencePlayToParams PlayToParams;
	PlayToParams.bExclusive = true;

	Player->PlayTo(Params, PlayToParams);
}

bool UADVSubsystem::IsAtSectionEnd(UMovieSceneSequencePlayer* Player) const
{
	const FQualifiedFrameTime Current = Player->GetCurrentTime();
	const FFrameTime CurrentInRate = FFrameRate::TransformTime(Current.Time, Current.Rate, ActiveDisplayRate);

	// Within one frame of the end (an exclusive PlayTo stops just before it)
	return CurrentInRate.AsDecimal() >= ActiveSectionEnd.AsDecimal() - 1.0;
}

void UADVSubsystem::JumpPastSection()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	// Land exactly on the section end: the first frame after the section is not skipped
	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionEnd;
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);
	Player->Play();
}

void UADVSubsystem::LoopToStart()
{
	UMovieSceneSequencePlayer* Player = ActivePlayer.Get();
	if (!Player) { return; }

	FMovieSceneSequencePlaybackParams Params;
	Params.Frame        = ActiveSectionStart;
	Params.PositionType = EMovieScenePositionType::Frame;
	Params.UpdateMethod = EUpdatePositionMethod::Jump;
	Player->SetPlaybackPosition(Params);

	PlayToSectionEnd();
}

void UADVSubsystem::OnPlayerStopped()
{
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

	// Forget the player (a later sequence registers itself) and give the keys back to the game
	ClearActivePlayer();
}
