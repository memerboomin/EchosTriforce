#include "ZGameInstance.h"
#include "ZGameData.h"
#include "ZProgression.h"
#include "EchosTriforce.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UZGameInstance::Init()
{
	Super::Init();
	UZGameData::Get(); // charge les données une fois
	NewGame();
}

UZGameInstance* UZGameInstance::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	return W ? Cast<UZGameInstance>(W->GetGameInstance()) : nullptr;
}

void UZGameInstance::NewGame()
{
	Party.Reset();
	ActiveParty.Reset();
	OwnedItems.Reset();
	Consumables.Reset();
	Flags.Reset();
	ClaimedRewards.Reset();
	RecentItems.Reset();
	Rupees = 250;
	Bombs = 10;
	Resonance = 0;
	Chapter = 2;
	Checkpoint = TEXT("Vestibule");
	PlayTime = 0.f;
	bArchivistMode = false;

	// Démonstration isolée du chapitre 17 : Link, Sheik, Mipha niveau 10, équipement rang 2 prêté par une mémoire
	const FName Members[] = { TEXT("link"), TEXT("sheik"), TEXT("mipha") };
	for (FName M : Members) { RecruitMember(M); }

	// Équipements de départ possédés (le reste du catalogue se débloque par quêtes, boutiques, contrats…)
	const TCHAR* Starting[] = {
		TEXT("OOT_001"),      // Épée Kokiri
		TEXT("OOT_005"),      // Bouclier Mojo
		TEXT("OOT_008"),      // Tunique Kokiri
		TEXT("OOT_023"),      // Grappin
		TEXT("OOT_022"),      // Boomerang
		TEXT("P_DEMO_KNIGHT_SWORD"),
		TEXT("MM_002"),       // Razor Sword
		TEXT("OOT_003"),      // Lame des géants (deux mains)
		TEXT("OOT_029"),      // Arc des fées
		TEXT("BOTW_093"),     // Ceremonial Trident (Mipha)
		TEXT("BOTW_201"), TEXT("BOTW_202"), TEXT("BOTW_203"), // ensemble Zora BOTW (3 pièces)
		TEXT("OOS/OOA_001"),  // Friendship Ring
		TEXT("OOS/OOA_002"),  // Power Ring L-1
		TEXT("OOS/OOA_005"),  // Armor Ring L-1
		TEXT("MM_029"),       // Masque du lapin
		TEXT("MM_030"),       // Masque de Keaton
		TEXT("MM_026"),       // Masque de Brême
	};
	for (const TCHAR* Id : Starting) { GrantItem(Id); }
	RecentItems.Reset();

	FString Why;
	if (FZCharacterState* L = FindMember(TEXT("link")))
	{
		ZProgression::Equip(*L, EZEquipSlot::ToolA, TEXT("OOT_023"), Why);
		ZProgression::Equip(*L, EZEquipSlot::ToolB, TEXT("OOT_022"), Why);
		L->Spirit = TEXT("GreatFairy"); // Grande Fée prêtée (salle 05)
		ZProgression::AutoPrepare(*L);
	}
	if (FZCharacterState* M = FindMember(TEXT("mipha")))
	{
		ZProgression::Equip(*M, EZEquipSlot::Weapon, TEXT("BOTW_093"), Why);
		M->Spirit = TEXT("Nayru");
		ZProgression::AutoPrepare(*M);
	}
	if (FZCharacterState* S = FindMember(TEXT("sheik")))
	{
		S->Spirit = TEXT("Farore");
		ZProgression::AutoPrepare(*S);
	}

	Consumables.Add(TEXT("POTION_RED"), 2);
	Consumables.Add(TEXT("POTION_GREEN"), 2);
	Consumables.Add(TEXT("REMEDY"), 2);
	Consumables.Add(TEXT("TOWEL"), 3);
	Consumables.Add(TEXT("FAIRY"), 1);
	Consumables.Add(TEXT("MILK"), 1);
}

