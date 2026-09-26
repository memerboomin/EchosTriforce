// Simulation de combat pure (chapitres 4 à 14, 17, 22-23) : ATB en mode Attente, dégâts, éléments, Brèche,
// Résonance, formes, invocations, duos et IA des ennemis du prototype. Aucune dépendance graphique :
// la présentation lit les événements produits et appelle AckPresentation() quand elle a fini de les jouer.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "ZTypes.h"
#include "ZProgression.h"

struct FZStatusInst
{
	EZStatus Status = EZStatus::None;
	int32 Remaining = 0;     // activations restantes (-1 = jusqu'au prochain début d'activation)
	float Magnitude = 0.f;
	int32 Source = -1;
	bool bSkipNextDecrement = false;
};

struct ECHOSTRIFORCE_API FZBattler
{
	int32 Id = -1;
	FName DefId;
	FString Name;
	bool bAlly = false;
	bool bBoss = false;
	bool bElite = false;
	int32 Level = 1;

	int32 MaxHP = 1, MaxMP = 0, HP = 1, MP = 0;
	bool bInfiniteMP = false;
	float STR = 0, MAG = 0;
	float ATQ = 0, AMAG = 0, DEF = 0, DFM = 0, AGI = 10, LCK = 5;
	float BaseATQ = 0, BaseAMAG = 0, BaseDEF = 0, BaseAGI = 10; // hors forme
	float Crit = 0, Evade = 0;
	float WeaponPower = 0, Focus = 0;
	FString WeaponKind;
	EZElement WeaponElement = EZElement::Neutral;
	bool bShield = false;
	bool bTwoHanded = false;
	float Resist[ZElementCount] = {};
	TSet<EZStatus> Immune;
	TMap<EZStatus, float> StatusRes;
	TMap<FName, float> Special;

	float J = 0.f;
	bool bKO = false;
	TArray<FZStatusInst> Statuses;

	TArray<FName> Techniques;  // préparées (alliés) ou actions (ennemis)
	TArray<FName> Forms;       // formes débloquées (Link)
	FName Spirit;
	FName Stance;

	FName Form;
	int32 FormLeft = 0;
	int32 FormRank = 2;
	int32 Corruption = 0;

	float Breach = 0.f;
	float BreachMax = 100.f;

	FName AI;
	int32 Phase = 0;
	int32 PendingPhase = 0;
	int32 Cycle = 0;
	FName Prepared;
	int32 Valves = 0;

	TMap<FName, int32> Uses;
	bool bAnalyzed = false;
	TSet<EZElement> KnownElements;
	int32 LastShockSerial = -1;

	int32 XP = 0, AP = 0, Rupees = 0;
	FString Color;
	FString Mesh;
	float Scale = 1.f;
	bool bSpectre = false;

	bool Has(EZStatus S) const;
	const FZStatusInst* Find(EZStatus S) const;
	FZStatusInst* Find(EZStatus S);
	float Mag(EZStatus S) const { const FZStatusInst* I = Find(S); return I ? I->Magnitude : 0.f; }
	bool IsAlive() const { return !bKO; }
	float GetSpecial(FName K) const { const float* V = Special.Find(K); return V ? *V : 0.f; }
	float HPPct() const { return MaxHP > 0 ? (float)HP / (float)MaxHP : 0.f; }
};

enum class EZCmd : uint8
{
	Attack, Ability, Item, Guard, Transform, Revert, Summon, Duo, Flee, Interact, Stance
};

struct FZCommand
{
	EZCmd Type = EZCmd::Attack;
	int32 Actor = -1;
	FName Id;
	TArray<int32> Targets;
	int32 Partner = -1;
};

enum class EZEvt : uint8
{
	Ready, ActionStart, ActionEnd, Damage, Heal, MPHeal, Miss, StatusOn, StatusOff, Reaction, KO, Revive,
	BreachBreak, Resonance, FormStart, FormEnd, Summon, Duo, Prepare, PrepareCancel, PhaseChange, Message,
	Victory, Defeat, Fled, Gauge, Valve, Skip, Analyze, DoT
};

struct FZEvent
{
	EZEvt Type = EZEvt::Message;
	int32 Source = -1;
	int32 Target = -1;
	int32 Amount = 0;
	EZElement Element = EZElement::Neutral;
	EZStatus Status = EZStatus::None;
	bool bCrit = false;
	bool bWeak = false;
	bool bResist = false;
	FName Id;
	FString Text;
	FString Anim;
};

enum class EZSimState : uint8
{
	Idle, Filling, AwaitingCommand, Presenting, Victory, Defeat, Fled
};

struct FZBattleResult
{
	bool bVictory = false;
	int32 XP = 0;
	int32 AP = 0;
	int32 Rupees = 0;
	int32 ResonanceKept = 0;
};

/** Paramètres d'une action résolue (technique, attaque, invocation, duo, objet). */
struct FZActionSpec
{
	FName Id;
	FString Name;
	EZAbilityKind Kind = EZAbilityKind::Physical;
	EZTarget Target = EZTarget::Enemy;
	float Power = 1.f;
	int32 Hits = 1;
	EZElement Element = EZElement::Neutral;
	int32 Breach = 0;
	float IgnoreDef = 0.f;
	float ExecutePower = 0.f;
	TArray<FZStatusApply> Apply;
	TArray<EZStatus> Remove;
	int32 TargetGauge = 0;
	int32 TargetGaugeBoss = 0;
	int32 SelfGauge = 0;
	bool bDispel = false;
	float HealPct = 0.f;
	float BarrierPct = 0.f;
	float ReviveHPPct = 0.f;
	float HealPower = 0.f;       // soin de groupe additionnel (duo)
	float HealHPPct = 0.f;       // objets
	float HealMPPct = 0.f;
	float MPRestorePct = 0.f;
	int32 ResonanceGain = 0;
	bool bIsAttack = false;
	bool bIsTechnique = false;
	bool bIgnoreBlind = false;
	bool bHookshot = false;
	bool bAnalyze = false;
	bool bCancelPrepare = false;
	bool bNoResonance = false;
	bool bLastAllyElement = false;
	float SelfDamagePct = 0.f;
	FString Anim;
};

