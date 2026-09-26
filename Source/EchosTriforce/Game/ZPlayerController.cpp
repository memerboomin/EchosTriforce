#include "ZPlayerController.h"
#include "ZGameInstance.h"
#include "ZGameData.h"
#include "ZBattleDirector.h"
#include "ZBattleHUD.h"
#include "ZExploreHUD.h"
#include "ZMenuWidget.h"
#include "ZUI.h"
#include "ZExploreCharacter.h"
#include "ZCistern.h"
#include "ZEnvironment.h"
#include "ZProgression.h"
#include "ZVisuals.h"
#include "ZBattlePawn.h"
#include "EchosTriforce.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "GameFramework/PlayerStart.h"
#include "UnrealClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Framework/Application/SlateApplication.h"

AZGameMode::AZGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AZPlayerController::StaticClass();
}

void AZGameMode::StartPlay()
{
	UWorld* W = GetWorld();
	if (!TActorIterator<AZEnvironment>(W)) W->SpawnActor<AZEnvironment>(AZEnvironment::StaticClass(), FTransform::Identity);
	// Carte de décor de Twilight Princess (Tools/ue/import_tp.py, acteurs « ZTPStage ») : pas de Citerne
	bool bTPStage = false;
	for (TActorIterator<AActor> It(W); It && !bTPStage; ++It) bTPStage = It->ActorHasTag(TEXT("ZTPStage"));
	if (!bTPStage && !TActorIterator<AZCistern>(W)) W->SpawnActor<AZCistern>(AZCistern::StaticClass(), FTransform::Identity);
	Super::StartPlay();
}

// ---------------------------------------------------------------------------------------------------------------------

AZPlayerController::AZPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
}

void AZPlayerController::CreateInput()
{
	if (IMC) return;
	IMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Echos"));
	IA_Move = NewObject<UInputAction>(this, TEXT("IA_Move"));
	IA_Move->ValueType = EInputActionValueType::Axis2D;
	IA_Look = NewObject<UInputAction>(this, TEXT("IA_Look"));
	IA_Look->ValueType = EInputActionValueType::Axis2D;
	IA_Interact = NewObject<UInputAction>(this, TEXT("IA_Interact"));
	IA_Strike = NewObject<UInputAction>(this, TEXT("IA_Strike"));
	IA_Jump = NewObject<UInputAction>(this, TEXT("IA_Jump"));
	IA_Menu = NewObject<UInputAction>(this, TEXT("IA_Menu"));

	auto MapAxis = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(IA_Move, Key);
		if (bSwizzle) M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		if (bNegate) M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	};
	// ZQSD (AZERTY) et WASD (QWERTY) et flèches
	MapAxis(EKeys::Z, true, false); MapAxis(EKeys::W, true, false); MapAxis(EKeys::Up, true, false);
	MapAxis(EKeys::S, true, true); MapAxis(EKeys::Down, true, true);
	MapAxis(EKeys::D, false, false); MapAxis(EKeys::Right, false, false);
	MapAxis(EKeys::Q, false, true); MapAxis(EKeys::A, false, true); MapAxis(EKeys::Left, false, true);
	IMC->MapKey(IA_Move, EKeys::Gamepad_Left2D);

	FEnhancedActionKeyMapping& Mouse = IMC->MapKey(IA_Look, EKeys::Mouse2D);
	UInputModifierNegate* NegY = NewObject<UInputModifierNegate>(this);
	NegY->bX = false; NegY->bY = true; NegY->bZ = false;
	Mouse.Modifiers.Add(NegY);
	FEnhancedActionKeyMapping& Pad = IMC->MapKey(IA_Look, EKeys::Gamepad_Right2D);
	UInputModifierNegate* NegY2 = NewObject<UInputModifierNegate>(this);
	NegY2->bX = false; NegY2->bY = true; NegY2->bZ = false;
	Pad.Modifiers.Add(NegY2);

	for (const FKey& K : { EKeys::E, EKeys::Enter }) IMC->MapKey(IA_Interact, K);
	for (const FKey& K : { EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom }) IMC->MapKey(IA_Jump, K);
	for (const FKey& K : { EKeys::F, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Left }) IMC->MapKey(IA_Strike, K);
	for (const FKey& K : { EKeys::Tab, EKeys::M, EKeys::Escape, EKeys::Gamepad_Special_Right }) IMC->MapKey(IA_Menu, K);
}

void AZPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateInput();
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AZPlayerController::OnMove);
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AZPlayerController::OnLook);
		EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AZPlayerController::OnInteract);
		EIC->BindAction(IA_Strike, ETriggerEvent::Started, this, &AZPlayerController::OnStrike);
		EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AZPlayerController::OnJump);
		EIC->BindAction(IA_Menu, ETriggerEvent::Started, this, &AZPlayerController::OnMenu);
	}
}

void AZPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		CreateInput();
		Sub->AddMappingContext(IMC, 0);
	}
	TActorIterator<AZCistern> CisternIt(GetWorld());
	if (CisternIt) Cistern = *CisternIt;
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -50.f;
		PlayerCameraManager->ViewPitchMax = 6.f;
	}

	// Automatisation (vérification par captures d'écran)
	FString Shots;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZShots="), Shots, false))
	{
		TArray<FString> Parts;
		Shots.ParseIntoArray(Parts, TEXT(","));
		for (const FString& P : Parts) ShotTimes.Add(FCString::Atof(*P));
	}
	FParse::Value(FCommandLine::Get(), TEXT("ZQuitAt="), QuitAt);
	FParse::Value(FCommandLine::Get(), TEXT("ZShotName="), ShotName);
	if (ShotName.IsEmpty()) ShotName = TEXT("echos");
	bAutoPlay = FParse::Param(FCommandLine::Get(), TEXT("ZAutoPlay"));
	FParse::Value(FCommandLine::Get(), TEXT("ZAuto="), AutoScript);
	FString Keys;
	if (FParse::Value(FCommandLine::Get(), TEXT("ZKeys="), Keys, false)) Keys.ParseIntoArray(AutoKeys, TEXT(","));

	ShowTitle();
	if (AutoScript == TEXT("explore")) StartNewGame();
	else if (AutoScript == TEXT("nereide")) StartArena(TEXT("ENC_NEREIDE"), { TEXT("nereide") }, TEXT("R10"), FParse::Param(FCommandLine::Get(), TEXT("ZArchivist")));
	else if (AutoScript == TEXT("sentinel")) StartArena(TEXT("ENC_ECLUSE"), { TEXT("sentinel_elec") }, TEXT("R07"), FParse::Param(FCommandLine::Get(), TEXT("ZArchivist")));
	else if (AutoScript == TEXT("chuchu")) StartArena(TEXT("ENC_BASSIN"), { TEXT("chuchu_water"), TEXT("chuchu_water") }, TEXT("R02"), false);
	else if (AutoScript == TEXT("booth")) StartBooth();
	else if (AutoScript.StartsWith(TEXT("menu")))
	{
		StartNewGame();
		if (FParse::Param(FCommandLine::Get(), TEXT("ZArchivist"))) if (UZGameInstance* GI = UZGameInstance::Get(this)) GI->UnlockEverything();
		OpenMenu(FCString::Atoi(*AutoScript.Mid(4)));
	}
}

void AZPlayerController::StartBooth()
{
	// Studio photo : personnages et créatures alignés face caméra (réglage des accessoires)
	ClearWidgets();
	Mode = EMode::Battle;
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (GI) { GI->NewGame(); GI->RecruitMember(TEXT("mipha")); }
	FString Row;
	FParse::Value(FCommandLine::Get(), TEXT("ZBoothRow="), Row);
	const bool bCreatures = Row == TEXT("creatures");
	const FVector Base = bCreatures ? FVector(14300, -700, 0) : FVector(-300, -700, 0);
	const bool bClose = FParse::Param(FCommandLine::Get(), TEXT("ZBoothClose"));
	TArray<FName> Ids = bCreatures ? TArray<FName>{ TEXT("octorok"), TEXT("chuchu_water"), TEXT("sentinel_elec"), TEXT("nereide") } : TArray<FName>{ TEXT("link"), TEXT("sheik"), TEXT("mipha") };
	float Y = 0.f;
	for (int32 i = 0; i < Ids.Num(); ++i)
	{
		AZBattlePawn* P = GetWorld()->SpawnActor<AZBattlePawn>(AZBattlePawn::StaticClass(), FTransform::Identity);
		FZBattler B;
		B.DefId = Ids[i];
		if (bCreatures)
		{
			const FZEnemyDef* D = UZGameData::Get().FindEnemy(Ids[i]);
			B.Mesh = D->Mesh; B.Color = D->Color; B.Scale = D->Scale;
			P->InitEnemy(B);
			Y += i == 0 ? 0.f : (Ids[i] == FName(TEXT("nereide")) ? 420.f : 230.f);
		}
		else
		{
			const FZCharacterState* S = GI ? GI->FindMember(Ids[i]) : nullptr;
			P->InitAlly(B, S ? S->GetEquip(EZEquipSlot::Torso) : FString(), S ? S->GetEquip(EZEquipSlot::Weapon) : FString(), S ? S->GetEquip(EZEquipSlot::Offhand) : FString());
			Y += i == 0 ? 0.f : 150.f;
		}
		float BoothYaw = 0.f;
		FParse::Value(FCommandLine::Get(), TEXT("ZBoothYaw="), BoothYaw);
		P->SetHome(Base + FVector(0, Y, 0), FRotator(0, 180.f + BoothYaw, 0));
		FString BoothAnim;
		if (!bCreatures && FParse::Value(FCommandLine::Get(), TEXT("ZBoothAnim="), BoothAnim)) P->PreviewAnim(BoothAnim);
	}
	if (!TitleCam) TitleCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
	const float Dist = bCreatures ? 1250.f : (bClose ? 260.f : 520.f);
	int32 Focus = -1;
	FParse::Value(FCommandLine::Get(), TEXT("ZBoothFocus="), Focus);
	const float FocusY = Focus >= 0 ? Focus * 150.f : Y * 0.5f;
	const FVector Look = Base + FVector(0, FocusY, bCreatures ? 170.f : (bClose ? 150.f : 110.f));
	TitleCam->SetActorLocation(Look + FVector(-Dist, bClose ? -60.f : 0.f, bClose ? 10.f : 40.f));
	TitleCam->SetActorRotation((Look - TitleCam->GetActorLocation()).Rotation());
	TitleCam->GetCameraComponent()->SetFieldOfView(bClose ? 30.f : 50.f);
	SetViewTarget(TitleCam);
}

