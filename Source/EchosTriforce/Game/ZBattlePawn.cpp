#include "ZBattlePawn.h"
#include "ZCombat.h"
#include "ZTPAnim.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"

AZBattlePawn::AZBattlePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Visual);
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Visual);
	Glow->SetRelativeLocation(FVector(0, 0, 120));
	Glow->SetIntensity(0.f);
	Glow->SetAttenuationRadius(500.f);
	Glow->SetCastShadows(false);
	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Root);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	Ring->SetRelativeLocation(FVector(0, 0, 2));
	Ring->SetVisibility(false);
}

void AZBattlePawn::InitAlly(const FZBattler& B, const FString& InOutfit, const FString& InWeapon, const FString& InOffhand)
{
	CharId = B.DefId;
	bAllySide = true;
	Outfit = InOutfit;
	Weapon = InWeapon;
	Offhand = InOffhand;
	CurrentForm = B.Form;
	RebuildLook();
}

void AZBattlePawn::InitEnemy(const FZBattler& B)
{
	CharId = B.DefId;
	bAllySide = false;
	Look = ZVis::EnemyLook(B.Mesh, ZNames::HexColor(B.Color, FLinearColor(0.5f, 0.5f, 0.5f)), B.Scale);
	ZVis::Build(this, Visual, Body, Look, Parts);
	bHumanoid = Body->GetSkeletalMeshAsset() != nullptr;
	if (bHumanoid) PlayLoop(TEXT("Idle"));
}

void AZBattlePawn::RebuildLook()
{
	Look = ZVis::CharacterLook(CharId, CurrentForm, Outfit, Weapon, Offhand);
	ZVis::Build(this, Visual, Body, Look, Parts);
	ZVis::SetArtVisible(this, TEXT("hilt"), false); // en combat l'épée est en main : fourreau vide
	if (!ZVis::SetupTPAnim(Body, Look)) Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	ZVis::ShowWeaponsDrawn(this, true);
	bCalmIdle = false;
	SheatheT = -1.f;
	bHumanoid = Body->GetSkeletalMeshAsset() != nullptr;
	if (bHumanoid && !bDead) PlayLoop(TEXT("Idle"));
}

void AZBattlePawn::SetFormVisual(FName Form)
{
	CurrentForm = Form;
	RebuildLook();
}

void AZBattlePawn::SetHome(const FVector& Loc, const FRotator& Rot)
{
	Home = Loc;
	HomeRot = Rot;
	SetActorLocationAndRotation(Loc, Rot);
	MoveFrom = MoveTo = Loc;
	MoveT = 1.f; MoveDur = 0.f;
}

UZTPAnimInstance* AZBattlePawn::TPAnim() const
{
	return Look.AnimSet.IsEmpty() ? nullptr : Cast<UZTPAnimInstance>(Body->GetAnimInstance());
}

FString AZBattlePawn::AnimFor(const FString& Name) const
{
	if (!bAllySide || (bCalmIdle && Name == TEXT("Idle"))) return Name;
	static const TMap<FString, FString> Armed = {
		{ TEXT("Idle"), TEXT("SwordIdle") }, { TEXT("Attack1"), TEXT("SwordSlash") }, { TEXT("Attack2"), TEXT("SwordThrust") },
		{ TEXT("Charged"), TEXT("SwordSpin") }, { TEXT("Attack3"), TEXT("Cast") }, { TEXT("Dash"), TEXT("Guard") }, { TEXT("Jump"), TEXT("Victory") },
	};
	const FString* Mapped = Armed.Find(Name);
	return Mapped && ZVis::AnimIn(Look.AnimSet, *Mapped) ? *Mapped : Name;
}

void AZBattlePawn::PlayLoop(const FString& Name)
{
	if (!bHumanoid) return;
	if (UAnimSequence* A = ZVis::AnimIn(Look.AnimSet, AnimFor(Name)))
	{
		if (UZTPAnimInstance* TP = TPAnim()) TP->Play(A, true, 0.2f);
		else Body->PlayAnimation(A, true);
	}
}

void AZBattlePawn::PlayOnce(const FString& Name, float BlendBackAfter)
{
	if (!bHumanoid) return;
	if (UAnimSequence* A = ZVis::AnimIn(Look.AnimSet, AnimFor(Name)))
	{
		if (UZTPAnimInstance* TP = TPAnim()) TP->Play(A, false, 0.1f);
		else Body->PlayAnimation(A, false);
		// fondu de retour à l'attente un peu avant la fin (lecteur de TP)
		IdleBackTimer = BlendBackAfter > 0.f ? BlendBackAfter : FMath::Max(0.05f, A->GetPlayLength() - (TPAnim() ? 0.15f : 0.f));
	}
}

