#include "ZBattleDirector.h"
#include "ZBattlePawn.h"
#include "ZGameData.h"
#include "ZVisuals.h"
#include "EchosTriforce.h"
#include "Camera/CameraActor.h"
#include "Misc/CommandLine.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

namespace
{
	const TSet<FString>& MeleeAnims()
	{
		static const TSet<FString> S = { TEXT("Slash"), TEXT("Thrust"), TEXT("Smash"), TEXT("Spin"), TEXT("Bite"), TEXT("Roll"), TEXT("Bash"), TEXT("Duo") };
		return S;
	}
}

AZBattleDirector::AZBattleDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	SpiritMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Spirit"));
	SpiritMesh->SetupAttachment(GetRootComponent());
	SpiritMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpiritMesh->SetCastShadow(false);
	SpiritMesh->SetVisibility(false);
	SpiritLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SpiritLight"));
	SpiritLight->SetupAttachment(GetRootComponent());
	SpiritLight->SetIntensity(0.f);
	SpiritLight->SetAttenuationRadius(2500.f);
	SpiritLight->SetCastShadows(false);
}

float AZBattleDirector::Speed() const
{
	const UZGameInstance* GI = UZGameInstance::Get(this);
	return GI ? FMath::Max(0.5f, GI->AnimSpeed) : 1.f;
}

void AZBattleDirector::BeginBattle(FName InEncounter, const TArray<FName>& Enemies, const FVector& InCenter, const FRotator& InFacing, bool bPreemptive)
{
	EncounterId = InEncounter;
	EnemyList = Enemies;
	Center = InCenter;
	Facing = FRotator(0.f, InFacing.Yaw, 0.f);
	bPreemptiveStart = bPreemptive;
	SetActorLocation(Center);

	Sim = FZCombatSim();
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (!GI) return;
	GI->SetupBattle(Sim, Enemies, bPreemptive);
	Sim.bGiantArena = Enemies.Contains(FName(TEXT("nereide")));

	DisplayHP.SetNum(Sim.Actors.Num());
	DisplayMP.SetNum(Sim.Actors.Num());
	for (int32 i = 0; i < Sim.Actors.Num(); ++i)
	{
		DisplayHP[i] = Sim.Actors[i].HP;
		DisplayMP[i] = Sim.Actors[i].MP;
	}
	DisplayResonance = Sim.Resonance;
	Queue.Reset();
	Popups.Reset();
	RecentLog.Reset();
	bShowingResult = false;
	Outcome = EZSimState::Idle;
	StartDelay = 1.0f;
	EventTimer = 0.f;
	BigBanner = bPreemptive ? TEXT("Attaque surprise ! Jauges +200") : (Sim.Actors.ContainsByPredicate([](const FZBattler& B) { return B.bBoss; }) ? TEXT("Gardien de la citerne") : TEXT("Combat !"));
	BigBannerAge = 0.f;
	BigBannerColor = FLinearColor(0.6f, 1.f, 0.95f);
	BigBannerImage = NAME_None;
	SpawnPawns();
}

namespace
{
	const FVector2D AllySlots[] = { FVector2D(-260, 30), FVector2D(-215, 145), FVector2D(-170, 255) };
	const FVector2D EnemySlotsProbe[] = { FVector2D(460, -330), FVector2D(430, -170), FVector2D(380, -330), FVector2D(470, 40), FVector2D(520, 380) };
}

void AZBattleDirector::CameraRig(bool bBoss, FVector& OutBase, FVector& OutLook, float& OutFov) const
{
	// Caméra basse, à hauteur d'épaule derrière l'équipe (maquette 05) ; réglable par -ZCamBase=x,y,z -ZCamLook=x,y,z -ZCamFov=
	auto CmdVec = [](const TCHAR* Key, const FVector& Def)
	{
		FString V;
		TArray<FString> P;
		if (!FParse::Value(FCommandLine::Get(), Key, V, false)) return Def;
		V.ParseIntoArray(P, TEXT(","));
		return P.Num() == 3 ? FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])) : Def;
	};
	OutBase = CmdVec(TEXT("ZCamBase="), bBoss ? FVector(-900.f, -20.f, 270.f) : FVector(-1050.f, 80.f, 320.f));
	OutLook = CmdVec(TEXT("ZCamLook="), bBoss ? FVector(300.f, 75.f, 190.f) : FVector(350.f, -80.f, 90.f));
	OutFov = bBoss ? 62.f : 60.f;
	FParse::Value(FCommandLine::Get(), TEXT("ZCamFov="), OutFov);
}