void AZPlayerController::ClearWidgets()
{
	if (Title) { Title->RemoveFromParent(); Title = nullptr; }
	if (BattleHUD) { BattleHUD->RemoveFromParent(); BattleHUD = nullptr; }
	if (Menu) { Menu->RemoveFromParent(); Menu = nullptr; }
}

void AZPlayerController::SetUIFocus(UUserWidget* W)
{
	FInputModeUIOnly M;
	if (W) M.SetWidgetToFocus(W->TakeWidget());
	M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(M);
	bShowMouseCursor = true;
	if (W) W->SetKeyboardFocus();
}

void AZPlayerController::SetGameFocus()
{
	FInputModeGameOnly M;
	SetInputMode(M);
	bShowMouseCursor = false;
}

void AZPlayerController::ShowTitle()
{
	ClearWidgets();
	if (ExploreHUD) { ExploreHUD->RemoveFromParent(); ExploreHUD = nullptr; }
	if (Hero) { Hero->Destroy(); Hero = nullptr; }
	for (AZFollower* F : Followers) { if (F) F->Destroy(); }
	Followers.Reset();
	Mode = EMode::Title;
	if (!TitleCam)
	{
		TitleCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		TitleCam->GetCameraComponent()->SetFieldOfView(62.f);
	}
	{
		auto CmdVec = [](const TCHAR* Key, const FVector& Def)
		{
			FString V;
			TArray<FString> P;
			if (!FParse::Value(FCommandLine::Get(), Key, V, false)) return Def;
			V.ParseIntoArray(P, TEXT(","));
			return P.Num() == 3 ? FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])) : Def;
		};
		TitleFrom = CmdVec(TEXT("ZTitleCam="), FVector(13050, 0, 420));
		TitleTo = CmdVec(TEXT("ZTitleLook="), FVector(15500, 0, 330));
		TitleCam->SetActorLocation(TitleFrom);
		TitleCam->SetActorRotation((TitleTo - TitleFrom).Rotation());
	}
	SetViewTarget(TitleCam);

	UZGameInstance* GI = UZGameInstance::Get(this);
	Title = CreateWidget<UZTitleWidget>(this, UZTitleWidget::StaticClass());
	Title->Options = {
		TEXT("Nouvelle partie — Citerne des mémoires"),
		GI && GI->HasSave() ? TEXT("Continuer") : TEXT("Continuer (aucune sauvegarde)"),
		TEXT("Combat rapide : Gardien Néréide"),
		TEXT("Combat rapide : Sentinelle électrique"),
		TEXT("Combat rapide : Chuchus aqueux"),
		TEXT("Mode Archiviste : tout débloqué + Néréide"),
		TEXT("Quitter"),
	};
	Title->OnChoose = [this](int32 I)
	{
		switch (I)
		{
		case 0: StartNewGame(); break;
		case 1: ContinueGame(); break;
		case 2: StartArena(TEXT("ENC_NEREIDE"), { TEXT("nereide") }, TEXT("R10"), false); break;
		case 3: StartArena(TEXT("ENC_ECLUSE"), { TEXT("sentinel_elec") }, TEXT("R07"), false); break;
		case 4: StartArena(TEXT("ENC_BASSIN"), { TEXT("chuchu_water"), TEXT("chuchu_water") }, TEXT("R02"), false); break;
		case 5: StartArena(TEXT("ENC_NEREIDE"), { TEXT("nereide") }, TEXT("R10"), true); break;
		default: UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); break;
		}
	};
	Title->AddToViewport(10);
	SetUIFocus(Title);
}

void AZPlayerController::StartNewGame()
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (GI)
	{
		GI->NewGame();
		// Prototype : Mipha rejoint en salle 05 (chapitre 17)
		GI->ActiveParty.Remove(TEXT("mipha"));
		GI->Party.RemoveAll([](const FZCharacterState& S) { return S.Id == FName(TEXT("mipha")); });
		GI->Checkpoint = TEXT("R01");
	}
	Visited.Init(false, AZCistern::Rooms().Num());
	if (Cistern)
	{
		for (AZEncounterActor* E : Cistern->Encounters) { if (E) E->Destroy(); }
		for (AZInteractable* I : Cistern->Interactables) { if (I) I->Destroy(); }
		Cistern->Encounters.Reset();
		Cistern->Interactables.Reset();
		Cistern->SpawnGameplay();
	}
	EnterExplore(true);
	if (ExploreHUD && Cistern)
	{
		ExploreHUD->ShowDialog(TEXT("Sheik"), TEXT("« Cette citerne est une mémoire d'Hyrule qui se fige. Trois sources scellent la voie du gardien. Restons ensemble, Link : les Zoras ont laissé une tenue dans l'atelier, à l'ouest du bassin. »"));
	}
}

