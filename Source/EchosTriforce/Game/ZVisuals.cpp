#include "ZVisuals.h"
#include "ZGameData.h"
#include "ZTPAnim.h"
#include "EchosTriforce.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"

namespace
{
	template <typename T>
	T* LoadCached(const FString& Path)
	{
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		if (TWeakObjectPtr<UObject>* C = Cache.Find(Path))
		{
			if (C->IsValid()) return Cast<T>(C->Get());
			if (!C->IsValid() && C->IsExplicitlyNull()) return nullptr;
		}
		T* Obj = nullptr;
		if (FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))
		{
			Obj = LoadObject<T>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn);
		}
		Cache.Add(Path, Obj);
		return Obj;
	}

	FString BasicPath(const FString& Shape)
	{
		return FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), *Shape, *Shape);
	}

	FZPartSpec P(const FString& Mesh, const FString& Fallback, FName Bone, FVector Loc, FRotator Rot, FVector Scale, FLinearColor Color, float Emissive = 0.f, FName Tag = NAME_None)
	{
		FZPartSpec S;
		S.Mesh = Mesh; S.Fallback = Fallback; S.Bone = Bone; S.Loc = Loc; S.Rot = Rot; S.Scale = Scale; S.Color = Color; S.Emissive = Emissive; S.Tag = Tag;
		return S;
	}

	FVector CmdVec(const TCHAR* Key, const FVector& Def)
	{
		FString V;
		if (!FParse::Value(FCommandLine::Get(), Key, V, false)) return Def;
		TArray<FString> P;
		V.ParseIntoArray(P, TEXT(","));
		return P.Num() == 3 ? FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])) : Def;
	}
	FVector HatOffset() { return CmdVec(TEXT("ZHatLoc="), FVector(9, -2, 0)); }
	FRotator HatRotation() { const FVector R = CmdVec(TEXT("ZHatRot="), FVector(-90, 0, 0)); return FRotator(R.X, R.Y, R.Z); }
	FVector FinOffset() { return CmdVec(TEXT("ZFinLoc="), FVector(12, 4, 0)); }
	FVector SwordOffset() { return CmdVec(TEXT("ZSwordLoc="), FVector(-9, 3, 0)); }
	FRotator SwordRotation() { const FVector R = CmdVec(TEXT("ZSwordRot="), FVector(60, 0, 0)); return FRotator(R.X, R.Y, R.Z); }
	// Os weapon_r (lame le long de son axe, cf. Tools/blender/make_anims.py)
	FVector WeaponOffset() { return CmdVec(TEXT("ZWeaponLoc="), FVector(0, 0, 0)); }
	FRotator WeaponRotation() { const FVector R = CmdVec(TEXT("ZWeaponRot="), FVector(0, 0, -90)); return FRotator(R.X, R.Y, R.Z); }

	const FLinearColor Steel(0.78f, 0.80f, 0.84f);
	const FLinearColor Leather(0.36f, 0.22f, 0.12f);
	const FLinearColor Gold(0.95f, 0.72f, 0.25f);
}

namespace ZVis
{
	UStaticMesh* Mesh(const FString& PathOrShape)
	{
		if (PathOrShape.IsEmpty()) return nullptr;
		if (!PathOrShape.StartsWith(TEXT("/"))) return LoadCached<UStaticMesh>(BasicPath(PathOrShape));
		FString Path = PathOrShape;
		if (!Path.Contains(TEXT(".")))
		{
			Path += TEXT(".") + FPaths::GetBaseFilename(Path);
		}
		return LoadCached<UStaticMesh>(Path);
	}

