// Types de données partagés : définitions chargées depuis Content/Data/*.json (voir Tools/build_game_data.py).
#pragma once

#include "CoreMinimal.h"
#include "ZTypes.generated.h"

UENUM(BlueprintType)
enum class EZElement : uint8
{
	Neutral, Fire, Water, Ice, Thunder, Wind, Earth, Light, Shadow,
	MAX UMETA(Hidden)
};
static constexpr int32 ZElementCount = (int32)EZElement::MAX;

UENUM(BlueprintType)
enum class EZStatus : uint8
{
	None,
	Wet, Burn, Freeze, Shock, Poison, Sleep, Silence, Blind, Taunt, Fragile, Mire,
	Haste, Slow, Guard, Barrier, Regen, MagicShield, Rampart, ZoraBarrier, SprUp, Evade,
	Stability, BreachStability, Exposed, Marked, Intercept, MirrorParry, MPCostUp, Disarm, Knockback,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EZSlot : uint8
{
	None, Weapon, Offhand, Head, Torso, Legs, Outfit, Accessory, Relic, Tool, Ammo, Traversal, Spell, Upgrade, Cosmetic
};

// Emplacements d'un personnage (chapitre 10)
UENUM(BlueprintType)
enum class EZEquipSlot : uint8
{
	Weapon, Offhand, Head, Torso, Legs, AccessoryA, AccessoryB, Relic, ToolA, ToolB,
	MAX UMETA(Hidden)
};
static constexpr int32 ZEquipSlotCount = (int32)EZEquipSlot::MAX;

UENUM(BlueprintType)
enum class EZAbilityKind : uint8
{
	Physical, Magical, Heal, Support, Revive, Passive, Summon
};

UENUM(BlueprintType)
enum class EZTarget : uint8
{
	Enemy, AllEnemies, Ally, AllAllies, Self, DeadAlly
};

USTRUCT(BlueprintType)
struct FZMod
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Stat;
	UPROPERTY(BlueprintReadOnly) float Value = 0.f;
};

USTRUCT(BlueprintType)
struct FZStatusApply
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) EZStatus Status = EZStatus::None;
	UPROPERTY(BlueprintReadOnly) float Chance = 1.f;
	UPROPERTY(BlueprintReadOnly) int32 Duration = 2;
	UPROPERTY(BlueprintReadOnly) float Magnitude = 0.f;
};

USTRUCT(BlueprintType)
struct FZItemDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Game;
	UPROPERTY(BlueprintReadOnly) FString GameName;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString NameFR;
	UPROPERTY(BlueprintReadOnly) FString Profile;
	UPROPERTY(BlueprintReadOnly) FString Category;
	UPROPERTY(BlueprintReadOnly) EZSlot Slot = EZSlot::None;
	UPROPERTY(BlueprintReadOnly) int32 Hands = 0;
	UPROPERTY(BlueprintReadOnly) FString Kind;
	UPROPERTY(BlueprintReadOnly) int32 Rank = 1;
	UPROPERTY(BlueprintReadOnly) int32 Power = 0;
	UPROPERTY(BlueprintReadOnly) int32 Focus = 0;
	UPROPERTY(BlueprintReadOnly) int32 Def = 0;
	UPROPERTY(BlueprintReadOnly) int32 MDef = 0;
	UPROPERTY(BlueprintReadOnly) EZElement Element = EZElement::Neutral;
	UPROPERTY(BlueprintReadOnly) TArray<FZMod> Mods;
	UPROPERTY(BlueprintReadOnly) TArray<FZMod> SetBonus;
	UPROPERTY(BlueprintReadOnly) FString SetFamily;
	UPROPERTY(BlueprintReadOnly) FName Teaches;
	UPROPERTY(BlueprintReadOnly) int32 TeachAP = 0;
	UPROPERTY(BlueprintReadOnly) FString Unlock;
	UPROPERTY(BlueprintReadOnly) FString UnlockText;
	UPROPERTY(BlueprintReadOnly) int32 GateChapter = 1;
	UPROPERTY(BlueprintReadOnly) FString Source;
	UPROPERTY(BlueprintReadOnly) FString Effect;
	UPROPERTY(BlueprintReadOnly) FString CanonNote;
	UPROPERTY(BlueprintReadOnly) FString Quest;
	UPROPERTY(BlueprintReadOnly) FString Special;
	UPROPERTY(BlueprintReadOnly) bool EffectModeled = true;
	UPROPERTY(BlueprintReadOnly) FString CanonicalIdentity;

	FString DisplayName() const { return NameFR.IsEmpty() ? Name : NameFR; }
	bool IsWearable() const
	{
		return Slot == EZSlot::Weapon || Slot == EZSlot::Offhand || Slot == EZSlot::Head || Slot == EZSlot::Torso
			|| Slot == EZSlot::Legs || Slot == EZSlot::Outfit || Slot == EZSlot::Accessory || Slot == EZSlot::Relic
			|| Slot == EZSlot::Tool;
	}
};