void AZPlayerController::ContinueGame()
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (!GI || !GI->LoadFromSlot()) return;
	Visited.Init(false, AZCistern::Rooms().Num());
	if (Cistern)
	{
		for (AZEncounterActor* E : Cistern->Encounters) { if (E) E->Destroy(); }
		for (AZInteractable* I : Cistern->Interactables) { if (I) I->Destroy(); }
		Cistern->Encounters.Reset();
		Cistern->Interactables.Reset();
		Cistern->SpawnGameplay();
	}
	EnterExplore(true);
}

void AZPlayerController::SpawnParty(const FVector& At, float Yaw)
{
	UWorld* W = GetWorld();
	if (!Hero)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Hero = W->SpawnActor<AZExploreCharacter>(AZExploreCharacter::StaticClass(), FTransform(FRotator(0, Yaw, 0), At), P);
	}
	else
	{
		Hero->SetActorLocationAndRotation(At, FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
		Hero->Trail.Reset();
	}
	Possess(Hero);
	SetControlRotation(FRotator(-5.f, Yaw, 0));
	ExploreTime = 0.f;
	RefreshPartyLooks();
}

void AZPlayerController::RefreshPartyLooks()
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (!GI || !Hero) return;
	Hero->RefreshLook();
	TArray<FName> Companions;
	for (FName Id : GI->ActiveParty) { if (Id != FName(TEXT("link"))) Companions.Add(Id); }
	while (Followers.Num() > Companions.Num()) { if (Followers.Last()) Followers.Last()->Destroy(); Followers.Pop(); }
	for (int32 i = 0; i < Companions.Num(); ++i)
	{
		if (!Followers.IsValidIndex(i))
		{
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FVector Side = Hero->GetActorRightVector() * (i == 0 ? -150.f : 150.f);
			AZFollower* F = GetWorld()->SpawnActor<AZFollower>(AZFollower::StaticClass(), FTransform(Hero->GetActorRotation(), Hero->GetActorLocation() - Hero->GetActorForwardVector() * 110.f + Side), P);
			Followers.Add(F);
		}
		AZFollower* F = Followers[i];
		F->Leader = Hero;
		F->SlotIndex = i + 1;
		const FZCharacterState* S = GI->FindMember(Companions[i]);
		F->ApplyLook(Companions[i], S ? S->GetEquip(EZEquipSlot::Torso) : FString(), S ? S->GetEquip(EZEquipSlot::Weapon) : FString(), S ? S->GetEquip(EZEquipSlot::Offhand) : FString(), true);
	}
}

void AZPlayerController::EnterExplore(bool bAtCheckpoint)
{
	ClearWidgets();
	bArena = false;
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (!Hero || bAtCheckpoint)
	{
		FName Room = GI && !GI->Checkpoint.IsEmpty() ? FName(*GI->Checkpoint) : FName(TEXT("R01"));
		// Captures : -ZRoom=R05 -ZYaw=90 place l'équipe dans une salle donnée
		FString RoomArg;
		float Yaw = 0.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZRoom="), RoomArg)) Room = FName(*RoomArg);
		FParse::Value(FCommandLine::Get(), TEXT("ZYaw="), Yaw);
		APlayerStart* Start = nullptr;
		if (!Cistern)
		{
			TActorIterator<APlayerStart> It(GetWorld());
			if (It) Start = *It;
		}
		FString TPStart; // captures dans un décor de TP : -ZTPStart=x,y,z,lacet
		TArray<FString> TPV;
		if (!Cistern && FParse::Value(FCommandLine::Get(), TEXT("ZTPStart="), TPStart, false) && TPStart.ParseIntoArray(TPV, TEXT(",")) == 4)
		{
			SpawnParty(FVector(FCString::Atof(*TPV[0]), FCString::Atof(*TPV[1]), FCString::Atof(*TPV[2])), FCString::Atof(*TPV[3]));
		}
		else if (Start) SpawnParty(Start->GetActorLocation(), Start->GetActorRotation().Yaw); // décor de TP : départ de la carte
		else SpawnParty(AZCistern::RoomSpawn(Room == FName(TEXT("Vestibule")) ? FName(TEXT("R01")) : Room), Yaw);
	}
	if (Hero)
	{
		Hero->SetActorHiddenInGame(false);
		Hero->SetActorEnableCollision(true);
		for (AZFollower* F : Followers) { if (F) { F->SetActorHiddenInGame(false); F->SetActorEnableCollision(true); } }
		SetViewTargetWithBlend(Hero, 0.6f, VTBlend_EaseInOut, 2.f);
	}
	if (!ExploreHUD)
	{
		ExploreHUD = CreateWidget<UZExploreHUD>(this, UZExploreHUD::StaticClass());
		ExploreHUD->AddToViewport(1);
		TArray<FBox2D> Boxes;
		for (const FZRoomDef& R : AZCistern::Rooms()) Boxes.Add(FBox2D(R.Center - R.Half, R.Center + R.Half));
		ExploreHUD->Minimap->Rooms = Boxes;
	}
	ExploreHUD->SetVisibility(ESlateVisibility::HitTestInvisible);
	CurrentRoom = INDEX_NONE;
	if (Visited.Num() != AZCistern::Rooms().Num()) Visited.Init(false, AZCistern::Rooms().Num());
	Mode = EMode::Explore;
	SetGameFocus();
	if (Cistern) Cistern->RefreshDoors();
}