	USkeletalMesh* Skeletal(const FString& Name)
	{
		if (Name.StartsWith(TEXT("/")))
		{
			FString Path = Name;
			if (!Path.Contains(TEXT("."))) Path += TEXT(".") + FPaths::GetBaseFilename(Path);
			if (USkeletalMesh* M = LoadCached<USkeletalMesh>(Path)) return M;
			return LoadCached<USkeletalMesh>(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		}
		if (Name == TEXT("Quinn")) return LoadCached<USkeletalMesh>(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
		if (Name == TEXT("Manny")) return LoadCached<USkeletalMesh>(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		return nullptr;
	}

	USkeletalMesh* ArtSkeletal(const FString& Name)
	{
		if (Name.IsEmpty()) return nullptr;
		FString Path = Name;
		if (!Path.Contains(TEXT("."))) Path += TEXT(".") + FPaths::GetBaseFilename(Path);
		return LoadCached<USkeletalMesh>(Path);
	}

	UAnimSequence* Anim(const FString& Name)
	{
		static const TMap<FString, FString> Paths = {
			{ TEXT("Idle"), TEXT("Unarmed/MM_Idle") },
			{ TEXT("Attack1"), TEXT("Unarmed/Attack/MM_Attack_01") },
			{ TEXT("Attack2"), TEXT("Unarmed/Attack/MM_Attack_02") },
			{ TEXT("Attack3"), TEXT("Unarmed/Attack/MM_Attack_03") },
			{ TEXT("Charged"), TEXT("Unarmed/Attack/MM_ChargedAttack") },
			{ TEXT("Death"), TEXT("Death/MM_Death_Front_01") },
			{ TEXT("DeathBack"), TEXT("Death/MM_Death_Back_01") },
			{ TEXT("Jog"), TEXT("Unarmed/Jog/MF_Unarmed_Jog_Fwd") },
			{ TEXT("Walk"), TEXT("Unarmed/Walk/MF_Unarmed_Walk_Fwd") },
			{ TEXT("Dash"), TEXT("Unarmed/Jump/MM_Dash") },
			{ TEXT("Jump"), TEXT("Unarmed/Jump/MM_Jump") },
			{ TEXT("Land"), TEXT("Unarmed/Jump/MM_Land") },
			{ TEXT("Fall"), TEXT("Unarmed/Jump/MM_Fall_Loop") },
		};
		// Animations d'arme générées (Tools/blender/make_anims.py)
		static const TMap<FString, FString> Art = {
			{ TEXT("SwordIdle"), TEXT("A_Sword_Idle") }, { TEXT("SwordSlash"), TEXT("A_Sword_Slash") }, { TEXT("SwordThrust"), TEXT("A_Sword_Thrust") },
			{ TEXT("SwordSpin"), TEXT("A_Sword_Spin") }, { TEXT("Cast"), TEXT("A_Cast") }, { TEXT("Guard"), TEXT("A_Guard") }, { TEXT("Victory"), TEXT("A_Victory") },
		};
		if (const FString* A = Art.Find(Name))
		{
			return LoadCached<UAnimSequence>(FString::Printf(TEXT("/Game/Art/Anims/%s.%s"), **A, **A));
		}
		const FString* Rel = Paths.Find(Name);
		if (!Rel) return nullptr;
		const FString Base = FPaths::GetBaseFilename(*Rel);
		return LoadCached<UAnimSequence>(FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/%s.%s"), **Rel, *Base));
	}

	UAnimSequence* AnimIn(const FString& AnimSet, const FString& Name)
	{
		if (AnimSet.IsEmpty()) return Anim(Name);
		// Rôles du jeu (et actions du combat) -> animations d'origine de Link (BCK d'AlAnm.arc, cf. Tools/tp/link_anims.json ;
		// « run », « battle_idle » et « cast » recombinent bas et haut du corps comme le jeu)
		static const TMap<FString, FString> TPLink = {
			{ TEXT("Idle"), TEXT("waits") }, { TEXT("Walk"), TEXT("walks") }, { TEXT("Jog"), TEXT("run") }, { TEXT("Run"), TEXT("run") },
			{ TEXT("Jump"), TEXT("jumpst") }, { TEXT("Fall"), TEXT("jumps") }, { TEXT("Land"), TEXT("jumpsed") }, { TEXT("Dash"), TEXT("rollf") },
			{ TEXT("Attack1"), TEXT("cutr") }, { TEXT("Attack2"), TEXT("cutfr") }, { TEXT("Attack3"), TEXT("cast") }, { TEXT("Charged"), TEXT("cutt") },
			{ TEXT("Death"), TEXT("die") }, { TEXT("DeathBack"), TEXT("die") }, { TEXT("Hit"), TEXT("dam") }, { TEXT("HitHeavy"), TEXT("damfb") },
			{ TEXT("SwordIdle"), TEXT("battle_idle") }, { TEXT("SwordSlash"), TEXT("cutr") }, { TEXT("SwordThrust"), TEXT("cutfr") },
			{ TEXT("SwordSpin"), TEXT("cutt") }, { TEXT("Cast"), TEXT("cast") }, { TEXT("Guard"), TEXT("atdefl") }, { TEXT("Victory"), TEXT("finisha") },
			{ TEXT("Slash"), TEXT("cutr") }, { TEXT("Thrust"), TEXT("cutfr") }, { TEXT("Spin"), TEXT("cutt") }, { TEXT("Smash"), TEXT("cuteb") },
			{ TEXT("Roll"), TEXT("rollf") }, { TEXT("Bash"), TEXT("atgpush") }, { TEXT("Duo"), TEXT("cutjst") }, { TEXT("Bite"), TEXT("cutfr") },
			{ TEXT("Chant"), TEXT("cast") }, { TEXT("Beam"), TEXT("cast") }, { TEXT("Heal"), TEXT("cast") }, { TEXT("Summon"), TEXT("geta") },
			{ TEXT("Shoot"), TEXT("ashoot") }, { TEXT("Throw"), TEXT("bombthrow") }, { TEXT("Hook"), TEXT("hsshoot") }, { TEXT("Taunt"), TEXT("atgpush") },
			{ TEXT("Item"), TEXT("bindrinkst") },
		};
		const FString* Bck = AnimSet == TEXT("TPLink") ? TPLink.Find(Name) : nullptr;
		const FString A = FString::Printf(TEXT("A_%s_%s"), *AnimSet, Bck ? **Bck : *Name.ToLower()); // nom de BCK direct accepté
		return LoadCached<UAnimSequence>(FString::Printf(TEXT("/Game/Art/TP/%s/Anims/%s.%s"), *AnimSet, *A, *A));
	}

	UZTPAnimInstance* SetupTPAnim(USkeletalMeshComponent* Body, const FZLook& Look)
	{
		if (!Body || Look.AnimSet.IsEmpty() || !Body->GetSkeletalMeshAsset()) return nullptr;
		if (Body->GetAnimationMode() != EAnimationMode::AnimationBlueprint || Body->GetAnimClass() != UZTPAnimInstance::StaticClass())
		{
			Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Body->SetAnimInstanceClass(UZTPAnimInstance::StaticClass());
		}
		return Cast<UZTPAnimInstance>(Body->GetAnimInstance());
	}

	void ShowWeaponsDrawn(AActor* Owner, bool bDrawn)
	{
		SetArtVisible(Owner, TEXT("weapon"), bDrawn);
		SetArtVisible(Owner, TEXT("shield"), bDrawn);
		SetArtVisible(Owner, TEXT("weapon_back"), !bDrawn);
		SetArtVisible(Owner, TEXT("shield_back"), !bDrawn);
	}

	UMaterialInterface* ToonMaterial(bool bSkeletal)
	{
		if (UMaterialInterface* M = LoadCached<UMaterialInterface>(bSkeletal ? TEXT("/Game/Art/Materials/M_ZToonSkin.M_ZToonSkin") : TEXT("/Game/Art/Materials/M_ZToon.M_ZToon")))
		{
			return M;
		}
		if (bSkeletal) return LoadCached<UMaterialInterface>(TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New"));
		return LoadCached<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	UMaterialInstanceDynamic* Tint(UMeshComponent* C, const FLinearColor& Color, float Emissive)
	{
		if (!C) return nullptr;
		const bool bSkel = C->IsA<USkeletalMeshComponent>();
		UMaterialInterface* Base = ToonMaterial(bSkel);
		if (!Base) return nullptr;
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, C);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetVectorParameterValue(TEXT("Paint Tint"), Color);
		MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		const int32 N = FMath::Max(1, C->GetNumMaterials());
		const TArray<FName> SlotNames = C->GetMaterialSlotNames();
		for (int32 i = 0; i < N; ++i)
		{
			const FString SlotName = SlotNames.IsValidIndex(i) ? SlotNames[i].ToString() : FString();
			// Emplacements nommés dans Blender : couleur fixe par rôle, le reste prend la couleur de la pièce
			struct FSlotStyle { const TCHAR* Name; FLinearColor Color; float Emissive; float Rough; bool bRelative; };
			static const FSlotStyle Styles[] = {
				{ TEXT("Glow"), FLinearColor(0.2f, 0.95f, 1.f), 10.f, 0.5f, false },
				{ TEXT("Metal"), FLinearColor(0.80f, 0.83f, 0.88f), 0.f, 0.3f, false },
				{ TEXT("Hilt"), FLinearColor(0.26f, 0.2f, 0.62f), 0.f, 0.5f, false },
				{ TEXT("Red"), FLinearColor(0.72f, 0.1f, 0.1f), 0.f, 0.6f, false },
				{ TEXT("White"), FLinearColor(0.95f, 0.95f, 0.92f), 0.2f, 0.5f, false },
				{ TEXT("Moss"), FLinearColor(0.24f, 0.44f, 0.16f), 0.f, 0.95f, false },
				{ TEXT("Accent"), FLinearColor(0.95f, 0.74f, 0.28f), 0.15f, 0.4f, false },
				{ TEXT("Eye"), FLinearColor(1.f, 0.95f, 0.8f), 2.f, 0.5f, false },
				{ TEXT("Dark"), FLinearColor(0.45f, 0.45f, 0.45f), 0.f, 0.8f, true },
			};
			// Matériau d'art dédié à ce maillage et cet emplacement (ex. MI_SM_Golem_Body : pierre moussue)
			if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(C))
			{
				if (SMC->GetStaticMesh() && !SlotName.IsEmpty())
				{
					const FString MatName = FString::Printf(TEXT("MI_%s_%s"), *SMC->GetStaticMesh()->GetName(), *SlotName);
					if (UMaterialInterface* ArtMat = LoadCached<UMaterialInterface>(FString::Printf(TEXT("/Game/Art/Materials/%s.%s"), *MatName, *MatName)))
					{
						UMaterialInstanceDynamic* X = UMaterialInstanceDynamic::Create(ArtMat, C);
						X->SetScalarParameterValue(TEXT("Emissive"), Emissive);
						C->SetMaterial(i, X);
						continue;
					}
				}
			}
			const FSlotStyle* Found = nullptr;
			for (const FSlotStyle& St : Styles) { if (SlotName.Contains(St.Name)) { Found = &St; break; } }
			if (Found)
			{
				UMaterialInstanceDynamic* X = UMaterialInstanceDynamic::Create(Base, C);
				X->SetVectorParameterValue(TEXT("Color"), Found->bRelative ? Color * Found->Color : Found->Color);
				X->SetScalarParameterValue(TEXT("Emissive"), Found->Emissive);
				X->SetScalarParameterValue(TEXT("Roughness"), Found->Rough);
				C->SetMaterial(i, X);
			}
			else
			{
				C->SetMaterial(i, MID);
			}
		}
		return MID;
	}

	FLinearColor OutfitColor(const FString& OutfitItem, const FLinearColor& Default)
	{
		if (OutfitItem.IsEmpty()) return Default;
		const FZItemDef* It = UZGameData::Get().FindItem(OutfitItem);
		if (!It) return Default;
		const FString N = It->Name.ToLower();
		if (It->Profile == TEXT("EAU") || N.Contains(TEXT("zora")) || N.Contains(TEXT("blue"))) return FLinearColor(0.12f, 0.30f, 0.62f);
		if (It->Profile == TEXT("IGNI") || N.Contains(TEXT("goron")) || N.Contains(TEXT("red"))) return FLinearColor(0.62f, 0.14f, 0.10f);
		if (It->Profile == TEXT("FROID")) return FLinearColor(0.55f, 0.70f, 0.82f);
		if (It->Profile == TEXT("ISOL")) return FLinearColor(0.75f, 0.62f, 0.20f);
		if (It->Profile == TEXT("GARDE")) return FLinearColor(0.55f, 0.55f, 0.62f);
		if (It->Profile == TEXT("SAGE")) return FLinearColor(0.38f, 0.22f, 0.55f);
		if (N.Contains(TEXT("wild")) || N.Contains(TEXT("champion"))) return FLinearColor(0.18f, 0.32f, 0.62f);
		return Default;
	}

	FZTunic OutfitTunic(const FString& OutfitItem)
	{
		FZTunic T;
		const FZItemDef* It = OutfitItem.IsEmpty() ? nullptr : UZGameData::Get().FindItem(OutfitItem);
		if (!It) return T;
		const FString N = It->Name.ToLower();
		auto Set = [&T](float H, float S, float V) { T.Hue = H; T.Sat = S; T.Val = V; };
		if (It->Profile == TEXT("EAU") || N.Contains(TEXT("zora")) || N.Contains(TEXT("blue"))) Set(0.60f, 1.05f, 1.05f);
		else if (It->Profile == TEXT("IGNI") || N.Contains(TEXT("goron")) || N.Contains(TEXT("red"))) Set(0.99f, 1.1f, 1.1f);
		else if (It->Profile == TEXT("FROID")) Set(0.55f, 0.45f, 1.3f);
		else if (It->Profile == TEXT("ISOL")) Set(0.13f, 1.0f, 1.2f);
		else if (It->Profile == TEXT("GARDE")) Set(0.62f, 0.15f, 1.1f);
		else if (It->Profile == TEXT("SAGE")) Set(0.77f, 0.9f, 1.0f);
		else if (N.Contains(TEXT("wild")) || N.Contains(TEXT("champion"))) Set(0.60f, 1.2f, 1.0f);
		return T;
	}

	FZLook CharacterLook(FName CharId, FName Form, const FString& OutfitItem, const FString& WeaponItem, const FString& OffhandItem)
	{
		FZLook L;
		const FString Id = CharId.ToString();
		const UZGameData& DB = UZGameData::Get();
		const FZItemDef* Weapon = DB.FindItem(WeaponItem);
		const FString WKind = Weapon ? Weapon->Kind : FString();

		// Repères d'os du mannequin UE5 : tête X = haut, Y = avant/arrière, Z = latéral ; main : lame selon Z
		const FVector HatLoc = HatOffset();
		const FRotator HatRot = HatRotation();
		auto AddWeapon = [&L, &WKind](FLinearColor Hilt)
		{
			if (WKind == TEXT("Bow"))
			{
				L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Bow"), TEXT("Cylinder"), TEXT("hand_l"), FVector(0, 0, 0), FRotator(0, 0, 0), FVector(0.05f, 0.05f, 1.1f), Leather, 0.f, TEXT("weapon")));
				return;
			}
			const bool bHeavy = WKind == TEXT("Heavy");
			const float Len = bHeavy ? 1.25f : 0.85f;
			L.Parts.Add(P(bHeavy ? TEXT("/Game/Art/Meshes/SM_Sword_Great") : TEXT("/Game/Art/Meshes/SM_Sword_Hero"), TEXT("Cube"), TEXT("hand_r"),
				SwordOffset(), SwordRotation(), FVector(0.05f, 0.012f * (bHeavy ? 2.f : 1.f), Len), Steel, 0.f, TEXT("weapon")));
		};

		if (Form == TEXT("Wolf"))
		{
			L.Body = TEXT("");
			L.Height = 110.f;
			const FLinearColor Fur(0.38f, 0.40f, 0.44f);
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Wolf"), TEXT("Cylinder"), NAME_None, FVector(0, 0, 70), FRotator(90, 0, 0), FVector(0.55f, 0.55f, 1.25f), Fur));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(70, 0, 95), FRotator::ZeroRotator, FVector(0.5f, 0.45f, 0.45f), Fur));
			L.Parts.Add(P(TEXT(""), TEXT("Cone"), NAME_None, FVector(105, 0, 90), FRotator(-90, 0, 0), FVector(0.25f, 0.25f, 0.4f), FLinearColor(0.2f, 0.2f, 0.22f)));
			L.Parts.Add(P(TEXT(""), TEXT("Cone"), NAME_None, FVector(62, 12, 125), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 0.25f), Fur));
			L.Parts.Add(P(TEXT(""), TEXT("Cone"), NAME_None, FVector(62, -12, 125), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 0.25f), Fur));
			for (int32 i = 0; i < 4; ++i)
			{
				const float X = i < 2 ? 40.f : -40.f;
				const float Y = (i % 2 == 0) ? 18.f : -18.f;
				L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), NAME_None, FVector(X, Y, 25), FRotator::ZeroRotator, FVector(0.14f, 0.14f, 0.55f), Fur));
			}
			L.Parts.Add(P(TEXT(""), TEXT("Cone"), NAME_None, FVector(-80, 0, 90), FRotator(120, 0, 0), FVector(0.2f, 0.2f, 0.6f), Fur));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(95, 12, 102), FRotator::ZeroRotator, FVector(0.06f), FLinearColor(0.3f, 0.8f, 1.f), 3.f));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(95, -12, 102), FRotator::ZeroRotator, FVector(0.06f), FLinearColor(0.3f, 0.8f, 1.f), 3.f));
			return L;
		}

		auto Paint = [&L](FLinearColor Boot, FLinearColor Leg, FLinearColor Belt, FLinearColor Sleeve, FLinearColor Glove, FLinearColor Skin, FLinearColor Hair, float SkirtH = 70.f, float NeckH = 148.f, float HairH = 166.f)
		{
			L.Paint.bUse = true;
			L.Paint.Boot = Boot; L.Paint.Leg = Leg; L.Paint.Belt = Belt; L.Paint.Sleeve = Sleeve; L.Paint.Glove = Glove;
			L.Paint.Skin = Skin; L.Paint.Hair = Hair; L.Paint.SkirtH = SkirtH; L.Paint.NeckH = NeckH; L.Paint.HairH = HairH;
		};
		const FLinearColor Skin(0.95f, 0.76f, 0.60f);
		const FLinearColor Cream(0.86f, 0.81f, 0.68f);
		const FLinearColor Brown(0.30f, 0.18f, 0.09f);
		if (Id == TEXT("link"))
		{
			L.Body = TEXT("Manny");
			FString BodyOverride;
			if (FParse::Value(FCommandLine::Get(), TEXT("ZLinkMesh="), BodyOverride)) L.Body = BodyOverride;
			// Link extrait de Twilight Princess (Tools/tp, Tools/ue/import_tp.py) : modèle, armes et animations d'origine.
			// -ZNoTP : Link « hybride » précédent. Les formes gardent leurs silhouettes pour l'instant.
			static const TCHAR* TPLink = TEXT("/Game/Art/TP/TPLink/SKM_TPLink");
			if (BodyOverride.IsEmpty() && Form.IsNone() && !FParse::Param(FCommandLine::Get(), TEXT("ZNoTP")) && ArtSkeletal(TPLink))
			{
				L.BodyArt = TPLink;
				L.AnimSet = TEXT("TPLink");
				L.Height = 165.f;
				L.Tunic = OutfitTunic(OutfitItem);
				const bool bMaster = Weapon && Weapon->Name.Contains(TEXT("Master"));
				const FZItemDef* Sh = OffhandItem.IsEmpty() ? nullptr : DB.FindItem(OffhandItem);
				const FString Sword = bMaster ? TEXT("MasterSword") : TEXT("OrdonSword");
				const FString Shield = (Sh && Sh->Name.Contains(TEXT("Hylian"))) ? TEXT("HylianShield") : TEXT("OrdonShield");
				auto Add = [&L](const FString& Part, const TCHAR* Tag)
				{
					L.Attach.Add({ FString::Printf(TEXT("/Game/Art/TP/TPLink/SKM_TPLink_%s"), *Part), Tag });
				};
				// en main (combat) et rangés dans le dos (exploration) : ShowWeaponsDrawn choisit
				Add(TEXT("Sheath"), TEXT("gear"));
				Add(Sword, TEXT("weapon"));
				Add(Sword + TEXT("Back"), TEXT("weapon_back"));
				if (Sh && WKind != TEXT("Bow"))
				{
					Add(Shield, TEXT("shield"));
					Add(Shield + TEXT("Back"), TEXT("shield_back"));
				}
				return L;
			}
			// Modèle texturé d'après les planches de Link (Tools/blender/make_character.py) : bonnet, bouclier et fourreau inclus
			// Link « hybride » (Tools/art/CH_Link, pièces séparées, LOD, textures cuites) ; -ZOldLink ou son absence : l'ancien
			static const TCHAR* LinkHybrid = TEXT("/Game/Art/Characters/LinkHybrid/SKM_LinkHybrid");
			const bool bHybrid = !FParse::Param(FCommandLine::Get(), TEXT("ZOldLink")) && ArtSkeletal(LinkHybrid) != nullptr;
			const TCHAR* LinkArt = bHybrid ? LinkHybrid : TEXT("/Game/Art/Characters/Link/SKM_Link");
			const bool bArt = BodyOverride.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("ZNoArt")) && (Form.IsNone() || Form == TEXT("Giant")) && ArtSkeletal(LinkArt) != nullptr;
			if (bArt)
			{
				L.BodyArt = LinkArt;
				L.Attach.Add({ bHybrid ? TEXT("/Game/Art/Characters/LinkHybrid/SKM_LinkHybrid_Hilt") : TEXT("/Game/Art/Characters/Link/SKM_Link_Hilt"), TEXT("hilt") });
				L.Tunic = OutfitTunic(OutfitItem);
			}
			L.BodyColor = OutfitColor(OutfitItem, FLinearColor(0.20f, 0.45f, 0.18f));
			Paint(Brown, Cream, FLinearColor(0.22f, 0.13f, 0.06f), Cream, FLinearColor(0.34f, 0.21f, 0.11f), Skin, FLinearColor(0.95f, 0.80f, 0.40f));
			if (Form == TEXT("Deku")) Paint(FLinearColor(0.35f, 0.22f, 0.1f), FLinearColor(0.4f, 0.26f, 0.12f), FLinearColor(0.25f, 0.5f, 0.18f), FLinearColor(0.4f, 0.26f, 0.12f), FLinearColor(0.4f, 0.26f, 0.12f), FLinearColor(0.45f, 0.3f, 0.14f), FLinearColor(0.25f, 0.5f, 0.18f));
			if (Form == TEXT("Goron")) Paint(FLinearColor(0.5f, 0.36f, 0.2f), FLinearColor(0.55f, 0.4f, 0.22f), FLinearColor(0.35f, 0.22f, 0.1f), FLinearColor(0.62f, 0.45f, 0.25f), FLinearColor(0.45f, 0.45f, 0.5f), FLinearColor(0.66f, 0.5f, 0.3f), FLinearColor(0.9f, 0.9f, 0.9f));
			if (Form == TEXT("Zora")) Paint(FLinearColor(0.6f, 0.8f, 0.9f), FLinearColor(0.75f, 0.88f, 0.95f), FLinearColor(0.35f, 0.25f, 0.12f), FLinearColor(0.7f, 0.85f, 0.92f), FLinearColor(0.55f, 0.8f, 0.9f), FLinearColor(0.85f, 0.92f, 0.97f), FLinearColor(0.55f, 0.8f, 0.9f), 60.f);
			if (Form == TEXT("Oni")) Paint(FLinearColor(0.3f, 0.26f, 0.22f), FLinearColor(0.22f, 0.25f, 0.45f), FLinearColor(0.7f, 0.12f, 0.12f), FLinearColor(0.75f, 0.76f, 0.82f), FLinearColor(0.3f, 0.26f, 0.22f), FLinearColor(0.92f, 0.92f, 0.95f), FLinearColor(0.95f, 0.95f, 0.98f), 50.f);
			if (Form == TEXT("Majora")) Paint(FLinearColor(0.12f, 0.08f, 0.14f), FLinearColor(0.18f, 0.1f, 0.25f), FLinearColor(0.7f, 0.5f, 0.1f), FLinearColor(0.35f, 0.15f, 0.45f), FLinearColor(0.12f, 0.08f, 0.14f), FLinearColor(0.55f, 0.2f, 0.65f), FLinearColor(0.95f, 0.8f, 0.4f));
			if (Form == TEXT("Deku"))
			{
				L.Scale = 0.62f;
				L.BodyColor = FLinearColor(0.45f, 0.30f, 0.14f);
				L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Hat_Leaf"), TEXT("Cone"), TEXT("head"), FVector(0, 0, 18), FRotator(0, 0, 0), FVector(0.55f, 0.55f, 0.35f), FLinearColor(0.25f, 0.55f, 0.18f)));
				L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), TEXT("head"), FVector(0, 16, 6), FRotator(90, 0, 0), FVector(0.12f, 0.12f, 0.2f), FLinearColor(0.35f, 0.22f, 0.1f)));
				L.Parts.Add(P(TEXT(""), TEXT("Sphere"), TEXT("head"), FVector(6, 10, 10), FRotator::ZeroRotator, FVector(0.06f), FLinearColor(1.f, 0.6f, 0.1f), 5.f));
				L.Parts.Add(P(TEXT(""), TEXT("Sphere"), TEXT("head"), FVector(-6, 10, 10), FRotator::ZeroRotator, FVector(0.06f), FLinearColor(1.f, 0.6f, 0.1f), 5.f));
				L.Height = 115.f;
				return L;
			}
			if (Form == TEXT("Goron"))
			{
				L.Scale = 1.3f;
				L.ScaleAxes = FVector(1.35f, 1.35f, 1.f);
				L.BodyColor = FLinearColor(0.62f, 0.45f, 0.25f);
				L.Parts.Add(P(TEXT(""), TEXT("Sphere"), TEXT("spine_03"), FVector(0, 5, 0), FRotator::ZeroRotator, FVector(0.75f, 0.65f, 0.7f), FLinearColor(0.55f, 0.4f, 0.22f)));
				L.Height = 240.f;
				return L;
			}
			if (Form == TEXT("Zora"))
			{
				L.Scale = 1.05f;
				L.BodyColor = FLinearColor(0.70f, 0.85f, 0.92f);
				L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Fin_Head"), TEXT("Cone"), TEXT("head"), FinOffset(), HatRotation(), FVector(0.25f, 0.12f, 0.6f), FLinearColor(0.55f, 0.8f, 0.9f)));
				L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Fin_Arm"), TEXT("Cone"), TEXT("lowerarm_l"), FVector(0, 0, 0), FRotator(0, 0, 90), FVector(0.1f, 0.3f, 0.5f), FLinearColor(0.5f, 0.85f, 0.95f), 0.6f));
				L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Fin_Arm"), TEXT("Cone"), TEXT("lowerarm_r"), FVector(0, 0, 0), FRotator(0, 0, -90), FVector(0.1f, 0.3f, 0.5f), FLinearColor(0.5f, 0.85f, 0.95f), 0.6f));
				L.Height = 195.f;
				return L;
			}
			if (Form == TEXT("Oni"))
			{
				L.Scale = 1.18f;
				L.BodyColor = FLinearColor(0.85f, 0.86f, 0.9f);
				L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Sword_Helix"), TEXT("Cube"), TEXT("hand_r"), SwordOffset(), SwordRotation(), FVector(0.07f, 0.02f, 1.4f), FLinearColor(0.7f, 0.85f, 1.f), 2.f, TEXT("weapon")));
				L.Parts.Add(P(TEXT(""), TEXT("Sphere"), TEXT("head"), FVector(10, -3, 0), FRotator::ZeroRotator, FVector(0.3f, 0.34f, 0.3f), FLinearColor(0.95f, 0.95f, 0.98f)));
				L.Height = 215.f;
				return L;
			}
			if (Form == TEXT("Giant"))
			{
				L.Scale = 2.6f;
				if (!bArt) L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Hat_Link"), TEXT("Cone"), TEXT("head"), HatLoc, HatRot, FVector(0.32f, 0.32f, 0.5f), L.BodyColor, 0.f, TEXT("hat")));
				L.Height = 470.f;
				return L;
			}
			if (Form == TEXT("Majora"))
			{
				L.BodyColor = FLinearColor(0.30f, 0.14f, 0.45f);
				L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Mask_Majora"), TEXT("Cylinder"), TEXT("head"), FVector(0, 14, 8), FRotator(90, 0, 0), FVector(0.42f, 0.42f, 0.04f), FLinearColor(0.55f, 0.2f, 0.7f), 0.8f));
				L.Parts.Add(P(TEXT(""), TEXT("Sphere"), TEXT("head"), FVector(6, 16, 12), FRotator::ZeroRotator, FVector(0.07f), FLinearColor(1.f, 0.85f, 0.1f), 6.f));
				L.Parts.Add(P(TEXT(""), TEXT("Sphere"), TEXT("head"), FVector(-6, 16, 12), FRotator::ZeroRotator, FVector(0.07f), FLinearColor(1.f, 0.85f, 0.1f), 6.f));
				L.Parts.Add(P(TEXT(""), TEXT("Cone"), TEXT("head"), FVector(14, 8, 22), FRotator(0, 0, 35), FVector(0.06f, 0.06f, 0.3f), FLinearColor(1.f, 0.7f, 0.2f)));
				L.Parts.Add(P(TEXT(""), TEXT("Cone"), TEXT("head"), FVector(-14, 8, 22), FRotator(0, 0, -35), FVector(0.06f, 0.06f, 0.3f), FLinearColor(1.f, 0.7f, 0.2f)));
				AddWeapon(FLinearColor(0.4f, 0.1f, 0.5f));
				return L;
			}
			// Link normal : bonnet vert, épée, bouclier (le modèle texturé porte déjà bonnet, bouclier et fourreau)
			if (!bArt) L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Hat_Link"), TEXT("Cone"), TEXT("head"), HatLoc, HatRot, FVector(0.32f, 0.32f, 0.5f), L.BodyColor * 1.1f, 0.f, TEXT("hat")));
			AddWeapon(FLinearColor(0.25f, 0.2f, 0.6f));
			if (bArt)
			{
				// Le modèle texturé a l'os weapon_r : l'épée suit la prise des animations d'arme
				for (FZPartSpec& Part : L.Parts)
				{
					if (Part.Tag == TEXT("weapon") && Part.Bone == FName(TEXT("hand_r"))) { Part.Bone = TEXT("weapon_r"); Part.Loc = WeaponOffset(); Part.Rot = WeaponRotation(); }
				}
			}
			if (!bArt && !OffhandItem.IsEmpty() && WKind != TEXT("Heavy") && WKind != TEXT("Bow"))
			{
				const FZItemDef* Sh = DB.FindItem(OffhandItem);
				const bool bHylian = Sh && Sh->Name.Contains(TEXT("Hylian"));
				const bool bMirror = Sh && Sh->Name.Contains(TEXT("Mirror"));
				const FLinearColor ShC = bMirror ? FLinearColor(0.8f, 0.82f, 0.9f) : (bHylian ? FLinearColor(0.12f, 0.2f, 0.55f) : FLinearColor(0.45f, 0.28f, 0.12f));
				L.Parts.Add(P(bHylian ? TEXT("/Game/Art/Meshes/SM_Shield_Hylian") : TEXT("/Game/Art/Meshes/SM_Shield_Round"), TEXT("Cylinder"), TEXT("lowerarm_l"),
					FVector(-12, 0, -8), FRotator(0, 0, 90), FVector(0.55f, 0.55f, 0.05f), ShC, 0.f, TEXT("shield")));
			}
			return L;
		}
		if (Id == TEXT("sheik"))
		{
			L.Body = TEXT("Quinn");
			L.BodyColor = FLinearColor(0.16f, 0.2f, 0.42f);
			Paint(FLinearColor(0.82f, 0.80f, 0.74f), FLinearColor(0.14f, 0.18f, 0.38f), FLinearColor(0.62f, 0.12f, 0.12f), FLinearColor(0.14f, 0.18f, 0.38f), FLinearColor(0.85f, 0.83f, 0.78f), Skin, FLinearColor(0.92f, 0.9f, 0.84f), 40.f, 152.f, 160.f);
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Wrap_Sheik"), TEXT("Sphere"), TEXT("head"), FVector(9, 1, 0), FRotator::ZeroRotator, FVector(0.19f, 0.21f, 0.2f), FLinearColor(0.92f, 0.9f, 0.85f)));
			return L;
		}
		if (Id == TEXT("mipha"))
		{
			L.Body = TEXT("Quinn");
			L.BodyColor = FLinearColor(0.72f, 0.14f, 0.16f);
			Paint(FLinearColor(0.75f, 0.14f, 0.16f), FLinearColor(0.93f, 0.94f, 0.97f), FLinearColor(0.82f, 0.84f, 0.9f), FLinearColor(0.93f, 0.94f, 0.97f), FLinearColor(0.75f, 0.14f, 0.16f), FLinearColor(0.93f, 0.94f, 0.97f), FLinearColor(0.8f, 0.16f, 0.2f), 60.f, 150.f, 162.f);
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Fin_Head"), TEXT("Cone"), TEXT("head"), FinOffset(), HatRotation(), FVector(0.28f, 0.14f, 0.65f), FLinearColor(0.8f, 0.16f, 0.2f)));
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Trident"), TEXT("Cylinder"), TEXT("hand_r"), SwordOffset(), SwordRotation(), FVector(0.04f, 0.04f, 1.6f), Gold, 0.3f, TEXT("weapon")));
			return L;
		}
		if (Id == TEXT("zora_npc"))
		{
			// Mécanicien zora de l'atelier (PNJ)
			L.Body = TEXT("Manny");
			L.BodyColor = FLinearColor(0.55f, 0.75f, 0.85f);
			Paint(FLinearColor(0.45f, 0.62f, 0.72f), FLinearColor(0.75f, 0.86f, 0.92f), FLinearColor(0.3f, 0.22f, 0.12f), FLinearColor(0.7f, 0.84f, 0.92f),
				FLinearColor(0.5f, 0.7f, 0.82f), FLinearColor(0.82f, 0.9f, 0.96f), FLinearColor(0.45f, 0.65f, 0.8f), 60.f);
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Fin_Head"), TEXT("Cone"), TEXT("head"), FinOffset(), HatRotation(), FVector(0.25f, 0.12f, 0.6f), FLinearColor(0.45f, 0.65f, 0.8f)));
			return L;
		}
		// Autres compagnons : silhouettes simples colorées
		const FZCharacterDef* Def = DB.FindCharacter(CharId);
		L.Body = (Id == TEXT("daruk") || Id == TEXT("revali")) ? TEXT("Manny") : TEXT("Quinn");
		L.BodyColor = Def ? ZNames::HexColor(Def->Color) : FLinearColor::Gray;
		if (Id == TEXT("daruk")) { L.Scale = 1.35f; L.ScaleAxes = FVector(1.3f, 1.3f, 1.f); L.Height = 245.f; Paint(FLinearColor(0.45f, 0.32f, 0.18f), FLinearColor(0.6f, 0.44f, 0.24f), FLinearColor(0.2f, 0.35f, 0.7f), FLinearColor(0.62f, 0.45f, 0.25f), FLinearColor(0.5f, 0.36f, 0.2f), FLinearColor(0.66f, 0.5f, 0.3f), FLinearColor(0.95f, 0.95f, 0.92f)); }
		if (Id == TEXT("impa")) Paint(FLinearColor(0.3f, 0.3f, 0.34f), FLinearColor(0.16f, 0.18f, 0.3f), FLinearColor(0.6f, 0.12f, 0.1f), FLinearColor(0.16f, 0.18f, 0.3f), FLinearColor(0.3f, 0.2f, 0.12f), Skin, FLinearColor(0.92f, 0.92f, 0.94f));
		if (Id == TEXT("urbosa")) Paint(FLinearColor(0.85f, 0.65f, 0.25f), FLinearColor(0.2f, 0.35f, 0.65f), FLinearColor(0.85f, 0.65f, 0.25f), FLinearColor(0.62f, 0.42f, 0.28f), FLinearColor(0.85f, 0.65f, 0.25f), FLinearColor(0.62f, 0.42f, 0.28f), FLinearColor(0.75f, 0.2f, 0.12f));
		if (Id == TEXT("revali")) Paint(FLinearColor(0.85f, 0.7f, 0.2f), FLinearColor(0.22f, 0.3f, 0.6f), FLinearColor(0.3f, 0.5f, 0.85f), FLinearColor(0.22f, 0.3f, 0.6f), FLinearColor(0.85f, 0.85f, 0.85f), FLinearColor(0.22f, 0.3f, 0.6f), FLinearColor(0.2f, 0.25f, 0.55f));
		if (Id == TEXT("midna")) Paint(FLinearColor(0.1f, 0.12f, 0.14f), FLinearColor(0.12f, 0.14f, 0.16f), FLinearColor(0.2f, 0.9f, 0.8f), FLinearColor(0.12f, 0.14f, 0.16f), FLinearColor(0.2f, 0.25f, 0.25f), FLinearColor(0.3f, 0.35f, 0.4f), FLinearColor(1.f, 0.55f, 0.1f));
		if (Id == TEXT("midna")) { L.Scale = 0.6f; L.Height = 110.f; }
		return L;
	}

	FZLook EnemyLook(const FString& Kind, const FLinearColor& Color, float Scale)
	{
		FZLook L;
		L.Body = TEXT("");
		L.bArtReplacesFallbacks = true;
		L.Scale = Scale;
		const FLinearColor Eye(1.f, 0.95f, 0.8f);
		if (Kind == TEXT("Octorok"))
		{
			L.Height = 120.f;
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Octorok"), TEXT("Sphere"), NAME_None, FVector(0, 0, 70), FRotator::ZeroRotator, FVector(1.0f, 1.0f, 1.05f), Color));
			L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), NAME_None, FVector(55, 0, 72), FRotator(90, 0, 0), FVector(0.35f, 0.35f, 0.35f), Color * 0.8f));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(35, 20, 95), FRotator::ZeroRotator, FVector(0.18f), Eye, 0.5f));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(35, -20, 95), FRotator::ZeroRotator, FVector(0.18f), Eye, 0.5f));
			for (int32 i = 0; i < 4; ++i)
			{
				const float A = FMath::DegreesToRadians(45.f + 90.f * i);
				L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), NAME_None, FVector(FMath::Cos(A) * 30, FMath::Sin(A) * 30, 15), FRotator(0, 0, 0), FVector(0.14f, 0.14f, 0.35f), Color * 0.7f));
			}
		}
		else if (Kind == TEXT("Chuchu"))
		{
			L.Height = 100.f;
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Chuchu"), TEXT("Sphere"), NAME_None, FVector(0, 0, 45), FRotator::ZeroRotator, FVector(1.1f, 1.1f, 0.9f), Color, 0.35f));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(40, 16, 65), FRotator::ZeroRotator, FVector(0.14f, 0.12f, 0.2f), FLinearColor(0.05f, 0.05f, 0.08f)));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(40, -16, 65), FRotator::ZeroRotator, FVector(0.14f, 0.12f, 0.2f), FLinearColor(0.05f, 0.05f, 0.08f)));
		}
		else if (Kind == TEXT("Construct"))
		{
			L.Height = 190.f;
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Sentinel"), TEXT("Cube"), NAME_None, FVector(0, 0, 110), FRotator::ZeroRotator, FVector(1.1f, 1.1f, 0.8f), Color));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(0, 0, 150), FRotator::ZeroRotator, FVector(0.9f, 0.9f, 0.6f), Color * 0.85f));
			L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), NAME_None, FVector(55, 0, 150), FRotator(90, 0, 0), FVector(0.3f, 0.3f, 0.1f), FLinearColor(1.f, 0.85f, 0.2f), 8.f));
			for (int32 i = 0; i < 4; ++i)
			{
				const float A = FMath::DegreesToRadians(45.f + 90.f * i);
				L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), NAME_None, FVector(FMath::Cos(A) * 55, FMath::Sin(A) * 55, 35), FRotator(FMath::Sin(A) * 20, 0, -FMath::Cos(A) * 20), FVector(0.15f, 0.15f, 0.8f), Color * 0.6f));
			}
		}
		else if (Kind == TEXT("Golem"))
		{
			const FLinearColor Stone = Color;
			const FLinearColor Core(0.2f, 0.95f, 1.f);
			L.Height = 200.f;
			L.Parts.Add(P(TEXT("/Game/Art/Meshes/SM_Golem"), TEXT("Cube"), NAME_None, FVector(0, 0, 135), FRotator::ZeroRotator, FVector(1.1f, 1.3f, 0.9f), Stone));
			L.Parts.Add(P(TEXT(""), TEXT("Cube"), NAME_None, FVector(0, 0, 80), FRotator::ZeroRotator, FVector(0.7f, 0.9f, 0.3f), Stone * 0.85f));
			L.Parts.Add(P(TEXT(""), TEXT("Cube"), NAME_None, FVector(8, 0, 200), FRotator::ZeroRotator, FVector(0.45f, 0.5f, 0.4f), Stone * 1.05f));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(52, 0, 140), FRotator::ZeroRotator, FVector(0.35f), Core, 12.f));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(30, 12, 205), FRotator::ZeroRotator, FVector(0.08f), Core, 12.f));
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(30, -12, 205), FRotator::ZeroRotator, FVector(0.08f), Core, 12.f));
			for (int32 s = -1; s <= 1; s += 2)
			{
				L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(0, s * 80, 165), FRotator::ZeroRotator, FVector(0.55f), Stone * 0.9f));
				L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), NAME_None, FVector(10, s * 95, 110), FRotator(-15, 0, s * 10), FVector(0.35f, 0.35f, 0.9f), Stone));
				L.Parts.Add(P(TEXT(""), TEXT("Cube"), NAME_None, FVector(25, s * 100, 50), FRotator::ZeroRotator, FVector(0.45f, 0.45f, 0.45f), Stone * 0.95f));
				L.Parts.Add(P(TEXT(""), TEXT("Cube"), NAME_None, FVector(0, s * 40, 30), FRotator::ZeroRotator, FVector(0.45f, 0.4f, 0.6f), Stone * 0.8f));
				L.Parts.Add(P(TEXT(""), TEXT("Cylinder"), NAME_None, FVector(25, s * 100, 80), FRotator::ZeroRotator, FVector(0.5f, 0.5f, 0.05f), Core, 6.f));
			}
		}
		else if (Kind == TEXT("Keese"))
		{
			L.Height = 60.f;
			L.Parts.Add(P(TEXT(""), TEXT("Sphere"), NAME_None, FVector(0, 0, 160), FRotator::ZeroRotator, FVector(0.4f), Color));
			L.Parts.Add(P(TEXT(""), TEXT("Plane"), NAME_None, FVector(0, 35, 165), FRotator(0, 0, 20), FVector(0.5f, 0.4f, 1.f), Color * 0.7f));
			L.Parts.Add(P(TEXT(""), TEXT("Plane"), NAME_None, FVector(0, -35, 165), FRotator(0, 0, -20), FVector(0.5f, 0.4f, 1.f), Color * 0.7f));
		}
		else
		{
			L.Body = TEXT("Manny");
			L.BodyColor = Color;
		}
		return L;
	}

	void Build(AActor* Owner, USceneComponent* Parent, USkeletalMeshComponent* Body, const FZLook& Look, TArray<UStaticMeshComponent*>& OutParts)
	{
		for (UStaticMeshComponent* Old : OutParts) { if (Old) Old->DestroyComponent(); }
		OutParts.Reset();
		TArray<USkeletalMeshComponent*> OldArt;
		Owner->GetComponents<USkeletalMeshComponent>(OldArt);
		for (USkeletalMeshComponent* C : OldArt) { if (C->ComponentHasTag(TEXT("ZArt"))) C->DestroyComponent(); }
		USkeletalMesh* Art = Body ? ArtSkeletal(Look.BodyArt) : nullptr;
		if (Art)
		{
			// Personnage texturé : matériaux de l'asset (M_ZToonTex), seule la teinte de tunique varie
			Body->SetSkeletalMesh(Art);
			Body->EmptyOverrideMaterials();
			Body->SetVisibility(true);
			auto ApplyTunic = [&Look](USkeletalMeshComponent* C)
			{
				for (int32 i = 0; i < C->GetNumMaterials(); ++i)
				{
					if (UMaterialInstanceDynamic* MID = C->CreateDynamicMaterialInstance(i))
					{
						MID->SetScalarParameterValue(TEXT("TunicHue"), Look.Tunic.Hue);
						MID->SetScalarParameterValue(TEXT("TunicSat"), Look.Tunic.Sat);
						MID->SetScalarParameterValue(TEXT("TunicVal"), Look.Tunic.Val);
					}
				}
			};
			ApplyTunic(Body);
			for (const FZArtAttach& A : Look.Attach)
			{
				USkeletalMesh* M = ArtSkeletal(A.Mesh);
				if (!M) continue;
				USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Owner);
				C->SetSkeletalMesh(M);
				C->ComponentTags = { TEXT("ZArt"), A.Tag };
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->SetupAttachment(Body);
				C->RegisterComponent();
				C->SetLeaderPoseComponent(Body);
			}
		}
		else if (Body)
		{
			USkeletalMesh* SK = Skeletal(Look.Body);
			Body->SetSkeletalMesh(SK);
			Body->SetVisibility(SK != nullptr);
			if (SK)
			{
				if (UMaterialInstanceDynamic* MID = Tint(Body, Look.BodyColor))
				{
					if (Look.Paint.bUse)
					{
						const FZPaint& Pt = Look.Paint;
						MID->SetVectorParameterValue(TEXT("BootColor"), Pt.Boot);
						MID->SetVectorParameterValue(TEXT("LegColor"), Pt.Leg);
						MID->SetVectorParameterValue(TEXT("BeltColor"), Pt.Belt);
						MID->SetVectorParameterValue(TEXT("SleeveColor"), Pt.Sleeve);
						MID->SetVectorParameterValue(TEXT("GloveColor"), Pt.Glove);
						MID->SetVectorParameterValue(TEXT("SkinColor"), Pt.Skin);
						MID->SetVectorParameterValue(TEXT("HairColor"), Pt.Hair);
						MID->SetScalarParameterValue(TEXT("SkirtH"), Pt.SkirtH);
						MID->SetScalarParameterValue(TEXT("NeckH"), Pt.NeckH);
						MID->SetScalarParameterValue(TEXT("HairH"), Pt.HairH);
					}
					else
					{
						// Sans palette : tout le corps prend la couleur principale
						for (const TCHAR* P : { TEXT("BootColor"), TEXT("LegColor"), TEXT("BeltColor"), TEXT("SleeveColor"), TEXT("GloveColor"), TEXT("SkinColor"), TEXT("HairColor") })
						{
							MID->SetVectorParameterValue(P, Look.BodyColor);
						}
					}
				}
			}
		}
		Parent->SetRelativeScale3D(Look.ScaleAxes * Look.Scale);
		const bool bArtPrimary = Look.bArtReplacesFallbacks && Look.Parts.Num() > 0 && Mesh(Look.Parts[0].Mesh) != nullptr;
		for (const FZPartSpec& S : Look.Parts)
		{
			if (bArtPrimary && S.Mesh.IsEmpty()) continue;
			UStaticMesh* M = Mesh(S.Mesh);
			const bool bFallback = (M == nullptr);
			if (!M) M = Mesh(S.Fallback);
			if (!M) continue;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
			C->SetStaticMesh(M);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(true);
			C->ComponentTags.Add(S.Tag);
			const bool bOnBone = Body && Body->GetSkeletalMeshAsset() && !S.Bone.IsNone() && Body->GetBoneIndex(S.Bone) != INDEX_NONE;
			C->SetupAttachment(bOnBone ? (USceneComponent*)Body : Parent, bOnBone ? S.Bone : NAME_None);
			C->RegisterComponent();
			// Les formes de base mesurent 100 cm : l'échelle de repli les ramène à la taille voulue.
			// Un maillage d'art (Blender) est déjà modélisé à l'échelle : on garde l'échelle 1.
			// Créature en maillage d'art : posée sur sa base (le décalage vertical de la forme de repli ne s'applique pas)
			const bool bGrounded = bArtPrimary && !bFallback && S.Bone.IsNone() && &S == &Look.Parts[0];
			C->SetRelativeLocation(bGrounded ? FVector(S.Loc.X, S.Loc.Y, -M->GetBoundingBox().Min.Z) : S.Loc);
			C->SetRelativeRotation(S.Rot);
			C->SetRelativeScale3D(bFallback ? S.Scale : FVector::OneVector);
			Tint(C, S.Color, S.Emissive);
			OutParts.Add(C);
		}
	}

	void SetArtVisible(AActor* Owner, FName Tag, bool bVisible)
	{
		if (!Owner) return;
		TArray<USkeletalMeshComponent*> Comps;
		Owner->GetComponents<USkeletalMeshComponent>(Comps);
		for (USkeletalMeshComponent* C : Comps)
		{
			if (C->ComponentHasTag(TEXT("ZArt")) && C->ComponentHasTag(Tag)) C->SetVisibility(bVisible);
		}
	}
}