USTRUCT(BlueprintType)
struct FZAbilityDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString School;
	UPROPERTY(BlueprintReadOnly) int32 MP = 0;
	UPROPERTY(BlueprintReadOnly) int32 AP = 0;
	UPROPERTY(BlueprintReadOnly) EZAbilityKind Kind = EZAbilityKind::Physical;
	UPROPERTY(BlueprintReadOnly) EZTarget Target = EZTarget::Enemy;
	UPROPERTY(BlueprintReadOnly) float Power = 0.f;
	UPROPERTY(BlueprintReadOnly) EZElement Element = EZElement::Neutral;
	UPROPERTY(BlueprintReadOnly) int32 Hits = 1;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) FString Source;
	UPROPERTY(BlueprintReadOnly) FString Tier;
	UPROPERTY(BlueprintReadOnly) int32 Resonance = 0;
	UPROPERTY(BlueprintReadOnly) int32 Breach = 0;
	UPROPERTY(BlueprintReadOnly) float IgnoreDefPct = 0.f;
	UPROPERTY(BlueprintReadOnly) float ExecutePower = 0.f;
	UPROPERTY(BlueprintReadOnly) float RequireHPPct = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<FZStatusApply> Apply;
	UPROPERTY(BlueprintReadOnly) TArray<EZStatus> Remove;
	UPROPERTY(BlueprintReadOnly) int32 PerBattle = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetGauge = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetGaugeBoss = 0;
	UPROPERTY(BlueprintReadOnly) int32 SelfGauge = 0;
	UPROPERTY(BlueprintReadOnly) FString Requires;
	UPROPERTY(BlueprintReadOnly) FString Anim;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Flags;
	UPROPERTY(BlueprintReadOnly) float HealPct = 0.f;
	UPROPERTY(BlueprintReadOnly) float BarrierPct = 0.f;
	UPROPERTY(BlueprintReadOnly) float ReviveHPPct = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 ResonanceGain = 0;
	UPROPERTY(BlueprintReadOnly) bool Dispel = false;

	bool HasFlag(const TCHAR* Flag) const { return Flags.Contains(Flag); }
	FName PreparedFollowUp() const
	{
		for (const FString& F : Flags)
		{
			if (F.StartsWith(TEXT("Prepare:"))) { return FName(*F.Mid(8)); }
		}
		return NAME_None;
	}
	bool IsSilenceable() const { return Kind == EZAbilityKind::Magical || School == TEXT("Chant") || School == TEXT("Magic"); }
};