void AZPlayerController::StartArena(FName EncounterId, const TArray<FName>& Enemies, FName Room, bool bArchivist)
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (GI)
	{
		GI->NewGame();
		GI->GrantItem(TEXT("OOT_010"));
		GI->GrantItem(TEXT("MM_022"));
		if (FZCharacterState* L = GI->FindMember(TEXT("link")))
		{
			FString Why;
			ZProgression::Equip(*L, EZEquipSlot::Torso, TEXT("OOT_010"), Why);
		}
		if (bArchivist)
		{
			GI->UnlockEverything();
			GI->Resonance = 60;
		}
		int32 R = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("ZStartR="), R)) GI->Resonance = R;
	}
	bArena = true;
	for (const FZRoomDef& Rd : AZCistern::Rooms())
	{
		if (Rd.Id != Room) continue;
		StartBattle(EncounterId, Enemies, FVector(Rd.Center.X, Rd.Center.Y, 0), false);
		return;
	}
}

void AZPlayerController::StartBattle(FName EncounterId, const TArray<FName>& Enemies, const FVector& Near, bool bPreemptive)
{
	ClearWidgets();
	if (ExploreHUD) ExploreHUD->SetVisibility(ESlateVisibility::Collapsed);
	if (Hero)
	{
		Hero->SetActorHiddenInGame(true);
		Hero->SetActorEnableCollision(false);
		Hero->GetCharacterMovement()->StopMovementImmediately();
	}
	for (AZFollower* F : Followers) { if (F) { F->SetActorHiddenInGame(true); F->SetActorEnableCollision(false); } }
	for (AZEncounterActor* E : Cistern ? Cistern->Encounters : TArray<AZEncounterActor*>()) { if (E && E->EncounterId == EncounterId) E->SetActorHiddenInGame(true); }

	FRotator Facing;
	const FVector Center = AZCistern::BattleCenter(Near, Facing);
	if (!Director)
	{
		Director = GetWorld()->SpawnActor<AZBattleDirector>(AZBattleDirector::StaticClass(), FTransform(Center));
		Director->OnBattleEnded.AddUObject(this, &AZPlayerController::HandleBattleEnded);
	}
	ActiveEncounter = EncounterId;
	Director->BeginBattle(EncounterId, Enemies, Center, Facing, bPreemptive);
	BattleHUD = CreateWidget<UZBattleHUD>(this, UZBattleHUD::StaticClass());
	BattleHUD->Director = Director;
	Director->HUD = BattleHUD;
	BattleHUD->AddToViewport(5);
	Mode = EMode::Battle;
	SetUIFocus(BattleHUD);
}

void AZPlayerController::HandleBattleEnded(EZSimState Outcome, FName EncounterId)
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (BattleHUD) { BattleHUD->RemoveFromParent(); BattleHUD = nullptr; }
	if (bArena)
	{
		ShowTitle();
		return;
	}
	AZEncounterActor* Enc = nullptr;
	if (Cistern)
	{
		for (AZEncounterActor* E : Cistern->Encounters) { if (E && E->EncounterId == EncounterId) Enc = E; }
	}
	if (Outcome == EZSimState::Victory)
	{
		if (GI) GI->SetFlag(FName(*(TEXT("done.") + EncounterId.ToString())));
		if (Enc) { Cistern->Encounters.Remove(Enc); Enc->Destroy(); }
		EnterExplore(false);
		if (EncounterId == FName(TEXT("ENC_NEREIDE")) && ExploreHUD)
		{
			if (GI && GI->ClaimReward(TEXT("seal.water"))) GI->SetFlag(TEXT("seal.water"));
			ExploreHUD->ShowDialog(TEXT("Sceau aquatique"), TEXT("Le Gardien Néréide s'apaise. La première source est libérée : la mémoire de la citerne peut à nouveau couler. Fin de la démonstration « Citerne des mémoires » — merci d'avoir joué ! Tu peux continuer à explorer, tester l'équipement (Tab) ou relancer un combat rapide depuis l'écran titre."));
		}
		RefreshPartyLooks();
	}
	else if (Outcome == EZSimState::Fled)
	{
		if (Enc) { Enc->SetActorHiddenInGame(false); Enc->Cooldown = 4.f; }
		EnterExplore(false);
		if (Hero && Enc) Hero->SetActorLocation(Hero->GetActorLocation() - (Enc->GetActorLocation() - Hero->GetActorLocation()).GetSafeNormal2D() * 300.f);
	}
	else
	{
		// Défaite → point de contrôle : aucune perte d'XP ni d'objet permanent
		if (GI) GI->RestoreParty();
		if (Enc) { Enc->SetActorHiddenInGame(false); Enc->Cooldown = 4.f; }
		EnterExplore(true);
		if (ExploreHUD) ExploreHUD->Toast(TEXT("Retour au point de contrôle — change d'équipement avec Tab"));
	}
}

