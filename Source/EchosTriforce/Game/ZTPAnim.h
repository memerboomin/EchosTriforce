// Lecteur d'animations natif des personnages de Twilight Princess (squelette d'origine, sans Animation Blueprint) :
// locomotion attente / marche / course pilotée par la vitesse, actions jouées par-dessus, fondus entre les deux.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ZTPAnim.generated.h"

class UAnimSequence;

/** Une animation échantillonnée à un instant, avec son poids dans le mélange. */
struct FZTPTrack
{
	const UAnimSequence* Seq = nullptr;
	float Time = 0.f;
	float Weight = 0.f;
};

struct FZTPAnimInstanceProxy : public FAnimInstanceProxy
{
	FZTPAnimInstanceProxy() = default;
	explicit FZTPAnimInstanceProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	TArray<FZTPTrack> Tracks;

protected:
	virtual bool Evaluate(FPoseContext& Output) override;
};

UCLASS(Transient, NotBlueprintable)
class ECHOSTRIFORCE_API UZTPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Locomotion d'exploration : Idle à l'arrêt, Walk puis Run selon la vitesse (cm/s). */
	void SetLocomotion(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun);
	void SetSpeed(float CmPerSec) { Speed = CmPerSec; }

	/** Action (combat, studio) : fondu depuis la pose courante ; en boucle, ou tient sa dernière image. */
	void Play(UAnimSequence* Seq, bool bLoop, float BlendIn = 0.15f, float Rate = 1.f);
	/** Action unique qui rend la main à la locomotion à la fin. */
	void PlayOneShot(UAnimSequence* Seq, float BlendIn = 0.1f, float BlendOut = 0.2f, float Rate = 1.f);
	void StopActions(float BlendOut = 0.2f);
	bool IsOneShotPlaying() const;

	/** Vitesses auxquelles la marche et la course de TP ne glissent pas (lecture à vitesse 1). */
	float WalkNominal = 170.f;
	float RunNominal = 480.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FZTPAnimInstanceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	struct FAction
	{
		UAnimSequence* Seq = nullptr;
		float Time = 0.f;
		float Rate = 1.f;
		bool bLoop = false;
		bool bOneShot = false;
		float Weight = 0.f;
		float Target = 1.f;
		float FadeIn = 0.15f;
		float FadeOut = 0.2f;
	};
	TArray<FAction> Actions;

	UPROPERTY(Transient) TArray<TObjectPtr<UAnimSequence>> Held; // garde les séquences vivantes pour le GC
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Run;

	float Speed = 0.f;
	float SmoothSpeed = 0.f;
	float IdleTime = 0.f;
	float Phase = 0.f;
};