USTRUCT(BlueprintType)
struct FZFormDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString MaskItem;
	UPROPERTY(BlueprintReadOnly) int32 Resonance = 60;
	UPROPERTY(BlueprintReadOnly) float ATQ = 1.f;
	UPROPERTY(BlueprintReadOnly) float AMAG = 1.f;
	UPROPERTY(BlueprintReadOnly) float DEF = 1.f;
	UPROPERTY(BlueprintReadOnly) float AGI = 1.f;
	UPROPERTY(BlueprintReadOnly) TMap<FString, float> Taken;
	UPROPERTY(BlueprintReadOnly) TArray<FName> Commands;
	UPROPERTY(BlueprintReadOnly) float Scale = 1.f;
	UPROPERTY(BlueprintReadOnly) FString Color;
	UPROPERTY(BlueprintReadOnly) FString Weakness;
	UPROPERTY(BlueprintReadOnly) bool Late = false;
	UPROPERTY(BlueprintReadOnly) FString Flag;
	UPROPERTY(BlueprintReadOnly) bool ArenaOnly = false;
	UPROPERTY(BlueprintReadOnly) bool NoItems = false;
	UPROPERTY(BlueprintReadOnly) int32 Corruption = 0;
};

USTRUCT(BlueprintType)
struct FZSpiritDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Resonance = 60;
	UPROPERTY(BlueprintReadOnly) EZAbilityKind Kind = EZAbilityKind::Support;
	UPROPERTY(BlueprintReadOnly) EZTarget Target = EZTarget::AllAllies;
	UPROPERTY(BlueprintReadOnly) float Power = 0.f;
	UPROPERTY(BlueprintReadOnly) EZElement Element = EZElement::Neutral;
	UPROPERTY(BlueprintReadOnly) TArray<EZStatus> Remove;
	UPROPERTY(BlueprintReadOnly) TArray<FZStatusApply> Apply;
	UPROPERTY(BlueprintReadOnly) FString Desc;
	UPROPERTY(BlueprintReadOnly) FString Acquire;
	UPROPERTY(BlueprintReadOnly) FString Passive;
	UPROPERTY(BlueprintReadOnly) FString Color;
	UPROPERTY(BlueprintReadOnly) bool CancelPrepare = false;
	UPROPERTY(BlueprintReadOnly) float MPRestorePct = 0.f;
	UPROPERTY(BlueprintReadOnly) bool Dispel = false;
};

USTRUCT(BlueprintType)
struct FZDuoDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FName A;
	UPROPERTY(BlueprintReadOnly) FName B;
	UPROPERTY(BlueprintReadOnly) int32 Resonance = 40;
	UPROPERTY(BlueprintReadOnly) int32 MPA = 0;
	UPROPERTY(BlueprintReadOnly) int32 MPB = 0;
	UPROPERTY(BlueprintReadOnly) EZAbilityKind Kind = EZAbilityKind::Physical;
	UPROPERTY(BlueprintReadOnly) EZTarget Target = EZTarget::Enemy;
	UPROPERTY(BlueprintReadOnly) float Power = 1.f;
	UPROPERTY(BlueprintReadOnly) int32 Hits = 1;
	UPROPERTY(BlueprintReadOnly) EZElement Element = EZElement::Neutral;
	UPROPERTY(BlueprintReadOnly) TArray<FZStatusApply> Apply;
	UPROPERTY(BlueprintReadOnly) float HealPower = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Breach = 0;
	UPROPERTY(BlueprintReadOnly) FString Desc;
	UPROPERTY(BlueprintReadOnly) FName RequireForm;
	UPROPERTY(BlueprintReadOnly) int32 TargetGauge = 0;
};

USTRUCT(BlueprintType)
struct FZStatMult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float HP = 1.f;
	UPROPERTY(BlueprintReadOnly) float MP = 1.f;
	UPROPERTY(BlueprintReadOnly) float STR = 1.f;
	UPROPERTY(BlueprintReadOnly) float MAG = 1.f;
	UPROPERTY(BlueprintReadOnly) float VIT = 1.f;
	UPROPERTY(BlueprintReadOnly) float SPR = 1.f;
	UPROPERTY(BlueprintReadOnly) float AGI = 1.f;
	UPROPERTY(BlueprintReadOnly) float LCK = 1.f;
	UPROPERTY(BlueprintReadOnly) float DEF = 1.f;
};

