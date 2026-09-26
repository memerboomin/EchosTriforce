// Directeur de combat : fait tourner FZCombatSim et rejoue ses événements (animations, nombres, caméra).
// « La présentation reçoit les événements déjà calculés ; si l'animation est accélérée, le résultat reste identique. »
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZCombat.h"
#include "ZGameInstance.h"
#include "ZBattleDirector.generated.h"

class AZBattlePawn;
class ACameraActor;
class UZBattleHUD;
class UStaticMeshComponent;
class UPointLightComponent;

struct FZPopup
{
	FVector World;
	FString Text;
	FLinearColor Color;
	float Age = 0.f;
	float Size = 30.f;
	float Life = 1.2f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FZOnBattleEnded, EZSimState /*Outcome*/, FName /*EncounterId*/);

UCLASS()
class ECHOSTRIFORCE_API AZBattleDirector : public AActor
{
	GENERATED_BODY()

public:
	AZBattleDirector();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	void BeginBattle(FName EncounterId, const TArray<FName>& Enemies, const FVector& Center, const FRotator& Facing, bool bPreemptive);
	bool SubmitCommand(const FZCommand& Cmd, FString& OutReason);
	void ConfirmResult();   // fermer l'écran de victoire / défaite
	void RetryBattle();     // défaite : réessayer

	const FZCombatSim& GetSim() const { return Sim; }
	FZCombatSim& GetSimMutable() { return Sim; }
	bool IsAwaitingInput() const;
	int32 GetDisplayHP(int32 A) const { return DisplayHP.IsValidIndex(A) ? DisplayHP[A] : 0; }
	int32 GetDisplayMP(int32 A) const { return DisplayMP.IsValidIndex(A) ? DisplayMP[A] : 0; }
	int32 GetDisplayResonance() const { return DisplayResonance; }
	AZBattlePawn* GetPawn(int32 A) const { return Pawns.IsValidIndex(A) ? Pawns[A] : nullptr; }
	void SetTargetHighlight(const TArray<int32>& Targets);
	int32 GetFocusEnemy() const;

	FString Banner;          // nom de l'action en cours
	float BannerAge = 99.f;
	FString BigBanner;       // BRÈCHE, phase, invocation…
	float BigBannerAge = 99.f;
	FLinearColor BigBannerColor = FLinearColor::White;
	FName BigBannerImage;    // forme (concept art) affichée pendant la transformation
	TArray<FZPopup> Popups;
	TArray<FString> RecentLog;

	bool bShowingResult = false;
	EZSimState Outcome = EZSimState::Idle;
	FZVictoryReport Report;
	FName EncounterId;
	FZOnBattleEnded OnBattleEnded;

	UPROPERTY() UZBattleHUD* HUD = nullptr;

private:
	void SpawnPawns();
	/** Décalages caméra (repère local du combat : x avant, y droite, z hauteur). */
	void CameraRig(bool bBoss, FVector& OutBase, FVector& OutLook, float& OutFov) const;
	/** Orientation du combat la plus dégagée (caméra et emplacements sans mur) parmi 8 directions. */
	FRotator PickFacing(const FRotator& Preferred, bool bBoss) const;
	void PlayEvent(const FZEvent& E, float& OutDuration);
	void AddPopup(int32 Actor, const FString& Text, const FLinearColor& Color, float Size = 30.f, float Height = 1.f);
	void Shake(float Strength);
	void UpdateCamera(float Dt);
	void SpawnProjectile(int32 From, int32 To, const FLinearColor& Color, float Duration);
	void SpawnSpirit(const FLinearColor& Color, float Duration);
	void FinishBattle();
	float Speed() const;

	FZCombatSim Sim;
	TArray<FZEvent> Queue;
	float EventTimer = 0.f;
	float StartDelay = 1.0f;
	TArray<int32> DisplayHP, DisplayMP;
	int32 DisplayResonance = 0;
	TArray<int32> Highlighted;

	UPROPERTY() TArray<AZBattlePawn*> Pawns;
	UPROPERTY() ACameraActor* Camera = nullptr;
	UPROPERTY() TArray<UStaticMeshComponent*> Projectiles;
	UPROPERTY() UStaticMeshComponent* SpiritMesh = nullptr;
	UPROPERTY() UPointLightComponent* SpiritLight = nullptr;

	struct FProjectile { int32 Comp; FVector From, To; float T, Dur; };
	TArray<FProjectile> Flying;
	float SpiritT = -1.f, SpiritDur = 0.f;

	FVector Center = FVector::ZeroVector;
	FRotator Facing = FRotator::ZeroRotator;
	FVector CamBase = FVector::ZeroVector;
	FVector CamLook = FVector::ZeroVector;
	float ShakeAmt = 0.f;
	float Time = 0.f;
	TArray<FName> EnemyList;
	bool bPreemptiveStart = false;
	int32 LastActionActor = -1;
	FVector CamFocusOffset = FVector::ZeroVector;
	FVector CamFocusCurrent = FVector::ZeroVector;
};