FZCharacterState* UZGameInstance::FindMember(FName Id)
{
	return Party.FindByPredicate([Id](const FZCharacterState& S) { return S.Id == Id; });
}

const FZCharacterState* UZGameInstance::FindMember(FName Id) const
{
	return Party.FindByPredicate([Id](const FZCharacterState& S) { return S.Id == Id; });
}

TArray<FZCharacterState*> UZGameInstance::GetActiveParty()
{
	TArray<FZCharacterState*> Out;
	for (FName Id : ActiveParty) { if (FZCharacterState* S = FindMember(Id)) Out.Add(S); }
	return Out;
}

FZDerivedStats UZGameInstance::GetDerived(FName Id) const
{
	const FZCharacterState* S = FindMember(Id);
	return S ? ZProgression::ComputeDerived(*S) : FZDerivedStats();
}

void UZGameInstance::RecruitMember(FName Id)
{
	if (FindMember(Id)) return;
	// Les compagnons rejoignent au niveau de Link
	const FZCharacterState* Link = FindMember(TEXT("link"));
	const int32 Level = Link ? Link->Level : 10;
	FZCharacterState S = ZProgression::MakeCharacter(Id, Level);
	for (const FString& E : S.Equip) { if (!E.IsEmpty()) GrantItem(E); }
	Party.Add(S);
	if (ActiveParty.Num() < 3) ActiveParty.Add(Id);
}

void UZGameInstance::RestoreParty()
{
	for (FZCharacterState& S : Party) { S.CurrentHP = -1; S.CurrentMP = -1; }
}

void UZGameInstance::GrantItem(const FString& ItemId, int32 Count)
{
	if (!UZGameData::Get().FindItem(ItemId)) return;
	OwnedItems.FindOrAdd(ItemId) += Count;
	RecentItems.Remove(ItemId);
	RecentItems.Insert(ItemId, 0);
	if (RecentItems.Num() > 20) RecentItems.SetNum(20);
}

int32 UZGameInstance::CountEquipped(const FString& ItemId) const
{
	int32 N = 0;
	for (const FZCharacterState& S : Party)
	{
		TSet<FString> Seen;
		for (const FString& E : S.Equip) { if (E == ItemId && !Seen.Contains(E)) { ++N; Seen.Add(E); } }
	}
	return N;
}

TArray<FName> UZGameInstance::UnlockedForms() const
{
	TArray<FName> Out;
	for (const FZFormDef& F : UZGameData::Get().Forms)
	{
		const bool bByMask = !F.MaskItem.IsEmpty() && Owns(F.MaskItem);
		const bool bByFlag = HasFlag(FName(*F.Flag));
		if (bByMask || bByFlag) Out.Add(F.Id);
	}
	return Out;
}

void UZGameInstance::SetupBattle(FZCombatSim& Sim, const TArray<FName>& Enemies, bool bPreemptive)
{
	const UZGameData& DB = UZGameData::Get();
	for (FZCharacterState* S : GetActiveParty())
	{
		const int32 Id = Sim.AddAlly(*S, ZProgression::ComputeDerived(*S));
		if (S->Id == FName(TEXT("link"))) { Sim.Actors[Id].Forms = UnlockedForms(); }
	}
	for (FName E : Enemies)
	{
		if (const FZEnemyDef* Def = DB.FindEnemy(E)) Sim.AddEnemy(*Def);
	}
	Sim.Items = Consumables;
	Sim.Bombs = Bombs;
	Sim.SpeedScale = BattleSpeed;
	Sim.EnemyDamageScale = DifficultyDamage;
	Sim.Start((uint32)(FDateTime::Now().GetTicks() & 0x7fffffff), Resonance, bPreemptive, false);
}

void UZGameInstance::SyncFromBattle(const FZCombatSim& Sim)
{
	for (const FZBattler& B : Sim.Actors)
	{
		if (!B.bAlly) continue;
		if (FZCharacterState* S = FindMember(B.DefId))
		{
			S->CurrentHP = B.bKO ? 1 : B.HP;
			S->CurrentMP = B.MP;
		}
	}
	Consumables = Sim.Items;
	Bombs = Sim.Bombs;
}

