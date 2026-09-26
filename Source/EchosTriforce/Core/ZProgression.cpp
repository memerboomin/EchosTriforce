#include "ZProgression.h"
#include "ZGameData.h"

namespace ZProgression
{
	int32 XPToNext(int32 L)
	{
		return 50 + 25 * L + 10 * L * L;
	}

	int32 TotalXPForLevel(int32 N)
	{
		int32 Total = 0;
		for (int32 L = 1; L < N; ++L) { Total += XPToNext(L); }
		return Total;
	}

	int32 LevelForXP(int32 TotalXP)
	{
		int32 Level = 1;
		int32 Acc = 0;
		while (Level < 99 && Acc + XPToNext(Level) <= TotalXP)
		{
			Acc += XPToNext(Level);
			++Level;
		}
		return Level;
	}

	FZBaseStats LinkCurve(int32 Level)
	{
		const int32 x = FMath::Clamp(Level, 1, 99) - 1;
		FZBaseStats S;
		S.HP = FMath::Min(9999, (int32)FMath::FloorToDouble(180.0 + 42.0 * x + 0.6 * x * x));
		S.MP = FMath::Min(500, 25 + 4 * x);
		S.STR = 12 + 2 * x;
		S.MAG = 10 + 2 * x;
		S.VIT = 10 + (int32)FMath::FloorToDouble(1.5 * x);
		S.SPR = S.VIT;
		S.AGI = 12 + x / 4;
		S.LCK = 5 + x / 6;
		return S;
	}

	FZBaseStats CharacterBase(const FZCharacterDef& Def, int32 Level)
	{
		const FZBaseStats L = LinkCurve(Level);
		const FZStatMult& M = Def.Mult;
		auto F = [](int32 V, float Mult) { return (int32)FMath::FloorToDouble((double)V * Mult + 1e-9); };
		FZBaseStats S;
		S.HP = FMath::Min(9999, F(L.HP, M.HP));
		S.MP = FMath::Min(500, F(L.MP, M.MP));
		S.STR = FMath::Min(255, F(L.STR, M.STR));
		S.MAG = FMath::Min(255, F(L.MAG, M.MAG));
		S.VIT = FMath::Min(255, F(L.VIT, M.VIT));
		S.SPR = FMath::Min(255, F(L.SPR, M.SPR));
		S.AGI = FMath::Min(80, F(L.AGI, M.AGI));
		S.LCK = FMath::Min(50, F(L.LCK, M.LCK));
		return S;
	}

	int32 EnemyXPReward(int32 Level)
	{
		return XPToNext(Level) / 12;
	}

	int32 PassiveCapacity(int32 Level)
	{
		return FMath::Min(30, 6 + (Level - 1) / 3);
	}

	int32 RankPower(int32 Rank)
	{
		static const int32 Powers[] = { 0, 12, 24, 38, 54, 74 };
		return Powers[FMath::Clamp(Rank, 1, 5)];
	}

	static int32 NarrativeRankForLevel(int32 Level)
	{
		if (Level < 8) return 1;
		if (Level < 22) return 2;
		if (Level < 35) return 3;
		if (Level < 48) return 4;
		return 5;
	}

	static FString AccessoryFamily(const FZItemDef& Item)
	{
		// « Power Ring L-1 / L-2 / L-3 » : une famille ne cumule que son meilleur rang
		FString Id = Item.CanonicalIdentity.IsEmpty() ? Item.Name.ToLower() : Item.CanonicalIdentity;
		int32 Pos;
		if (Id.FindLastChar(TEXT(' '), Pos))
		{
			const FString Tail = Id.Mid(Pos + 1);
			if (Tail.StartsWith(TEXT("l-")) || Tail.IsNumeric()) { Id = Id.Left(Pos); }
		}
		return Id;
	}