USTRUCT(BlueprintType)
struct FZCharacterDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Role;
	UPROPERTY(BlueprintReadOnly) FZStatMult Mult;
	UPROPERTY(BlueprintReadOnly) TArray<FName> Abilities;
	UPROPERTY(BlueprintReadOnly) TMap<FString, FString> Equipment;
	UPROPERTY(BlueprintReadOnly) FString Color;
	UPROPERTY(BlueprintReadOnly) FString Recruit;
	UPROPERTY(BlueprintReadOnly) FString Portrait;
	UPROPERTY(BlueprintReadOnly) FString Identity;
	UPROPERTY(BlueprintReadOnly) FString Weapon;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Stances;
	UPROPERTY(BlueprintReadOnly) FString Lore;
};

USTRUCT(BlueprintType)
struct FZEnemyDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Family;
	UPROPERTY(BlueprintReadOnly) int32 Level = 1;
	UPROPERTY(BlueprintReadOnly) int32 HP = 100;
	UPROPERTY(BlueprintReadOnly) int32 MP = 0;
	UPROPERTY(BlueprintReadOnly) int32 ATQ = 10;
	UPROPERTY(BlueprintReadOnly) int32 AMAG = 10;
	UPROPERTY(BlueprintReadOnly) int32 DEF = 10;
	UPROPERTY(BlueprintReadOnly) int32 DFM = 10;
	UPROPERTY(BlueprintReadOnly) int32 AGI = 10;
	UPROPERTY(BlueprintReadOnly) int32 LCK = 5;
	UPROPERTY(BlueprintReadOnly) TMap<FString, float> Resist;
	UPROPERTY(BlueprintReadOnly) TMap<FString, float> StatusResist;
	UPROPERTY(BlueprintReadOnly) TArray<FName> Abilities;
	UPROPERTY(BlueprintReadOnly) FName AI;
	UPROPERTY(BlueprintReadOnly) int32 BreachThreshold = 100;
	UPROPERTY(BlueprintReadOnly) bool Elite = false;
	UPROPERTY(BlueprintReadOnly) bool Boss = false;
	UPROPERTY(BlueprintReadOnly) int32 XP = 0;
	UPROPERTY(BlueprintReadOnly) int32 AP = 3;
	UPROPERTY(BlueprintReadOnly) int32 Rupees = 0;
	UPROPERTY(BlueprintReadOnly) FString Color;
	UPROPERTY(BlueprintReadOnly) FString Mesh;
	UPROPERTY(BlueprintReadOnly) float Scale = 1.f;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) bool Spectre = false;
};

USTRUCT(BlueprintType)
struct FZEncounterDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) TArray<FName> Enemies;
	UPROPERTY(BlueprintReadOnly) int32 Room = 0;
	UPROPERTY(BlueprintReadOnly) bool Boss = false;
	UPROPERTY(BlueprintReadOnly) FString Tutorial;
};

USTRUCT(BlueprintType)
struct FZConsumableDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Desc;
	UPROPERTY(BlueprintReadOnly) EZTarget Target = EZTarget::Ally;
	UPROPERTY(BlueprintReadOnly) float HealHPPct = 0.f;
	UPROPERTY(BlueprintReadOnly) float HealMPPct = 0.f;
	UPROPERTY(BlueprintReadOnly) float Revive = 0.f;
	UPROPERTY(BlueprintReadOnly) TArray<EZStatus> Cleanse;
	UPROPERTY(BlueprintReadOnly) int32 Price = 0;
	UPROPERTY(BlueprintReadOnly) int32 Max = 10;
};