FRotator AZBattleDirector::PickFacing(const FRotator& Preferred, bool bBoss) const
{
	UWorld* W = GetWorld();
	if (!W) return Preferred;
	FVector B, L;
	float Fov;
	CameraRig(bBoss, B, L, Fov);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ZBattleFacing), false, this);
	float BestScore = -1e9f;
	FRotator Best = Preferred;
	// Ordre : direction préférée, puis quarts de tour, puis diagonales
	static const float Offsets[] = { 0.f, 90.f, -90.f, 180.f, 45.f, -45.f, 135.f, -135.f };
	for (int32 k = 0; k < 8; ++k)
	{
		const FRotator R(0.f, Preferred.Yaw + Offsets[k], 0.f);
		const FVector Fwd = R.Vector();
		const FVector Right = FRotationMatrix(R).GetUnitAxis(EAxis::Y);
		auto Local = [&](float X, float Y) { return Center + Fwd * X + Right * Y; };
		// ligne de vue de la caméra depuis la tête de l'équipe
		const FVector From = Local(-215.f, 145.f) + FVector(0, 0, 160.f);
		const FVector Cam = Local(B.X, B.Y) + FVector(0, 0, B.Z);
		FHitResult Hit;
		float Free = 1.f;
		if (W->SweepSingleByChannel(Hit, From, Cam, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(30.f), Q)) Free = Hit.Time;
		// emplacements des combattants atteignables depuis le centre sans traverser de mur
		int32 Blocked = 0;
		for (const FVector2D& S : AllySlots) { if (W->LineTraceSingleByChannel(Hit, Center + FVector(0, 0, 120), Local(S.X, S.Y) + FVector(0, 0, 120), ECC_Camera, Q)) ++Blocked; }
		for (const FVector2D& S : EnemySlotsProbe) { if (W->LineTraceSingleByChannel(Hit, Center + FVector(0, 0, 120), Local(S.X, S.Y) + FVector(0, 0, 120), ECC_Camera, Q)) ++Blocked; }
		const float Score = Free * 10.f - Blocked * 2.5f - k * 0.05f;
		if (Score > BestScore) { BestScore = Score; Best = R; }
	}
	return Best;
}

