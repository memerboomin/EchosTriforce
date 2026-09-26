// Croissance (chapitre 7), apprentissage (8) et calcul de l'équipement (10-11).
#pragma once

#include "CoreMinimal.h"
#include "ZTypes.h"

struct FZBaseStats
{
	int32 HP = 0, MP = 0, STR = 0, MAG = 0, VIT = 0, SPR = 0, AGI = 0, LCK = 0;
};

/** Statistiques finales d'un personnage équipé, avec leur décomposition pour l'UI. */
struct ECHOSTRIFORCE_API FZDerivedStats
{
	int32 MaxHP = 1, MaxMP = 0;
	float STR = 0, MAG = 0, VIT = 0, SPR = 0, AGI = 0, LCK = 0;
	float ATQ = 0, AMAG = 0, DEF = 0, DFM = 0;
	float WeaponPower = 0, Focus = 0, EquipDef = 0, EquipMDef = 0;
	float Crit = 0, Evade = 0;
	float Resist[ZElementCount] = {};
	TSet<EZStatus> Immune;
	TMap<FName, float> Special; // DMG_*, TAKEN_*, HEAL_*, etc.
	FString WeaponKind;
	EZElement WeaponElement = EZElement::Neutral;
	bool bTwoHanded = false;
	bool bShield = false;
	bool bSetComplete = false;
	TArray<FName> GrantedAbilities; // enseignées par l'équipement porté
	TArray<FString> Notes;          // lignes de décomposition affichées dans le menu Statut

	float GetSpecial(FName Key) const { const float* V = Special.Find(Key); return V ? *V : 0.f; }
};

namespace ZProgression
{
	ECHOSTRIFORCE_API int32 XPToNext(int32 Level);
	ECHOSTRIFORCE_API int32 TotalXPForLevel(int32 Level);
	ECHOSTRIFORCE_API int32 LevelForXP(int32 TotalXP);
	ECHOSTRIFORCE_API FZBaseStats LinkCurve(int32 Level);
	ECHOSTRIFORCE_API FZBaseStats CharacterBase(const FZCharacterDef& Def, int32 Level);
	ECHOSTRIFORCE_API int32 EnemyXPReward(int32 Level);
	ECHOSTRIFORCE_API int32 PassiveCapacity(int32 Level);
	ECHOSTRIFORCE_API int32 RankPower(int32 Rank);

	/** Calcule les statistiques d'un personnage équipé (formules du chapitre 5). */
	ECHOSTRIFORCE_API FZDerivedStats ComputeDerived(const FZCharacterState& State);

	/** Techniques utilisables : kit du personnage + équipement porté + techniques maîtrisées. */
	ECHOSTRIFORCE_API TArray<FName> AvailableAbilities(const FZCharacterState& State, bool bIncludePassives = false);
	ECHOSTRIFORCE_API bool IsMastered(const FZCharacterState& State, FName AbilityId);
	ECHOSTRIFORCE_API int32 PassiveCostUsed(const FZCharacterState& State);

	/** Peut-on placer cet objet dans cet emplacement ? (transaction validée sur l'ensemble final, chapitre 23) */
	ECHOSTRIFORCE_API bool CanEquip(const FZCharacterState& State, EZEquipSlot Slot, const FZItemDef& Item, FString& OutReason);
	/** Applique la transaction : gère deux mains, tenue monobloc, doublons d'accessoires. */
	ECHOSTRIFORCE_API bool Equip(FZCharacterState& State, EZEquipSlot Slot, const FString& ItemId, FString& OutReason);
	ECHOSTRIFORCE_API void Unequip(FZCharacterState& State, EZEquipSlot Slot);
	ECHOSTRIFORCE_API bool SlotAccepts(EZEquipSlot Slot, EZSlot ItemSlot);

	/** Crée l'état initial d'un personnage au niveau donné avec son équipement de départ. */
	ECHOSTRIFORCE_API FZCharacterState MakeCharacter(FName Id, int32 Level);
	/** Remplit les 6 emplacements préparés avec les techniques disponibles. */
	ECHOSTRIFORCE_API void AutoPrepare(FZCharacterState& State);
}
