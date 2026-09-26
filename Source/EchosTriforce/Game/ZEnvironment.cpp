#include "ZEnvironment.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInterface.h"

AZEnvironment::AZEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(GetRootComponent());
	Sun->SetRelativeRotation(FRotator(-38.f, 35.f, 0.f));
	Sun->SetIntensity(10.f);
	Sun->SetLightColor(FLinearColor(1.f, 0.93f, 0.80f));
	Sun->bEnableLightShaftBloom = true;
	Sun->BloomScale = 0.12f;
	Sun->BloomThreshold = 4.f;
	Sun->BloomTint = FColor(255, 236, 200);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->bUseTemperature = false;
	Sun->SetDynamicShadowCascades(4);
	Sun->SetLightSourceAngle(1.2f);

	Sky = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Sky"));
	Sky->SetupAttachment(GetRootComponent());
	Sky->SetRayleighScatteringScale(0.0331f);
	Sky->SetMieScatteringScale(0.0025f);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(GetRootComponent());
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetRealTimeCapture(true);
	SkyLight->SetIntensity(1.15f);
	SkyLight->SetLowerHemisphereColor(FLinearColor(0.22f, 0.3f, 0.32f));
	SkyLight->bLowerHemisphereIsBlack = false;

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(GetRootComponent());
	Clouds->SetLayerBottomAltitude(3.5f);
	Clouds->SetLayerHeight(5.f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(GetRootComponent());
	Fog->SetRelativeLocation(FVector(0, 0, -200));
	Fog->SetFogDensity(0.012f);
	Fog->SetFogHeightFalloff(0.18f);
	Fog->SetFogInscatteringColor(FLinearColor(0.42f, 0.62f, 0.78f));
	Fog->SetDirectionalInscatteringExponent(8.f);
	Fog->SetDirectionalInscatteringColor(FLinearColor(0.8f, 0.75f, 0.6f));
	Fog->SetStartDistance(1200.f);

	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Post"));
	Post->SetupAttachment(GetRootComponent());
	Post->bUnbound = true;
	FPostProcessSettings& S = Post->Settings;
	S.bOverride_BloomIntensity = true; S.BloomIntensity = 0.55f;
	S.bOverride_BloomThreshold = true; S.BloomThreshold = 1.4f;
	S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = true; S.AutoExposureMinBrightness = -1.f;
	S.bOverride_AutoExposureMaxBrightness = true; S.AutoExposureMaxBrightness = 8.f;
	S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.05f;
	S.bOverride_AutoExposureSpeedUp = true; S.AutoExposureSpeedUp = 6.f;
	S.bOverride_AutoExposureSpeedDown = true; S.AutoExposureSpeedDown = 6.f;
	S.bOverride_ColorSaturation = true; S.ColorSaturation = FVector4(1.14f, 1.16f, 1.2f, 1.16f);
	S.bOverride_ColorContrast = true; S.ColorContrast = FVector4(1.1f, 1.1f, 1.1f, 1.08f);
	S.bOverride_ColorGain = true; S.ColorGain = FVector4(1.0f, 1.01f, 1.03f, 1.f);
	S.bOverride_ColorGammaShadows = true; S.ColorGammaShadows = FVector4(0.96f, 1.0f, 1.06f, 1.f);
	S.bOverride_VignetteIntensity = true; S.VignetteIntensity = 0.34f;
	S.bOverride_SceneFringeIntensity = true; S.SceneFringeIntensity = 0.f;
	S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.45f;
	S.bOverride_FilmToe = true; S.FilmToe = 0.58f;
	S.bOverride_FilmShoulder = true; S.FilmShoulder = 0.24f;
	S.bOverride_LumenSceneLightingQuality = true; S.LumenSceneLightingQuality = 1.f;
}

void AZEnvironment::BeginPlay()
{
	Super::BeginPlay();
	// Contour « cel-shading » si le matériau de post-process a été généré par Tools/create_assets.py
	if (UMaterialInterface* Outline = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/Materials/PP_ZOutline.PP_ZOutline"), nullptr, LOAD_Quiet | LOAD_NoWarn))
	{
		Post->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Outline));
	}
}
