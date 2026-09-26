#include "ZCombat.h"
#include "ZGameData.h"
#include "EchosTriforce.h"

namespace
{
	const FName NAME_Nereide(TEXT("nereide"));
	const FName NAME_Deluge(TEXT("E_NER_DELUGE"));
	const FName NAME_Crue(TEXT("E_NER_CRUE"));
	const FName NAME_Jet(TEXT("E_NER_JET"));
	const FName NAME_Oni(TEXT("Oni"));
	const FName NAME_Majora(TEXT("Majora"));
	const FName NAME_Wolf(TEXT("Wolf"));

	double FloorEps(double V) { return FMath::FloorToDouble(V + 1e-6); }
	// Les coefficients des fiches (P0,7, P1,6...) sont stockés en float : on les ramène à 4 décimales exactes
	double Coef(float V) { return FMath::RoundToDouble((double)V * 10000.0) / 10000.0; }
}

// ---------------------------------------------------------------------------------------------------------------------
// FZBattler

bool FZBattler::Has(EZStatus S) const { return Find(S) != nullptr; }

const FZStatusInst* FZBattler::Find(EZStatus S) const
{
	for (const FZStatusInst& I : Statuses) { if (I.Status == S) return &I; }
	return nullptr;
}

FZStatusInst* FZBattler::Find(EZStatus S)
{
	for (FZStatusInst& I : Statuses) { if (I.Status == S) return &I; }
	return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Préparation

int32 FZCombatSim::AddAlly(const FZCharacterState& St, const FZDerivedStats& D)
{
	const UZGameData& DB = UZGameData::Get();
	const FZCharacterDef* Def = DB.FindCharacter(St.Id);
	FZBattler B;
	B.Id = Actors.Num();
	B.DefId = St.Id;
	B.Name = Def ? Def->Name : St.Id.ToString();
	B.bAlly = true;
	B.Level = St.Level;
	B.MaxHP = D.MaxHP;
	B.MaxMP = D.MaxMP;
	B.HP = St.CurrentHP < 0 ? D.MaxHP : FMath::Clamp(St.CurrentHP, 0, D.MaxHP);
	B.MP = St.CurrentMP < 0 ? D.MaxMP : FMath::Clamp(St.CurrentMP, 0, D.MaxMP);
	B.bKO = B.HP <= 0;
	B.STR = D.STR; B.MAG = D.MAG;
	B.ATQ = B.BaseATQ = D.ATQ;
	B.AMAG = B.BaseAMAG = D.AMAG;
	B.DEF = B.BaseDEF = D.DEF;
	B.DFM = D.DFM;
	B.AGI = B.BaseAGI = D.AGI;
	B.LCK = D.LCK;
	B.Crit = D.Crit;
	B.Evade = D.Evade;
	B.WeaponPower = D.WeaponPower;
	B.Focus = D.Focus;
	B.WeaponKind = D.WeaponKind;
	B.WeaponElement = D.WeaponElement;
	B.bShield = D.bShield;
	B.bTwoHanded = D.bTwoHanded;
	for (int32 e = 0; e < ZElementCount; ++e) { B.Resist[e] = D.Resist[e]; }
	B.Immune = D.Immune;
	B.Special = D.Special;
	for (const TPair<FName, float>& KV : D.Special)
	{
		const FString K = KV.Key.ToString();
		if (K.StartsWith(TEXT("STATUSRES_")))
		{
			const int64 V = StaticEnum<EZStatus>()->GetValueByNameString(K.Mid(10));
			if (V != INDEX_NONE) { B.StatusRes.Add((EZStatus)V, FMath::Clamp(KV.Value, 0.f, 1.f)); }
		}
	}
	B.Techniques = St.Prepared;
	// Les outils de combat (école Reliques) ont leur propre commande et ne consomment pas les 6 emplacements
	for (FName A : ZProgression::AvailableAbilities(St))
	{
		const FZAbilityDef* Ab = DB.FindAbility(A);
		if (Ab && Ab->School == TEXT("Relic")) { B.Techniques.AddUnique(A); }
	}
	B.Spirit = St.Spirit;
	B.Stance = St.Stance;
	B.Color = Def ? Def->Color : FString();
	B.FormRank = St.Level < 8 ? 1 : St.Level < 22 ? 2 : St.Level < 35 ? 3 : St.Level < 48 ? 4 : 5;
	Actors.Add(B);
	return B.Id;
}

int32 FZCombatSim::AddEnemy(const FZEnemyDef& Def)
{
	FZBattler B;
	B.Id = Actors.Num();
	B.DefId = Def.Id;
	B.Name = Def.Name;
	B.bAlly = false;
	B.bBoss = Def.Boss;
	B.bElite = Def.Elite;
	B.Level = Def.Level;
	B.MaxHP = B.HP = Def.HP;
	B.bInfiniteMP = Def.MP >= 9999;
	B.MaxMP = B.MP = Def.MP;
	B.ATQ = B.BaseATQ = Def.ATQ;
	B.AMAG = B.BaseAMAG = Def.AMAG;
	B.DEF = B.BaseDEF = Def.DEF;
	B.DFM = Def.DFM;
	B.AGI = B.BaseAGI = Def.AGI;
	B.LCK = Def.LCK;
	for (const TPair<FString, float>& KV : Def.Resist)
	{
		B.Resist[(int32)ZNames::ElementFromString(KV.Key)] = FMath::Clamp(KV.Value, -0.5f, 0.75f);
	}
	for (const TPair<FString, float>& KV : Def.StatusResist)
	{
		const int64 V = StaticEnum<EZStatus>()->GetValueByNameString(KV.Key);
		if (V != INDEX_NONE) { B.StatusRes.Add((EZStatus)V, KV.Value); }
	}
	if (Def.Boss) { B.StatusRes.FindOrAdd(EZStatus::Sleep) = 1.f; }
	B.Techniques = Def.Abilities;
	B.AI = Def.AI;
	B.BreachMax = (float)Def.BreachThreshold;
	B.XP = Def.XP;
	B.AP = Def.AP;
	B.Rupees = Def.Rupees;
	B.Color = Def.Color;
	B.Mesh = Def.Mesh;
	B.Scale = Def.Scale;
	B.bSpectre = Def.Spectre;
	Actors.Add(B);
	return B.Id;
}

int32 FZCombatSim::AddFixture(const FZBattler& In)
{
	FZBattler B = In;
	B.Id = Actors.Num();
	B.BaseATQ = B.ATQ; B.BaseAMAG = B.AMAG; B.BaseDEF = B.DEF; B.BaseAGI = B.AGI;
	Actors.Add(B);
	return B.Id;
}

void FZCombatSim::Start(uint32 Seed, int32 InitialResonance, bool bPreemptive, bool bAmbushed)
{
	Rng.Initialize((int32)Seed);
	Resonance = FMath::Clamp(InitialResonance, 0, 100);

	// Suffixes A/B pour les ennemis homonymes
	TMap<FString, int32> Count;
	for (const FZBattler& B : Actors) { if (!B.bAlly) Count.FindOrAdd(B.Name)++; }
	TMap<FString, int32> Seen;
	for (FZBattler& B : Actors)
	{
		if (!B.bAlly && Count[B.Name] > 1)
		{
			const int32 N = Seen.FindOrAdd(B.Name)++;
			B.Name += FString::Printf(TEXT(" %c"), TCHAR('A' + N));
		}
	}
	for (FZBattler& B : Actors)
	{
		if (B.bKO) { B.J = 0; continue; }
		float J = Rng.FRandRange(0.f, 250.f) + 8.f * B.AGI;
		if (B.bAlly && bPreemptive) J += 200.f;
		if (!B.bAlly && bAmbushed) J += 200.f;
		J += 1000.f * B.GetSpecial(TEXT("START_GAUGE_PCT"));
		B.J = FMath::Clamp(J, 0.f, 900.f);
	}
	State = EZSimState::Filling;
	Log.Add(TEXT("Le combat commence."));
}

// ---------------------------------------------------------------------------------------------------------------------
// Boucle ATB (mode Attente)

float FZCombatSim::EffectiveAGI(const FZBattler& B) const
{
	float AGI = FMath::Min(80.f, B.AGI);
	if (B.Has(EZStatus::Mire) && !B.Has(EZStatus::Slow)) { AGI *= 0.8f; }
	return AGI;
}

float FZCombatSim::SpeedMult(const FZBattler& B) const
{
	// Seul le plus fort effet de chaque catégorie s'applique
	float M = 1.f;
	if (B.Has(EZStatus::Haste)) M *= 1.25f;
	if (B.Has(EZStatus::Slow)) M *= 0.8f;
	return M;
}

void FZCombatSim::Tick(float Dt)
{
	if (State != EZSimState::Filling) return;

	ReadyQueue.RemoveAll([this](int32 A) { return Actors[A].bKO; });
	if (ReadyQueue.Num() == 0)
	{
		struct FCross { float T; int32 Id; };
		TArray<FCross> Crossings;
		for (FZBattler& B : Actors)
		{
			if (B.bKO) continue;
			const float Rate = (100.f + 3.f * EffectiveAGI(B)) * SpeedMult(B) * SpeedScale;
			if (B.J >= 1000.f)
			{
				Crossings.Add({ 0.f, B.Id });
				continue;
			}
			const float NewJ = B.J + Rate * Dt;
			if (NewJ >= 1000.f)
			{
				Crossings.Add({ (1000.f - B.J) / FMath::Max(Rate, 1.f), B.Id });
			}
			B.J = FMath::Min(1000.f, NewJ);
		}
		Crossings.Sort([](const FCross& A, const FCross& B) { return A.T < B.T || (A.T == B.T && A.Id < B.Id); });
		for (const FCross& C : Crossings) { ReadyQueue.AddUnique(C.Id); }
	}
	if (ReadyQueue.Num() > 0)
	{
		const int32 A = ReadyQueue[0];
		ReadyQueue.RemoveAt(0);
		StartTurn(A);
	}
}

void FZCombatSim::StartTurn(int32 A)
{
	++ActivationSerial;
	ActiveActivationActor = A;
	ActivationGain = 0;
	if (!BeginActivation(A))
	{
		if (!IsOver()) State = EZSimState::Presenting;
		if (bAutoAck) AckPresentation();
		return;
	}
	if (Actors[A].bAlly)
	{
		Current = A;
		State = EZSimState::AwaitingCommand;
		Emit(EZEvt::Ready, A, A);
	}
	else
	{
		EnemyTurn(A);
		if (!IsOver()) State = EZSimState::Presenting;
		if (bAutoAck) AckPresentation();
	}
}

bool FZCombatSim::BeginActivation(int32 A)
{
	FZBattler& B = Actors[A];
	// Garde : jusqu'au début de la prochaine activation du porteur
	if (B.Has(EZStatus::Guard)) { RemoveStatus(A, EZStatus::Guard, false); }
	for (FZBattler& O : Actors)
	{
		if (FZStatusInst* I = O.Find(EZStatus::Intercept))
		{
			if (I->Source == A) { RemoveStatus(O.Id, EZStatus::Intercept, false); }
		}
	}
	if (B.Has(EZStatus::Sleep) || B.Has(EZStatus::Freeze))
	{
		const bool bSleep = B.Has(EZStatus::Sleep);
		Emit(EZEvt::Skip, A, A, 0, FString::Printf(TEXT("%s perd son activation (%s)"), *B.Name, bSleep ? TEXT("Sommeil") : TEXT("Gel")));
		RemoveStatus(A, bSleep ? EZStatus::Sleep : EZStatus::Freeze);
		FZStatusApply Stab; Stab.Status = EZStatus::Stability; Stab.Duration = 2; Stab.Chance = 1.f;
		ApplyStatus(A, A, Stab);
		if (FZStatusInst* S = Actors[A].Find(EZStatus::Stability)) { S->bSkipNextDecrement = true; }
		EndActivation(A);
		return false;
	}
	return true;
}

void FZCombatSim::EndActivation(int32 A, bool bSkipFormTick)
{
	FZBattler& B = Actors[A];
	if (!B.bKO)
	{
		// Dégâts périodiques (sans critique)
		if (B.Has(EZStatus::Poison))
		{
			int32 D = FMath::Min(100, (int32)FloorEps(B.MaxHP * 0.03));
			if (B.bAlly) D = FMath::Min(D, B.HP - 1); // non létal sur les alliés
			if (D > 0) { B.HP -= D; Emit(EZEvt::DoT, A, A, D, TEXT("Poison")); }
		}
		if (B.Has(EZStatus::Burn))
		{
			const int32 D = FMath::Min(120, (int32)FloorEps(B.MaxHP * 0.04));
			if (D > 0) { DealDamage(-1, A, D, EZElement::Fire, false, false, false, TEXT("Burn")); }
		}
		float RegenPct = B.GetSpecial(TEXT("REGEN_PCT")) + B.Mag(EZStatus::Regen);
		if (!B.bKO && RegenPct > 0.f && B.HP < B.MaxHP)
		{
			const int32 H = FMath::Min(B.MaxHP - B.HP, (int32)FloorEps(B.MaxHP * RegenPct));
			if (H > 0) { B.HP += H; Emit(EZEvt::Heal, A, A, H, TEXT("Régénération")); }
		}
	}
	// Durées : diminuent à la fin des activations de la cible
	for (int32 i = B.Statuses.Num() - 1; i >= 0; --i)
	{
		FZStatusInst& I = B.Statuses[i];
		if (I.Remaining < 0) continue;
		if (I.bSkipNextDecrement) { I.bSkipNextDecrement = false; continue; }
		if (--I.Remaining <= 0)
		{
			const EZStatus S = I.Status;
			B.Statuses.RemoveAt(i);
			Emit(EZEvt::StatusOff, A, A, 0, ZNames::StatusName(S));
			Events.Last().Status = S;
		}
	}
	if (!B.bKO && !B.Form.IsNone() && !bSkipFormTick)
	{
		const bool bCorrupted = B.Form == NAME_Majora && B.Corruption >= 60;
		if (--B.FormLeft <= 0 || bCorrupted) { ExitForm(A, false); }
	}
	B.J = 0.f;
	if (Current == A) Current = -1;
	ActiveActivationActor = -1;
	CheckEnd();
}

void FZCombatSim::AckPresentation()
{
	if (State == EZSimState::Presenting)
	{
		State = EZSimState::Filling;
	}
}

TArray<FZEvent> FZCombatSim::ConsumeEvents()
{
	TArray<FZEvent> Out = MoveTemp(Events);
	Events.Reset();
	return Out;
}

void FZCombatSim::Emit(const FZEvent& E)
{
	Events.Add(E);
	if (!E.Text.IsEmpty() && (E.Type == EZEvt::Message || E.Type == EZEvt::ActionStart || E.Type == EZEvt::Skip))
	{
		Log.Add(E.Text);
	}
}

void FZCombatSim::Emit(EZEvt Type, int32 Source, int32 Target, int32 Amount, const FString& Text)
{
	FZEvent E;
	E.Type = Type; E.Source = Source; E.Target = Target; E.Amount = Amount; E.Text = Text;
	Emit(E);
}

// ---------------------------------------------------------------------------------------------------------------------
// Requêtes

TArray<int32> FZCombatSim::LivingEnemiesOf(int32 Actor) const
{
	TArray<int32> Out;
	for (const FZBattler& B : Actors) { if (!B.bKO && B.bAlly != Actors[Actor].bAlly) Out.Add(B.Id); }
	return Out;
}

TArray<int32> FZCombatSim::LivingAlliesOf(int32 Actor) const
{
	TArray<int32> Out;
	for (const FZBattler& B : Actors) { if (!B.bKO && B.bAlly == Actors[Actor].bAlly) Out.Add(B.Id); }
	return Out;
}

int32 FZCombatSim::MPCost(int32 Actor, const FZAbilityDef& Ab) const
{
	const FZBattler& B = Actors[Actor];
	float Mult = 1.f + B.GetSpecial(TEXT("MP_COST_PCT"));
	if (Ab.Element != EZElement::Neutral)
	{
		Mult += B.GetSpecial(FName(*(TEXT("MP_COST_") + StaticEnum<EZElement>()->GetNameStringByValue((int64)Ab.Element))));
	}
	if (Ab.School == TEXT("Relic")) Mult += B.GetSpecial(TEXT("TOOL_MP_PCT"));
	if (B.Has(EZStatus::MPCostUp)) Mult += 0.2f;
	return FMath::Max(0, FMath::RoundToInt(Ab.MP * FMath::Max(0.f, Mult)));
}

int32 FZCombatSim::FormCost(int32 Actor, const FZFormDef& F) const
{
	return FMath::Max(0, F.Resonance + (int32)Actors[Actor].GetSpecial(TEXT("FORM_COST")));
}

bool FZCombatSim::CanUseAbility(int32 Actor, FName AbilityId, FString& Why) const
{
	const UZGameData& DB = UZGameData::Get();
	const FZAbilityDef* Ab = DB.FindAbility(AbilityId);
	if (!Ab) { Why = TEXT("Technique inconnue"); return false; }
	const FZBattler& B = Actors[Actor];
	if (B.bKO) { Why = TEXT("KO"); return false; }
	if (Ab->Kind == EZAbilityKind::Passive) { Why = TEXT("Aptitude passive"); return false; }
	const bool bFormCmd = Ab->School == TEXT("Form");
	if (!B.Form.IsNone())
	{
		const FZFormDef* F = DB.FindForm(B.Form);
		if (!bFormCmd || !F || !F->Commands.Contains(AbilityId)) { Why = TEXT("Indisponible sous cette forme"); return false; }
	}
	else if (bFormCmd) { Why = TEXT("Exige la forme correspondante"); return false; }
	if (Ab->IsSilenceable() && B.Has(EZStatus::Silence)) { Why = TEXT("Silence : magie et chants bloqués"); return false; }
	if (!B.bInfiniteMP && MPCost(Actor, *Ab) > B.MP) { Why = TEXT("PM insuffisants"); return false; }
	if (Ab->Resonance > Resonance) { Why = FString::Printf(TEXT("Résonance insuffisante (%d requis)"), Ab->Resonance); return false; }
	if (Ab->PerBattle > 0)
	{
		const int32* U = B.Uses.Find(AbilityId);
		if (U && *U >= Ab->PerBattle) { Why = TEXT("Déjà utilisé dans ce combat"); return false; }
	}
	if (Ab->RequireHPPct > 0.f)
	{
		const float Need = FMath::Min(Ab->RequireHPPct, 1.f - B.GetSpecial(TEXT("BEAM_HP_RELIEF")));
		if (B.HPPct() + 1e-4f < Need) { Why = FString::Printf(TEXT("Exige PV à %d %%"), FMath::RoundToInt(Need * 100)); return false; }
	}
	if (Ab->Requires == TEXT("shield") && !B.bShield) { Why = TEXT("Exige un bouclier"); return false; }
	if (Ab->Requires == TEXT("heavy") && B.WeaponKind != TEXT("Heavy")) { Why = TEXT("Exige une arme lourde"); return false; }
	if (Ab->Requires == TEXT("spear") && B.WeaponKind != TEXT("Spear")) { Why = TEXT("Exige une lance"); return false; }
	if (Ab->HasFlag(TEXT("ConsumesBomb")) && Bombs <= 0) { Why = TEXT("Plus de bombes"); return false; }
	if (Ab->Kind == EZAbilityKind::Revive)
	{
		bool bAnyKO = false;
		for (const FZBattler& O : Actors) { bAnyKO |= (O.bKO && O.bAlly == B.bAlly); }
		if (!bAnyKO) { Why = TEXT("Aucun allié KO"); return false; }
	}
	return true;
}

bool FZCombatSim::CanTransform(int32 Actor, FName FormId, FString& Why) const
{
	const FZFormDef* F = UZGameData::Get().FindForm(FormId);
	const FZBattler& B = Actors[Actor];
	if (!F) { Why = TEXT("Forme inconnue"); return false; }
	if (!B.Forms.Contains(FormId)) { Why = TEXT("Forme non débloquée"); return false; }
	if (!B.Form.IsNone()) { Why = TEXT("Déjà transformé"); return false; }
	if (F->ArenaOnly && !bGiantArena) { Why = TEXT("Arène incompatible (giant_allowed)"); return false; }
	const int32 Cost = FormCost(Actor, *F);
	if (Resonance < Cost) { Why = FString::Printf(TEXT("Résonance insuffisante (%d requis)"), Cost); return false; }
	return true;
}

bool FZCombatSim::CanSummon(int32 Actor, FString& Why) const
{
	const FZBattler& B = Actors[Actor];
	const FZSpiritDef* S = UZGameData::Get().FindSpirit(B.Spirit);
	if (!S) { Why = TEXT("Aucun esprit lié"); return false; }
	if (B.Uses.Contains(S->Id)) { Why = TEXT("Invocation déjà utilisée"); return false; }
	if (Resonance < S->Resonance) { Why = FString::Printf(TEXT("Résonance insuffisante (%d requis)"), S->Resonance); return false; }
	return true;
}

TArray<FName> FZCombatSim::DuosFor(int32 Actor) const
{
	TArray<FName> Out;
	for (const FZDuoDef& D : UZGameData::Get().Duos)
	{
		if (D.A == Actors[Actor].DefId || D.B == Actors[Actor].DefId)
		{
			const FName Other = D.A == Actors[Actor].DefId ? D.B : D.A;
			for (const FZBattler& O : Actors) { if (O.bAlly && O.DefId == Other) { Out.Add(D.Id); break; } }
		}
	}
	return Out;
}

bool FZCombatSim::CanDuo(int32 Actor, FName DuoId, int32& OutPartner, FString& Why) const
{
	const FZDuoDef* D = UZGameData::Get().FindDuo(DuoId);
	OutPartner = -1;
	if (!D) { Why = TEXT("Duo inconnu"); return false; }
	const FZBattler& B = Actors[Actor];
	const FName Other = D->A == B.DefId ? D->B : D->A;
	for (const FZBattler& O : Actors) { if (O.bAlly && O.DefId == Other) OutPartner = O.Id; }
	if (OutPartner < 0) { Why = TEXT("Partenaire absent"); return false; }
	const FZBattler& P = Actors[OutPartner];
	if (P.bKO) { Why = FString::Printf(TEXT("%s est KO"), *P.Name); return false; }
	if (P.Has(EZStatus::Sleep) || P.Has(EZStatus::Freeze)) { Why = FString::Printf(TEXT("%s est entravé"), *P.Name); return false; }
	if (!ReadyQueue.Contains(OutPartner)) { Why = FString::Printf(TEXT("%s doit aussi être prêt (jauge pleine)"), *P.Name); return false; }
	if (!D->RequireForm.IsNone())
	{
		const FZBattler& LinkB = D->A == B.DefId ? B : P;
		if (LinkB.Form != D->RequireForm) { Why = TEXT("Exige Link en loup"); return false; }
	}
	if (Resonance < D->Resonance) { Why = FString::Printf(TEXT("Résonance insuffisante (%d requis)"), D->Resonance); return false; }
	const FZBattler& AB = D->A == B.DefId ? B : P;
	const FZBattler& BB = D->A == B.DefId ? P : B;
	if (AB.MP < D->MPA || BB.MP < D->MPB) { Why = TEXT("PM insuffisants"); return false; }
	return true;
}

bool FZCombatSim::CanInteractValve(int32 Actor, FString& Why) const
{
	for (const FZBattler& B : Actors)
	{
		if (!B.bKO && B.Prepared == NAME_Deluge) return true;
	}
	Why = TEXT("Aucune vanne ne peut infléchir la situation");
	return false;
}

bool FZCombatSim::CanUseItem(int32 Actor, FName ItemId, FString& Why) const
{
	const FZConsumableDef* C = UZGameData::Get().FindConsumable(ItemId);
	if (!C) { Why = TEXT("Objet inconnu"); return false; }
	const int32* N = Items.Find(ItemId);
	if (!N || *N <= 0) { Why = TEXT("Aucun exemplaire"); return false; }
	if (Actors[Actor].Form == NAME_Wolf) { Why = TEXT("Le loup ne peut pas utiliser d'objets"); return false; }
	if (C->Target == EZTarget::DeadAlly)
	{
		bool bAnyKO = false;
		for (const FZBattler& O : Actors) { bAnyKO |= (O.bKO && O.bAlly); }
		if (!bAnyKO) { Why = TEXT("Aucun allié KO"); return false; }
	}
	return true;
}

bool FZCombatSim::CanFlee(FString& Why) const
{
	for (const FZBattler& B : Actors)
	{
		if (!B.bAlly && B.bBoss && !B.bKO) { Why = TEXT("Impossible de fuir un gardien"); return false; }
	}
	return true;
}

TArray<int32> FZCombatSim::PredictNextActors(int32 Count) const
{
	struct FNext { float T; int32 Id; };
	TArray<FNext> Timeline;
	float Base = 0.f;
	for (int32 A : ReadyQueue) { Timeline.Add({ Base, A }); Base += 0.001f; }
	for (const FZBattler& B : Actors)
	{
		if (B.bKO) continue;
		const float Rate = (100.f + 3.f * EffectiveAGI(B)) * SpeedMult(B) * SpeedScale;
		const bool bQueued = ReadyQueue.Contains(B.Id) || B.Id == Current;
		float T = bQueued ? 1000.f / Rate : (1000.f - B.J) / Rate;
		for (int32 k = 0; k < Count; ++k)
		{
			Timeline.Add({ T + 0.01f, B.Id });
			T += 1000.f / Rate;
		}
	}
	Timeline.Sort([](const FNext& A, const FNext& B) { return A.T < B.T || (A.T == B.T && A.Id < B.Id); });
	TArray<int32> Out;
	for (const FNext& N : Timeline)
	{
		if (Out.Num() >= Count) break;
		Out.Add(N.Id);
	}
	return Out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Formules

float FZCombatSim::ElementMultiplier(const FZBattler& T, EZElement Element) const
{
	float E = 1.f;
	if (Element != EZElement::Neutral) { E = 1.f - T.Resist[(int32)Element]; }
	if (!T.Form.IsNone())
	{
		if (const FZFormDef* F = UZGameData::Get().FindForm(T.Form))
		{
			if (const float* Tk = F->Taken.Find(StaticEnum<EZElement>()->GetNameStringByValue((int64)Element)))
			{
				E = FMath::Max(0.1f, E * *Tk);
			}
		}
	}
	return FMath::Max(0.f, E);
}

int32 FZCombatSim::ComputeDamage(const FZBattler& A, const FZBattler& T, bool bPhysical, float Power, EZElement Element,
	bool bCrit, float Variance, float IgnoreDef, float* OutElemMult, bool bApplyReactions) const
{
	const double Atk = bPhysical ? A.ATQ : A.AMAG;
	double Def = bPhysical ? T.DEF : T.DFM;
	if (bPhysical && T.Has(EZStatus::Fragile)) Def *= 1.0 - FMath::Max(0.15f, T.Mag(EZStatus::Fragile));
	if (!bPhysical && T.Has(EZStatus::SprUp)) Def *= 1.0 + T.Mag(EZStatus::SprUp);
	Def *= 1.0 - IgnoreDef;
	const double Base = (2.0 * Atk + 10.0) * Coef(Power) * 100.0 / (100.0 + Def);

	double E = Coef(ElementMultiplier(T, Element));
	if (bApplyReactions)
	{
		if (T.Has(EZStatus::Wet) && Element == EZElement::Thunder) E *= 1.5;
		if (T.Has(EZStatus::Wet) && Element == EZElement::Fire) E *= 0.75;
		if (T.Has(EZStatus::Freeze) && bPhysical) E *= 1.25;
	}
	if (OutElemMult) *OutElemMult = (float)E;
	if (E <= 0.0) return 0;

	const double C = bCrit ? 1.5 : 1.0;
	double G = 1.0;
	if (T.Has(EZStatus::Guard))
	{
		const float Strength = FMath::Max(0.5f, T.GetSpecial(TEXT("GUARD_STRENGTH")));
		G = 1.0 - Strength;
	}
	// Bonus de dégâts distincts : additionnés, plafonnés à +50 %
	float Bonus = A.GetSpecial(TEXT("DMG_ALL"));
	if (bPhysical) Bonus += A.GetSpecial(TEXT("DMG_PHYS"));
	if (Element != EZElement::Neutral)
	{
		Bonus += A.GetSpecial(FName(*(TEXT("DMG_") + StaticEnum<EZElement>()->GetNameStringByValue((int64)Element))));
	}
	if (bPhysical && A.WeaponKind == TEXT("Blade")) Bonus += A.GetSpecial(TEXT("DMG_BLADE"));
	if (T.bSpectre) Bonus += A.GetSpecial(TEXT("DMG_VS_Spectre"));
	Bonus = FMath::Min(Bonus, 0.5f);

	double Taken = T.GetSpecial(TEXT("TAKEN_ALL"));
	if (bPhysical) Taken += T.GetSpecial(TEXT("TAKEN_PHYS"));
	Taken = FMath::Clamp(Taken, -0.5, 0.5);

	double S = 1.0;
	if (!bPhysical && T.Has(EZStatus::MagicShield)) S *= T.Mag(EZStatus::MagicShield) > 0.f ? T.Mag(EZStatus::MagicShield) : 0.5;
	if (bPhysical && T.Has(EZStatus::Rampart)) S *= 0.75;
	if (T.Has(EZStatus::ZoraBarrier)) S *= 0.65;
	if (T.Has(EZStatus::Exposed)) S *= 1.25;
	if (T.Has(EZStatus::Marked)) S *= 1.0 + T.Mag(EZStatus::Marked);

	const double Dmg = FloorEps(Base * E * C * G * Variance * (1.0 + Bonus) * (1.0 + Taken) * S);
	return (int32)FMath::Clamp(Dmg, 1.0, 9999.0);
}

int32 FZCombatSim::ComputeHeal(const FZBattler& A, float Power) const
{
	return (int32)FloorEps((2.0 * A.AMAG + 30.0) * Coef(Power) * (1.0 + Coef(A.GetSpecial(TEXT("HEAL_DONE")))));
}

// ---------------------------------------------------------------------------------------------------------------------
// Commandes

bool FZCombatSim::Submit(const FZCommand& Cmd, FString* OutReason)
{
	FString Why;
	auto Fail = [&](const FString& R) { if (OutReason) *OutReason = R; return false; };
	if (State != EZSimState::AwaitingCommand || Cmd.Actor != Current) return Fail(TEXT("Ce n'est pas le tour de ce personnage"));
	const UZGameData& DB = UZGameData::Get();

	switch (Cmd.Type)
	{
	case EZCmd::Ability:
		if (!CanUseAbility(Cmd.Actor, Cmd.Id, Why)) return Fail(Why);
		break;
	case EZCmd::Transform:
		if (!CanTransform(Cmd.Actor, Cmd.Id, Why)) return Fail(Why);
		break;
	case EZCmd::Summon:
		if (!CanSummon(Cmd.Actor, Why)) return Fail(Why);
		break;
	case EZCmd::Duo:
	{
		int32 Partner;
		if (!CanDuo(Cmd.Actor, Cmd.Id, Partner, Why)) return Fail(Why);
		break;
	}
	case EZCmd::Interact:
		if (!CanInteractValve(Cmd.Actor, Why)) return Fail(Why);
		break;
	case EZCmd::Item:
		if (!CanUseItem(Cmd.Actor, Cmd.Id, Why)) return Fail(Why);
		break;
	case EZCmd::Flee:
		if (!CanFlee(Why)) return Fail(Why);
		break;
	case EZCmd::Revert:
		if (Actors[Cmd.Actor].Form.IsNone()) return Fail(TEXT("Aucune forme active"));
		// Retour volontaire gratuit au début d'une activation : consomme la durée restante, pas l'activation
		ExitForm(Cmd.Actor, true);
		Emit(EZEvt::Ready, Cmd.Actor, Cmd.Actor);
		return true;
	default:
		break;
	}
	(void)DB;
	Resolve(Cmd);
	if (!IsOver()) State = EZSimState::Presenting;
	if (bAutoAck) AckPresentation();
	return true;
}

FZActionSpec FZCombatSim::SpecFromAbility(const FZAbilityDef& Ab) const
{
	FZActionSpec S;
	S.Id = Ab.Id;
	S.Name = Ab.Name;
	S.Kind = Ab.Kind;
	S.Target = Ab.Target;
	S.Power = Ab.Power;
	S.Hits = FMath::Max(1, Ab.Hits);
	S.Element = Ab.Element;
	S.Breach = Ab.Breach;
	S.IgnoreDef = Ab.IgnoreDefPct;
	S.ExecutePower = Ab.ExecutePower;
	S.Apply = Ab.Apply;
	S.Remove = Ab.Remove;
	S.TargetGauge = Ab.TargetGauge;
	S.TargetGaugeBoss = Ab.TargetGaugeBoss != 0 ? Ab.TargetGaugeBoss : Ab.TargetGauge;
	S.SelfGauge = Ab.SelfGauge;
	S.bDispel = Ab.Dispel;
	S.HealPct = Ab.HealPct;
	S.BarrierPct = Ab.BarrierPct;
	S.ReviveHPPct = Ab.ReviveHPPct;
	S.ResonanceGain = Ab.ResonanceGain;
	S.bIsTechnique = Ab.School != TEXT("Basic") && Ab.School != TEXT("Enemy");
	S.bIgnoreBlind = Ab.HasFlag(TEXT("IgnoreBlind"));
	S.bHookshot = Ab.HasFlag(TEXT("Hookshot"));
	S.bAnalyze = Ab.HasFlag(TEXT("Analyze"));
	S.bLastAllyElement = Ab.HasFlag(TEXT("LastAllyElement"));
	S.SelfDamagePct = Ab.HasFlag(TEXT("SelfDamage10")) ? 0.1f : 0.f;
	S.Anim = Ab.Anim;
	return S;
}

void FZCombatSim::Resolve(const FZCommand& Cmd)
{
	const UZGameData& DB = UZGameData::Get();
	const int32 A = Cmd.Actor;
	FZBattler& B = Actors[A];
	bSpendingResonance = false;

	switch (Cmd.Type)
	{
	case EZCmd::Attack:
	{
		FZActionSpec S;
		S.Id = TEXT("ATTACK");
		S.Name = TEXT("Attaquer");
		S.Kind = EZAbilityKind::Physical;
		S.Target = EZTarget::Enemy;
		S.Power = 1.f + B.GetSpecial(TEXT("ATTACK_POWER"));
		if (B.WeaponKind == TEXT("Heavy") && B.Form.IsNone()) S.Power = FMath::Max(S.Power, 1.25f);
		S.Element = B.Form.IsNone() ? B.WeaponElement : EZElement::Neutral;
		S.bIsAttack = true;
		S.Anim = B.WeaponKind == TEXT("Bow") ? TEXT("Shoot") : (B.WeaponKind == TEXT("Spear") ? TEXT("Thrust") : TEXT("Slash"));
		Execute(A, S, Cmd.Targets);
		break;
	}
	case EZCmd::Ability:
	{
		const FZAbilityDef* Ab = DB.FindAbility(Cmd.Id);
		const int32 Cost = B.bInfiniteMP ? 0 : MPCost(A, *Ab);
		B.MP -= Cost;
		if (Ab->Resonance > 0) { Resonance -= Ab->Resonance; bSpendingResonance = true; Emit(EZEvt::Resonance, A, A, -Ab->Resonance); }
		B.Uses.FindOrAdd(Cmd.Id)++;
		if (Ab->HasFlag(TEXT("ConsumesBomb"))) --Bombs;
		FZActionSpec S = SpecFromAbility(*Ab);
		if (Ab->HasFlag(TEXT("Valve")))
		{
			FZCommand V = Cmd; V.Type = EZCmd::Interact;
			Resolve(V);
			return;
		}
		if (S.SelfDamagePct > 0.f)
		{
			const int32 D = FMath::Min(B.HP - 1, (int32)FloorEps(B.MaxHP * S.SelfDamagePct));
			if (D > 0) { B.HP -= D; Emit(EZEvt::DoT, A, A, D, S.Name); }
		}
		if (!Execute(A, S, Cmd.Targets))
		{
			B.MP += Cost; // soin sans cible valide : remboursé
		}
		break;
	}
	case EZCmd::Item:
	{
		const FZConsumableDef* C = DB.FindConsumable(Cmd.Id);
		FZActionSpec S;
		S.Id = C->Id;
		S.Name = C->Name;
		S.Target = C->Target;
		S.Kind = C->Revive > 0.f ? EZAbilityKind::Revive : ((C->HealHPPct > 0.f || C->HealMPPct > 0.f) ? EZAbilityKind::Heal : EZAbilityKind::Support);
		S.HealHPPct = C->HealHPPct;
		S.HealMPPct = C->HealMPPct;
		S.ReviveHPPct = C->Revive;
		S.Remove = C->Cleanse;
		S.Anim = TEXT("Item");
		S.Power = 0.f;
		// Consommation seulement après validation finale
		if (Execute(A, S, Cmd.Targets)) { Items.FindOrAdd(Cmd.Id)--; }
		break;
	}
	case EZCmd::Guard:
	{
		FZEvent E; E.Type = EZEvt::ActionStart; E.Source = A; E.Target = A; E.Text = FString::Printf(TEXT("%s se met en garde"), *B.Name); E.Anim = TEXT("Guard"); E.Id = TEXT("GUARD");
		Emit(E);
		FZStatusInst G; G.Status = EZStatus::Guard; G.Remaining = -1; G.Source = A;
		B.Statuses.RemoveAll([](const FZStatusInst& I) { return I.Status == EZStatus::Guard; });
		B.Statuses.Add(G);
		FZEvent On; On.Type = EZEvt::StatusOn; On.Source = A; On.Target = A; On.Status = EZStatus::Guard; On.Text = ZNames::StatusName(EZStatus::Guard);
		Emit(On);
		const float Regen = B.GetSpecial(TEXT("GUARD_REGEN"));
		if (Regen > 0.f && B.HP < B.MaxHP)
		{
			const int32 H = FMath::Min(B.MaxHP - B.HP, (int32)FloorEps(B.MaxHP * Regen));
			B.HP += H;
			Emit(EZEvt::Heal, A, A, H);
		}
		Emit(EZEvt::ActionEnd, A, A);
		break;
	}
	case EZCmd::Transform:
	{
		const FZFormDef* F = DB.FindForm(Cmd.Id);
		const int32 Cost = FormCost(A, *F);
		Resonance -= Cost;
		Emit(EZEvt::Resonance, A, A, -Cost);
		EnterForm(A, *F);
		// La transformation coûte une activation ; la durée de 3 activations commence à la suivante
		EndActivation(A, true);
		return;
	}
	case EZCmd::Summon:
	{
		const FZSpiritDef* Sp = DB.FindSpirit(B.Spirit);
		Resonance -= Sp->Resonance;
		bSpendingResonance = true;
		Emit(EZEvt::Resonance, A, A, -Sp->Resonance);
		B.Uses.Add(Sp->Id, 1);
		FZEvent E; E.Type = EZEvt::Summon; E.Source = A; E.Id = Sp->Id; E.Text = Sp->Name; E.Element = Sp->Element;
		Emit(E);
		FZActionSpec S;
		S.Id = Sp->Id;
		S.Name = Sp->Name;
		S.Kind = Sp->Kind;
		S.Target = Sp->Target;
		S.Power = Sp->Power;
		S.Element = Sp->Element;
		S.Apply = Sp->Apply;
		S.Remove = Sp->Remove;
		S.bDispel = Sp->Dispel;
		S.MPRestorePct = Sp->MPRestorePct;
		S.bCancelPrepare = Sp->CancelPrepare;
		S.Anim = TEXT("Summon");
		S.bNoResonance = true;
		Execute(A, S, Cmd.Targets);
		if (Sp->Dispel)
		{
			for (FZBattler& O : Actors)
			{
				if (O.bAlly || O.bKO) continue;
				O.Statuses.RemoveAll([](const FZStatusInst& I) { return !ZNames::IsDebuff(I.Status) && I.Status != EZStatus::Stability && I.Status != EZStatus::BreachStability; });
			}
		}
		break;
	}
	case EZCmd::Duo:
	{
		const FZDuoDef* D = DB.FindDuo(Cmd.Id);
		int32 P = -1;
		FString Why;
		CanDuo(A, Cmd.Id, P, Why);
		FZBattler& PB = Actors[P];
		Resonance -= D->Resonance;
		bSpendingResonance = true;
		Emit(EZEvt::Resonance, A, A, -D->Resonance);
		FZBattler& AB = D->A == B.DefId ? B : PB;
		FZBattler& BB = D->A == B.DefId ? PB : B;
		AB.MP -= D->MPA;
		BB.MP -= D->MPB;
		ReadyQueue.Remove(P);
		FZEvent E; E.Type = EZEvt::Duo; E.Source = A; E.Target = P; E.Id = D->Id; E.Text = D->Name; E.Element = D->Element;
		Emit(E);
		FZActionSpec S;
		S.Id = D->Id;
		S.Name = D->Name;
		S.Kind = D->Kind;
		S.Target = D->Target;
		S.Power = D->Power;
		S.Hits = D->Hits;
		S.Element = D->Element;
		S.Apply = D->Apply;
		S.Breach = D->Breach;
		S.HealPower = D->HealPower;
		S.TargetGauge = D->TargetGauge;
		S.bNoResonance = true;
		S.Anim = TEXT("Duo");
		// Le duo utilise la meilleure attaque des deux participants
		const int32 Striker = (D->Kind == EZAbilityKind::Magical ? PB.AMAG > B.AMAG : PB.ATQ > B.ATQ) ? P : A;
		Execute(Striker, S, Cmd.Targets);
		EndActivation(P);
		break;
	}
	case EZCmd::Flee:
	{
		Emit(EZEvt::Fled, A, A, 0, TEXT("L'équipe prend la fuite."));
		State = EZSimState::Fled;
		return;
	}
	case EZCmd::Interact:
	{
		FZEvent E; E.Type = EZEvt::ActionStart; E.Source = A; E.Target = A; E.Text = FString::Printf(TEXT("%s ferme une vanne"), *B.Name); E.Anim = TEXT("Hook"); E.Id = TEXT("INTERACT_VALVE");
		Emit(E);
		for (FZBattler& O : Actors)
		{
			if (O.bKO || O.Prepared != NAME_Deluge) continue;
			O.Valves++;
			FZEvent V; V.Type = EZEvt::Valve; V.Source = A; V.Target = O.Id; V.Amount = O.Valves; V.Text = FString::Printf(TEXT("Vanne %d / 2"), O.Valves);
			Emit(V);
			if (O.Valves >= 2)
			{
				O.Prepared = NAME_None;
				O.Valves = 0;
				Emit(EZEvt::PrepareCancel, A, O.Id, 0, TEXT("Les vannes sont closes : le Déluge est annulé !"));
				GainResonance(6);
			}
			else
			{
				GainResonance(6);
			}
		}
		Emit(EZEvt::ActionEnd, A, A);
		break;
	}
	case EZCmd::Stance:
	{
		const FZCharacterDef* Def = DB.FindCharacter(B.DefId);
		if (Def && Def->Stances.Num() > 1)
		{
			const int32 Idx = Def->Stances.IndexOfByKey(B.Stance.ToString());
			B.Stance = FName(*Def->Stances[(Idx + 1) % Def->Stances.Num()]);
			Emit(EZEvt::Message, A, A, 0, FString::Printf(TEXT("%s adopte la posture %s"), *B.Name, *B.Stance.ToString()));
		}
		break;
	}
	default:
		break;
	}

	if (!IsOver())
	{
		if (B.Form == NAME_Majora) { B.Corruption += 20; }
		EndActivation(A);
	}
}

TArray<int32> FZCombatSim::ResolveTargets(int32 Actor, const FZActionSpec& Spec, const TArray<int32>& Wanted, bool& bRefund) const
{
	bRefund = false;
	TArray<int32> Out;
	const int32 W = Wanted.Num() > 0 ? Wanted[0] : -1;
	const bool bValid = Actors.IsValidIndex(W);
	switch (Spec.Target)
	{
	case EZTarget::Enemy:
	{
		if (bValid && !Actors[W].bKO && IsOpponent(Actor, W)) { Out.Add(W); break; }
		// Reciblage déterminé : vivant de plus petit id
		const TArray<int32> L = LivingEnemiesOf(Actor);
		if (L.Num() > 0) Out.Add(L[0]);
		break;
	}
	case EZTarget::AllEnemies:
		Out = LivingEnemiesOf(Actor);
		break;
	case EZTarget::Ally:
	{
		if (bValid && !Actors[W].bKO && !IsOpponent(Actor, W)) { Out.Add(W); break; }
		if (Spec.Kind == EZAbilityKind::Heal) { bRefund = true; break; }
		Out.Add(Actor);
		break;
	}
	case EZTarget::AllAllies:
		Out = LivingAlliesOf(Actor);
		break;
	case EZTarget::Self:
		Out.Add(Actor);
		break;
	case EZTarget::DeadAlly:
	{
		if (bValid && Actors[W].bKO && !IsOpponent(Actor, W)) { Out.Add(W); break; }
		for (const FZBattler& B : Actors) { if (B.bKO && B.bAlly == Actors[Actor].bAlly) { Out.Add(B.Id); break; } }
		if (Out.Num() == 0) bRefund = true;
		break;
	}
	}
	return Out;
}

bool FZCombatSim::Execute(int32 ActorId, const FZActionSpec& Spec, TArray<int32> Wanted)
{
	FZBattler& A = Actors[ActorId];

	// Provocation : l'ennemi vise le provocateur pour sa prochaine action mono-cible compatible
	if (!A.bAlly && Spec.Target == EZTarget::Enemy)
	{
		if (const FZStatusInst* T = A.Find(EZStatus::Taunt))
		{
			if (Actors.IsValidIndex(T->Source) && !Actors[T->Source].bKO) { Wanted = { T->Source }; }
			RemoveStatus(ActorId, EZStatus::Taunt);
		}
	}

	bool bRefund = false;
	TArray<int32> Targets = ResolveTargets(ActorId, Spec, Wanted, bRefund);
	if (bRefund || Targets.Num() == 0)
	{
		FZEvent E; E.Type = EZEvt::Message; E.Source = ActorId; E.Id = TEXT("Refund");
		E.Text = TEXT("Aucune cible valide : coût remboursé.");
		Emit(E);
		return false;
	}

	// Interception : un allié protégé renvoie le coup vers son protecteur
	if (!A.bAlly && Spec.Target == EZTarget::Enemy && Targets.Num() == 1)
	{
		if (const FZStatusInst* I = Actors[Targets[0]].Find(EZStatus::Intercept))
		{
			const int32 Protector = I->Source;
			if (Actors.IsValidIndex(Protector) && !Actors[Protector].bKO && Protector != Targets[0])
			{
				Emit(EZEvt::Message, Protector, Targets[0], 0, FString::Printf(TEXT("%s s'interpose !"), *Actors[Protector].Name));
				RemoveStatus(Targets[0], EZStatus::Intercept, false);
				Targets[0] = Protector;
			}
		}
	}

	EZElement Element = Spec.Element;
	if (Spec.bLastAllyElement) Element = LastAllyElement;

	FZEvent Start;
	Start.Type = EZEvt::ActionStart;
	Start.Source = ActorId;
	Start.Target = Targets[0];
	Start.Id = Spec.Id;
	Start.Text = Spec.Name;
	Start.Anim = Spec.Anim;
	Start.Element = Element;
	Start.Amount = Targets.Num();
	Emit(Start);

	bActionUseful = false;
	bActionWeakness = false;
	const bool bPhysical = Spec.Kind == EZAbilityKind::Physical;

	if (Spec.Kind == EZAbilityKind::Physical || Spec.Kind == EZAbilityKind::Magical)
	{
		for (int32 T : Targets)
		{
			bool bHitAny = false;
			bool bWeak = false;
			for (int32 h = 0; h < Spec.Hits; ++h)
			{
				FZBattler& Tg = Actors[T];
				if (Tg.bKO) break;
				if (bPhysical)
				{
					const float Blind = (A.Has(EZStatus::Blind) && !Spec.bIgnoreBlind) ? 25.f : 0.f;
					const float Dodge = FMath::Min(20.f, Tg.Evade + Tg.Mag(EZStatus::Evade));
					const float HitChance = FMath::Clamp(100.f - Blind - Dodge, 60.f, 100.f);
					if (HitChance < 100.f && Rng.FRand() * 100.f >= HitChance)
					{
						Emit(EZEvt::Miss, ActorId, T, 0, TEXT("Raté"));
						continue;
					}
				}
				const bool bCrit = !bNoCrit && Rng.FRand() * 100.f < FMath::Min(30.f, 5.f + 0.25f * A.LCK + A.Crit);
				const float V = bNoVariance ? 1.f : Rng.FRandRange(0.95f, 1.05f);
				float P = Spec.Power;
				if (Spec.ExecutePower > 0.f && Tg.HPPct() < 0.25f) P = Spec.ExecutePower;
				float EM = 1.f;

				// Parade miroir : renvoie le prochain sort mono-cible à 50 %
				if (!bPhysical && Spec.Target == EZTarget::Enemy && Tg.Has(EZStatus::MirrorParry))
				{
					const int32 Raw = ComputeDamage(A, Tg, false, P, Element, false, V, Spec.IgnoreDef, &EM, false);
					RemoveStatus(T, EZStatus::MirrorParry);
					Emit(EZEvt::Reaction, T, ActorId, 0, TEXT("Parade miroir !"));
					DealDamage(T, ActorId, FMath::Max(1, Raw / 2), Element, false, false, false, TEXT("Reflect"));
					bHitAny = true;
					continue;
				}

				const bool bWetReact = Tg.Has(EZStatus::Wet) && (Element == EZElement::Thunder || Element == EZElement::Fire);
				const bool bFreezeReact = Tg.Has(EZStatus::Freeze) && (bPhysical || Element == EZElement::Fire);
				int32 Dmg = ComputeDamage(A, Tg, bPhysical, P, Element, bCrit, V, Spec.IgnoreDef, &EM, true);
				if (Spec.bIsTechnique && A.bAlly && Dmg > 0)
				{
					const float Tech = FMath::Min(0.5f, A.GetSpecial(TEXT("DMG_TECH")));
					Dmg = (int32)FloorEps(Dmg * (1.0 + Tech));
				}
				const float BaseE = ElementMultiplier(Tg, Element);
				bWeak = BaseE > 1.f + 1e-3f;
				const bool bResist = BaseE < 1.f - 1e-3f;
				if (Element != EZElement::Neutral) Tg.KnownElements.Add(Element);

				// Réaction élémentaire (profondeur 1)
				if (bWetReact)
				{
					RemoveStatus(T, EZStatus::Wet);
					Emit(EZEvt::Reaction, ActorId, T, 0, Element == EZElement::Thunder ? TEXT("Mouillé + Foudre ×1,5 !") : TEXT("Mouillé : feu ×0,75"));
				}
				if (bFreezeReact)
				{
					RemoveStatus(T, EZStatus::Freeze);
					Emit(EZEvt::Reaction, ActorId, T, 0, Element == EZElement::Fire ? TEXT("Le feu libère du Gel") : TEXT("Gel brisé ×1,25 !"));
				}

				// Protection effective : +2 Résonance si une garde alliée a réduit le coup
				if (Tg.bAlly && (Tg.Has(EZStatus::Guard) || Tg.Has(EZStatus::Rampart) || Tg.Has(EZStatus::MagicShield) || Tg.Has(EZStatus::ZoraBarrier) || bResist))
				{
					GainResonance(2);
				}

				if (!A.bAlly && Tg.bAlly && EnemyDamageScale != 1.f) Dmg = FMath::Max(1, (int32)FloorEps(Dmg * EnemyDamageScale));
				DealDamage(ActorId, T, Dmg, Element, bCrit, bWeak, bResist, Spec.Anim);
				bHitAny = true;
				if (A.bAlly) { bActionUseful = true; if (bWeak) bActionWeakness = true; }

				// Sommeil : les dégâts directs réveillent
				if (!Actors[T].bKO && Actors[T].Has(EZStatus::Sleep)) RemoveStatus(T, EZStatus::Sleep);
			}

			FZBattler& Tg = Actors[T];
			if (bHitAny && !Tg.bKO)
			{
				if (A.bAlly && !Tg.bAlly)
				{
					AddBreach(ActorId, T, 10 + Spec.Breach + (bWeak ? 20 : 0));
					// Grappin sur un conduit pendant la préparation de la Crue (prototype, chapitre 17)
					if (Spec.bHookshot && Tg.AI == NAME_Nereide && Tg.Prepared == NAME_Crue)
					{
						Tg.Prepared = NAME_Jet;
						Emit(EZEvt::PrepareCancel, ActorId, T, 0, TEXT("Conduit crocheté : la Crue n'est plus qu'un Jet !"));
						AddBreach(ActorId, T, 80);
					}
				}
				for (const FZStatusApply& S : Spec.Apply) { ApplyStatus(ActorId, T, S); }
				const int32 G = Tg.bBoss ? Spec.TargetGaugeBoss : Spec.TargetGauge;
				if (G != 0)
				{
					Tg.J = FMath::Clamp(Tg.J + G, 0.f, 1000.f);
					if (Tg.J < 1000.f) ReadyQueue.Remove(T);
					Emit(EZEvt::Gauge, ActorId, T, G);
				}
				if (Spec.bDispel)
				{
					const int32 Before = Tg.Statuses.Num();
					Tg.Statuses.RemoveAll([](const FZStatusInst& I) { return !ZNames::IsDebuff(I.Status) && I.Status != EZStatus::Stability && I.Status != EZStatus::BreachStability; });
					if (Tg.Statuses.Num() != Before) Emit(EZEvt::Message, ActorId, T, 0, TEXT("Bonus dissipés"));
				}
			}
		}
		// Soin de groupe additionnel (duo Marée du courage)
		if (Spec.HealPower > 0.f)
		{
			for (int32 Al : LivingAlliesOf(ActorId))
			{
				FZBattler& X = Actors[Al];
				const int32 H = FMath::Min(X.MaxHP - X.HP, ComputeHeal(A, Spec.HealPower));
				X.HP += H;
				Emit(EZEvt::Heal, ActorId, Al, H);
			}
		}
		if (Spec.bCancelPrepare)
		{
			for (FZBattler& O : Actors)
			{
				if (!O.bAlly && !O.Prepared.IsNone())
				{
					O.Prepared = NAME_None;
					Emit(EZEvt::PrepareCancel, ActorId, O.Id, 0, TEXT("La catastrophe télégraphiée est annulée !"));
				}
			}
		}
	}
	else if (Spec.Kind == EZAbilityKind::Heal)
	{
		for (int32 T : Targets)
		{
			FZBattler& Tg = Actors[T];
			if (Tg.bKO) continue;
			int32 H = 0;
			if (Spec.HealHPPct > 0.f)
			{
				H = (int32)FloorEps(Tg.MaxHP * Spec.HealHPPct * (1.0 + A.GetSpecial(TEXT("POTION_HEAL"))));
			}
			else if (Spec.HealPct > 0.f)
			{
				H = (int32)FloorEps(Tg.MaxHP * Spec.HealPct);
			}
			else if (Spec.Power > 0.f)
			{
				H = ComputeHeal(A, Spec.Power);
			}
			H = (int32)FloorEps(H * (1.0 + Tg.GetSpecial(TEXT("HEAL_TAKEN"))));
			H = FMath::Clamp(H, 0, Tg.MaxHP - Tg.HP);
			if (H > 0)
			{
				Tg.HP += H;
				Emit(EZEvt::Heal, ActorId, T, H);
				if (H > 0) bActionUseful = true;
			}
			if (Spec.HealMPPct > 0.f)
			{
				const int32 M = FMath::Clamp((int32)FloorEps(Tg.MaxMP * Spec.HealMPPct), 0, Tg.MaxMP - Tg.MP);
				Tg.MP += M;
				Emit(EZEvt::MPHeal, ActorId, T, M);
				if (M > 0) bActionUseful = true;
			}
			for (EZStatus S : Spec.Remove) { if (Tg.Has(S)) { RemoveStatus(T, S); bActionUseful = true; } }
		}
	}
	else if (Spec.Kind == EZAbilityKind::Revive)
	{
		for (int32 T : Targets)
		{
			FZBattler& Tg = Actors[T];
			if (!Tg.bKO) continue;
			Tg.bKO = false;
			Tg.HP = FMath::Max(1, (int32)FloorEps(Tg.MaxHP * Spec.ReviveHPPct));
			Tg.J = 0.f;
			Emit(EZEvt::Revive, ActorId, T, Tg.HP);
			bActionUseful = true;
		}
	}
	else // Support
	{
		for (int32 T : Targets)
		{
			FZBattler& Tg = Actors[T];
			if (Tg.bKO) continue;
			for (FZStatusApply S : Spec.Apply)
			{
				if (S.Status == EZStatus::Barrier && Spec.BarrierPct > 0.f) S.Magnitude = FMath::FloorToFloat(Tg.MaxHP * Spec.BarrierPct);
				if (S.Status == EZStatus::Intercept) { S.Magnitude = 0.f; }
				ApplyStatus(ActorId, T, S);
				bActionUseful = true;
			}
			// Interception : le protégé reçoit le statut, le lanceur la Garde
			for (const FZStatusApply& S : Spec.Apply)
			{
				if (S.Status == EZStatus::Intercept)
				{
					if (FZStatusInst* I = Tg.Find(EZStatus::Intercept)) { I->Source = ActorId; I->Remaining = -1; }
					FZStatusInst G; G.Status = EZStatus::Guard; G.Remaining = -1; G.Source = ActorId;
					A.Statuses.RemoveAll([](const FZStatusInst& X) { return X.Status == EZStatus::Guard; });
					A.Statuses.Add(G);
					FZEvent On; On.Type = EZEvt::StatusOn; On.Source = ActorId; On.Target = ActorId; On.Status = EZStatus::Guard; On.Text = ZNames::StatusName(EZStatus::Guard);
					Emit(On);
				}
			}
			for (EZStatus S : Spec.Remove) { if (Tg.Has(S)) { RemoveStatus(T, S); bActionUseful = true; } }
			if (Spec.TargetGauge != 0 && !IsOpponent(ActorId, T))
			{
				Tg.J = FMath::Clamp(Tg.J + Spec.TargetGauge, 0.f, 1000.f);
				Emit(EZEvt::Gauge, ActorId, T, Spec.TargetGauge);
			}
			if (Spec.bDispel && IsOpponent(ActorId, T))
			{
				Tg.Statuses.RemoveAll([](const FZStatusInst& I) { return !ZNames::IsDebuff(I.Status) && I.Status != EZStatus::Stability && I.Status != EZStatus::BreachStability; });
				Emit(EZEvt::Message, ActorId, T, 0, TEXT("Bonus dissipés"));
				bActionUseful = true;
			}
			if (Spec.bAnalyze && IsOpponent(ActorId, T))
			{
				Tg.bAnalyzed = true;
				Emit(EZEvt::Analyze, ActorId, T, 0, FString::Printf(TEXT("Analyse : %s"), *Tg.Name));
				bActionUseful = true;
			}
			if (Spec.MPRestorePct > 0.f && !IsOpponent(ActorId, T))
			{
				const int32 M = FMath::Clamp((int32)FloorEps(Tg.MaxMP * Spec.MPRestorePct), 0, Tg.MaxMP - Tg.MP);
				Tg.MP += M;
				Emit(EZEvt::MPHeal, ActorId, T, M);
			}
		}
		if (Spec.ResonanceGain > 0) bActionUseful = true;
	}

	if (A.bAlly && Element != EZElement::Neutral) LastAllyElement = Element;

	// Résonance d'équipe
	if (A.bAlly && !bSpendingResonance && !Spec.bNoResonance)
	{
		if (Spec.ResonanceGain > 0)
		{
			GainResonance(Spec.ResonanceGain); // remplace le gain normal
		}
		else
		{
			GainResonance((bActionUseful ? 6 : 0) + (bActionWeakness ? 4 : 0));
		}
	}
	Emit(EZEvt::ActionEnd, ActorId, Targets[0]);
	CheckEnd();
	return true;
}

void FZCombatSim::ApplyStatus(int32 Source, int32 Target, const FZStatusApply& S)
{
	FZBattler& B = Actors[Target];
	if (B.bKO || S.Status == EZStatus::None) return;
	if (B.Immune.Contains(S.Status))
	{
		Emit(EZEvt::Message, Source, Target, 0, FString::Printf(TEXT("%s : immunisé contre %s"), *B.Name, *ZNames::StatusName(S.Status)));
		return;
	}
	const float* Res = B.StatusRes.Find(S.Status);
	const float Chance = S.Chance * (1.f - (Res ? FMath::Clamp(*Res, 0.f, 1.f) : 0.f));
	if (Chance < 1.f && Rng.FRand() >= Chance)
	{
		if (ZNames::IsDebuff(S.Status)) Emit(EZEvt::Message, Source, Target, 0, FString::Printf(TEXT("%s résiste à %s"), *B.Name, *ZNames::StatusName(S.Status)));
		return;
	}

	EZStatus Status = S.Status;
	int32 Duration = S.Duration;
	// Stabilité : aucun nouveau saut d'action
	if ((Status == EZStatus::Freeze || Status == EZStatus::Sleep) && B.Has(EZStatus::Stability))
	{
		Emit(EZEvt::Message, Source, Target, 0, FString::Printf(TEXT("%s est stable : pas d'interruption"), *B.Name));
		return;
	}
	if (Status == EZStatus::Shock)
	{
		if (B.LastShockSerial == ActivationSerial) return; // une fois par activation adverse
		B.LastShockSerial = ActivationSerial;
		const float Drop = B.bBoss ? 100.f : 200.f;
		B.J = FMath::Max(0.f, B.J - Drop);
		ReadyQueue.Remove(Target);
		FZEvent E; E.Type = EZEvt::StatusOn; E.Source = Source; E.Target = Target; E.Status = EZStatus::Shock; E.Amount = -(int32)Drop;
		E.Text = FString::Printf(TEXT("Choc ! jauge -%d"), (int32)Drop);
		Emit(E);
		return;
	}
	if (Status == EZStatus::Freeze && B.bBoss)
	{
		Status = EZStatus::Slow; Duration = 1; // boss : Lenteur une activation à la place
	}
	if (Status == EZStatus::Freeze && B.Has(EZStatus::Burn)) RemoveStatus(Target, EZStatus::Burn);
	if (Status == EZStatus::Burn && B.Has(EZStatus::Freeze)) RemoveStatus(Target, EZStatus::Freeze);
	if (Status == EZStatus::Haste && B.Has(EZStatus::Slow)) RemoveStatus(Target, EZStatus::Slow);
	if (Status == EZStatus::Slow && B.Has(EZStatus::Haste)) RemoveStatus(Target, EZStatus::Haste);
	if (Status == EZStatus::Haste) Duration += (int32)Actors[Source].GetSpecial(TEXT("HASTE_BONUS"));

	FZStatusInst* Existing = B.Find(Status);
	if (Existing)
	{
		Existing->Remaining = FMath::Max(Existing->Remaining, Duration); // pas de cumul, durée rafraîchie
		Existing->Magnitude = FMath::Max(Existing->Magnitude, S.Magnitude);
		Existing->Source = Source;
		Existing->bSkipNextDecrement = (Target == ActiveActivationActor);
	}
	else
	{
		FZStatusInst I;
		I.Status = Status;
		I.Remaining = Duration;
		I.Magnitude = S.Magnitude;
		I.Source = Source;
		I.bSkipNextDecrement = (Target == ActiveActivationActor);
		B.Statuses.Add(I);
	}
	FZEvent E; E.Type = EZEvt::StatusOn; E.Source = Source; E.Target = Target; E.Status = Status; E.Text = ZNames::StatusName(Status);
	Emit(E);
}

void FZCombatSim::RemoveStatus(int32 Target, EZStatus S, bool bEvent)
{
	FZBattler& B = Actors[Target];
	const int32 N = B.Statuses.RemoveAll([S](const FZStatusInst& I) { return I.Status == S; });
	if (N > 0 && bEvent)
	{
		FZEvent E; E.Type = EZEvt::StatusOff; E.Source = Target; E.Target = Target; E.Status = S; E.Text = ZNames::StatusName(S);
		Emit(E);
	}
}

void FZCombatSim::DealDamage(int32 Source, int32 Target, int32 Amount, EZElement Element, bool bCrit, bool bWeak, bool bResist, const FString& Anim)
{
	FZBattler& B = Actors[Target];
	if (B.bKO) return;
	if (FZStatusInst* Bar = B.Find(EZStatus::Barrier))
	{
		const int32 Absorb = FMath::Min(Amount, (int32)Bar->Magnitude);
		Bar->Magnitude -= Absorb;
		Amount -= Absorb;
		if (Absorb > 0) Emit(EZEvt::Message, Source, Target, Absorb, FString::Printf(TEXT("Barrière : %d absorbés"), Absorb));
		if (Bar->Magnitude <= 0.f) RemoveStatus(Target, EZStatus::Barrier);
	}
	B.HP = FMath::Max(0, B.HP - Amount);
	FZEvent E;
	E.Type = EZEvt::Damage; E.Source = Source; E.Target = Target; E.Amount = Amount; E.Element = Element;
	E.bCrit = bCrit; E.bWeak = bWeak; E.bResist = bResist; E.Anim = Anim;
	Emit(E);
	if (B.HP <= 0)
	{
		B.bKO = true;
		B.J = 0.f;
		B.Statuses.Reset();
		B.Prepared = NAME_None;
		ReadyQueue.Remove(Target);
		if (!B.Form.IsNone())
		{
			B.Form = NAME_None;
			B.FormLeft = 0;
			RecomputeFormStats(B);
		}
		Emit(EZEvt::KO, Source, Target, 0, FString::Printf(TEXT("%s est vaincu"), *B.Name));
	}
}

void FZCombatSim::GainResonance(int32 Amount)
{
	if (Amount <= 0) return;
	const int32 Allowed = FMath::Min(Amount, 12 - ActivationGain);
	if (Allowed <= 0) return;
	const int32 Before = Resonance;
	Resonance = FMath::Min(100, Resonance + Allowed);
	ActivationGain += Allowed;
	if (Resonance != Before) Emit(EZEvt::Resonance, -1, -1, Resonance - Before);
}

void FZCombatSim::AddBreach(int32 Source, int32 Target, int32 Amount)
{
	FZBattler& B = Actors[Target];
	if (B.bKO || B.bAlly || Amount <= 0) return;
	if (B.Has(EZStatus::BreachStability)) return;
	B.Breach += Amount;
	if (B.Breach >= B.BreachMax)
	{
		B.Breach = 0.f;
		B.J = FMath::Max(0.f, B.J - 200.f);
		ReadyQueue.Remove(Target);
		FZStatusInst Ex; Ex.Status = EZStatus::Exposed; Ex.Remaining = 1; Ex.Source = Source;
		FZStatusInst St; St.Status = EZStatus::BreachStability; St.Remaining = 2; St.Source = Source;
		B.Statuses.RemoveAll([](const FZStatusInst& I) { return I.Status == EZStatus::Exposed || I.Status == EZStatus::BreachStability; });
		B.Statuses.Add(Ex);
		B.Statuses.Add(St);
		Emit(EZEvt::BreachBreak, Source, Target, 0, FString::Printf(TEXT("BRÈCHE ! %s est exposé (+25 %% dégâts)"), *B.Name));
		if (!B.Prepared.IsNone() && B.Prepared != NAME_Deluge)
		{
			const FZAbilityDef* P = UZGameData::Get().FindAbility(B.Prepared);
			Emit(EZEvt::PrepareCancel, Source, Target, 0, FString::Printf(TEXT("Préparation interrompue : %s"), P ? *P->Name : TEXT("?")));
			B.Prepared = NAME_None;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Formes

void FZCombatSim::RecomputeFormStats(FZBattler& B)
{
	const FZFormDef* F = B.Form.IsNone() ? nullptr : UZGameData::Get().FindForm(B.Form);
	if (!F)
	{
		B.ATQ = B.BaseATQ; B.AMAG = B.BaseAMAG; B.DEF = B.BaseDEF; B.AGI = B.BaseAGI;
		return;
	}
	// Puissance d'arme et focus remplacés par la valeur de forme du rang narratif
	const float FormPower = (float)ZProgression::RankPower(B.FormRank);
	B.ATQ = (2.f * FMath::FloorToFloat(B.STR) + FormPower) * F->ATQ;
	B.AMAG = (2.f * FMath::FloorToFloat(B.MAG) + FormPower) * F->AMAG;
	B.DEF = B.BaseDEF * F->DEF;
	B.AGI = FMath::Min(80.f, B.BaseAGI * F->AGI);
}

void FZCombatSim::EnterForm(int32 A, const FZFormDef& F)
{
	FZBattler& B = Actors[A];
	B.Form = F.Id;
	B.FormLeft = 3;
	B.Corruption = 0;
	RecomputeFormStats(B);
	FZEvent E; E.Type = EZEvt::FormStart; E.Source = A; E.Target = A; E.Id = F.Id; E.Text = F.Name;
	Emit(E);
}

void FZCombatSim::ExitForm(int32 A, bool bVoluntary)
{
	FZBattler& B = Actors[A];
	if (B.Form.IsNone()) return;
	const FName Old = B.Form;
	B.Form = NAME_None;
	B.FormLeft = 0;
	RecomputeFormStats(B);
	FZEvent E; E.Type = EZEvt::FormEnd; E.Source = A; E.Target = A; E.Id = Old;
	E.Text = bVoluntary ? FString::Printf(TEXT("%s reprend sa forme"), *B.Name) : FString::Printf(TEXT("La forme de %s se dissipe"), *B.Name);
	Emit(E);
	if (Old == NAME_Oni)
	{
		FZStatusApply S; S.Status = EZStatus::MPCostUp; S.Duration = 2; S.Chance = 1.f;
		ApplyStatus(A, A, S);
	}
	if (Old == NAME_Majora)
	{
		const int32 D = FMath::Min(B.HP - 1, (int32)FloorEps(B.MaxHP * 0.1));
		if (D > 0) { B.HP -= D; Emit(EZEvt::DoT, A, A, D, TEXT("Contrecoup de Majora")); }
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// IA ennemie (déterministe hors sélection pondérée, graine sauvegardable)

FName FZCombatSim::ChooseEnemyAction(FZBattler& E)
{
	const FString AI = E.AI.ToString();
	auto Pick = [&](const TCHAR* Id) { return FName(Id); };
	if (AI == TEXT("octorok"))
	{
		return Rng.FRand() < 0.7f || E.MP < 4 ? Pick(TEXT("E_OCTO_ROCK")) : Pick(TEXT("E_OCTO_INK"));
	}
	if (AI == TEXT("chuchu"))
	{
		return (E.Cycle % 2 == 0 && E.MP >= 4) ? Pick(TEXT("E_CHUCHU_SPLASH")) : Pick(TEXT("E_CHUCHU_SLAM"));
	}
	if (AI == TEXT("sentinel"))
	{
		return (E.Cycle % 2 == 0) ? Pick(TEXT("E_SENT_BOLT")) : Pick(TEXT("E_SENT_CHARGE"));
	}
	if (AI == TEXT("nereide"))
	{
		switch (E.Phase)
		{
		case 0: return (E.Cycle % 2 == 0) ? Pick(TEXT("E_NER_JET")) : Pick(TEXT("E_NER_CRUE_PREP"));
		case 1:
		{
			static const TCHAR* Cycle[] = { TEXT("E_NER_WETALL"), TEXT("E_NER_ARC_PREP"), TEXT("E_NER_JET") };
			return Pick(Cycle[E.Cycle % 3]);
		}
		default:
		{
			static const TCHAR* Cycle[] = { TEXT("E_NER_WAVE"), TEXT("E_NER_STRIKE"), TEXT("E_NER_DELUGE_PREP") };
			return Pick(Cycle[E.Cycle % 3]);
		}
		}
	}
	TArray<FName> Options;
	for (FName A : E.Techniques)
	{
		const FZAbilityDef* Ab = UZGameData::Get().FindAbility(A);
		if (Ab && Ab->PreparedFollowUp().IsNone() && (E.bInfiniteMP || Ab->MP <= E.MP)) Options.Add(A);
	}
	return Options.Num() > 0 ? Options[Rng.RandRange(0, Options.Num() - 1)] : FName(TEXT("E_CHUCHU_SLAM"));
}

void FZCombatSim::EnemyTurn(int32 EId)
{
	const UZGameData& DB = UZGameData::Get();
	FZBattler& E = Actors[EId];

	// Transition de phase enregistrée après une action complète : commence maintenant
	if (E.PendingPhase != E.Phase)
	{
		E.Phase = E.PendingPhase;
		E.Cycle = 0;
		static const TCHAR* Names[] = { TEXT("Phase A — Conduits"), TEXT("Phase B — Surcharge"), TEXT("Phase C — Rupture") };
		Emit(EZEvt::PhaseChange, EId, EId, E.Phase, Names[FMath::Clamp(E.Phase, 0, 2)]);
	}

	FName ActionId;
	const bool bWasPrepared = !E.Prepared.IsNone();
	if (bWasPrepared)
	{
		ActionId = E.Prepared;
		E.Prepared = NAME_None;
		E.Valves = 0;
	}
	else
	{
		ActionId = ChooseEnemyAction(E);
	}
	const FZAbilityDef* Ab = DB.FindAbility(ActionId);
	if (!Ab)
	{
		EndActivation(EId);
		return;
	}
	if (!E.bInfiniteMP) E.MP = FMath::Max(0, E.MP - Ab->MP);

	const FName FollowUp = Ab->PreparedFollowUp();
	if (!FollowUp.IsNone())
	{
		const FZAbilityDef* F = DB.FindAbility(FollowUp);
		FZEvent S; S.Type = EZEvt::ActionStart; S.Source = EId; S.Target = EId; S.Id = ActionId; S.Text = Ab->Name; S.Anim = Ab->Anim;
		Emit(S);
		E.Prepared = FollowUp;
		FZEvent P; P.Type = EZEvt::Prepare; P.Source = EId; P.Target = EId; P.Id = FollowUp; P.Text = F ? F->Name : FollowUp.ToString(); P.Element = F ? F->Element : EZElement::Neutral;
		Emit(P);
		Emit(EZEvt::ActionEnd, EId, EId);
	}
	else
	{
		TArray<int32> Wanted;
		const TArray<int32> Foes = LivingEnemiesOf(EId);
		if (Foes.Num() > 0) Wanted.Add(Foes[Rng.RandRange(0, Foes.Num() - 1)]);
		Execute(EId, SpecFromAbility(*Ab), Wanted);
	}
	E.Cycle++; // l'index de cycle n'avance que sur une action résolue

	if (E.AI == NAME_Nereide && !E.bKO)
	{
		const float P = E.HPPct();
		const int32 Want = P < 0.33f ? 2 : (P < 0.66f ? 1 : 0);
		if (Want > E.PendingPhase) E.PendingPhase = Want;
	}
	if (!IsOver()) EndActivation(EId);
}

void FZCombatSim::AfterAction(int32 Actor)
{
	(void)Actor;
}

void FZCombatSim::CheckEnd()
{
	if (IsOver()) return;
	bool bAnyEnemy = false, bAnyAlly = false;
	for (const FZBattler& B : Actors)
	{
		if (B.bKO) continue;
		(B.bAlly ? bAnyAlly : bAnyEnemy) = true;
	}
	// Une réaction ne déclenche pas de phase supplémentaire : les transitions de boss sont vérifiées ici aussi
	for (FZBattler& B : Actors)
	{
		if (!B.bAlly && !B.bKO && B.AI == NAME_Nereide)
		{
			const float P = B.HPPct();
			const int32 Want = P < 0.33f ? 2 : (P < 0.66f ? 1 : 0);
			if (Want > B.PendingPhase) B.PendingPhase = Want;
		}
	}
	if (!bAnyEnemy)
	{
		Result.bVictory = true;
		Result.XP = 0; Result.Rupees = 0; Result.AP = 0;
		for (const FZBattler& B : Actors)
		{
			if (B.bAlly) continue;
			Result.XP += B.XP;
			Result.Rupees += B.Rupees;
			Result.AP = FMath::Max(Result.AP, B.AP);
		}
		Result.ResonanceKept = FMath::Min(Resonance, 25);
		State = EZSimState::Victory;
		Emit(EZEvt::Victory, -1, -1, Result.XP, TEXT("Victoire !"));
	}
	else if (!bAnyAlly)
	{
		State = EZSimState::Defeat;
		Emit(EZEvt::Defeat, -1, -1, 0, TEXT("L'équipe est tombée…"));
	}
}