void AZBattlePawn::PlayAction(const FString& Anim, const FVector& TargetLoc, float Duration)
{
	static const TSet<FString> Melee = { TEXT("Slash"), TEXT("Thrust"), TEXT("Smash"), TEXT("Spin"), TEXT("Bite"), TEXT("Roll"), TEXT("Bash"), TEXT("Duo") };
	const bool bMelee = Melee.Contains(Anim);
	if (bMelee)
	{
		const FVector Dir = (TargetLoc - Home).GetSafeNormal2D();
		const float Stop = bAllySide ? 170.f : 220.f * Look.Scale;
		MoveFrom = GetActorLocation();
		MoveTo = TargetLoc - Dir * Stop;
		MoveTo.Z = Home.Z;
		MoveDur = Duration * 0.45f;
		MoveT = 0.f;
		bReturning = false;
		SetActorRotation(Dir.Rotation());
	}
	if (bHumanoid && !Look.AnimSet.IsEmpty() && ZVis::AnimIn(Look.AnimSet, Anim))
	{
		PlayOnce(Anim, 0.f); // animation d'origine propre à chaque action (taille, estoc, attaque tournoyante…)
	}
	else if (bHumanoid)
	{
		FString A = TEXT("Attack1");
		if (Anim == TEXT("Spin") || Anim == TEXT("Smash") || Anim == TEXT("Roll")) A = TEXT("Charged");
		else if (Anim == TEXT("Thrust") || Anim == TEXT("Bite")) A = TEXT("Attack2");
		else if (Anim == TEXT("Cast") || Anim == TEXT("Chant") || Anim == TEXT("Heal") || Anim == TEXT("Summon") || Anim == TEXT("Beam")) A = TEXT("Attack3");
		else if (Anim == TEXT("Shoot") || Anim == TEXT("Throw") || Anim == TEXT("Hook")) A = TEXT("Attack2");
		else if (Anim == TEXT("Guard") || Anim == TEXT("Taunt") || Anim == TEXT("Item")) A = TEXT("Dash");
		PlayOnce(A, 0.f);
	}
	else if (!bMelee)
	{
		HitT = 0.f; HitStrength = -0.6f; // petit gonflement d'incantation
	}
}

void AZBattlePawn::ReturnHome(float Duration)
{
	if (FVector::DistSquared2D(GetActorLocation(), Home) < 25.f)
	{
		SetActorRotation(HomeRot);
		return;
	}
	MoveFrom = GetActorLocation();
	MoveTo = Home;
	MoveDur = Duration;
	MoveT = 0.f;
	bReturning = true;
}

void AZBattlePawn::PlayHit(bool bHeavy)
{
	HitT = 0.f;
	HitStrength = bHeavy ? 1.6f : 1.f;
	if (!bDead && TPAnim() && !IsBusy()) PlayOnce(bHeavy ? TEXT("HitHeavy") : TEXT("Hit"), 0.f);
}

void AZBattlePawn::PlayDeath()
{
	bDead = true;
	DeathT = 0.f;
	SetGlow(false, FLinearColor::White);
	if (bHumanoid)
	{
		if (UAnimSequence* A = ZVis::AnimIn(Look.AnimSet, TEXT("Death")))
		{
			if (UZTPAnimInstance* TP = TPAnim()) TP->Play(A, false, 0.1f);
			else Body->PlayAnimation(A, false);
		}
		IdleBackTimer = -1.f;
	}
}

void AZBattlePawn::PlayRevive()
{
	bDead = false;
	DeathT = -1.f;
	Visual->SetRelativeLocation(FVector::ZeroVector);
	Visual->SetRelativeRotation(FRotator::ZeroRotator);
	RebuildLook();
}

void AZBattlePawn::PlayVictory()
{
	if (bDead) return;
	if (TPAnim())
	{
		// Link de TP : moulinet et épée au fourreau (ANM_FINISH), puis attente normale
		PlayOnce(TEXT("Victory"), 0.f);
		if (UAnimSequence* A = ZVis::AnimIn(Look.AnimSet, TEXT("Victory"))) SheatheT = A->GetPlayLength() * 0.8f;
		bCalmIdle = true;
		return;
	}
	PlayOnce(TEXT("Jump"), 0.f);
}

