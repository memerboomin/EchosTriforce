#include "ZGameData.h"
#include "EchosTriforce.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UZGameData& UZGameData::Get()
{
	static UZGameData* Instance = nullptr;
	if (!Instance)
	{
		Instance = NewObject<UZGameData>(GetTransientPackage(), TEXT("ZGameData"));
		Instance->AddToRoot();
		FString Error;
		const FString Dir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
		if (!Instance->LoadAll(Dir, Error))
		{
			UE_LOG(LogEchos, Error, TEXT("Chargement des données impossible : %s"), *Error);
		}
	}
	return *Instance;
}

template <typename TFile>
bool UZGameData::LoadFile(const FString& Path, TFile& Out, FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = FString::Printf(TEXT("fichier introuvable : %s"), *Path);
		return false;
	}
	FText Reason;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Out, 0, 0, false, &Reason))
	{
		OutError = FString::Printf(TEXT("JSON invalide %s : %s"), *Path, *Reason.ToString());
		return false;
	}
	return true;
}

bool UZGameData::LoadAll(const FString& DataDir, FString& OutError)
{
	FZItemFile ItemFile; FZAbilityFile AbilityFile; FZFormFile FormFile; FZSpiritFile SpiritFile; FZDuoFile DuoFile;
	FZCharacterFile CharFile; FZEnemyFile EnemyFile; FZEncounterFile EncFile; FZConsumableFile ConsFile; FZPassiveFile PassFile;
	FZQuestFile QuestFile;

	auto P = [&](const TCHAR* Name) { return FPaths::Combine(DataDir, Name); };
	if (!LoadFile(P(TEXT("items.json")), ItemFile, OutError)) return false;
	if (!LoadFile(P(TEXT("abilities.json")), AbilityFile, OutError)) return false;
	if (!LoadFile(P(TEXT("forms.json")), FormFile, OutError)) return false;
	if (!LoadFile(P(TEXT("spirits.json")), SpiritFile, OutError)) return false;
	if (!LoadFile(P(TEXT("duos.json")), DuoFile, OutError)) return false;
	if (!LoadFile(P(TEXT("characters.json")), CharFile, OutError)) return false;
	if (!LoadFile(P(TEXT("enemies.json")), EnemyFile, OutError)) return false;
	if (!LoadFile(P(TEXT("encounters.json")), EncFile, OutError)) return false;
	if (!LoadFile(P(TEXT("consumables.json")), ConsFile, OutError)) return false;
	if (!LoadFile(P(TEXT("passives.json")), PassFile, OutError)) return false;
	if (!LoadFile(P(TEXT("quests.json")), QuestFile, OutError)) return false;

	Items = MoveTemp(ItemFile.Items);
	Abilities = MoveTemp(AbilityFile.Abilities);
	Forms = MoveTemp(FormFile.Forms);
	Spirits = MoveTemp(SpiritFile.Spirits);
	Duos = MoveTemp(DuoFile.Duos);
	Characters = MoveTemp(CharFile.Characters);
	Enemies = MoveTemp(EnemyFile.Enemies);
	Encounters = MoveTemp(EncFile.Encounters);
	Consumables = MoveTemp(ConsFile.Consumables);
	Passives = MoveTemp(PassFile.Passives);
	Quests = MoveTemp(QuestFile.Quests);

	ItemIndex.Reset();
	GameOrder.Reset();
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (ItemIndex.Contains(Items[i].Id))
		{
			OutError = FString::Printf(TEXT("identifiant d'objet dupliqué : %s"), *Items[i].Id);
			return false;
		}
		ItemIndex.Add(Items[i].Id, i);
		GameOrder.AddUnique(Items[i].Game);
	}
	auto Index = [](auto& Array, TMap<FName, int32>& Map)
	{
		Map.Reset();
		for (int32 i = 0; i < Array.Num(); ++i) { Map.Add(Array[i].Id, i); }
	};
	Index(Abilities, AbilityIndex);
	Index(Forms, FormIndex);
	Index(Spirits, SpiritIndex);
	Index(Duos, DuoIndex);
	Index(Characters, CharIndex);
	Index(Enemies, EnemyIndex);
	Index(Encounters, EncounterIndex);
	Index(Consumables, ConsumableIndex);
	Index(Passives, PassiveIndex);

	// Validation de contenu au chargement (chapitre 23)
	for (const FZItemDef& It : Items)
	{
		if (It.Rank < 1 || It.Rank > 5)
		{
			OutError = FString::Printf(TEXT("rang hors bornes pour %s"), *It.Id);
			return false;
		}
		if (!It.Teaches.IsNone() && !AbilityIndex.Contains(It.Teaches) && !PassiveIndex.Contains(It.Teaches))
		{
			OutError = FString::Printf(TEXT("technique inconnue %s enseignée par %s"), *It.Teaches.ToString(), *It.Id);
			return false;
		}
	}
	for (const FZEnemyDef& E : Enemies)
	{
		for (FName A : E.Abilities)
		{
			if (!AbilityIndex.Contains(A))
			{
				OutError = FString::Printf(TEXT("action inconnue %s pour l'ennemi %s"), *A.ToString(), *E.Id.ToString());
				return false;
			}
		}
	}

	bLoaded = true;
	UE_LOG(LogEchos, Log, TEXT("Données chargées : %d objets, %d techniques, %d formes, %d esprits, %d personnages, %d ennemis"),
		Items.Num(), Abilities.Num(), Forms.Num(), Spirits.Num(), Characters.Num(), Enemies.Num());
	return true;
}

#define ZFIND(Func, Type, Key, IndexMap, Array) \
	const Type* UZGameData::Func(Key Id) const { const int32* I = IndexMap.Find(Id); return I ? &Array[*I] : nullptr; }

ZFIND(FindItem, FZItemDef, const FString&, ItemIndex, Items)
ZFIND(FindAbility, FZAbilityDef, FName, AbilityIndex, Abilities)
ZFIND(FindForm, FZFormDef, FName, FormIndex, Forms)
ZFIND(FindSpirit, FZSpiritDef, FName, SpiritIndex, Spirits)
ZFIND(FindDuo, FZDuoDef, FName, DuoIndex, Duos)
ZFIND(FindCharacter, FZCharacterDef, FName, CharIndex, Characters)
ZFIND(FindEnemy, FZEnemyDef, FName, EnemyIndex, Enemies)
ZFIND(FindEncounter, FZEncounterDef, FName, EncounterIndex, Encounters)
ZFIND(FindConsumable, FZConsumableDef, FName, ConsumableIndex, Consumables)
ZFIND(FindPassive, FZPassiveDef, FName, PassiveIndex, Passives)

#undef ZFIND
