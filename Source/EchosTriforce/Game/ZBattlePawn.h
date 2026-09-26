// Représentation visuelle d'un combattant : lit les ordres du directeur de combat, ne calcule rien.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZVisuals.h"
#include "ZBattlePawn.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
struct FZBattler;

UCLASS()
class ECHOSTRIFORCE_API AZBattlePawn : public AActor
{
	GENERATED_BODY()

public:
	AZBattlePawn();
	virtual void Tick(float DeltaSeconds) override;

	void InitAlly(const FZBattler& B, const FString& Outfit, const FString& Weapon, const FString& Offhand);
	void InitEnemy(const FZBattler& B);
	void SetHome(const FVector& Loc, const FRotator& Rot);

	/** Action : fente vers la cible (corps à corps) ou incantation sur place. */
	void PlayAction(const FString& Anim, const FVector& TargetLoc, float Duration);
	void ReturnHome(float Duration);
	void PlayHit(bool bHeavy);
	void PlayDeath();
	void PlayRevive();
	void PlayVictory();
	void SetFormVisual(FName Form);
	void SetGlow(bool bOn, const FLinearColor& Color, float Intensity = 8000.f);
	void SetSelected(bool bOn);
	void SetGuarding(bool bOn) { bGuarding = bOn; }

	FVector GetFocusLocation() const;
	FVector GetTopLocation() const;
	float GetVisualHeight() const { return Look.Height * Look.Scale * GetActorScale3D().Z; }
	bool IsBusy() const { return MoveT < MoveDur; }

	UPROPERTY(VisibleAnywhere) USceneComponent* Root;
	UPROPERTY(VisibleAnywhere) USceneComponent* Visual;
	UPROPERTY(VisibleAnywhere) USkeletalMeshComponent* Body;
	UPROPERTY(VisibleAnywhere) UPointLightComponent* Glow;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Ring;
	UPROPERTY() TArray<UStaticMeshComponent*> Parts;

	/** Studio photo : joue une animation en boucle (nom ZVis::Anim). */
	void PreviewAnim(const FString& Name) { PlayLoop(Name); }

	FName CharId;
	bool bAllySide = true;
	FString Outfit, Weapon, Offhand;

private:
	void RebuildLook();
	/** Animation d'arme (Tools/blender/make_anims.py) équivalente pour les alliés, si elle existe. */
	FString AnimFor(const FString& Name) const;
	void PlayLoop(const FString& Name);
	void PlayOnce(const FString& Name, float BlendBackAfter);
	/** Lecteur natif des personnages extraits de Twilight Princess (fondus), nullptr sinon. */
	class UZTPAnimInstance* TPAnim() const;

	FZLook Look;
	FName CurrentForm;
	FVector Home = FVector::ZeroVector;
	FRotator HomeRot = FRotator::ZeroRotator;
	FVector MoveFrom = FVector::ZeroVector, MoveTo = FVector::ZeroVector;
	float MoveT = 1.f, MoveDur = 0.f;
	bool bReturning = false;
	float HitT = 10.f;
	float HitStrength = 1.f;
	float DeathT = -1.f;
	float Time = 0.f;
	float IdleBackTimer = -1.f;
	float SelectedPulse = 0.f;
	bool bSelected = false;
	bool bDead = false;
	bool bGuarding = false;
	bool bHumanoid = true;
	bool bCalmIdle = false; // après la victoire : épée rangée, attente normale
	float SheatheT = -1.f;  // moment où l'épée rejoint le fourreau pendant la pose de victoire
};
