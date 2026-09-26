#include "ZExploreCharacter.h"
#include "EchosTriforce.h"
#include "ZGameInstance.h"
#include "ZVisuals.h"
#include "ZTPAnim.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"

namespace
{
	UClass* LocomotionAnimClass()
	{
		static TWeakObjectPtr<UClass> Cached;
		if (!Cached.IsValid())
		{
			Cached = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"), nullptr, LOAD_Quiet | LOAD_NoWarn);
		}
		return Cached.Get();
	}
}

AZPartyCharacter::AZPartyCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(38.f, 90.f);
	GetMesh()->SetRelativeLocation(FVector(0, 0, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(0, -90.f, 0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->bOrientRotationToMovement = true;
	M->RotationRate = FRotator(0, 620.f, 0);
	M->MaxWalkSpeed = 520.f;
	M->BrakingDecelerationWalking = 1800.f;
	M->JumpZVelocity = 520.f;
	bUseControllerRotationYaw = false;
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GetMesh());
}

void AZPartyCharacter::ApplyLook(FName InId, const FString& Outfit, const FString& Weapon, const FString& Offhand, bool bSheathed)
{
	CharId = InId;
	FZLook Look = ZVis::CharacterLook(InId, NAME_None, Outfit, Weapon, Offhand);
	if (bSheathed && !Look.BodyArt.IsEmpty())
	{
		// Modèle texturé : épée au fourreau et bouclier font partie de l'équipement dorsal
		Look.Parts.RemoveAll([](const FZPartSpec& P) { return P.Tag == TEXT("weapon") || P.Tag == TEXT("shield"); });
	}
	else if (bSheathed)
	{
		// En exploration l'épée et le bouclier se portent dans le dos, comme sur la maquette
		for (FZPartSpec& P : Look.Parts)
		{
			if (P.Tag == TEXT("weapon") && InId == FName(TEXT("link")))
			{
				// Dos (os spine_05 : X = haut, -Y = arrière, Z = latéral) : épée en diagonale
				P.Bone = TEXT("spine_05");
				P.Loc = FVector(-14, -22, -8);
				P.Rot = FRotator(-55, 0, 0);
			}
			if (P.Tag == TEXT("shield"))
			{
				P.Bone = TEXT("spine_05");
				P.Loc = FVector(-6, -30, 0);
				P.Rot = FRotator(0, 0, 90);
			}
		}
	}
	ZVis::Build(this, VisualRoot, GetMesh(), Look, Parts);
	VisualRoot->SetRelativeScale3D(FVector::OneVector);
	GetMesh()->SetRelativeScale3D(Look.ScaleAxes * Look.Scale);
	AnimSet = Look.AnimSet;
	if (UZTPAnimInstance* TP = ZVis::SetupTPAnim(GetMesh(), Look))
	{
		// Link de TP : attente, marche et course d'origine ; armes rangées dans le dos en exploration
		TP->SetLocomotion(ZVis::AnimIn(AnimSet, TEXT("Idle")), ZVis::AnimIn(AnimSet, TEXT("Walk")), ZVis::AnimIn(AnimSet, TEXT("Run")));
		ZVis::ShowWeaponsDrawn(this, !bSheathed);
	}
	else if (UClass* ABP = LocomotionAnimClass())
	{
		GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		GetMesh()->SetAnimInstanceClass(ABP);
	}
	UE_LOG(LogEchos, Verbose, TEXT("ApplyLook %s mesh=%s anim=%s skel=%s"), *InId.ToString(), *GetNameSafe(GetMesh()->GetSkeletalMeshAsset()),
		*GetNameSafe(GetMesh()->GetAnimInstance()), GetMesh()->GetSkeletalMeshAsset() ? *GetNameSafe(GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()) : TEXT("-"));
}

AZExploreCharacter::AZExploreCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetRootComponent());
	Boom->TargetArmLength = 440.f;
	Boom->SocketOffset = FVector(0, 0, 70.f);
	Boom->bUsePawnControlRotation = true;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 9.f;
	Boom->ProbeSize = 14.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(68.f);
	Camera->SetRelativeRotation(FRotator(-5.f, 0, 0));
}

void AZExploreCharacter::RefreshLook()
{
	const UZGameInstance* GI = UZGameInstance::Get(this);
	const FZCharacterState* S = GI ? GI->FindMember(TEXT("link")) : nullptr;
	ApplyLook(TEXT("link"), S ? S->GetEquip(EZEquipSlot::Torso) : FString(), S ? S->GetEquip(EZEquipSlot::Weapon) : FString(), S ? S->GetEquip(EZEquipSlot::Offhand) : FString(), true);
}