void AZBattleDirector::SpawnPawns()
{
	for (AZBattlePawn* P : Pawns) { if (P) P->Destroy(); }
	Pawns.Reset();
	UWorld* W = GetWorld();
	const bool bBossFight = Sim.Actors.ContainsByPredicate([](const FZBattler& B) { return B.bBoss; });
	Facing = PickFacing(Facing, bBossFight);
	UZGameInstance* GI = UZGameInstance::Get(this);
	const FVector Fwd = Facing.Vector();
	const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
	auto Local = [&](float X, float Y) { return Center + Fwd * X + Right * Y; };
	auto Ground = [W](FVector P)
	{
		FHitResult Hit;
		FCollisionQueryParams Q;
		Q.bTraceComplex = false;
		if (W->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 400), P - FVector(0, 0, 1200), ECC_Visibility, Q))
		{
			P.Z = Hit.ImpactPoint.Z;
		}
		return P;
	};

	int32 AllyIdx = 0, EnemyIdx = 0;
	int32 EnemyCount = 0;
	for (const FZBattler& B : Sim.Actors) { if (!B.bAlly) ++EnemyCount; }
	TArray<FVector2D> EnemySlots;
	if (EnemyCount == 1) EnemySlots = { FVector2D(430, -170) };
	else if (EnemyCount == 2) EnemySlots = { FVector2D(380, -330), FVector2D(470, 40) };
	else EnemySlots = { FVector2D(360, -400), FVector2D(480, -100), FVector2D(380, 200), FVector2D(520, 380) };

	for (const FZBattler& B : Sim.Actors)
	{
		AZBattlePawn* P = W->SpawnActor<AZBattlePawn>(AZBattlePawn::StaticClass(), FTransform(Facing, Center));
		if (B.bAlly)
		{
			const FVector2D S = AllySlots[FMath::Min(AllyIdx++, 2)];
			const FZCharacterState* St = GI ? GI->FindMember(B.DefId) : nullptr;
			P->InitAlly(B, St ? St->GetEquip(EZEquipSlot::Torso) : FString(), St ? St->GetEquip(EZEquipSlot::Weapon) : FString(), St ? St->GetEquip(EZEquipSlot::Offhand) : FString());
			P->SetHome(Ground(Local(S.X, S.Y)), Facing);
		}
		else
		{
			FVector2D S = EnemySlots[FMath::Min(EnemyIdx++, EnemySlots.Num() - 1)];
			if (B.bBoss) S = FVector2D(460, -330);
			P->InitEnemy(B);
			P->SetHome(Ground(Local(S.X, S.Y)), FRotator(0, Facing.Yaw + 180.f, 0));
		}
		Pawns.Add(P);
	}

	// Caméra : derrière l'équipe, légèrement à gauche, comme la maquette de combat
	if (!Camera)
	{
		Camera = W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity);
		Camera->GetCameraComponent()->SetFieldOfView(58.f);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		Camera->GetCameraComponent()->PostProcessBlendWeight = 0.f;
	}
	const bool bBoss = Sim.Actors.ContainsByPredicate([](const FZBattler& B) { return B.bBoss; });
	FVector B, L;
	float Fov;
	CameraRig(bBoss, B, L, Fov);
	Camera->GetCameraComponent()->SetFieldOfView(Fov);
	CamBase = Local(B.X, B.Y) + FVector(0, 0, B.Z);
	CamLook = Local(L.X, L.Y) + FVector(0, 0, L.Z);
	{
		// Si un mur reste entre l'équipe et la caméra, on la rapproche (comme un bras de caméra) en la relevant un peu
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ZBattleCam), false, this);
		const FVector From = Local(-215.f, 145.f) + FVector(0, 0, 160.f);
		FHitResult Hit;
		if (W->SweepSingleByChannel(Hit, From, CamBase, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(30.f), Q))
		{
			const FVector Dir = (CamBase - From).GetSafeNormal();
			const float Lost = 1.f - Hit.Time;
			CamBase = Hit.Location - Dir * 40.f + FVector(0, 0, 60.f * Lost);
			Camera->GetCameraComponent()->SetFieldOfView(Fov + 12.f * Lost);
		}
	}
	CamBase.Z += Center.Z * 0.f;
	Camera->SetActorLocation(CamBase);
	Camera->SetActorRotation((CamLook - CamBase).Rotation());
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetViewTargetWithBlend(Camera, 0.7f, VTBlend_EaseInOut, 2.f);
	}
}

bool AZBattleDirector::IsAwaitingInput() const
{
	return !bShowingResult && StartDelay <= 0.f && Sim.GetState() == EZSimState::AwaitingCommand && Queue.Num() == 0 && EventTimer <= 0.f;
}

bool AZBattleDirector::SubmitCommand(const FZCommand& Cmd, FString& OutReason)
{
	if (!IsAwaitingInput()) { OutReason = TEXT("Patiente…"); return false; }
	const bool bOk = Sim.Submit(Cmd, &OutReason);
	if (bOk)
	{
		SetTargetHighlight({});
		Queue.Append(Sim.ConsumeEvents());
	}
	return bOk;
}

void AZBattleDirector::SetTargetHighlight(const TArray<int32>& Targets)
{
	Highlighted = Targets;
	for (int32 i = 0; i < Pawns.Num(); ++i)
	{
		if (Pawns[i]) Pawns[i]->SetSelected(Targets.Contains(i));
	}
}