class ECHOSTRIFORCE_API FZCombatSim
{
public:
	// --- Préparation
	int32 AddAlly(const FZCharacterState& State, const FZDerivedStats& Stats);
	int32 AddEnemy(const FZEnemyDef& Def);
	int32 AddFixture(const FZBattler& B);
	void Start(uint32 Seed, int32 InitialResonance = 0, bool bPreemptive = false, bool bAmbushed = false);

	// --- Boucle
	void Tick(float DeltaSeconds);
	bool Submit(const FZCommand& Cmd, FString* OutReason = nullptr);
	void AckPresentation();
	TArray<FZEvent> ConsumeEvents();

	EZSimState GetState() const { return State; }
	int32 GetCurrentActor() const { return Current; }
	bool IsOver() const { return State == EZSimState::Victory || State == EZSimState::Defeat || State == EZSimState::Fled; }

	// --- Requêtes pour l'interface (expliquer pourquoi une commande est grisée)
	bool CanUseAbility(int32 Actor, FName AbilityId, FString& OutReason) const;
	bool CanTransform(int32 Actor, FName FormId, FString& OutReason) const;
	bool CanSummon(int32 Actor, FString& OutReason) const;
	bool CanDuo(int32 Actor, FName DuoId, int32& OutPartner, FString& OutReason) const;
	TArray<FName> DuosFor(int32 Actor) const;
	bool CanInteractValve(int32 Actor, FString& OutReason) const;
	bool CanUseItem(int32 Actor, FName ItemId, FString& OutReason) const;
	bool CanFlee(FString& OutReason) const;
	int32 MPCost(int32 Actor, const FZAbilityDef& Ab) const;
	int32 FormCost(int32 Actor, const FZFormDef& F) const;
	TArray<int32> PredictNextActors(int32 Count) const;
	TArray<int32> LivingEnemiesOf(int32 Actor) const;
	TArray<int32> LivingAlliesOf(int32 Actor) const;
	bool IsOpponent(int32 A, int32 B) const { return Actors[A].bAlly != Actors[B].bAlly; }

	// --- Formules (publiques pour les tests de non-régression)
	int32 ComputeDamage(const FZBattler& A, const FZBattler& T, bool bPhysical, float Power, EZElement Element,
		bool bCrit, float Variance, float IgnoreDef = 0.f, float* OutElemMult = nullptr, bool bApplyReactions = false) const;
	int32 ComputeHeal(const FZBattler& A, float Power) const;
	float ElementMultiplier(const FZBattler& T, EZElement Element) const;

	// --- Données
	TArray<FZBattler> Actors;
	int32 Resonance = 0;
	TMap<FName, int32> Items;  // consommables disponibles
	int32 Bombs = 10;
	FZBattleResult Result;
	TArray<FString> Log;

	// --- Options
	bool bNoVariance = false;
	bool bNoCrit = false;
	bool bAutoAck = false;
	float SpeedScale = 1.f;
	float EnemyDamageScale = 1.f; // difficulté : Histoire 0,7 / Standard 1 / Héroïque 1,2
	bool bGiantArena = false;
	int32 ActivationSerial = 0;

private:
	void StartTurn(int32 A);
	bool BeginActivation(int32 A);
	void EndActivation(int32 A, bool bSkipFormTick = false);
	void EnemyTurn(int32 E);
	FName ChooseEnemyAction(FZBattler& E);
	void Resolve(const FZCommand& Cmd);
	bool Execute(int32 Actor, const FZActionSpec& Spec, TArray<int32> Targets);
	TArray<int32> ResolveTargets(int32 Actor, const FZActionSpec& Spec, const TArray<int32>& Wanted, bool& bRefund) const;
	void ApplyStatus(int32 Source, int32 Target, const FZStatusApply& S);
	void RemoveStatus(int32 Target, EZStatus S, bool bEvent = true);
	void DealDamage(int32 Source, int32 Target, int32 Amount, EZElement Element, bool bCrit, bool bWeak, bool bResist, const FString& Anim);
	void GainResonance(int32 Amount);
	void AddBreach(int32 Source, int32 Target, int32 Amount);
	void EnterForm(int32 A, const FZFormDef& F);
	void ExitForm(int32 A, bool bVoluntary);
	void RecomputeFormStats(FZBattler& B);
	void CheckEnd();
	void Emit(const FZEvent& E);
	void Emit(EZEvt Type, int32 Source, int32 Target, int32 Amount = 0, const FString& Text = FString());
	float SpeedMult(const FZBattler& B) const;
	float EffectiveAGI(const FZBattler& B) const;
	FZActionSpec SpecFromAbility(const FZAbilityDef& Ab) const;
	void AfterAction(int32 Actor);

	EZSimState State = EZSimState::Idle;
	FRandomStream Rng;
	int32 Current = -1;
	int32 ActiveActivationActor = -1;
	int32 ActivationGain = 0;
	TArray<int32> ReadyQueue;
	TArray<FZEvent> Events;
	EZElement LastAllyElement = EZElement::Neutral;
	bool bActionUseful = false;
	bool bActionWeakness = false;
	bool bSpendingResonance = false;
};