	FZDerivedStats ComputeDerived(const FZCharacterState& State)
	{
		const UZGameData& DB = UZGameData::Get();
		FZDerivedStats D;
		const FZCharacterDef* Def = DB.FindCharacter(State.Id);
		if (!Def) { return D; }
		const FZBaseStats B = CharacterBase(*Def, State.Level);

		TMap<FName, float> Sum;
		auto AddMods = [&Sum](const TArray<FZMod>& Mods)
		{
			for (const FZMod& M : Mods) { Sum.FindOrAdd(M.Stat) += M.Value; }
		};

		// Arme + main secondaire
		const FZItemDef* Weapon = DB.FindItem(State.GetEquip(EZEquipSlot::Weapon));
		if (Weapon)
		{
			D.WeaponPower = Weapon->Power;
			D.Focus = Weapon->Focus;
			D.WeaponKind = Weapon->Kind;
			D.WeaponElement = Weapon->Element;
			D.bTwoHanded = Weapon->Hands >= 2;
			AddMods(Weapon->Mods);
			if (!Weapon->Teaches.IsNone()) { D.GrantedAbilities.AddUnique(Weapon->Teaches); }
			D.Notes.Add(FString::Printf(TEXT("Arme : %s (puissance %d)"), *Weapon->DisplayName(), Weapon->Power));
		}
		else
		{
			// Arme intrinsèque des compagnons (aiguilles, magie) : puissance du rang narratif
			D.WeaponPower = RankPower(NarrativeRankForLevel(State.Level));
			D.WeaponKind = Def->Weapon;
		}
		if (const FZItemDef* Off = DB.FindItem(State.GetEquip(EZEquipSlot::Offhand)))
		{
			if (!D.bTwoHanded)
			{
				D.EquipDef += Off->Def;
				D.bShield = Off->Kind == TEXT("Shield");
				AddMods(Off->Mods);
				if (!Off->Teaches.IsNone()) { D.GrantedAbilities.AddUnique(Off->Teaches); }
				D.Notes.Add(FString::Printf(TEXT("Bouclier : %s (DEF +%d)"), *Off->DisplayName(), Off->Def));
			}
		}

		// Tête / torse / jambes (une tenue monobloc occupe les trois et ne compte qu'une fois)
		TArray<const FZItemDef*> Armor;
		for (EZEquipSlot S : { EZEquipSlot::Head, EZEquipSlot::Torso, EZEquipSlot::Legs })
		{
			if (const FZItemDef* It = DB.FindItem(State.GetEquip(S))) { Armor.AddUnique(It); }
		}
		for (const FZItemDef* It : Armor)
		{
			D.EquipDef += It->Def;
			D.EquipMDef += It->MDef;
			AddMods(It->Mods);
			if (!It->Teaches.IsNone()) { D.GrantedAbilities.AddUnique(It->Teaches); }
			for (const FZMod& M : It->Mods)
			{
				if (M.Stat.ToString().StartsWith(TEXT("RES_")))
				{
					const EZElement E = ZNames::ElementFromString(M.Stat.ToString().Mid(4));
					D.Notes.Add(FString::Printf(TEXT("%s : %s %+d %% de résistance"), *It->DisplayName(), *ZNames::ElementName(E), FMath::RoundToInt(M.Value * 100.f)));
				}
			}
		}
		if (Armor.Num() == 1 && Armor[0]->Slot == EZSlot::Outfit)
		{
			AddMods(Armor[0]->SetBonus);
			D.bSetComplete = true;
		}
		else if (Armor.Num() == 3 && !Armor[0]->SetFamily.IsEmpty()
			&& Armor[0]->SetFamily == Armor[1]->SetFamily && Armor[1]->SetFamily == Armor[2]->SetFamily)
		{
			AddMods(Armor[1]->SetBonus);
			D.bSetComplete = true;
			D.Notes.Add(TEXT("Bonus d'ensemble complet actif"));
		}

		// Accessoires A / B : une famille ne cumule que son meilleur rang
		const FZItemDef* AccA = DB.FindItem(State.GetEquip(EZEquipSlot::AccessoryA));
		const FZItemDef* AccB = DB.FindItem(State.GetEquip(EZEquipSlot::AccessoryB));
		if (AccA && AccB && AccessoryFamily(*AccA) == AccessoryFamily(*AccB))
		{
			if (AccB->Rank > AccA->Rank) { AccA = AccB; }
			AccB = nullptr;
			D.Notes.Add(TEXT("Accessoires de même famille : seul le meilleur rang compte"));
		}
		for (const FZItemDef* Acc : { AccA, AccB })
		{
			if (!Acc) continue;
			AddMods(Acc->Mods);
			if (!Acc->Teaches.IsNone()) { D.GrantedAbilities.AddUnique(Acc->Teaches); }
		}
		if (const FZItemDef* Relic = DB.FindItem(State.GetEquip(EZEquipSlot::Relic)))
		{
			AddMods(Relic->Mods);
			if (!Relic->Teaches.IsNone()) { D.GrantedAbilities.AddUnique(Relic->Teaches); }
		}
		for (EZEquipSlot S : { EZEquipSlot::ToolA, EZEquipSlot::ToolB })
		{
			if (const FZItemDef* Tool = DB.FindItem(State.GetEquip(S)))
			{
				if (!Tool->Teaches.IsNone()) { D.GrantedAbilities.AddUnique(Tool->Teaches); }
			}
		}

		// Passifs maîtrisés et équipés
		for (FName P : State.Passives)
		{
			const FZPassiveDef* PD = DB.FindPassive(P);
			if (PD && IsMastered(State, P)) { AddMods(PD->Mods); }
		}

		auto Get = [&Sum](const TCHAR* K) { const float* V = Sum.Find(FName(K)); return V ? *V : 0.f; };
		auto Pct = [&Get](const TCHAR* K) { return FMath::Clamp(Get(K), -0.5f, 0.4f); };

		D.STR = FMath::Min(255.f, (B.STR + Get(TEXT("STR"))) * (1.f + Pct(TEXT("STR_PCT"))));
		D.MAG = FMath::Min(255.f, (float)B.MAG * (1.f + Pct(TEXT("MAG_PCT"))));
		D.VIT = FMath::Min(255.f, (float)B.VIT);
		D.SPR = FMath::Min(255.f, B.SPR + Get(TEXT("SPR")));
		D.AGI = FMath::Clamp(B.AGI + Get(TEXT("AGI")), 1.f, 80.f);
		D.LCK = FMath::Min(50.f, (float)B.LCK);
		D.MaxHP = FMath::Clamp((int32)FMath::FloorToDouble(B.HP * (1.0 + FMath::Clamp(Get(TEXT("HP_PCT")), -0.5f, 0.5f)) + 1e-9), 1, 9999);
		D.MaxMP = FMath::Clamp((int32)FMath::FloorToDouble(B.MP * (1.0 + FMath::Clamp(Get(TEXT("MP_PCT")), -0.5f, 0.5f)) + 1e-9), 0, 500);

		D.ATQ = 2.f * FMath::FloorToFloat(D.STR) + D.WeaponPower;
		D.AMAG = 2.f * FMath::FloorToFloat(D.MAG) + D.Focus;
		D.DEF = FMath::FloorToFloat((D.VIT + D.EquipDef) * Def->Mult.DEF * (1.f + Pct(TEXT("DEF_PCT"))));
		D.DFM = FMath::FloorToFloat((D.SPR + D.EquipMDef) * (1.f + Pct(TEXT("MDEF_PCT"))));
		D.Crit = Get(TEXT("CRIT"));
		D.Evade = Get(TEXT("EVADE"));

		for (int32 e = 0; e < ZElementCount; ++e)
		{
			const FString Key = FString(TEXT("RES_")) + StaticEnum<EZElement>()->GetNameStringByValue(e);
			D.Resist[e] = FMath::Clamp(Get(*Key), -0.5f, 0.75f);
		}

		static const TCHAR* Consumed[] = { TEXT("STR"), TEXT("STR_PCT"), TEXT("MAG_PCT"), TEXT("SPR"), TEXT("AGI"), TEXT("HP_PCT"),
			TEXT("MP_PCT"), TEXT("DEF_PCT"), TEXT("MDEF_PCT"), TEXT("CRIT"), TEXT("EVADE") };
		for (const TPair<FName, float>& KV : Sum)
		{
			const FString K = KV.Key.ToString();
			if (K.StartsWith(TEXT("IMMUNE_")))
			{
				const int64 V = StaticEnum<EZStatus>()->GetValueByNameString(K.Mid(7));
				if (V != INDEX_NONE && KV.Value >= 1.f) { D.Immune.Add((EZStatus)V); }
				else { D.Special.Add(FName(*(TEXT("STATUSRES_") + K.Mid(7))), KV.Value); }
				continue;
			}
			bool bConsumed = K.StartsWith(TEXT("RES_"));
			for (const TCHAR* C : Consumed) { bConsumed |= (K == C); }
			if (!bConsumed) { D.Special.Add(KV.Key, KV.Value); }
		}
		return D;
	}

