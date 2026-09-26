#include "ZTypes.h"

namespace ZNames
{
	FString ElementName(EZElement E)
	{
		switch (E)
		{
		case EZElement::Fire: return TEXT("Feu");
		case EZElement::Water: return TEXT("Eau");
		case EZElement::Ice: return TEXT("Glace");
		case EZElement::Thunder: return TEXT("Foudre");
		case EZElement::Wind: return TEXT("Vent");
		case EZElement::Earth: return TEXT("Terre");
		case EZElement::Light: return TEXT("Lumière");
		case EZElement::Shadow: return TEXT("Ombre");
		default: return TEXT("Neutre");
		}
	}

	// Symboles doublant la couleur (accessibilité, chapitre 21)
	FString ElementIcon(EZElement E)
	{
		switch (E)
		{
		case EZElement::Fire: return TEXT("▲");
		case EZElement::Water: return TEXT("●");
		case EZElement::Ice: return TEXT("✦");
		case EZElement::Thunder: return TEXT("◈");
		case EZElement::Wind: return TEXT("≈");
		case EZElement::Earth: return TEXT("■");
		case EZElement::Light: return TEXT("✧");
		case EZElement::Shadow: return TEXT("◐");
		default: return TEXT("◇");
		}
	}

	FLinearColor ElementColor(EZElement E)
	{
		switch (E)
		{
		case EZElement::Fire: return FLinearColor(1.0f, 0.36f, 0.18f);
		case EZElement::Water: return FLinearColor(0.25f, 0.78f, 1.0f);
		case EZElement::Ice: return FLinearColor(0.72f, 0.92f, 1.0f);
		case EZElement::Thunder: return FLinearColor(1.0f, 0.88f, 0.25f);
		case EZElement::Wind: return FLinearColor(0.55f, 1.0f, 0.7f);
		case EZElement::Earth: return FLinearColor(0.78f, 0.6f, 0.36f);
		case EZElement::Light: return FLinearColor(1.0f, 0.97f, 0.75f);
		case EZElement::Shadow: return FLinearColor(0.62f, 0.4f, 0.9f);
		default: return FLinearColor(0.92f, 0.92f, 0.92f);
		}
	}

	EZElement ElementFromString(const FString& S)
	{
		static const TCHAR* Names[] = { TEXT("Neutral"), TEXT("Fire"), TEXT("Water"), TEXT("Ice"), TEXT("Thunder"), TEXT("Wind"), TEXT("Earth"), TEXT("Light"), TEXT("Shadow") };
		for (int32 i = 0; i < ZElementCount; ++i)
		{
			if (S.Equals(Names[i], ESearchCase::IgnoreCase)) { return (EZElement)i; }
		}
		return EZElement::Neutral;
	}

	FString StatusName(EZStatus S)
	{
		switch (S)
		{
		case EZStatus::Wet: return TEXT("Mouillé");
		case EZStatus::Burn: return TEXT("Brûlure");
		case EZStatus::Freeze: return TEXT("Gel");
		case EZStatus::Shock: return TEXT("Choc");
		case EZStatus::Poison: return TEXT("Poison");
		case EZStatus::Sleep: return TEXT("Sommeil");
		case EZStatus::Silence: return TEXT("Silence");
		case EZStatus::Blind: return TEXT("Cécité");
		case EZStatus::Taunt: return TEXT("Provoqué");
		case EZStatus::Fragile: return TEXT("Fragilité");
		case EZStatus::Mire: return TEXT("Enlisement");
		case EZStatus::Haste: return TEXT("Hâte");
		case EZStatus::Slow: return TEXT("Lenteur");
		case EZStatus::Guard: return TEXT("Garde");
		case EZStatus::Barrier: return TEXT("Barrière");
		case EZStatus::Regen: return TEXT("Régénération");
		case EZStatus::MagicShield: return TEXT("Amour de Nayru");
		case EZStatus::Rampart: return TEXT("Rempart");
		case EZStatus::ZoraBarrier: return TEXT("Barrière zora");
		case EZStatus::SprUp: return TEXT("Esprit +");
		case EZStatus::Evade: return TEXT("Esquive +");
		case EZStatus::Stability: return TEXT("Stabilité");
		case EZStatus::BreachStability: return TEXT("Stabilité de Brèche");
		case EZStatus::Exposed: return TEXT("Brèche ouverte");
		case EZStatus::Marked: return TEXT("Marqué");
		case EZStatus::Intercept: return TEXT("Protégé");
		case EZStatus::MirrorParry: return TEXT("Parade miroir");
		case EZStatus::MPCostUp: return TEXT("Épuisement");
		case EZStatus::Disarm: return TEXT("Désarmé");
		case EZStatus::Knockback: return TEXT("Recul");
		default: return TEXT("");
		}
	}