FZVictoryReport UZGameInstance::ApplyBattleResult(const FZCombatSim& Sim)
{
	const UZGameData& DB = UZGameData::Get();
	FZVictoryReport R;
	SyncFromBattle(Sim);
	if (!Sim.Result.bVictory)
	{
		Resonance = 0;
		return R;
	}
	R.XP = Sim.Result.XP;
	R.AP = Sim.Result.AP;
	R.Rupees = Sim.Result.Rupees;

	float RupeeBonus = 0.f;
	for (FZCharacterState* S : GetActiveParty())
	{
		RupeeBonus = FMath::Max(RupeeBonus, ZProgression::ComputeDerived(*S).GetSpecial(TEXT("RUPEES_PCT")));
	}
	R.Rupees = FMath::FloorToInt(R.Rupees * (1.f + RupeeBonus));
	Rupees += R.Rupees;
	Resonance = Sim.Result.ResonanceKept;

	// XP complète à chaque personnage recruté, actif ou en réserve (chapitre 7)
	for (FZCharacterState& S : Party)
	{
		const FZDerivedStats Before = ZProgression::ComputeDerived(S);
		const int32 OldLevel = S.Level;
		S.XP += R.XP;
		S.Level = ZProgression::LevelForXP(S.XP);
		if (S.Level > OldLevel)
		{
			const FZDerivedStats After = ZProgression::ComputeDerived(S);
			// Un gain de niveau augmente PV et PM courants de la différence entre les maxima, sans soigner complètement
			const int32 CurHP = S.CurrentHP < 0 ? Before.MaxHP : S.CurrentHP;
			const int32 CurMP = S.CurrentMP < 0 ? Before.MaxMP : S.CurrentMP;
			S.CurrentHP = FMath::Min(After.MaxHP, CurHP + (After.MaxHP - Before.MaxHP));
			S.CurrentMP = FMath::Min(After.MaxMP, CurMP + (After.MaxMP - Before.MaxMP));
			const FZCharacterDef* Def = DB.FindCharacter(S.Id);
			R.Lines.Add(FString::Printf(TEXT("%s passe au niveau %d !  PV %d → %d · ATQ %d → %d"),
				Def ? *Def->Name : *S.Id.ToString(), S.Level, Before.MaxHP, After.MaxHP, (int32)Before.ATQ, (int32)After.ATQ));
		}
	}

	// PA : chaque pièce portée reçoit les PA complets ; un même identifiant une seule fois par combat
	for (FZCharacterState* S : GetActiveParty())
	{
		const FZDerivedStats D = ZProgression::ComputeDerived(*S);
		TSet<FName> Done;
		const FZCharacterDef* Def = DB.FindCharacter(S->Id);
		for (FName A : D.GrantedAbilities)
		{
			if (Done.Contains(A)) continue;
			Done.Add(A);
			int32 Need = 0;
			FString Name = A.ToString();
			if (const FZAbilityDef* Ab = DB.FindAbility(A)) { Need = Ab->AP; Name = Ab->Name; }
			else if (const FZPassiveDef* P = DB.FindPassive(A)) { Need = P->AP; Name = P->Name; }
			if (Need <= 0) continue;
			const bool bWas = ZProgression::IsMastered(*S, A);
			int32& Have = S->AbilityAP.FindOrAdd(A);
			Have = FMath::Min(Need, Have + R.AP);
			if (!bWas && Have >= Need)
			{
				R.Lines.Add(FString::Printf(TEXT("%s maîtrise « %s » : elle reste acquise sans l'équipement."), Def ? *Def->Name : *S->Id.ToString(), *Name));
			}
		}
		// Récupération de PM en fin de combat (Energy Glove / Belt)
		const int32 VictoryMP = (int32)D.GetSpecial(TEXT("VICTORY_MP"));
		if (VictoryMP > 0 && S->CurrentMP >= 0) S->CurrentMP = FMath::Min(D.MaxMP, S->CurrentMP + VictoryMP);
	}
	return R;
}

