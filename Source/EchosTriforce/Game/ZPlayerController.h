// Contrôleur principal : titre → exploration ↔ combat, menus, interactions, sauvegarde, captures automatiques.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "ZCombat.h"
#include "ZPlayerController.generated.h"

class UZTitleWidget;
class UZExploreHUD;
class UZBattleHUD;
class UZMenuWidget;
class AZBattleDirector;
class AZExploreCharacter;
class AZFollower;
class AZCistern;
class AZInteractable;
class AZEncounterActor;
class ACameraActor;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class ECHOSTRIFORCE_API AZGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AZGameMode();
	virtual void StartPlay() override;
};

UCLASS()
class ECHOSTRIFORCE_API AZPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AZPlayerController();
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	void ShowTitle();
	void StartNewGame();
	void ContinueGame();
	void StartArena(FName EncounterId, const TArray<FName>& Enemies, FName Room, bool bArchivist);
	void EnterExplore(bool bAtCheckpoint);
	void StartBattle(FName EncounterId, const TArray<FName>& Enemies, const FVector& Near, bool bPreemptive);
	void OpenMenu(int32 Tab = 0);
	void CloseMenu();
	void StartBooth();

private:
	enum class EMode : uint8 { Title, Explore, Battle, Menu };
	void CreateInput();
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnInteract();
	/** Espace / A : parler ou interagir s'il y a quelque chose devant Link, sinon sauter. */
	void OnJump();
	void OnStrike();
	void OnMenu();
	void HandleBattleEnded(EZSimState Outcome, FName EncounterId);
	void DoInteract(AZInteractable* I);
	void UpdateExplore(float Dt);
	void SpawnParty(const FVector& At, float Yaw);
	void RefreshPartyLooks();
	void ClearWidgets();
	void SetUIFocus(class UUserWidget* W);
	void SetGameFocus();
	void RunAutomation(float Dt);

	EMode Mode = EMode::Title;
	bool bArena = false;
	FName ActiveEncounter;
	int32 CurrentRoom = INDEX_NONE;
	TArray<bool> Visited;
	TArray<int32> PuzzleProgress;

	UPROPERTY() UZTitleWidget* Title = nullptr;
	UPROPERTY() UZExploreHUD* ExploreHUD = nullptr;
	UPROPERTY() UZBattleHUD* BattleHUD = nullptr;
	UPROPERTY() UZMenuWidget* Menu = nullptr;
	UPROPERTY() AZBattleDirector* Director = nullptr;
	UPROPERTY() AZExploreCharacter* Hero = nullptr;
	UPROPERTY() TArray<AZFollower*> Followers;
	UPROPERTY() AZCistern* Cistern = nullptr;
	UPROPERTY() ACameraActor* TitleCam = nullptr;
	UPROPERTY() AZInteractable* Focused = nullptr;

	UPROPERTY() UInputMappingContext* IMC = nullptr;
	UPROPERTY() UInputAction* IA_Move = nullptr;
	UPROPERTY() UInputAction* IA_Look = nullptr;
	UPROPERTY() UInputAction* IA_Interact = nullptr;
	UPROPERTY() UInputAction* IA_Strike = nullptr;
	UPROPERTY() UInputAction* IA_Jump = nullptr;
	FVector TitleFrom = FVector::ZeroVector;
	FVector TitleTo = FVector::ZeroVector;
	UPROPERTY() UInputAction* IA_Menu = nullptr;

	// Automatisation (captures d'écran de vérification) : -ZAuto=... -ZShots=4,9 -ZQuitAt=12 -ZShotName=...
	float AutoTime = 0.f;
	TArray<float> ShotTimes;
	int32 ShotIndex = 0;
	float QuitAt = -1.f;
	bool bAutoPlay = false;
	FString ShotName;
	FString AutoScript;
	float TitleT = 0.f;
	float ExploreTime = 0.f;
	TArray<FString> AutoKeys;
	int32 KeyIndex = 0;
};
