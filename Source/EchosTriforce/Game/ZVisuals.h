// Visuels des personnages, formes et ennemis. Utilise les maillages de /Game/Art (générés par Tools/blender)
// quand ils existent, sinon des formes de base : le jeu reste jouable sans assets finaux.
#pragma once

#include "CoreMinimal.h"
#include "ZTypes.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimSequence;
class UZTPAnimInstance;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USceneComponent;
class AActor;

struct FZPartSpec
{
	FString Mesh;          // chemin /Game/Art/... ou forme de base « Cube », « Sphere », « Cylinder », « Cone », « Plane »
	FString Fallback;      // forme de base si le maillage n'existe pas
	FName Bone;            // os d'attache (vide = racine visuelle)
	FVector Loc = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	FLinearColor Color = FLinearColor::White;
	float Emissive = 0.f;
	FName Tag;             // « weapon », « shield », « hat »… pour masquer selon l'équipement
};

/** Peinture par bandes du corps (matériau M_ZToonSkin : hauteur de la pose de référence). */
struct FZPaint
{
	bool bUse = false;
	FLinearColor Boot = FLinearColor(0.30f, 0.18f, 0.09f);
	FLinearColor Leg = FLinearColor(0.85f, 0.80f, 0.68f);
	FLinearColor Belt = FLinearColor(0.25f, 0.15f, 0.07f);
	FLinearColor Sleeve = FLinearColor(0.85f, 0.80f, 0.68f);
	FLinearColor Glove = FLinearColor(0.36f, 0.22f, 0.12f);
	FLinearColor Skin = FLinearColor(0.95f, 0.76f, 0.60f);
	FLinearColor Hair = FLinearColor(0.95f, 0.80f, 0.40f);
	float SkirtH = 70.f;
	float NeckH = 148.f;
	float HairH = 166.f;
};

/** Maillage squelettique suiveur (même squelette que le corps) : équipement dorsal, poignée d'épée… */
struct FZArtAttach
{
	FString Mesh;
	FName Tag;             // « gear », « hilt » : masquable selon le contexte (épée dégainée en combat)
};

/** Teinte de la tunique et du bonnet sur un personnage texturé (M_ZToonTex) ; Hue < 0 = dessin d'origine. */
struct FZTunic
{
	float Hue = -1.f;
	float Sat = 1.f;
	float Val = 1.f;
};

struct FZLook
{
	FString BodyArt;       // maillage texturé d'après les planches (/Game/Art/Characters/…) ; prioritaire sur Body
	FString AnimSet;       // animations d'origine d'un personnage extrait de Twilight Princess (« TPLink ») ; vide = mannequin
	TArray<FZArtAttach> Attach;
	FZTunic Tunic;
	FString Body;          // "Manny", "Quinn" ou "" (pas de squelette : créature en pièces)
	FLinearColor BodyColor = FLinearColor::White;
	FLinearColor SkinColor = FLinearColor(0.95f, 0.78f, 0.62f);
	float Scale = 1.f;
	FVector ScaleAxes = FVector::OneVector;
	float Height = 180.f;  // hauteur visuelle approximative (popups, caméra)
	TArray<FZPartSpec> Parts;
	FZPaint Paint;
	bool bArtReplacesFallbacks = false; // créatures : un maillage d'art remplace toutes les pièces de repli
};

namespace ZVis
{
	ECHOSTRIFORCE_API UStaticMesh* Mesh(const FString& PathOrShape);
	ECHOSTRIFORCE_API USkeletalMesh* Skeletal(const FString& Name);
	/** Maillage squelettique d'art exact (sans repli sur le mannequin), nullptr s'il n'existe pas. */
	ECHOSTRIFORCE_API USkeletalMesh* ArtSkeletal(const FString& Path);
	ECHOSTRIFORCE_API FZTunic OutfitTunic(const FString& OutfitItem);
	ECHOSTRIFORCE_API UAnimSequence* Anim(const FString& Name);
	/** Animation d'un rôle (« Idle », « SwordSlash », « Slash »…) pour un jeu d'animations (FZLook::AnimSet), sinon Anim(). */
	ECHOSTRIFORCE_API UAnimSequence* AnimIn(const FString& AnimSet, const FString& Name);
	/** Personnage de TP : lecteur d'animations natif (UZTPAnimInstance) ; nullptr pour les autres. */
	ECHOSTRIFORCE_API UZTPAnimInstance* SetupTPAnim(USkeletalMeshComponent* Body, const FZLook& Look);
	/** Personnage de TP : épée et bouclier en main (combat) ou rangés dans le dos (exploration). */
	ECHOSTRIFORCE_API void ShowWeaponsDrawn(AActor* Owner, bool bDrawn);
	ECHOSTRIFORCE_API UMaterialInterface* ToonMaterial(bool bSkeletal);
	ECHOSTRIFORCE_API UMaterialInstanceDynamic* Tint(UMeshComponent* C, const FLinearColor& Color, float Emissive = 0.f);

	/** Apparence d'un personnage selon sa tenue (couleur) et sa forme active. */
	ECHOSTRIFORCE_API FZLook CharacterLook(FName CharId, FName Form, const FString& OutfitItem, const FString& WeaponItem, const FString& OffhandItem);
	ECHOSTRIFORCE_API FZLook EnemyLook(const FString& MeshKind, const FLinearColor& Color, float Scale);
	ECHOSTRIFORCE_API FLinearColor OutfitColor(const FString& OutfitItem, const FLinearColor& Default);

	/** Construit les composants d'une apparence sous Parent. Retourne les pièces créées. */
	ECHOSTRIFORCE_API void Build(AActor* Owner, USceneComponent* Parent, USkeletalMeshComponent* Body, const FZLook& Look, TArray<UStaticMeshComponent*>& OutParts);
	/** Affiche ou masque les maillages suiveurs d'un personnage texturé portant ce tag (« hilt », « gear »). */
	ECHOSTRIFORCE_API void SetArtVisible(AActor* Owner, FName Tag, bool bVisible);
}