	bool IsMastered(const FZCharacterState& State, FName AbilityId)
	{
		const UZGameData& DB = UZGameData::Get();
		int32 Need = 0;
		if (const FZAbilityDef* A = DB.FindAbility(AbilityId)) { Need = A->AP; }
		else if (const FZPassiveDef* P = DB.FindPassive(AbilityId)) { Need = P->AP; }
		else { return false; }
		if (Need <= 0) { return false; }
		const int32* Have = State.AbilityAP.Find(AbilityId);
		return Have && *Have >= Need;
	}

	TArray<FName> AvailableAbilities(const FZCharacterState& State, bool bIncludePassives)
	{
		const UZGameData& DB = UZGameData::Get();
		TArray<FName> Out;
		if (const FZCharacterDef* Def = DB.FindCharacter(State.Id))
		{
			for (FName A : Def->Abilities) { Out.AddUnique(A); }
		}
		const FZDerivedStats D = ComputeDerived(State);
		for (FName A : D.GrantedAbilities) { Out.AddUnique(A); }
		for (const TPair<FName, int32>& KV : State.AbilityAP)
		{
			if (IsMastered(State, KV.Key)) { Out.AddUnique(KV.Key); }
		}
		Out.RemoveAll([&DB, bIncludePassives](FName A)
		{
			const FZAbilityDef* Ab = DB.FindAbility(A);
			if (!Ab) { return !bIncludePassives || !DB.FindPassive(A); }
			return !bIncludePassives && Ab->Kind == EZAbilityKind::Passive;
		});
		return Out;
	}