bool UZGameInstance::ClaimReward(FName RewardId)
{
	if (ClaimedRewards.Contains(RewardId)) return false;
	ClaimedRewards.Add(RewardId);
	return true;
}

bool UZGameInstance::SaveToSlot(int32 Slot)
{
	UZSaveGame* SG = Cast<UZSaveGame>(UGameplayStatics::CreateSaveGameObject(UZSaveGame::StaticClass()));
	SG->Party = Party;
	SG->ActiveParty = ActiveParty;
	SG->OwnedItems = OwnedItems;
	SG->Consumables = Consumables;
	SG->Flags = Flags;
	SG->ClaimedRewards = ClaimedRewards;
	SG->Rupees = Rupees;
	SG->Bombs = Bombs;
	SG->Resonance = Resonance;
	SG->Chapter = Chapter;
	SG->Checkpoint = Checkpoint;
	SG->PlayTime = PlayTime;
	SG->SavedAt = FDateTime::Now();
	const FString Name = FString::Printf(TEXT("Echos_%d"), Slot);
	// Copie précédente conservée
	if (UGameplayStatics::DoesSaveGameExist(Name, 0))
	{
		if (USaveGame* Old = UGameplayStatics::LoadGameFromSlot(Name, 0))
		{
			UGameplayStatics::SaveGameToSlot(Old, Name + TEXT("_backup"), 0);
		}
	}
	return UGameplayStatics::SaveGameToSlot(SG, Name, 0);
}

bool UZGameInstance::LoadFromSlot(int32 Slot)
{
	const FString Name = FString::Printf(TEXT("Echos_%d"), Slot);
	UZSaveGame* SG = Cast<UZSaveGame>(UGameplayStatics::LoadGameFromSlot(Name, 0));
	if (!SG)
	{
		SG = Cast<UZSaveGame>(UGameplayStatics::LoadGameFromSlot(Name + TEXT("_backup"), 0));
		if (!SG) return false;
		UE_LOG(LogEchos, Warning, TEXT("Sauvegarde principale illisible : restauration de la copie"));
	}
	Party = SG->Party;
	ActiveParty = SG->ActiveParty;
	OwnedItems = SG->OwnedItems;
	Consumables = SG->Consumables;
	Flags = SG->Flags;
	ClaimedRewards = SG->ClaimedRewards;
	Rupees = SG->Rupees;
	Bombs = SG->Bombs;
	Resonance = SG->Resonance;
	Chapter = SG->Chapter;
	Checkpoint = SG->Checkpoint;
	PlayTime = SG->PlayTime;
	// Un identifiant supprimé devient une entrée de compensation, pas un crash (chapitre 23)
	const UZGameData& DB = UZGameData::Get();
	TArray<FString> Missing;
	for (const TPair<FString, int32>& KV : OwnedItems) { if (!DB.FindItem(KV.Key)) Missing.Add(KV.Key); }
	for (const FString& M : Missing) { OwnedItems.Remove(M); Rupees += 100; }
	for (FZCharacterState& S : Party)
	{
		S.Equip.SetNum(ZEquipSlotCount);
		for (FString& E : S.Equip) { if (!E.IsEmpty() && !DB.FindItem(E)) E.Reset(); }
	}
	return true;
}

bool UZGameInstance::HasSave(int32 Slot) const
{
	return UGameplayStatics::DoesSaveGameExist(FString::Printf(TEXT("Echos_%d"), Slot), 0);
}

void UZGameInstance::UnlockEverything()
{
	bArchivistMode = true;
	for (const FZItemDef& It : UZGameData::Get().Items)
	{
		if (!OwnedItems.Contains(It.Id)) OwnedItems.Add(It.Id, 1);
	}
	SetFlag(TEXT("form.wolf"));
	SetFlag(TEXT("form.majora"));
	for (FZCharacterState& S : Party) { ZProgression::AutoPrepare(S); }
}
