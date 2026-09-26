// Exploration à la troisième personne (maquette « Temple des Marées ») : Link + deux compagnons qui suivent.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ZVisuals.h"
#include "ZExploreCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class ECHOSTRIFORCE_API AZPartyCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AZPartyCharacter();
	void ApplyLook(FName CharId, const FString& Outfit, const FString& Weapon, const FString& Offhand, bool bSheathed);
	FName CharId;
	/** Jeu d'animations de TP du personnage (vide = mannequin et ABP_Unarmed). */
	FString AnimSet;
protected:
	/** Personnage de TP : vitesse, saut et atterrissage transmis au lecteur d'animations. */
	void UpdateTPAnim();
	bool bTPAir = false;
	UPROPERTY() TArray<UStaticMeshComponent*> Parts;
	UPROPERTY(VisibleAnywhere) USceneComponent* VisualRoot;
};

UCLASS()
class ECHOSTRIFORCE_API AZExploreCharacter : public AZPartyCharacter
{
	GENERATED_BODY()
public:
	AZExploreCharacter();
	virtual void Tick(float DeltaSeconds) override;
	void RefreshLook();

	UPROPERTY(VisibleAnywhere) USpringArmComponent* Boom;
	UPROPERTY(VisibleAnywhere) UCameraComponent* Camera;

	/** Historique de positions pour les compagnons (file indienne). */
	TArray<FVector> Trail;
	/** Dernier point sûr (au sol, au niveau des dalles) pour sortir d'un creux. */
	FVector LastSafe = FVector::ZeroVector;
	float LowTime = 0.f;
	float SwingT = -1.f; // coup d'épée d'exploration (attaque préventive)
	void Swing();
	bool IsSwinging() const { return SwingT >= 0.f && SwingT < 0.35f; }
};

UCLASS()
class ECHOSTRIFORCE_API AZFollower : public AZPartyCharacter
{
	GENERATED_BODY()
public:
	AZFollower();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY() AZExploreCharacter* Leader = nullptr;
	float LowTime = 0.f;
	int32 SlotIndex = 1;
};