	int32 PassiveCostUsed(const FZCharacterState& State)
	{
		int32 Used = 0;
		for (FName P : State.Passives)
		{
			if (const FZPassiveDef* PD = UZGameData::Get().FindPassive(P)) { Used += PD->Cost; }
		}
		return Used;
	}

	bool SlotAccepts(EZEquipSlot Slot, EZSlot ItemSlot)
	{
		switch (Slot)
		{
		case EZEquipSlot::Weapon: return ItemSlot == EZSlot::Weapon;
		case EZEquipSlot::Offhand: return ItemSlot == EZSlot::Offhand;
		case EZEquipSlot::Head: return ItemSlot == EZSlot::Head || ItemSlot == EZSlot::Outfit;
		case EZEquipSlot::Torso: return ItemSlot == EZSlot::Torso || ItemSlot == EZSlot::Outfit;
		case EZEquipSlot::Legs: return ItemSlot == EZSlot::Legs || ItemSlot == EZSlot::Outfit;
		case EZEquipSlot::AccessoryA:
		case EZEquipSlot::AccessoryB: return ItemSlot == EZSlot::Accessory;
		case EZEquipSlot::Relic: return ItemSlot == EZSlot::Relic;
		case EZEquipSlot::ToolA:
		case EZEquipSlot::ToolB: return ItemSlot == EZSlot::Tool;
		default: return false;
		}
	}

	bool CanEquip(const FZCharacterState& State, EZEquipSlot Slot, const FZItemDef& Item, FString& OutReason)
	{
		const UZGameData& DB = UZGameData::Get();
		const FZCharacterDef* Def = DB.FindCharacter(State.Id);
		if (!SlotAccepts(Slot, Item.Slot))
		{
			OutReason = FString::Printf(TEXT("%s ne va pas dans l'emplacement %s"), *Item.DisplayName(), *ZNames::SlotName(Slot));
			return false;
		}
		const bool bLink = State.Id == FName(TEXT("link"));
		if (Slot == EZEquipSlot::Weapon && !bLink && Def)
		{
			if (Def->Weapon != Item.Kind)
			{
				OutReason = FString::Printf(TEXT("%s ne manie pas ce type d'arme"), *Def->Name);
				return false;
			}
		}
		if (Slot == EZEquipSlot::Offhand)
		{
			if (!bLink && (!Def || Def->Weapon != TEXT("Blade")))
			{
				OutReason = TEXT("Ce personnage ne porte pas de bouclier");
				return false;
			}
			if (const FZItemDef* W = DB.FindItem(State.GetEquip(EZEquipSlot::Weapon)))
			{
				if (W->Hands >= 2)
				{
					OutReason = FString::Printf(TEXT("%s occupe les deux mains"), *W->DisplayName());
					return false;
				}
			}
		}
		if ((Slot == EZEquipSlot::ToolA || Slot == EZEquipSlot::ToolB) && !bLink)
		{
			OutReason = TEXT("Seul Link utilise les outils en combat");
			return false;
		}
		if (Slot == EZEquipSlot::AccessoryA || Slot == EZEquipSlot::AccessoryB)
		{
			const EZEquipSlot Other = Slot == EZEquipSlot::AccessoryA ? EZEquipSlot::AccessoryB : EZEquipSlot::AccessoryA;
			if (State.GetEquip(Other) == Item.Id)
			{
				OutReason = TEXT("Cet accessoire est déjà porté");
				return false;
			}
		}
		if (Slot == EZEquipSlot::ToolA || Slot == EZEquipSlot::ToolB)
		{
			const EZEquipSlot Other = Slot == EZEquipSlot::ToolA ? EZEquipSlot::ToolB : EZEquipSlot::ToolA;
			if (State.GetEquip(Other) == Item.Id)
			{
				OutReason = TEXT("Cet outil est déjà assigné");
				return false;
			}
		}
		return true;
	}

