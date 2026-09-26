// Ciel, soleil, nuages, brume et post-process : ambiance lumineuse « aquarelle » des maquettes.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

UCLASS()
class ECHOSTRIFORCE_API AZEnvironment : public AActor
{
	GENERATED_BODY()
public:
	AZEnvironment();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) UDirectionalLightComponent* Sun;
	UPROPERTY(VisibleAnywhere) USkyAtmosphereComponent* Sky;
	UPROPERTY(VisibleAnywhere) USkyLightComponent* SkyLight;
	UPROPERTY(VisibleAnywhere) UVolumetricCloudComponent* Clouds;
	UPROPERTY(VisibleAnywhere) UExponentialHeightFogComponent* Fog;
	UPROPERTY(VisibleAnywhere) UPostProcessComponent* Post;
};