void AZPlayerController::OpenMenu(int32 Tab)
{
	if (Mode != EMode::Explore && Mode != EMode::Title) return;
	Menu = CreateWidget<UZMenuWidget>(this, UZMenuWidget::StaticClass());
	Menu->OnClose = [this]() { CloseMenu(); };
	Menu->OnEquipmentChanged = [this]() { RefreshPartyLooks(); };
	Menu->OnReturnToTitle = [this]() { CloseMenu(); ShowTitle(); };
	Menu->OpenAt(Tab);
	Menu->AddToViewport(20);
	if (ExploreHUD) ExploreHUD->SetVisibility(ESlateVisibility::Collapsed);
	Mode = EMode::Menu;
	SetUIFocus(Menu);
}

void AZPlayerController::CloseMenu()
{
	if (Menu) { Menu->RemoveFromParent(); Menu = nullptr; }
	if (Mode == EMode::Menu)
	{
		Mode = EMode::Explore;
		if (ExploreHUD) ExploreHUD->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetGameFocus();
		RefreshPartyLooks();
	}
}

void AZPlayerController::OnMove(const FInputActionValue& V)
{
	if (Mode != EMode::Explore || !Hero || (ExploreHUD && ExploreHUD->IsDialogOpen())) return;
	const FVector2D In = V.Get<FVector2D>();
	const FRotator Yaw(0, GetControlRotation().Yaw, 0);
	Hero->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), In.Y);
	Hero->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), In.X);
}

void AZPlayerController::OnLook(const FInputActionValue& V)
{
	if (Mode != EMode::Explore || ExploreTime < 1.2f) return; // ignore le saut de souris initial
	if (!AutoScript.IsEmpty()) return; // captures automatiques : la souris réelle ne doit pas bouger la caméra
	const FVector2D Raw = V.Get<FVector2D>();
	if (FMath::Abs(Raw.X) > 60.f || FMath::Abs(Raw.Y) > 60.f) return; // pic de repositionnement du curseur
	const FVector2D In = V.Get<FVector2D>();
	AddYawInput(In.X);
	AddPitchInput(In.Y);
}

void AZPlayerController::OnInteract()
{
	if (Mode != EMode::Explore) return;
	if (ExploreHUD && ExploreHUD->IsDialogOpen()) { ExploreHUD->CloseDialog(); return; }
	if (Focused) DoInteract(Focused);
}

void AZPlayerController::OnJump()
{
	if (Mode != EMode::Explore || !Hero) return;
	if ((ExploreHUD && ExploreHUD->IsDialogOpen()) || Focused) { OnInteract(); return; }
	Hero->Jump();
}

void AZPlayerController::OnStrike()
{
	if (Mode != EMode::Explore || !Hero) return;
	Hero->Swing();
	// Frapper le symbole ennemi donne +200 de jauge initiale à l'équipe
	if (!Cistern) return;
	for (AZEncounterActor* E : Cistern->Encounters)
	{
		if (!E || E->IsHidden() || E->Cooldown > 0.f) continue;
		const FVector To = E->GetActorLocation() - Hero->GetActorLocation();
		if (To.Size2D() < 300.f && FVector::DotProduct(To.GetSafeNormal2D(), Hero->GetActorForwardVector()) > 0.3f)
		{
			StartBattle(E->EncounterId, E->Enemies, E->GetActorLocation(), true);
			return;
		}
	}
}

void AZPlayerController::OnMenu()
{
	if (Mode == EMode::Explore) OpenMenu(1);
}