int32 AZBattleDirector::GetFocusEnemy() const
{
	for (int32 H : Highlighted) { if (Sim.Actors.IsValidIndex(H) && !Sim.Actors[H].bAlly && !Sim.Actors[H].bKO) return H; }
	for (const FZBattler& B : Sim.Actors) { if (!B.bAlly && B.bBoss && !B.bKO) return B.Id; }
	for (const FZBattler& B : Sim.Actors) { if (!B.bAlly && !B.bKO) return B.Id; }
	for (const FZBattler& B : Sim.Actors) { if (!B.bAlly) return B.Id; }
	return -1;
}

void AZBattleDirector::AddPopup(int32 Actor, const FString& Text, const FLinearColor& Color, float Size, float Height)
{
	AZBattlePawn* P = GetPawn(Actor);
	if (!P) return;
	FZPopup Pop;
	Pop.World = P->GetActorLocation() + FVector(0, 0, P->GetVisualHeight() * Height + 20.f) + FVector(FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(0.f, 20.f));
	Pop.Text = Text;
	Pop.Color = Color;
	Pop.Size = Size;
	Popups.Add(Pop);
}

void AZBattleDirector::Shake(float Strength)
{
	ShakeAmt = FMath::Max(ShakeAmt, Strength);
}

void AZBattleDirector::SpawnProjectile(int32 From, int32 To, const FLinearColor& Color, float Duration)
{
	AZBattlePawn* A = GetPawn(From);
	AZBattlePawn* B = GetPawn(To);
	if (!A || !B) return;
	UStaticMeshComponent* C = nullptr;
	int32 Index = INDEX_NONE;
	for (int32 i = 0; i < Projectiles.Num(); ++i)
	{
		if (Projectiles[i] && !Projectiles[i]->IsVisible()) { C = Projectiles[i]; Index = i; break; }
	}
	if (!C)
	{
		C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(ZVis::Mesh(TEXT("Sphere")));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetupAttachment(GetRootComponent());
		C->RegisterComponent();
		Index = Projectiles.Add(C);
	}
	ZVis::Tint(C, Color, 25.f);
	C->SetWorldScale3D(FVector(0.28f));
	C->SetVisibility(true);
	FProjectile P;
	P.Comp = Index;
	P.From = A->GetFocusLocation() + A->GetActorForwardVector() * 40.f;
	P.To = B->GetFocusLocation();
	P.T = 0.f;
	P.Dur = Duration;
	C->SetWorldLocation(P.From);
	Flying.Add(P);
}

void AZBattleDirector::SpawnSpirit(const FLinearColor& Color, float Duration)
{
	SpiritMesh->SetStaticMesh(ZVis::Mesh(TEXT("Sphere")));
	ZVis::Tint(SpiritMesh, Color, 30.f);
	SpiritMesh->SetWorldLocation(Center + FVector(0, 0, 520));
	SpiritMesh->SetVisibility(true);
	SpiritLight->SetWorldLocation(Center + FVector(0, 0, 450));
	SpiritLight->SetLightColor(Color);
	SpiritT = 0.f;
	SpiritDur = Duration;
}