void AZPartyCharacter::UpdateTPAnim()
{
	UZTPAnimInstance* TP = AnimSet.IsEmpty() ? nullptr : Cast<UZTPAnimInstance>(GetMesh()->GetAnimInstance());
	if (!TP) return;
	TP->SetSpeed(GetVelocity().Size2D());
	const bool bAir = GetCharacterMovement()->IsFalling();
	if (bAir && !bTPAir)
	{
		// élan du saut, tenu en l'air (l'animation se fige sur sa dernière image)
		TP->Play(ZVis::AnimIn(AnimSet, TEXT("Jump")), false, 0.1f);
	}
	else if (!bAir && bTPAir)
	{
		TP->PlayOneShot(ZVis::AnimIn(AnimSet, TEXT("Land")), 0.05f, 0.15f);
	}
	bTPAir = bAir;
}

void AZExploreCharacter::Swing()
{
	if (IsSwinging()) return;
	SwingT = 0.f;
	if (UZTPAnimInstance* TP = AnimSet.IsEmpty() ? nullptr : Cast<UZTPAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		// coup d'épée de TP : l'épée passe du dos à la main le temps du coup
		ZVis::ShowWeaponsDrawn(this, true);
		TP->PlayOneShot(ZVis::AnimIn(AnimSet, TEXT("Attack1")), 0.06f, 0.2f, 1.25f);
		return;
	}
	if (UAnimSequence* A = ZVis::Anim(TEXT("Attack1")))
	{
		GetMesh()->GetAnimInstance() ? GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(A, TEXT("DefaultSlot"), 0.1f, 0.2f, 1.3f) : nullptr;
	}
}

void AZExploreCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	if (SwingT >= 0.f)
	{
		SwingT += Dt;
		if (SwingT > 0.6f)
		{
			SwingT = -1.f;
			if (!AnimSet.IsEmpty()) ZVis::ShowWeaponsDrawn(this, false);
		}
	}
	UpdateTPAnim();
	const FVector P = GetActorLocation();
	// Rattrapage : sous le niveau des dalles plus d'une seconde (bassin, eau) → retour au dernier point sûr
	const float Floor = P.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (GetCharacterMovement()->IsMovingOnGround() && Floor > -12.f) { LastSafe = P; LowTime = 0.f; }
	else if (Floor < -25.f && !LastSafe.IsZero())
	{
		LowTime += Dt;
		if (LowTime > 1.f) { SetActorLocation(LastSafe + FVector(0, 0, 10), false, nullptr, ETeleportType::TeleportPhysics); LowTime = 0.f; }
	}
	if (Trail.Num() == 0 || FVector::DistSquared(Trail.Last(), P) > 30.f * 30.f)
	{
		Trail.Add(P);
		if (Trail.Num() > 200) Trail.RemoveAt(0, Trail.Num() - 200);
	}
}

AZFollower::AZFollower()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AZFollower::Tick(float Dt)
{
	Super::Tick(Dt);
	UpdateTPAnim();
	if (!Leader) return;
	// Formation de la maquette d'exploration : un compagnon de chaque côté, légèrement en retrait
	const float Spacing = 110.f;
	FVector Goal = Leader->GetActorLocation();
	float Acc = 0.f;
	for (int32 i = Leader->Trail.Num() - 1; i > 0; --i)
	{
		Acc += FVector::Dist(Leader->Trail[i], Leader->Trail[i - 1]);
		if (Acc >= Spacing) { Goal = Leader->Trail[i - 1]; break; }
	}
	const FVector Right = FRotationMatrix(FRotator(0, Leader->GetActorRotation().Yaw, 0)).GetUnitAxis(EAxis::Y);
	Goal += Right * (SlotIndex == 1 ? -150.f : 150.f);
	const FVector ToGoal = Goal - GetActorLocation();
	const float Dist = ToGoal.Size2D();
	const float Floor = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	LowTime = Floor < -25.f ? LowTime + Dt : 0.f;
	if (Dist > 1500.f || LowTime > 1.f)
	{
		// trop loin ou coincé dans un creux : on rejoint la trace du chef
		SetActorLocation((LowTime > 1.f ? Goal - Right * (SlotIndex == 1 ? -150.f : 150.f) : Goal) + FVector(0, 0, 20), false, nullptr, ETeleportType::TeleportPhysics);
		LowTime = 0.f;
		return;
	}
	if (Dist > 60.f)
	{
		const float Speed = FMath::Clamp(Dist / 250.f, 0.4f, 1.f);
		AddMovementInput(ToGoal.GetSafeNormal2D(), Speed, true);
	}
}