void AZBattlePawn::SetGlow(bool bOn, const FLinearColor& Color, float Intensity)
{
	Glow->SetLightColor(Color);
	Glow->SetIntensity(bOn ? Intensity : 0.f);
	Glow->SetRelativeLocation(FVector(0, 0, GetVisualHeight() * 0.6f));
}

void AZBattlePawn::SetSelected(bool bOn)
{
	bSelected = bOn;
	if (!Ring->GetStaticMesh())
	{
		Ring->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
		ZVis::Tint(Ring, FLinearColor(0.2f, 1.f, 0.9f), 4.f);
	}
	Ring->SetVisibility(bOn);
}

FVector AZBattlePawn::GetFocusLocation() const
{
	return GetActorLocation() + FVector(0, 0, GetVisualHeight() * 0.55f);
}

FVector AZBattlePawn::GetTopLocation() const
{
	return GetActorLocation() + FVector(0, 0, GetVisualHeight() + 30.f);
}

void AZBattlePawn::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;

	if (MoveT < MoveDur)
	{
		MoveT = FMath::Min(MoveDur, MoveT + Dt);
		const float A = FMath::InterpEaseInOut(0.f, 1.f, MoveT / FMath::Max(MoveDur, 0.01f), 2.f);
		FVector P = FMath::Lerp(MoveFrom, MoveTo, A);
		P.Z += FMath::Sin(A * PI) * 25.f;
		SetActorLocation(P);
		if (bReturning && MoveT >= MoveDur) SetActorRotation(HomeRot);
	}

	if (SheatheT > 0.f)
	{
		SheatheT -= Dt;
		if (SheatheT <= 0.f) ZVis::ShowWeaponsDrawn(this, false);
	}
	if (IdleBackTimer > 0.f)
	{
		IdleBackTimer -= Dt;
		if (IdleBackTimer <= 0.f && !bDead) PlayLoop(TEXT("Idle"));
	}

	// Recul et tremblement à l'impact
	FVector Offset = FVector::ZeroVector;
	FVector ScalePulse = FVector::OneVector;
	if (HitT < 0.35f)
	{
		HitT += Dt;
		const float K = 1.f - HitT / 0.35f;
		if (HitStrength > 0.f)
		{
			const FVector Back = -GetActorForwardVector() * 18.f * HitStrength * K;
			Offset += Back + FVector(FMath::Sin(Time * 90.f) * 6.f * K, 0, 0);
		}
		else
		{
			ScalePulse *= 1.f + (-HitStrength) * 0.15f * FMath::Sin(K * PI);
		}
	}
	// Respiration des créatures sans squelette
	if (!bHumanoid && !bDead)
	{
		const float Breath = FMath::Sin(Time * 2.2f + CharId.GetNumber()) * 0.03f;
		ScalePulse *= FVector(1.f - Breath, 1.f - Breath, 1.f + Breath * 1.5f);
		Offset.Z += FMath::Sin(Time * 1.7f) * (CharId == FName(TEXT("keese")) ? 12.f : 2.f);
	}
	if (bDead && DeathT >= 0.f)
	{
		DeathT += Dt;
		if (!bHumanoid)
		{
			const float K = FMath::Clamp(DeathT / 0.9f, 0.f, 1.f);
			ScalePulse *= FMath::Max(0.01f, 1.f - K);
			Offset.Z -= K * 40.f;
			if (K >= 1.f) Visual->SetVisibility(false, true);
		}
		else if (!bAllySide)
		{
			const float K = FMath::Clamp((DeathT - 1.2f) / 0.8f, 0.f, 1.f);
			if (K >= 1.f) Visual->SetVisibility(false, true);
		}
	}
	Visual->SetRelativeLocation(Offset);
	Visual->SetRelativeScale3D(Look.ScaleAxes * Look.Scale * ScalePulse);

	if (bSelected)
	{
		SelectedPulse += Dt;
		const float S = 1.1f + 0.08f * FMath::Sin(SelectedPulse * 6.f);
		const float R = FMath::Max(90.f, 60.f * Look.Scale);
		Ring->SetRelativeScale3D(FVector(R / 50.f * S, R / 50.f * S, 0.02f));
	}
	if (bGuarding && Glow->Intensity <= 0.f)
	{
		Glow->SetLightColor(FLinearColor(0.4f, 0.8f, 1.f));
		Glow->SetIntensity(2500.f);
	}
}