void AZBattleDirector::PlayEvent(const FZEvent& E, float& Dur)
{
	const UZGameData& DB = UZGameData::Get();
	const float S = 1.f / Speed();
	Dur = 0.f;
	auto Name = [this](int32 A) { return Sim.Actors.IsValidIndex(A) ? Sim.Actors[A].Name : FString(); };
	AZBattlePawn* Src = GetPawn(E.Source);
	AZBattlePawn* Tgt = GetPawn(E.Target);

	switch (E.Type)
	{
	case EZEvt::Ready:
		break;
	case EZEvt::ActionStart:
	{
		Banner = E.Text;
		BannerAge = 0.f;
		RecentLog.Add(FString::Printf(TEXT("%s : %s"), *Name(E.Source), *E.Text));
		LastActionActor = E.Source;
		if (Src)
		{
			const bool bMelee = MeleeAnims().Contains(E.Anim);
			const FVector TL = Tgt ? Tgt->GetActorLocation() : Src->GetActorLocation();
			Src->PlayAction(E.Anim, TL, 0.9f * S);
			CamFocusOffset = (Src->GetActorLocation() - Center) * 0.2f;
			if (!bMelee && Tgt && E.Target != E.Source && Sim.Actors.IsValidIndex(E.Target) && Sim.Actors[E.Source].bAlly != Sim.Actors[E.Target].bAlly)
			{
				SpawnProjectile(E.Source, E.Target, ZNames::ElementColor(E.Element), 0.38f * S);
			}
			Dur = (bMelee ? 0.55f : 0.5f) * S;
			if (E.Anim == TEXT("Guard")) Dur = 0.3f * S;
		}
		break;
	}
	case EZEvt::ActionEnd:
		if (AZBattlePawn* P = GetPawn(LastActionActor))
		{
			P->ReturnHome(0.3f * S);
			Dur = 0.3f * S;
		}
		CamFocusOffset = FVector::ZeroVector;
		break;
	case EZEvt::Damage:
	{
		if (DisplayHP.IsValidIndex(E.Target)) DisplayHP[E.Target] = FMath::Max(0, DisplayHP[E.Target] - E.Amount);
		FLinearColor C = FLinearColor::White;
		float Size = 34.f;
		if (E.Element != EZElement::Neutral) C = FMath::Lerp(FLinearColor::White, ZNames::ElementColor(E.Element), 0.55f);
		if (E.bCrit) { C = FLinearColor(1.f, 0.84f, 0.35f); Size = 44.f; }
		AddPopup(E.Target, ZNames::FormatInt(E.Amount) + (E.bCrit ? TEXT("!") : TEXT("")), C, Size);
		if (E.bWeak) AddPopup(E.Target, TEXT("FAIBLESSE"), FLinearColor(1.f, 0.55f, 0.2f), 20.f, 1.25f);
		else if (E.bResist) AddPopup(E.Target, TEXT("résiste"), FLinearColor(0.6f, 0.75f, 0.9f), 18.f, 1.25f);
		if (Tgt)
		{
			const bool bHeavy = E.bCrit || (Sim.Actors.IsValidIndex(E.Target) && E.Amount > Sim.Actors[E.Target].MaxHP * 0.12f);
			Tgt->PlayHit(bHeavy);
			if (bHeavy) Shake(8.f);
		}
		Dur = 0.28f * S;
		break;
	}
	case EZEvt::DoT:
		if (DisplayHP.IsValidIndex(E.Target)) DisplayHP[E.Target] = FMath::Max(0, DisplayHP[E.Target] - E.Amount);
		AddPopup(E.Target, ZNames::FormatInt(E.Amount), FLinearColor(0.8f, 0.45f, 1.f), 28.f);
		Dur = 0.3f * S;
		break;
	case EZEvt::Heal:
		if (DisplayHP.IsValidIndex(E.Target) && Sim.Actors.IsValidIndex(E.Target)) DisplayHP[E.Target] = FMath::Min(Sim.Actors[E.Target].MaxHP, DisplayHP[E.Target] + E.Amount);
		AddPopup(E.Target, TEXT("+") + ZNames::FormatInt(E.Amount), FLinearColor(0.4f, 1.f, 0.55f), 32.f);
		if (Tgt) Tgt->SetGlow(true, FLinearColor(0.4f, 1.f, 0.6f), 3000.f);
		Dur = 0.3f * S;
		break;
	case EZEvt::MPHeal:
		if (DisplayMP.IsValidIndex(E.Target) && Sim.Actors.IsValidIndex(E.Target)) DisplayMP[E.Target] = FMath::Min(Sim.Actors[E.Target].MaxMP, DisplayMP[E.Target] + E.Amount);
		AddPopup(E.Target, FString::Printf(TEXT("+%d PM"), E.Amount), FLinearColor(0.45f, 0.7f, 1.f), 26.f);
		Dur = 0.25f * S;
		break;
	case EZEvt::Miss:
		AddPopup(E.Target, TEXT("Raté"), FLinearColor(0.7f, 0.75f, 0.8f), 26.f);
		Dur = 0.25f * S;
		break;
	case EZEvt::StatusOn:
		if (E.Status == EZStatus::Guard) { if (Tgt) Tgt->SetGuarding(true); }
		if (!E.Text.IsEmpty()) AddPopup(E.Target, E.Text, ZNames::IsDebuff(E.Status) ? FLinearColor(1.f, 0.5f, 0.7f) : FLinearColor(0.45f, 1.f, 0.95f), 20.f, 1.15f);
		Dur = 0.15f * S;
		break;
	case EZEvt::StatusOff:
		if (E.Status == EZStatus::Guard) { if (Tgt) { Tgt->SetGuarding(false); Tgt->SetGlow(false, FLinearColor::White); } }
		break;
	case EZEvt::Reaction:
		AddPopup(E.Target, E.Text, FLinearColor(1.f, 0.92f, 0.3f), 24.f, 1.35f);
		Dur = 0.35f * S;
		break;
	case EZEvt::KO:
		if (DisplayHP.IsValidIndex(E.Target)) DisplayHP[E.Target] = 0;
		if (Tgt) Tgt->PlayDeath();
		RecentLog.Add(E.Text);
		Dur = 0.65f * S;
		break;
	case EZEvt::Revive:
		if (DisplayHP.IsValidIndex(E.Target)) DisplayHP[E.Target] = E.Amount;
		if (Tgt) { Tgt->PlayRevive(); Tgt->SetGlow(true, FLinearColor(1.f, 0.8f, 0.95f), 5000.f); }
		AddPopup(E.Target, TEXT("Réanimé"), FLinearColor(1.f, 0.85f, 0.95f), 26.f);
		Dur = 0.6f * S;
		break;
	case EZEvt::BreachBreak:
		BigBanner = TEXT("BRÈCHE !");
		BigBannerColor = FLinearColor(1.f, 0.6f, 0.2f);
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		if (Tgt) { Tgt->PlayHit(true); Tgt->SetGlow(true, FLinearColor(1.f, 0.5f, 0.1f), 12000.f); }
		Shake(14.f);
		RecentLog.Add(E.Text);
		Dur = 0.8f * S;
		break;
	case EZEvt::Resonance:
		DisplayResonance = FMath::Clamp(DisplayResonance + E.Amount, 0, 100);
		break;
	case EZEvt::FormStart:
	{
		const FZFormDef* F = DB.FindForm(E.Id);
		BigBanner = E.Text;
		BigBannerColor = F ? ZNames::HexColor(F->Color) : FLinearColor::White;
		BigBannerAge = 0.f;
		BigBannerImage = E.Id;
		if (Tgt)
		{
			Tgt->SetFormVisual(E.Id);
			Tgt->SetGlow(true, BigBannerColor, 15000.f);
		}
		Shake(6.f);
		Dur = 1.3f * S;
		break;
	}
	case EZEvt::FormEnd:
		if (Tgt) { Tgt->SetFormVisual(NAME_None); Tgt->SetGlow(false, FLinearColor::White); }
		AddPopup(E.Target, E.Text, FLinearColor(0.8f, 0.9f, 1.f), 20.f, 1.2f);
		Dur = 0.6f * S;
		break;
	case EZEvt::Summon:
	{
		const FZSpiritDef* Sp = DB.FindSpirit(E.Id);
		BigBanner = FString::Printf(TEXT("Invocation : %s"), *E.Text);
		BigBannerColor = Sp ? ZNames::HexColor(Sp->Color) : FLinearColor::White;
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		SpawnSpirit(BigBannerColor, 1.8f * S);
		Shake(5.f);
		Dur = 1.5f * S;
		break;
	}
	case EZEvt::Duo:
		BigBanner = FString::Printf(TEXT("Duo : %s"), *E.Text);
		BigBannerColor = FLinearColor(0.7f, 1.f, 0.9f);
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		if (Src) Src->SetGlow(true, BigBannerColor, 6000.f);
		if (Tgt) Tgt->SetGlow(true, BigBannerColor, 6000.f);
		Dur = 0.9f * S;
		break;
	case EZEvt::Prepare:
		if (Src) Src->SetGlow(true, ZNames::ElementColor(E.Element), 20000.f);
		AddPopup(E.Source, FString::Printf(TEXT("Prépare : %s"), *E.Text), FLinearColor(1.f, 0.8f, 0.4f), 24.f, 1.1f);
		RecentLog.Add(FString::Printf(TEXT("%s prépare %s"), *Name(E.Source), *E.Text));
		Dur = 0.8f * S;
		break;
	case EZEvt::PrepareCancel:
		if (Tgt) Tgt->SetGlow(false, FLinearColor::White);
		BigBanner = E.Text;
		BigBannerColor = FLinearColor(0.5f, 1.f, 0.85f);
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		RecentLog.Add(E.Text);
		Dur = 0.9f * S;
		break;
	case EZEvt::PhaseChange:
		BigBanner = E.Text;
		BigBannerColor = FLinearColor(1.f, 0.45f, 0.4f);
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		if (Src) Src->PlayHit(true);
		Shake(10.f);
		RecentLog.Add(E.Text);
		Dur = 1.2f * S;
		break;
	case EZEvt::Message:
		if (!E.Text.IsEmpty())
		{
			RecentLog.Add(E.Text);
			if (E.Target >= 0) AddPopup(E.Target, E.Text, FLinearColor(0.85f, 0.95f, 1.f), 18.f, 1.3f);
			Dur = 0.35f * S;
		}
		break;
	case EZEvt::Gauge:
		AddPopup(E.Target, FString::Printf(TEXT("Jauge %+d"), E.Amount), FLinearColor(1.f, 0.85f, 0.3f), 18.f, 1.2f);
		Dur = 0.2f * S;
		break;
	case EZEvt::Valve:
		BigBanner = E.Text;
		BigBannerColor = FLinearColor(0.4f, 0.9f, 1.f);
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		Dur = 0.6f * S;
		break;
	case EZEvt::Skip:
		AddPopup(E.Source, TEXT("Perd son activation"), FLinearColor(0.7f, 0.85f, 1.f), 20.f);
		RecentLog.Add(E.Text);
		Dur = 0.6f * S;
		break;
	case EZEvt::Analyze:
		AddPopup(E.Target, TEXT("Analysé"), FLinearColor(0.6f, 1.f, 0.95f), 22.f);
		RecentLog.Add(E.Text);
		Dur = 0.5f * S;
		break;
	case EZEvt::Victory:
		for (int32 i = 0; i < Pawns.Num(); ++i)
		{
			if (Sim.Actors[i].bAlly && Pawns[i]) Pawns[i]->PlayVictory();
		}
		BigBanner = TEXT("Victoire !");
		BigBannerColor = FLinearColor(1.f, 0.88f, 0.45f);
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		Dur = 1.2f * S;
		break;
	case EZEvt::Defeat:
		BigBanner = TEXT("L'équipe est tombée…");
		BigBannerColor = FLinearColor(1.f, 0.4f, 0.4f);
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		Dur = 1.2f * S;
		break;
	case EZEvt::Fled:
		BigBanner = TEXT("Fuite !");
		BigBannerAge = 0.f;
		BigBannerImage = NAME_None;
		Dur = 0.6f * S;
		break;
	default:
		break;
	}
	if (RecentLog.Num() > 8) RecentLog.RemoveAt(0, RecentLog.Num() - 8);
}