	bool Equip(FZCharacterState& State, EZEquipSlot Slot, const FString& ItemId, FString& OutReason)
	{
		const UZGameData& DB = UZGameData::Get();
		const FZItemDef* Item = DB.FindItem(ItemId);
		if (!Item) { OutReason = TEXT("Objet inconnu"); return false; }
		if (!CanEquip(State, Slot, *Item, OutReason)) { return false; }
		if (State.Equip.Num() < ZEquipSlotCount) { State.Equip.SetNum(ZEquipSlotCount); }

		const bool bArmorSlot = Slot == EZEquipSlot::Head || Slot == EZEquipSlot::Torso || Slot == EZEquipSlot::Legs;
		if (bArmorSlot)
		{
			// Retirer une tenue monobloc existante : elle occupe les trois emplacements
			for (EZEquipSlot S : { EZEquipSlot::Head, EZEquipSlot::Torso, EZEquipSlot::Legs })
			{
				const FZItemDef* Cur = DB.FindItem(State.GetEquip(S));
				if (Cur && Cur->Slot == EZSlot::Outfit)
				{
					State.SetEquip(EZEquipSlot::Head, FString());
					State.SetEquip(EZEquipSlot::Torso, FString());
					State.SetEquip(EZEquipSlot::Legs, FString());
					break;
				}
			}
			if (Item->Slot == EZSlot::Outfit)
			{
				State.SetEquip(EZEquipSlot::Head, ItemId);
				State.SetEquip(EZEquipSlot::Torso, ItemId);
				State.SetEquip(EZEquipSlot::Legs, ItemId);
				return true;
			}
		}
		if (Slot == EZEquipSlot::Weapon && Item->Hands >= 2)
		{
			State.SetEquip(EZEquipSlot::Offhand, FString());
		}
		State.SetEquip(Slot, ItemId);
		return true;
	}

	void Unequip(FZCharacterState& State, EZEquipSlot Slot)
	{
		const FZItemDef* Cur = UZGameData::Get().FindItem(State.GetEquip(Slot));
		if (Cur && Cur->Slot == EZSlot::Outfit)
		{
			State.SetEquip(EZEquipSlot::Head, FString());
			State.SetEquip(EZEquipSlot::Torso, FString());
			State.SetEquip(EZEquipSlot::Legs, FString());
			return;
		}
		State.SetEquip(Slot, FString());
	}

	FZCharacterState MakeCharacter(FName Id, int32 Level)
	{
		FZCharacterState S;
		S.Id = Id;
		S.Level = FMath::Clamp(Level, 1, 99);
		S.XP = TotalXPForLevel(S.Level);
		S.Equip.SetNum(ZEquipSlotCount);
		if (const FZCharacterDef* Def = UZGameData::Get().FindCharacter(Id))
		{
			const UEnum* SlotEnum = StaticEnum<EZEquipSlot>();
			for (const TPair<FString, FString>& KV : Def->Equipment)
			{
				FString Reason;
				if (KV.Key == TEXT("Outfit"))
				{
					Equip(S, EZEquipSlot::Torso, KV.Value, Reason);
					continue;
				}
				const int64 V = SlotEnum->GetValueByNameString(KV.Key);
				if (V != INDEX_NONE) { Equip(S, (EZEquipSlot)V, KV.Value, Reason); }
			}
			if (Def->Stances.Num() > 0) { S.Stance = FName(*Def->Stances[0]); }
		}
		AutoPrepare(S);
		return S;
	}

	void AutoPrepare(FZCharacterState& State)
	{
		const TArray<FName> Avail = AvailableAbilities(State);
		TArray<FName> Keep;
		for (FName P : State.Prepared)
		{
			if (Avail.Contains(P) && Keep.Num() < 6) { Keep.Add(P); }
		}
		for (FName A : Avail)
		{
			if (Keep.Num() >= 6) break;
			const FZAbilityDef* Ab = UZGameData::Get().FindAbility(A);
			if (Ab && Ab->School != TEXT("Relic")) { Keep.AddUnique(A); } // les outils ont leur propre commande
		}
		State.Prepared = Keep;
	}
}