void AZPlayerController::DoInteract(AZInteractable* I)
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (!GI || !ExploreHUD) return;
	const UZGameData& DB = UZGameData::Get();
	auto ItemName = [&DB](const FString& Id) { const FZItemDef* It = DB.FindItem(Id); return It ? It->DisplayName() : Id; };
	switch (I->Kind)
	{
	case EZInteractKind::Chest:
		if (I->bUsed) return;
		if (GI->ClaimReward(I->RewardId))
		{
			if (DB.FindConsumable(FName(*I->ItemId)))
			{
				GI->Consumables.FindOrAdd(FName(*I->ItemId)) += 3;
				ExploreHUD->Toast(FString::Printf(TEXT("Obtenu : %s ×3"), *DB.FindConsumable(FName(*I->ItemId))->Name));
			}
			else
			{
				GI->GrantItem(I->ItemId);
				ExploreHUD->Toast(FString::Printf(TEXT("Obtenu : %s"), *ItemName(I->ItemId)));
			}
		}
		break;
	case EZInteractKind::Atelier:
		ExploreHUD->ShowDialog(I->Label, I->Text);
		if (GI->ClaimReward(I->RewardId))
		{
			GI->GrantItem(I->ItemId);
			GI->SetFlag(I->RewardId);
			ExploreHUD->Toast(FString::Printf(TEXT("Obtenu : %s (Eau -50 %%)"), *ItemName(I->ItemId)));
		}
		break;
	case EZInteractKind::Valve:
		if (I->bUsed) return;
		if (!GI->Owns(I->ItemId)) { ExploreHUD->ShowDialog(I->Label, TEXT("Il faudrait un outil pour atteindre cette vanne.")); return; }
		GI->SetFlag(I->RewardId);
		ExploreHUD->ShowDialog(I->Label, I->Text + TEXT(" (L'outil fonctionne en exploration même s'il n'est pas assigné au combat.)"));
		break;
	case EZInteractKind::Companion:
		if (!GI->FindMember(TEXT("mipha")))
		{
			GI->RecruitMember(TEXT("mipha"));
			if (FZCharacterState* M = GI->FindMember(TEXT("mipha")))
			{
				FString Why;
				ZProgression::Equip(*M, EZEquipSlot::Weapon, TEXT("BOTW_093"), Why);
				M->Spirit = TEXT("Nayru");
				ZProgression::AutoPrepare(*M);
			}
		}
		GI->SetFlag(I->RewardId);
		GI->ClaimReward(I->RewardId);
		ExploreHUD->ShowDialog(I->Label, I->Text);
		RefreshPartyLooks();
		break;
	case EZInteractKind::Fountain:
		GI->RestoreParty();
		GI->Checkpoint = AZCistern::Rooms().IsValidIndex(CurrentRoom) ? AZCistern::Rooms()[CurrentRoom].Id.ToString() : GI->Checkpoint;
		if (!I->ItemId.IsEmpty() && GI->ClaimReward(I->RewardId))
		{
			GI->GrantItem(I->ItemId);
			ExploreHUD->Toast(FString::Printf(TEXT("Obtenu : %s — nouvelle forme !"), *ItemName(I->ItemId)));
		}
		GI->SaveToSlot();
		ExploreHUD->ShowDialog(I->Label, I->Text + TEXT(" (PV et PM restaurés, partie sauvegardée.)"));
		break;
	case EZInteractKind::SavePoint:
		GI->Checkpoint = AZCistern::Rooms().IsValidIndex(CurrentRoom) ? AZCistern::Rooms()[CurrentRoom].Id.ToString() : GI->Checkpoint;
		ExploreHUD->Toast(GI->SaveToSlot() ? TEXT("Partie sauvegardée") : TEXT("Échec de la sauvegarde"));
		break;
	case EZInteractKind::BossDoor:
		if (I->bUsed) return;
		GI->SetFlag(I->RewardId);
		ExploreHUD->ShowDialog(I->Label, I->Text);
		break;
	case EZInteractKind::Puzzle:
	{
		if (GI->HasFlag(TEXT("puzzle.r06"))) { ExploreHUD->Toast(TEXT("Les symboles brillent déjà")); return; }
		PuzzleProgress.Add(I->PuzzleIndex);
		ZVis::Tint(I->MeshB, FLinearColor(0.25f, 1.f, 0.95f), 6.f);
		bool bOk = true;
		for (int32 k = 0; k < PuzzleProgress.Num(); ++k) bOk &= PuzzleProgress[k] == k;
		if (!bOk)
		{
			PuzzleProgress.Reset();
			for (AZInteractable* P : Cistern->Interactables) { if (P && P->Kind == EZInteractKind::Puzzle) ZVis::Tint(P->MeshB, FLinearColor(0.2f, 0.4f, 0.45f), 0.5f); }
			ExploreHUD->Toast(TEXT("Les symboles s'éteignent… mauvais ordre"));
		}
		else if (PuzzleProgress.Num() == 3)
		{
			GI->SetFlag(TEXT("puzzle.r06"));
			ExploreHUD->Toast(TEXT("La grille des archives se lève !"));
		}
		else ExploreHUD->Toast(FString::Printf(TEXT("%s s'illumine"), *I->Label));
		break;
	}
	default:
		ExploreHUD->ShowDialog(I->Label, I->Text);
		break;
	}
	if (Cistern) Cistern->RefreshDoors();
}

void AZPlayerController::UpdateExplore(float Dt)
{
	if (!Hero || !ExploreHUD) return;
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (GI) GI->PlayTime += Dt;
	const FVector P = Hero->GetActorLocation();

	// Salle courante, objectif, mini-carte
	const int32 Room = AZCistern::RoomAt(P);
	if (Room != INDEX_NONE && Room != CurrentRoom)
	{
		CurrentRoom = Room;
		const FZRoomDef& R = AZCistern::Rooms()[Room];
		ExploreHUD->SetRoom(FString::Printf(TEXT("%02d · %s"), R.Number, *R.Name), R.Objective);
		if (Visited.IsValidIndex(Room)) Visited[Room] = true;
	}
	ExploreHUD->Minimap->Player = FVector2D(P.X, P.Y);
	ExploreHUD->Minimap->PlayerYaw = Hero->GetActorRotation().Yaw;
	ExploreHUD->Minimap->Visited = Visited;

	// Interaction la plus proche
	Focused = nullptr;
	float Best = TNumericLimits<float>::Max();
	if (Cistern)
	{
		for (AZInteractable* I : Cistern->Interactables)
		{
			if (!I || I->IsHidden()) continue;
			const float D = FVector::Dist2D(I->GetActorLocation(), P);
			if (D < I->Zone->GetScaledSphereRadius() + 60.f && D < Best && !I->PromptText().IsEmpty()) { Best = D; Focused = I; }
		}
	}
	ExploreHUD->SetPrompt(Focused ? Focused->PromptText() : FString());

	// Contact avec un ennemi visible
	if (Cistern && !ExploreHUD->IsDialogOpen())
	{
		for (AZEncounterActor* E : Cistern->Encounters)
		{
			if (!E || E->IsHidden() || E->Cooldown > 0.f) continue;
			if (FVector::Dist2D(E->GetActorLocation(), P) < E->Zone->GetScaledSphereRadius())
			{
				StartBattle(E->EncounterId, E->Enemies, E->GetActorLocation(), false);
				return;
			}
		}
	}
}

void AZPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	if (Mode == EMode::Explore) { ExploreTime += Dt; UpdateExplore(Dt); }
	// Test automatique : -ZWalk=secondes fait avancer Link tout droit (vérifie les bords d'eau), -ZJumpAt=secondes le fait sauter
	float WalkFor = 0.f, JumpAt = -1.f;
	if (Mode == EMode::Explore && Hero && FParse::Value(FCommandLine::Get(), TEXT("ZWalk="), WalkFor) && ExploreTime > 1.5f && ExploreTime < 1.5f + WalkFor)
	{
		Hero->AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), 1.f);
	}
	if (Mode == EMode::Explore && Hero && FParse::Value(FCommandLine::Get(), TEXT("ZJumpAt="), JumpAt) && ExploreTime >= JumpAt && ExploreTime - Dt < JumpAt) Hero->Jump();
	else ExploreTime = 0.f;
	if (Mode == EMode::Title && TitleCam)
	{
		TitleT += Dt;
		// lente dérive autour du point de vue choisi
		const FVector Pos = TitleFrom + FVector(FMath::Sin(TitleT * 0.08f) * 120.f, FMath::Sin(TitleT * 0.05f) * 160.f, FMath::Sin(TitleT * 0.11f) * 25.f);
		TitleCam->SetActorLocation(Pos);
		TitleCam->SetActorRotation((TitleTo - Pos).Rotation());
	}
	RunAutomation(Dt);
}

void AZPlayerController::RunAutomation(float Dt)
{
	if (ShotTimes.Num() == 0 && QuitAt < 0.f && !bAutoPlay && AutoKeys.Num() == 0) return;
	AutoTime += FMath::Min(Dt, 0.1f);
	// Touches simulées pour les captures (ex. -ZKeys=Enter,Down,Enter)
	if (AutoKeys.Num() > 0 && AutoTime > 2.f + 0.35f * KeyIndex && KeyIndex < AutoKeys.Num())
	{
		const FKey K(*AutoKeys[KeyIndex++]);
		FKeyEvent Down(K, FModifierKeysState(), 0, false, 0, 0);
		const bool bHandled = FSlateApplication::Get().ProcessKeyDownEvent(Down);
		FSlateApplication::Get().ProcessKeyUpEvent(Down);
		UE_LOG(LogTemp, Log, TEXT("ZAUTO key %s handled=%d t=%.2f"), *K.ToString(), bHandled, AutoTime);
	}
	if (ShotIndex < ShotTimes.Num() && AutoTime >= ShotTimes[ShotIndex])
	{
		FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("%s_%d"), *ShotName, ShotIndex), true, false);
		++ShotIndex;
	}
	if (QuitAt > 0.f && AutoTime >= QuitAt)
	{
		QuitAt = -1.f;
		ConsoleCommand(TEXT("quit"));
	}
	// Jeu automatique : choisit une commande raisonnable après un court délai (captures de combat)
	static float Wait = 0.f;
	if (bAutoPlay && Mode == EMode::Battle && Director && Director->IsAwaitingInput())
	{
		Wait += Dt;
		if (Wait < 1.6f) return;
		Wait = 0.f;
		const FZCombatSim& Sim = Director->GetSim();
		const int32 A = Sim.GetCurrentActor();
		const FZBattler& Me = Sim.Actors[A];
		FZCommand C; C.Actor = A; C.Type = EZCmd::Attack;
		const int32 Foe = Director->GetFocusEnemy();
		C.Targets = { Foe };
		FString Why;
		int32 Weakest = -1;
		for (const FZBattler& X : Sim.Actors) { if (X.bAlly && !X.bKO && (Weakest < 0 || X.HPPct() < Sim.Actors[Weakest].HPPct())) Weakest = X.Id; }
		if (Sim.CanInteractValve(A, Why)) C.Type = EZCmd::Interact;
		else if (Me.DefId == FName(TEXT("link")) && Me.Form.IsNone() && Sim.CanTransform(A, TEXT("Zora"), Why)) { C.Type = EZCmd::Transform; C.Id = TEXT("Zora"); }
		else if (Me.DefId == FName(TEXT("link")) && !Me.Form.IsNone() && Sim.CanUseAbility(A, TEXT("F_ZORA_WAVE"), Why)) { C.Type = EZCmd::Ability; C.Id = TEXT("F_ZORA_WAVE"); }
		else if (Me.DefId == FName(TEXT("mipha")) && Weakest >= 0 && Sim.Actors[Weakest].HPPct() < 0.5f && Sim.CanUseAbility(A, TEXT("MIPHA_HEAL"), Why)) { C.Type = EZCmd::Ability; C.Id = TEXT("MIPHA_HEAL"); C.Targets = { Weakest }; }
		else if (Me.DefId == FName(TEXT("sheik")) && Sim.CanSummon(A, Why)) { C.Type = EZCmd::Summon; }
		Director->SubmitCommand(C, Why);
	}
}