void AZBattleDirector::UpdateCamera(float Dt)
{
	if (!Camera) return;
	const FVector Right = FRotationMatrix(Facing).GetUnitAxis(EAxis::Y);
	FVector Pos = CamBase + Right * FMath::Sin(Time * 0.35f) * 35.f + FVector(0, 0, FMath::Sin(Time * 0.27f) * 12.f);
	CamFocusCurrent = FMath::VInterpTo(CamFocusCurrent, CamFocusOffset, Dt, 2.5f);
	FVector Look = CamLook + CamFocusCurrent;
	if (ShakeAmt > 0.f)
	{
		Pos += FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * ShakeAmt;
		ShakeAmt = FMath::Max(0.f, ShakeAmt - Dt * 40.f);
	}
	if (SpiritT >= 0.f)
	{
		Look = FMath::Lerp(Look, Center + FVector(0, 0, 450), 0.35f);
	}
	Camera->SetActorLocation(Pos);
	Camera->SetActorRotation((Look - Pos).Rotation());
}

void AZBattleDirector::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	UpdateCamera(Dt);
	BannerAge += Dt;
	BigBannerAge += Dt;

	for (int32 i = Flying.Num() - 1; i >= 0; --i)
	{
		FProjectile& P = Flying[i];
		P.T += Dt;
		const float A = FMath::Clamp(P.T / FMath::Max(P.Dur, 0.01f), 0.f, 1.f);
		FVector L = FMath::Lerp(P.From, P.To, A);
		L.Z += FMath::Sin(A * PI) * 60.f;
		if (Projectiles.IsValidIndex(P.Comp) && Projectiles[P.Comp])
		{
			Projectiles[P.Comp]->SetWorldLocation(L);
			if (A >= 1.f) Projectiles[P.Comp]->SetVisibility(false);
		}
		if (A >= 1.f) Flying.RemoveAt(i);
	}
	if (SpiritT >= 0.f)
	{
		SpiritT += Dt;
		const float A = FMath::Clamp(SpiritT / FMath::Max(SpiritDur, 0.01f), 0.f, 1.f);
		const float Env = FMath::Sin(A * PI);
		SpiritMesh->SetWorldScale3D(FVector(0.2f + 3.2f * Env));
		SpiritLight->SetIntensity(60000.f * Env);
		if (A >= 1.f) { SpiritT = -1.f; SpiritMesh->SetVisibility(false); SpiritLight->SetIntensity(0.f); }
	}
	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		Popups[i].Age += Dt;
		if (Popups[i].Age > Popups[i].Life) Popups.RemoveAt(i);
	}

	if (bShowingResult) return;
	if (StartDelay > 0.f) { StartDelay -= Dt; return; }

	if (EventTimer > 0.f)
	{
		EventTimer -= Dt;
		if (EventTimer > 0.f) return;
	}
	Queue.Append(Sim.ConsumeEvents());
	while (Queue.Num() > 0)
	{
		const FZEvent E = Queue[0];
		Queue.RemoveAt(0);
		float D = 0.f;
		PlayEvent(E, D);
		if (D > 0.f) { EventTimer = D; return; }
	}
	if (Sim.IsOver())
	{
		FinishBattle();
		return;
	}
	if (Sim.GetState() == EZSimState::Presenting) Sim.AckPresentation();
	if (Sim.GetState() == EZSimState::Filling)
	{
		Sim.Tick(Dt);
		Queue.Append(Sim.ConsumeEvents());
	}
	// Resynchronise les valeurs affichées lorsque rien n'est en cours de présentation
	if (Queue.Num() == 0 && EventTimer <= 0.f)
	{
		for (int32 i = 0; i < Sim.Actors.Num(); ++i)
		{
			DisplayHP[i] = Sim.Actors[i].HP;
			DisplayMP[i] = Sim.Actors[i].MP;
		}
		DisplayResonance = Sim.Resonance;
	}
}

