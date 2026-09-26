// État persistant de la partie : équipe, inventaire, collection, drapeaux, sauvegarde (chapitres 18-20).
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "ZTypes.h"
#include "ZCombat.h"
#include "ZGameInstance.generated.h"

UCLASS()
class ECHOSTRIFORCE_API UZSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 SchemaVersion = 1;
	UPROPERTY() TArray<FZCharacterState> Party;
	UPROPERTY() TArray<FName> ActiveParty;
	UPROPERTY() TMap<FString, int32> OwnedItems;
	UPROPERTY() TMap<FName, int32> Consumables;
	UPROPERTY() TArray<FName> Flags;
	UPROPERTY() TArray<FName> ClaimedRewards;
	UPROPERTY() int32 Rupees = 0;
	UPROPERTY() int32 Bombs = 10;
	UPROPERTY() int32 Resonance = 0;
	UPROPERTY() int32 Chapter = 2;
	UPROPERTY() FString Checkpoint;
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() float PlayTime = 0.f;
};

/** Rapport de fin de combat pour l'écran de victoire. */
struct FZVictoryReport
{
	int32 XP = 0, AP = 0, Rupees = 0;
	TArray<FString> Lines; // « Link passe au niveau 11 ! », « Mipha maîtrise Taillade »…
};

UCLASS()
class ECHOSTRIFORCE_API UZGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	static UZGameInstance* Get(const UObject* WorldContext);

	/** Nouvelle partie : prototype « Citerne des mémoires » (chapitre 17) : Link, Sheik, Mipha niveau 10, équipement rang 2. */
	void NewGame();

	// --- Équipe
	FZCharacterState* FindMember(FName Id);
	const FZCharacterState* FindMember(FName Id) const;
	TArray<FZCharacterState*> GetActiveParty();
	FZDerivedStats GetDerived(FName Id) const;
	void RecruitMember(FName Id);
	void RestoreParty(); // repos : PV/PM pleins

	// --- Collection
	bool Owns(const FString& ItemId) const { return OwnedItems.Contains(ItemId); }
	void GrantItem(const FString& ItemId, int32 Count = 1);
	int32 CountOwned() const { return OwnedItems.Num(); }
	/** Nombre de personnages qui portent cet objet (pour limiter au nombre d'exemplaires). */
	int32 CountEquipped(const FString& ItemId) const;
	bool IsItemUnlocked(const FString& ItemId) const { return Owns(ItemId); }
	TArray<FName> UnlockedForms() const;

	// --- Combat
	void SetupBattle(FZCombatSim& Sim, const TArray<FName>& Enemies, bool bPreemptive);
	FZVictoryReport ApplyBattleResult(const FZCombatSim& Sim);
	/** Sauvegarde PV/PM courants après un combat (défaite ou fuite comprises). */
	void SyncFromBattle(const FZCombatSim& Sim);

	// --- Drapeaux et récompenses uniques
	bool HasFlag(FName F) const { return Flags.Contains(F); }
	void SetFlag(FName F) { Flags.AddUnique(F); }
	bool ClaimReward(FName RewardId); // faux si déjà obtenue (pas de duplication au rechargement)

	// --- Sauvegarde atomique
	bool SaveToSlot(int32 Slot = 0);
	bool LoadFromSlot(int32 Slot = 0);
	bool HasSave(int32 Slot = 0) const;

	// --- Mode test : tout le catalogue devient possédé (collection complète)
	void UnlockEverything();

	// --- Données
	UPROPERTY() TArray<FZCharacterState> Party;
	UPROPERTY() TArray<FName> ActiveParty;
	UPROPERTY() TMap<FString, int32> OwnedItems;
	UPROPERTY() TMap<FName, int32> Consumables;
	UPROPERTY() TArray<FName> Flags;
	UPROPERTY() TArray<FName> ClaimedRewards;
	UPROPERTY() int32 Rupees = 0;
	UPROPERTY() int32 Bombs = 10;
	UPROPERTY() int32 Resonance = 0;
	UPROPERTY() int32 Chapter = 2;
	UPROPERTY() FString Checkpoint;
	UPROPERTY() float PlayTime = 0.f;

	// --- Options (chapitre 21)
	float BattleSpeed = 1.5f;   // multiplicateur de remplissage des jauges
	float AnimSpeed = 1.f;      // 1× / 2×
	float DifficultyDamage = 1.f; // Histoire 0,7 / Standard 1 / Héroïque 1,2
	int32 TextScale = 100;
	bool bHighContrast = false;
	bool bArchivistMode = false;
	TArray<FString> RecentItems;
};
