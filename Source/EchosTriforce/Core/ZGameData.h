// Base de données du jeu : lit Content/Data/*.json une fois (singleton), utilisable sans monde (tests).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZTypes.h"
#include "ZGameData.generated.h"

UCLASS()
class ECHOSTRIFORCE_API UZGameData : public UObject
{
	GENERATED_BODY()

public:
	static UZGameData& Get();

	bool LoadAll(const FString& DataDir, FString& OutError);
	bool IsLoaded() const { return bLoaded; }

	const FZItemDef* FindItem(const FString& Id) const;
	const FZAbilityDef* FindAbility(FName Id) const;
	const FZFormDef* FindForm(FName Id) const;
	const FZSpiritDef* FindSpirit(FName Id) const;
	const FZDuoDef* FindDuo(FName Id) const;
	const FZCharacterDef* FindCharacter(FName Id) const;
	const FZEnemyDef* FindEnemy(FName Id) const;
	const FZEncounterDef* FindEncounter(FName Id) const;
	const FZConsumableDef* FindConsumable(FName Id) const;
	const FZPassiveDef* FindPassive(FName Id) const;

	UPROPERTY() TArray<FZItemDef> Items;
	UPROPERTY() TArray<FZAbilityDef> Abilities;
	UPROPERTY() TArray<FZFormDef> Forms;
	UPROPERTY() TArray<FZSpiritDef> Spirits;
	UPROPERTY() TArray<FZDuoDef> Duos;
	UPROPERTY() TArray<FZCharacterDef> Characters;
	UPROPERTY() TArray<FZEnemyDef> Enemies;
	UPROPERTY() TArray<FZEncounterDef> Encounters;
	UPROPERTY() TArray<FZConsumableDef> Consumables;
	UPROPERTY() TArray<FZPassiveDef> Passives;
	UPROPERTY() TArray<FZQuestDef> Quests;

	/** Jeux présents dans le catalogue, dans l'ordre chronologique. */
	TArray<FString> GameOrder;

private:
	template <typename TFile>
	bool LoadFile(const FString& Path, TFile& Out, FString& OutError);

	bool bLoaded = false;
	TMap<FString, int32> ItemIndex;
	TMap<FName, int32> AbilityIndex, FormIndex, SpiritIndex, DuoIndex, CharIndex, EnemyIndex, EncounterIndex, ConsumableIndex, PassiveIndex;
};