void AZBattleDirector::FinishBattle()
{
	if (bShowingResult) return;
	Outcome = Sim.GetState();
	UZGameInstance* GI = UZGameInstance::Get(this);
	Report = FZVictoryReport();
	if (GI)
	{
		if (Outcome == EZSimState::Victory) Report = GI->ApplyBattleResult(Sim);
		else if (Outcome == EZSimState::Fled) GI->SyncFromBattle(Sim);
	}
	bShowingResult = true;
	SetTargetHighlight({});
}

void AZBattleDirector::ConfirmResult()
{
	if (!bShowingResult) return;
	const EZSimState Out = Outcome;
	const FName Enc = EncounterId;
	for (AZBattlePawn* P : Pawns) { if (P) P->Destroy(); }
	Pawns.Reset();
	if (Camera) { Camera->Destroy(); Camera = nullptr; }
	bShowingResult = false;
	OnBattleEnded.Broadcast(Out, Enc);
}

void AZBattleDirector::RetryBattle()
{
	if (UZGameInstance* GI = UZGameInstance::Get(this)) { GI->RestoreParty(); }
	BeginBattle(EncounterId, EnemyList, Center, Facing, false);
}

void AZBattleDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	for (AZBattlePawn* P : Pawns) { if (P) P->Destroy(); }
	Pawns.Reset();
	if (Camera) { Camera->Destroy(); Camera = nullptr; }
	Super::EndPlay(Reason);
}
