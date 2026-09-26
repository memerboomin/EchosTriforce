// Tests de non-régression du dossier (chapitres 5, 7, 17 et 23).
// Lancer : UnrealEditor-Cmd EchosTriforce.uproject -ExecCmds="Automation RunTests EchosTriforce; Quit" -nullrhi -unattended
#include "Misc/AutomationTest.h"
#include "ZCombat.h"
#include "ZGameData.h"
#include "ZProgression.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags ZTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FZBattler Fixture(const TCHAR* Name, bool bAlly, int32 HP, int32 MP, float ATQ, float AMAG, float DEF, float DFM, float AGI)
	{
		FZBattler B;
		B.Name = Name;
		B.DefId = FName(Name);
		B.bAlly = bAlly;
		B.MaxHP = B.HP = HP;
		B.MaxMP = B.MP = MP;
		B.ATQ = ATQ; B.AMAG = AMAG; B.DEF = DEF; B.DFM = DFM; B.AGI = AGI;
		B.LCK = 6;
		return B;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZDataLoadTest, "EchosTriforce.Data.Load", ZTestFlags)
bool FZDataLoadTest::RunTest(const FString&)
{
	const UZGameData& DB = UZGameData::Get();
	TestTrue(TEXT("données chargées"), DB.IsLoaded());
	TestEqual(TEXT("1 257 références + 1 prêt de démonstration"), DB.Items.Num(), 1258);
	TestNotNull(TEXT("Tunique Zora"), DB.FindItem(TEXT("OOT_010")));
	TestNotNull(TEXT("Masque Zora (forme)"), DB.FindItem(TEXT("MM_022")));
	TestNotNull(TEXT("Gardien Néréide"), DB.FindEnemy(TEXT("nereide")));
	TestEqual(TEXT("8 compagnons permanents"), DB.Characters.Num(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZLevelCurveTest, "EchosTriforce.Progression.LinkCurve", ZTestFlags)
bool FZLevelCurveTest::RunTest(const FString&)
{
	const FZBaseStats N1 = ZProgression::LinkCurve(1);
	const FZBaseStats N10 = ZProgression::LinkCurve(10);
	const FZBaseStats N30 = ZProgression::LinkCurve(30);
	const FZBaseStats N50 = ZProgression::LinkCurve(50);
	const FZBaseStats N99 = ZProgression::LinkCurve(99);
	TestEqual(TEXT("PV N1"), N1.HP, 180);
	TestEqual(TEXT("PV N10"), N10.HP, 606);
	TestEqual(TEXT("PV N30"), N30.HP, 1902);
	TestEqual(TEXT("PV N50"), N50.HP, 3678);
	TestEqual(TEXT("PV N99 plafonnés"), N99.HP, 9999);
	TestEqual(TEXT("PM N10"), N10.MP, 61);
	TestEqual(TEXT("PM N99"), N99.MP, 417);
	TestEqual(TEXT("FOR N10"), N10.STR, 30);
	TestEqual(TEXT("MAG N50"), N50.MAG, 108);
	TestEqual(TEXT("VIT N30"), N30.VIT, 53);
	TestEqual(TEXT("AGI N99"), N99.AGI, 36);
	TestEqual(TEXT("CHC N50"), N50.LCK, 13);
	TestEqual(TEXT("XP de 1 à 2"), ZProgression::XPToNext(1), 85);
	TestEqual(TEXT("Niveau pour XP totale N10"), ZProgression::LevelForXP(ZProgression::TotalXPForLevel(10)), 10);
	TestEqual(TEXT("Capacité de passifs N10"), ZProgression::PassiveCapacity(10), 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZTraceTest, "EchosTriforce.Combat.PrototypeTrace", ZTestFlags)
bool FZTraceTest::RunTest(const FString&)
{
	// Trace numérique de vérification du chapitre 17 (variance forcée à 1, sans critique)
	FZCombatSim Sim;
	Sim.bNoVariance = true;
	Sim.bNoCrit = true;
	const int32 Link = Sim.AddFixture(Fixture(TEXT("Link"), true, 606, 61, 84, 56, 48, 48, 14));
	const int32 Mipha = Sim.AddFixture(Fixture(TEXT("Mipha"), true, 575, 61, 78, 66, 40, 54, 14));
	const int32 Ner = Sim.AddFixture(Fixture(TEXT("Néréide"), false, 2800, 9999, 65, 60, 40, 40, 14));

	const FZBattler& L = Sim.Actors[Link];
	const FZBattler& N = Sim.Actors[Ner];
	TestEqual(TEXT("Link attaque Néréide (P1)"), Sim.ComputeDamage(L, N, true, 1.0f, EZElement::Neutral, false, 1.f), 127);
	TestEqual(TEXT("Grappin tactique (P0,7)"), Sim.ComputeDamage(L, N, true, 0.7f, EZElement::Neutral, false, 1.f), 89);
	TestEqual(TEXT("Jet d'eau sans tenue"), Sim.ComputeDamage(N, L, false, 1.0f, EZElement::Water, false, 1.f), 87);

	Sim.Actors[Link].Resist[(int32)EZElement::Water] = 0.5f;
	TestEqual(TEXT("Jet d'eau en tenue Zora"), Sim.ComputeDamage(N, Sim.Actors[Link], false, 1.0f, EZElement::Water, false, 1.f), 43);

	FZStatusInst Guard; Guard.Status = EZStatus::Guard; Guard.Remaining = -1;
	Sim.Actors[Link].Statuses.Add(Guard);
	TestEqual(TEXT("Jet d'eau en Zora + Garde"), Sim.ComputeDamage(N, Sim.Actors[Link], false, 1.0f, EZElement::Water, false, 1.f), 21);

	TestEqual(TEXT("Soin de Mipha (AMAG 66, P1,6)"), Sim.ComputeHeal(Sim.Actors[Mipha], 1.6f), 259);

	// Exemple calculable du chapitre 5 : Link N10, FOR 30, arme rang 2 → ATQ 84
	const FZBaseStats B10 = ZProgression::LinkCurve(10);
	TestEqual(TEXT("ATQ = 2 × FOR + puissance"), 2 * B10.STR + ZProgression::RankPower(2), 84);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZEquipTest, "EchosTriforce.Equipment.Rules", ZTestFlags)
bool FZEquipTest::RunTest(const FString&)
{
	FZCharacterState Link = ZProgression::MakeCharacter(TEXT("link"), 10);
	FString Why;

	// Tenue de l'eau : monobloc sur trois emplacements, Eau -50 %
	TestTrue(TEXT("équiper la tunique Zora"), ZProgression::Equip(Link, EZEquipSlot::Torso, TEXT("OOT_010"), Why));
	TestEqual(TEXT("tête occupée par la tenue"), Link.GetEquip(EZEquipSlot::Head), FString(TEXT("OOT_010")));
	TestEqual(TEXT("jambes occupées par la tenue"), Link.GetEquip(EZEquipSlot::Legs), FString(TEXT("OOT_010")));
	FZDerivedStats D = ZProgression::ComputeDerived(Link);
	TestEqual(TEXT("résistance eau 50 %"), D.Resist[(int32)EZElement::Water], 0.5f);

	// Armure Zora de TP : contrepartie -20 % feu et glace
	TestTrue(TEXT("équiper l'armure Zora (TP)"), ZProgression::Equip(Link, EZEquipSlot::Torso, TEXT("TP_009"), Why));
	D = ZProgression::ComputeDerived(Link);
	TestEqual(TEXT("TP : eau 50 %"), D.Resist[(int32)EZElement::Water], 0.5f);
	TestEqual(TEXT("TP : feu -20 %"), D.Resist[(int32)EZElement::Fire], -0.2f);

	// Une pièce de torse remplace une tenue monobloc (aucun cumul)
	const FZItemDef* Torso = nullptr;
	for (const FZItemDef& It : UZGameData::Get().Items) { if (It.Slot == EZSlot::Torso && It.Profile == TEXT("EAU")) { Torso = &It; break; } }
	if (TestNotNull(TEXT("une pièce de torse EAU existe"), Torso))
	{
		TestTrue(TEXT("équiper un torse"), ZProgression::Equip(Link, EZEquipSlot::Torso, Torso->Id, Why));
		TestTrue(TEXT("la tenue monobloc est retirée de la tête"), Link.GetEquip(EZEquipSlot::Head).IsEmpty());
		D = ZProgression::ComputeDerived(Link);
		TestEqual(TEXT("une seule pièce : 15 %"), D.Resist[(int32)EZElement::Water], 0.15f);
	}

	// Deux mains + bouclier (chapitre 23)
	TestTrue(TEXT("bouclier hylien"), ZProgression::Equip(Link, EZEquipSlot::Offhand, TEXT("OOT_006"), Why));
	TestTrue(TEXT("épée de Biggoron"), ZProgression::Equip(Link, EZEquipSlot::Weapon, TEXT("OOT_004"), Why));
	TestTrue(TEXT("le bouclier est retiré"), Link.GetEquip(EZEquipSlot::Offhand).IsEmpty());
	TestFalse(TEXT("impossible de remettre un bouclier"), ZProgression::Equip(Link, EZEquipSlot::Offhand, TEXT("OOT_006"), Why));

	// Anneaux de même famille : seul le meilleur rang compte
	TestTrue(TEXT("Power Ring L-1"), ZProgression::Equip(Link, EZEquipSlot::AccessoryA, TEXT("OOS/OOA_002"), Why));
	TestTrue(TEXT("Power Ring L-3"), ZProgression::Equip(Link, EZEquipSlot::AccessoryB, TEXT("OOS/OOA_004"), Why));
	D = ZProgression::ComputeDerived(Link);
	const FZDerivedStats Base = ZProgression::ComputeDerived(ZProgression::MakeCharacter(TEXT("link"), 10));
	TestTrue(TEXT("pas de cumul L-1 + L-3 (FOR ≤ +15 %)"), D.STR <= Base.STR * 1.15f + 0.01f);

	// Compagnons : un personnage ne manie que son type d'arme
	FZCharacterState Mipha = ZProgression::MakeCharacter(TEXT("mipha"), 10);
	TestFalse(TEXT("Mipha ne manie pas l'épée"), ZProgression::Equip(Mipha, EZEquipSlot::Weapon, TEXT("OOT_001"), Why));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZFormTest, "EchosTriforce.Combat.FormDuration", ZTestFlags)
bool FZFormTest::RunTest(const FString&)
{
	FZCombatSim Sim;
	Sim.bNoVariance = true;
	Sim.bNoCrit = true;
	Sim.bAutoAck = true;
	FZBattler L = Fixture(TEXT("Link"), true, 606, 61, 84, 56, 48, 48, 30);
	L.STR = 30; L.MAG = 28;
	L.Forms = { TEXT("Zora") };
	const int32 Link = Sim.AddFixture(L);
	Sim.AddFixture(Fixture(TEXT("Mur"), false, 99999, 0, 1, 1, 200, 200, 1));
	Sim.Start(7, 60);

	auto WaitLink = [&]()
	{
		for (int32 i = 0; i < 10000 && Sim.GetState() != EZSimState::AwaitingCommand; ++i) Sim.Tick(0.05f);
		return Sim.GetState() == EZSimState::AwaitingCommand && Sim.GetCurrentActor() == Link;
	};
	TestTrue(TEXT("tour de Link"), WaitLink());
	const int32 HPBefore = Sim.Actors[Link].HP;
	FZCommand T; T.Type = EZCmd::Transform; T.Actor = Link; T.Id = TEXT("Zora");
	TestTrue(TEXT("transformation Zora"), Sim.Submit(T));
	TestEqual(TEXT("Résonance dépensée"), Sim.Resonance, 0);
	TestEqual(TEXT("PV conservés"), Sim.Actors[Link].HP, HPBefore);
	TestEqual(TEXT("forme active"), Sim.Actors[Link].Form, FName(TEXT("Zora")));
	for (int32 k = 0; k < 3; ++k)
	{
		TestTrue(TEXT("tour de Link en forme"), WaitLink());
		TestEqual(TEXT("toujours en forme avant l'action"), Sim.Actors[Link].Form, FName(TEXT("Zora")));
		FZCommand G; G.Type = EZCmd::Guard; G.Actor = Link;
		Sim.Submit(G);
	}
	TestTrue(TEXT("forme terminée exactement après trois activations"), Sim.Actors[Link].Form.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZResonanceTest, "EchosTriforce.Combat.Resonance", ZTestFlags)
bool FZResonanceTest::RunTest(const FString&)
{
	FZCombatSim Sim;
	Sim.bNoVariance = true;
	Sim.bNoCrit = true;
	Sim.bAutoAck = true;
	FZBattler L = Fixture(TEXT("Link"), true, 606, 61, 84, 56, 48, 48, 30);
	L.Techniques = { TEXT("A05"), TEXT("S01") };
	const int32 Link = Sim.AddFixture(L);
	FZBattler Target = Fixture(TEXT("Cible"), false, 99999, 0, 1, 1, 40, 40, 1);
	Target.Resist[(int32)EZElement::Neutral] = 0.f;
	Sim.AddFixture(Target);
	Sim.Start(3, 0);
	for (int32 i = 0; i < 10000 && Sim.GetState() != EZSimState::AwaitingCommand; ++i) Sim.Tick(0.05f);
	// Volée : trois impacts, une seule génération de Résonance (+6)
	FZCommand V; V.Type = EZCmd::Ability; V.Actor = Link; V.Id = TEXT("A05"); V.Targets = { 1 };
	TestTrue(TEXT("Volée"), Sim.Submit(V));
	TestEqual(TEXT("+6 pour une action utile, multi-impact compris"), Sim.Resonance, 6);
	for (int32 i = 0; i < 10000 && Sim.GetState() != EZSimState::AwaitingCommand; ++i) Sim.Tick(0.05f);
	FZCommand S; S.Type = EZCmd::Ability; S.Actor = Link; S.Id = TEXT("S01");
	TestTrue(TEXT("Accord spirituel"), Sim.Submit(S));
	TestEqual(TEXT("Accord spirituel remplace le gain normal (+12, pas +18)"), Sim.Resonance, 18);
	for (int32 i = 0; i < 10000 && Sim.GetState() != EZSimState::AwaitingCommand; ++i) Sim.Tick(0.05f);
	FString Why;
	TestFalse(TEXT("Accord spirituel : 1 fois par combat"), Sim.CanUseAbility(Link, TEXT("S01"), Why));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZBossBattleTest, "EchosTriforce.Combat.NereideHeadless", ZTestFlags)
bool FZBossBattleTest::RunTest(const FString&)
{
	// Combat complet sans graphismes : Link / Sheik / Mipha niveau 10 contre le Gardien Néréide
	const UZGameData& DB = UZGameData::Get();
	FZCombatSim Sim;
	Sim.bAutoAck = true;
	TArray<FZCharacterState> Party = {
		ZProgression::MakeCharacter(TEXT("link"), 10),
		ZProgression::MakeCharacter(TEXT("sheik"), 10),
		ZProgression::MakeCharacter(TEXT("mipha"), 10) };
	FString Why;
	ZProgression::Equip(Party[0], EZEquipSlot::Torso, TEXT("OOT_010"), Why);
	ZProgression::Equip(Party[0], EZEquipSlot::ToolA, TEXT("OOT_023"), Why);
	for (const FZCharacterState& C : Party) { Sim.AddAlly(C, ZProgression::ComputeDerived(C)); }
	Sim.Items.Add(TEXT("POTION_RED"), 5);
	const int32 Boss = Sim.AddEnemy(*DB.FindEnemy(TEXT("nereide")));
	Sim.Start(42, 0);

	int32 Turns = 0;
	bool bSawDelugePrep = false;
	bool bValveUsed = false;
	int32 MaxPhase = 0;
	for (int32 Step = 0; Step < 200000 && !Sim.IsOver(); ++Step)
	{
		Sim.Tick(0.05f);
		for (const FZEvent& E : Sim.ConsumeEvents())
		{
			if (E.Type == EZEvt::Prepare && E.Id == FName(TEXT("E_NER_DELUGE"))) bSawDelugePrep = true;
		}
		MaxPhase = FMath::Max(MaxPhase, Sim.Actors[Boss].Phase);
		if (Sim.GetState() != EZSimState::AwaitingCommand) continue;
		const int32 A = Sim.GetCurrentActor();
		const FZBattler& Me = Sim.Actors[A];
		++Turns;
		FZCommand C; C.Actor = A; C.Targets = { Boss };
		FString R;
		// Politique simple : vannes contre le Déluge, grappin contre la Crue, soins sous 45 %, sinon attaque
		int32 Weakest = -1;
		for (const FZBattler& X : Sim.Actors) { if (X.bAlly && !X.bKO && (Weakest < 0 || X.HPPct() < Sim.Actors[Weakest].HPPct())) Weakest = X.Id; }
		if (Sim.CanInteractValve(A, R)) { C.Type = EZCmd::Interact; bValveUsed = true; }
		else if (Me.DefId == FName(TEXT("link")) && Sim.Actors[Boss].Prepared == FName(TEXT("E_NER_CRUE")) && Sim.CanUseAbility(A, TEXT("R01"), R)) { C.Type = EZCmd::Ability; C.Id = TEXT("R01"); }
		else if (Me.DefId == FName(TEXT("mipha")) && Weakest >= 0 && Sim.Actors[Weakest].HPPct() < 0.45f && Sim.CanUseAbility(A, TEXT("MIPHA_HEAL"), R)) { C.Type = EZCmd::Ability; C.Id = TEXT("MIPHA_HEAL"); C.Targets = { Weakest }; }
		else { C.Type = EZCmd::Attack; }
		if (!Sim.Submit(C, &R))
		{
			C.Type = EZCmd::Attack; C.Targets = { Boss };
			Sim.Submit(C);
		}
	}
	AddInfo(FString::Printf(TEXT("Combat terminé en %d commandes ; état %d ; phase max %d ; PV boss %d"), Turns, (int32)Sim.GetState(), MaxPhase, Sim.Actors[Boss].HP));
	TestTrue(TEXT("le combat se termine"), Sim.IsOver());
	TestTrue(TEXT("les trois phases sont atteintes ou le boss tombe"), MaxPhase >= 2 || Sim.Actors[Boss].bKO);
	TestTrue(TEXT("le Déluge est télégraphié"), bSawDelugePrep || Sim.Actors[Boss].bKO);
	if (Sim.GetState() == EZSimState::Victory)
	{
		TestEqual(TEXT("Néréide : 400 XP"), Sim.Result.XP, 400);
		TestEqual(TEXT("Néréide : 20 PA"), Sim.Result.AP, 20);
		TestEqual(TEXT("Néréide : 250 rubis"), Sim.Result.Rupees, 250);
	}
	(void)bValveUsed;
	return true;
}

#endif