	bool IsDebuff(EZStatus S)
	{
		switch (S)
		{
		case EZStatus::Wet: case EZStatus::Burn: case EZStatus::Freeze: case EZStatus::Shock: case EZStatus::Poison:
		case EZStatus::Sleep: case EZStatus::Silence: case EZStatus::Blind: case EZStatus::Taunt: case EZStatus::Fragile:
		case EZStatus::Mire: case EZStatus::Slow: case EZStatus::Exposed: case EZStatus::Marked: case EZStatus::MPCostUp:
		case EZStatus::Disarm:
			return true;
		default:
			return false;
		}
	}

	FString SlotName(EZEquipSlot S)
	{
		switch (S)
		{
		case EZEquipSlot::Weapon: return TEXT("Arme");
		case EZEquipSlot::Offhand: return TEXT("Main secondaire");
		case EZEquipSlot::Head: return TEXT("Tête");
		case EZEquipSlot::Torso: return TEXT("Torse");
		case EZEquipSlot::Legs: return TEXT("Jambes");
		case EZEquipSlot::AccessoryA: return TEXT("Accessoire A");
		case EZEquipSlot::AccessoryB: return TEXT("Accessoire B");
		case EZEquipSlot::Relic: return TEXT("Relique");
		case EZEquipSlot::ToolA: return TEXT("Outil 1");
		case EZEquipSlot::ToolB: return TEXT("Outil 2");
		default: return TEXT("?");
		}
	}

	FString ItemSlotName(EZSlot S)
	{
		switch (S)
		{
		case EZSlot::Weapon: return TEXT("Arme");
		case EZSlot::Offhand: return TEXT("Bouclier");
		case EZSlot::Head: return TEXT("Tête");
		case EZSlot::Torso: return TEXT("Torse");
		case EZSlot::Legs: return TEXT("Jambes");
		case EZSlot::Outfit: return TEXT("Tenue complète");
		case EZSlot::Accessory: return TEXT("Accessoire");
		case EZSlot::Relic: return TEXT("Relique");
		case EZSlot::Tool: return TEXT("Outil");
		case EZSlot::Ammo: return TEXT("Munition");
		case EZSlot::Traversal: return TEXT("Exploration");
		case EZSlot::Spell: return TEXT("Sort");
		case EZSlot::Upgrade: return TEXT("Amélioration");
		case EZSlot::Cosmetic: return TEXT("Apparence / trophée");
		default: return TEXT("—");
		}
	}

	FLinearColor HexColor(const FString& Hex, const FLinearColor& Fallback)
	{
		if (Hex.Len() < 7 || Hex[0] != TEXT('#')) { return Fallback; }
		const FColor C = FColor::FromHex(Hex);
		return FLinearColor::FromSRGBColor(C);
	}

	FString FormatInt(int64 Value)
	{
		FString Digits = FString::Printf(TEXT("%lld"), FMath::Abs(Value));
		FString Out;
		for (int32 i = 0; i < Digits.Len(); ++i)
		{
			if (i > 0 && (Digits.Len() - i) % 3 == 0) { Out.AppendChar(TEXT(' ')); }
			Out.AppendChar(Digits[i]);
		}
		return Value < 0 ? TEXT("-") + Out : Out;
	}
}