USTRUCT(BlueprintType)
struct FZPassiveDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Cost = 2;
	UPROPERTY(BlueprintReadOnly) int32 AP = 80;
	UPROPERTY(BlueprintReadOnly) TArray<FZMod> Mods;
	UPROPERTY(BlueprintReadOnly) FString Desc;
};

USTRUCT(BlueprintType)
struct FZQuestDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Chapter = 1;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Steps;
	UPROPERTY(BlueprintReadOnly) FString Reward;
};

// Wrappers racine des fichiers JSON
USTRUCT() struct FZItemFile { GENERATED_BODY() UPROPERTY() TArray<FZItemDef> Items; };
USTRUCT() struct FZAbilityFile { GENERATED_BODY() UPROPERTY() TArray<FZAbilityDef> Abilities; };
USTRUCT() struct FZFormFile { GENERATED_BODY() UPROPERTY() TArray<FZFormDef> Forms; };
USTRUCT() struct FZSpiritFile { GENERATED_BODY() UPROPERTY() TArray<FZSpiritDef> Spirits; };
USTRUCT() struct FZDuoFile { GENERATED_BODY() UPROPERTY() TArray<FZDuoDef> Duos; };
USTRUCT() struct FZCharacterFile { GENERATED_BODY() UPROPERTY() TArray<FZCharacterDef> Characters; };
USTRUCT() struct FZEnemyFile { GENERATED_BODY() UPROPERTY() TArray<FZEnemyDef> Enemies; };
USTRUCT() struct FZEncounterFile { GENERATED_BODY() UPROPERTY() TArray<FZEncounterDef> Encounters; };
USTRUCT() struct FZConsumableFile { GENERATED_BODY() UPROPERTY() TArray<FZConsumableDef> Consumables; };
USTRUCT() struct FZPassiveFile { GENERATED_BODY() UPROPERTY() TArray<FZPassiveDef> Passives; };
USTRUCT() struct FZQuestFile { GENERATED_BODY() UPROPERTY() TArray<FZQuestDef> Quests; };

// État persistant d'un personnage (sauvegardé)
USTRUCT(BlueprintType)
struct FZCharacterState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) FName Id;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 Level = 1;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 XP = 0; // XP totale
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FString> Equip; // indexé par EZEquipSlot
	UPROPERTY(SaveGame, BlueprintReadOnly) TMap<FName, int32> AbilityAP;
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FName> Prepared; // 6 techniques préparées
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FName> Passives;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName Spirit;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName Stance;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 CurrentHP = -1; // -1 = plein
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 CurrentMP = -1;

	const FString& GetEquip(EZEquipSlot S) const
	{
		static const FString Empty;
		const int32 I = (int32)S;
		return Equip.IsValidIndex(I) ? Equip[I] : Empty;
	}
	void SetEquip(EZEquipSlot S, const FString& ItemId)
	{
		if (Equip.Num() < ZEquipSlotCount) { Equip.SetNum(ZEquipSlotCount); }
		Equip[(int32)S] = ItemId;
	}
};

namespace ZNames
{
	ECHOSTRIFORCE_API FString ElementName(EZElement E);
	ECHOSTRIFORCE_API FString ElementIcon(EZElement E);
	ECHOSTRIFORCE_API FLinearColor ElementColor(EZElement E);
	ECHOSTRIFORCE_API EZElement ElementFromString(const FString& S);
	ECHOSTRIFORCE_API FString StatusName(EZStatus S);
	ECHOSTRIFORCE_API bool IsDebuff(EZStatus S);
	ECHOSTRIFORCE_API FString SlotName(EZEquipSlot S);
	ECHOSTRIFORCE_API FString ItemSlotName(EZSlot S);
	ECHOSTRIFORCE_API FLinearColor HexColor(const FString& Hex, const FLinearColor& Fallback = FLinearColor::Gray);
	ECHOSTRIFORCE_API FString FormatInt(int64 Value); // « 2 800 »
}
