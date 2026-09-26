#include "ZTPAnim.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationRuntime.h"

bool FZTPAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// Moyenne pondérée des poses : chaque piste est mélangée à l'accumulation selon sa part du poids total
	float Total = 0.f;
	for (const FZTPTrack& T : Tracks)
	{
		if (!T.Seq || T.Weight <= KINDA_SMALL_NUMBER) continue;
		const FAnimExtractContext Ctx(static_cast<double>(T.Time), false);
		if (Total <= 0.f)
		{
			FAnimationPoseData Data(Output);
			T.Seq->GetAnimationPose(Data, Ctx);
		}
		else
		{
			FCompactPose Pose;
			FBlendedCurve Curve;
			UE::Anim::FStackAttributeContainer Attributes;
			Pose.SetBoneContainer(&Output.Pose.GetBoneContainer());
			Curve.InitFrom(Output.Curve);
			FAnimationPoseData Data(Pose, Curve, Attributes);
			T.Seq->GetAnimationPose(Data, Ctx);
			FAnimationPoseData OutData(Output);
			FAnimationRuntime::BlendTwoPosesTogetherInPlace(OutData, Data, Total / (Total + T.Weight));
		}
		Total += T.Weight;
	}
	if (Total <= 0.f)
	{
		Output.ResetToRefPose();
	}
	else
	{
		Output.Pose.NormalizeRotations();
	}
	return true;
}

void UZTPAnimInstance::SetLocomotion(UAnimSequence* InIdle, UAnimSequence* InWalk, UAnimSequence* InRun)
{
	Idle = InIdle;
	Walk = InWalk;
	Run = InRun;
}

void UZTPAnimInstance::Play(UAnimSequence* Seq, bool bLoop, float BlendIn, float Rate)
{
	if (!Seq) return;
	const bool bHasLoco = Idle != nullptr;
	float Current = 0.f;
	for (FAction& A : Actions)
	{
		Current += A.Weight;
		A.Target = 0.f;
		A.FadeOut = BlendIn;
	}
	FAction N;
	N.Seq = Seq;
	N.bLoop = bLoop;
	N.Rate = Rate;
	N.FadeIn = BlendIn;
	// Rien à l'écran (pose de référence) : pas de fondu depuis la pose en T
	N.Weight = (!bHasLoco && Current <= KINDA_SMALL_NUMBER) ? 1.f : 0.f;
	Actions.Add(N);
	Held.AddUnique(Seq);
}

void UZTPAnimInstance::PlayOneShot(UAnimSequence* Seq, float BlendIn, float BlendOut, float Rate)
{
	Play(Seq, false, BlendIn, Rate);
	if (Actions.Num() > 0)
	{
		Actions.Last().bOneShot = true;
		Actions.Last().FadeOut = BlendOut;
	}
}

void UZTPAnimInstance::StopActions(float BlendOut)
{
	for (FAction& A : Actions)
	{
		A.Target = 0.f;
		A.FadeOut = BlendOut;
	}
}

bool UZTPAnimInstance::IsOneShotPlaying() const
{
	for (const FAction& A : Actions)
	{
		if (A.bOneShot && A.Target > 0.f) return true;
	}
	return false;
}

void UZTPAnimInstance::NativeUpdateAnimation(float Dt)
{
	Super::NativeUpdateAnimation(Dt);

	// Actions : temps, fin des actions uniques, fondus
	float ActionWeight = 0.f;
	for (FAction& A : Actions)
	{
		const float Len = A.Seq ? A.Seq->GetPlayLength() : 0.f;
		A.Time += Dt * A.Rate;
		if (A.bLoop && Len > 0.f) A.Time = FMath::Fmod(A.Time, Len);
		else A.Time = FMath::Min(A.Time, Len);
		if (A.bOneShot && A.Target > 0.f && A.Time >= Len - A.FadeOut * A.Rate) A.Target = 0.f;
		const float Fade = A.Target > A.Weight ? A.FadeIn : A.FadeOut;
		const float Step = Fade > 0.f ? Dt / Fade : 1.f;
		A.Weight = A.Target > A.Weight ? FMath::Min(A.Target, A.Weight + Step) : FMath::Max(A.Target, A.Weight - Step);
	}
	Actions.RemoveAll([](const FAction& A) { return A.Target <= 0.f && A.Weight <= 0.f; });
	for (const FAction& A : Actions) ActionWeight += A.Weight;

	TArray<FZTPTrack> Tracks;
	// Locomotion : ce qui reste du poids après les actions
	const float Loco = Idle ? FMath::Clamp(1.f - ActionWeight, 0.f, 1.f) : 0.f;
	if (Idle)
	{
		SmoothSpeed = FMath::FInterpTo(SmoothSpeed, Speed, Dt, 10.f);
		const float S = SmoothSpeed;
		const float Moving = FMath::Clamp((S - 15.f) / (WalkNominal - 15.f), 0.f, 1.f);
		const float RunK = Run ? FMath::Clamp((S - WalkNominal) / (RunNominal - WalkNominal), 0.f, 1.f) : 0.f;
		IdleTime += Dt;
		if (Idle->GetPlayLength() > 0.f) IdleTime = FMath::Fmod(IdleTime, Idle->GetPlayLength());
		// Marche et course partagent une phase de pas : elles restent synchronisées pendant le mélange
		const float WalkLen = Walk ? FMath::Max(Walk->GetPlayLength(), 0.1f) : 1.f;
		const float RunLen = Run ? FMath::Max(Run->GetPlayLength(), 0.1f) : WalkLen;
		const float Rate = FMath::Lerp(S / WalkNominal / WalkLen, S / RunNominal / RunLen, RunK);
		Phase = FMath::Fmod(Phase + Dt * FMath::Max(Rate, 0.f), 1.f);
		Tracks.Add({ Idle, IdleTime, Loco * (1.f - Moving) });
		if (Walk) Tracks.Add({ Walk, Phase * WalkLen, Loco * Moving * (1.f - RunK) });
		if (Run) Tracks.Add({ Run, Phase * RunLen, Loco * Moving * RunK });
	}
	for (const FAction& A : Actions) Tracks.Add({ A.Seq, A.Time, A.Weight });
	GetProxyOnGameThread<FZTPAnimInstanceProxy>().Tracks = MoveTemp(Tracks);

	// On ne garde en vie que ce qui sert encore
	Held.RemoveAll([this](const TObjectPtr<UAnimSequence>& S)
	{
		return !Actions.ContainsByPredicate([&S](const FAction& A) { return A.Seq == S; });
	});
}
