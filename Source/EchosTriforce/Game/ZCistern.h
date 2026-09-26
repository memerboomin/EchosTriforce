// « Citerne des mémoires » (chapitre 17) : dix salles construites en code à partir d'un kit de blocs,
// avec coffres, fontaine, atelier, vannes, ennemis visibles et porte du gardien.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZCistern.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class USphereComponent;
class UPointLightComponent;
class USkeletalMeshComponent;
class UTextRenderComponent;

struct FZRoomDef
{
	FName Id;
	FString Name;
	FString Objective;
	FVector2D Center;
	FVector2D Half;
	bool bPool = false;
	bool bRound = false;
	int32 Number = 0;
};

UENUM()
enum class EZInteractKind : uint8
{
	Chest, SavePoint, Fountain, Atelier, Valve, BossDoor, Puzzle, Companion, Sign
};

UCLASS()
class ECHOSTRIFORCE_API AZInteractable : public AActor
{
	GENERATED_BODY()
public:
	AZInteractable();
	virtual void Tick(float Dt) override;
	void Setup(EZInteractKind InKind, FName InReward, const FString& InItem, const FString& InLabel, const FString& InText);
	void RefreshState();
	FString PromptText() const;

	UPROPERTY(VisibleAnywhere) USphereComponent* Zone;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* MeshA;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* MeshB;
	UPROPERTY(VisibleAnywhere) UPointLightComponent* Light;
	/** Personnage (Mipha, mécanicien zora) : squelette animé + accessoires. */
	UPROPERTY(VisibleAnywhere) USceneComponent* FigureRoot;
	UPROPERTY(VisibleAnywhere) USkeletalMeshComponent* Figure;
	UPROPERTY() TArray<UStaticMeshComponent*> FigureParts;

	EZInteractKind Kind = EZInteractKind::Sign;
	bool bArt = false; // modèle d'art (Tools/blender/make_env_kit.py) au lieu des formes de base
	FName RewardId;
	FString ItemId;
	FString Label;
	FString Text;
	int32 PuzzleIndex = 0;
	bool bUsed = false;
private:
	float Time = 0.f;
};

UCLASS()
class ECHOSTRIFORCE_API AZEncounterActor : public AActor
{
	GENERATED_BODY()
public:
	AZEncounterActor();
	virtual void Tick(float Dt) override;
	void Setup(FName InEncounter, const TArray<FName>& InEnemies, float WanderRadius);

	UPROPERTY(VisibleAnywhere) USphereComponent* Zone;
	UPROPERTY(VisibleAnywhere) USceneComponent* Visual;
	UPROPERTY() TArray<UStaticMeshComponent*> Parts;

	FName EncounterId;
	TArray<FName> Enemies;
	FVector HomeLoc;
	float Radius = 300.f;
	float Cooldown = 0.f;
private:
	FVector WanderGoal;
	float Time = 0.f;
};

UCLASS()
class ECHOSTRIFORCE_API AZCistern : public AActor
{
	GENERATED_BODY()
public:
	AZCistern();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	static const TArray<FZRoomDef>& Rooms();
	static int32 RoomAt(const FVector& World);
	static FVector RoomSpawn(FName RoomId);
	static FVector BattleCenter(const FVector& Near, FRotator& OutFacing);

	void SpawnGameplay();
	void RefreshDoors();

	UPROPERTY() TArray<AZInteractable*> Interactables;
	UPROPERTY() TArray<AZEncounterActor*> Encounters;
	UPROPERTY() TMap<FName, UInstancedStaticMeshComponent*> Gates;

private:
	void Build();
	UInstancedStaticMeshComponent* Kit(FName Key, const FString& MeshPath, const FString& Fallback, const FLinearColor& Color, float Emissive, bool bCollide);
	void Box(FName Key, const FVector& Center, const FVector& Size, float Yaw = 0.f);
	void Cyl(FName Key, const FVector& Base, float Radius, float Height);
	void Pillar(const FVector& Base, float Height);
	void Arch(const FVector& Center, float Yaw, float Width, float Height);
	void WallWithOpenings(const FVector2D& A, const FVector2D& B, const TArray<float>& Openings, float Height, const FVector2D& Inside = FVector2D::ZeroVector, int32 Seed = 0);
	/** Instance d'un maillage d'art du kit à l'échelle 1 (pivot au sol). */
	void Put(FName Key, const FVector& Loc, float Yaw, float Scale = 1.f);
	void Waterfall(const FVector& Top, float Width, float Height, float Yaw);
	void Banner(const FVector& Base, float Yaw);
	void Gate(FName Id, const FVector& Center, float Yaw, float Width);

	UPROPERTY() TMap<FName, UInstancedStaticMeshComponent*> Kits;
	bool bBuilt = false;
};
