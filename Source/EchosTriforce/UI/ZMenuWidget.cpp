#include "ZMenuWidget.h"
#include "ZUI.h"
#include "ZGameInstance.h"
#include "ZGameData.h"
#include "ZProgression.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Kismet/GameplayStatics.h"

using namespace ZUI;

namespace
{
	const TCHAR* TabNames[] = { TEXT("Statut"), TEXT("Équipement"), TEXT("Techniques"), TEXT("Formes & Esprits"), TEXT("Collection"), TEXT("Journal"), TEXT("Système") };
	constexpr int32 NumTabs = 7;
	const TCHAR* TypeNames[] = { TEXT("Tous types"), TEXT("Armes"), TEXT("Boucliers"), TEXT("Tenues"), TEXT("Accessoires"), TEXT("Masques & reliques"), TEXT("Outils & munitions"), TEXT("Exploration & autres") };
	constexpr int32 NumTypes = 8;

	USizeBox* Sized(UWidgetTree* T, UWidget* C, float W, float H)
	{
		USizeBox* S = T->ConstructWidget<USizeBox>();
		if (W > 0) S->SetWidthOverride(W);
		if (H > 0) S->SetHeightOverride(H);
		S->SetContent(C);
		return S;
	}

	bool MatchType(const FZItemDef& It, int32 Type)
	{
		switch (Type)
		{
		case 1: return It.Slot == EZSlot::Weapon;
		case 2: return It.Slot == EZSlot::Offhand;
		case 3: return It.Slot == EZSlot::Head || It.Slot == EZSlot::Torso || It.Slot == EZSlot::Legs || It.Slot == EZSlot::Outfit;
		case 4: return It.Slot == EZSlot::Accessory;
		case 5: return It.Slot == EZSlot::Relic || It.Slot == EZSlot::Spell;
		case 6: return It.Slot == EZSlot::Tool || It.Slot == EZSlot::Ammo;
		case 7: return It.Slot == EZSlot::Traversal || It.Slot == EZSlot::Upgrade || It.Slot == EZSlot::Cosmetic;
		default: return true;
		}
	}

	FString Pct(float V) { return FString::Printf(TEXT("%+d %%"), FMath::RoundToInt(V * 100.f)); }
}

TSharedRef<SWidget> UZMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Build();
	return Super::RebuildWidget();
}

void UZMenuWidget::MakeRows(UVerticalBox* Box, int32 Count, int32 ColumnIdx, TArray<FRowW>& Out, float LabelSize)
{
	for (int32 i = 0; i < Count; ++i)
	{
		FRowW R;
		R.Btn = WidgetTree->ConstructWidget<UZEntryButton>(UZEntryButton::StaticClass());
		R.Btn->Index = i;
		R.Btn->OnHover = [this, ColumnIdx](int32 I)
		{
			if (Column != ColumnIdx) return;
			int32& S = ColumnIdx == 0 ? MemberSel : (ColumnIdx == 1 ? Sel1 : Sel2);
			const int32 Scroll = ColumnIdx == 1 ? Scroll1 : (ColumnIdx == 2 ? Scroll2 : 0);
			if (S != I + Scroll) { S = I + Scroll; Refresh(); }
		};
		R.Btn->OnPick = [this, ColumnIdx](int32 I)
		{
			const int32 Scroll = ColumnIdx == 1 ? Scroll1 : (ColumnIdx == 2 ? Scroll2 : 0);
			Column = ColumnIdx;
			(ColumnIdx == 0 ? MemberSel : (ColumnIdx == 1 ? Sel1 : Sel2)) = I + Scroll;
			Refresh();
			Confirm();
		};
		R.Btn->Bind();
		R.BG = MakePanel(WidgetTree, FLinearColor::Transparent, 14.f, FMargin(12, 3, 14, 3));
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		R.Icon = MakeText(WidgetTree, TEXT(""), LabelSize - 3.f, Text);
		H->AddChildToHorizontalBox(Sized(WidgetTree, R.Icon, 26, 0));
		R.Label = MakeText(WidgetTree, TEXT(""), LabelSize, Text);
		UHorizontalBoxSlot* LS = H->AddChildToHorizontalBox(R.Label);
		LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LS->SetVerticalAlignment(VAlign_Center);
		R.Value = MakeText(WidgetTree, TEXT(""), LabelSize - 4.f, TextDim);
		H->AddChildToHorizontalBox(R.Value)->SetPadding(FMargin(10, 0, 0, 0));
		R.BG->SetContent(H);
		R.Btn->SetFillContent(R.BG);
		Box->AddChildToVerticalBox(R.Btn)->SetPadding(FMargin(0, 1));
		Out.Add(R);
	}
}

void UZMenuWidget::Build()
{
	SetIsFocusable(true);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = MakePanel(WidgetTree, FLinearColor(0.f, 0.015f, 0.02f, 0.72f), 0.f, FMargin(0));
	UCanvasPanelSlot* DS = Place(Root, Dim, FAnchors(0, 0, 1, 1), FVector2D::ZeroVector, FVector2D::ZeroVector);
	DS->SetOffsets(FMargin(0));

	// Onglets
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < NumTabs; ++i)
	{
		UZEntryButton* B = WidgetTree->ConstructWidget<UZEntryButton>(UZEntryButton::StaticClass());
		B->Index = i;
		B->OnPick = [this](int32 I) { SwitchTab(I - Tab); };
		B->Bind();
		UBorder* BG = MakePanel(WidgetTree, PanelBG, 18.f, FMargin(22, 8), PanelBorder, 1.f);
		UTextBlock* T = MakeText(WidgetTree, TabNames[i], 20, Text, true, 20);
		BG->SetContent(T);
		B->SetFillContent(BG);
		Tabs->AddChildToHorizontalBox(B)->SetPadding(FMargin(4, 0));
		TabBG.Add(BG);
		TabText.Add(T);
	}
	Place(Root, Tabs, FAnchors(0.5f, 0.f), FVector2D(0, 26), FVector2D(0.5f, 0.f));
	HeaderInfo = MakeText(WidgetTree, TEXT(""), 17, Gold, true);
	Place(Root, HeaderInfo, FAnchors(1.f, 0.f), FVector2D(-36, 80), FVector2D(1.f, 0.f));

	// Colonne équipe
	UBorder* PartyPanel = MakePanel(WidgetTree, PanelBG, 14.f, FMargin(8), PanelBorder, 1.2f);
	UVerticalBox* PV = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 i = 0; i < 8; ++i)
	{
		FRowW R;
		R.Btn = WidgetTree->ConstructWidget<UZEntryButton>(UZEntryButton::StaticClass());
		R.Btn->Index = i;
		R.Btn->OnHover = [this](int32 I) { if (Column == 0 && MemberSel != I) { MemberSel = I; Refresh(); } };
		R.Btn->OnPick = [this](int32 I) { Column = 0; MemberSel = I; Refresh(); Confirm(); };
		R.Btn->Bind();
		R.BG = MakePanel(WidgetTree, FLinearColor::Transparent, 30.f, FMargin(6, 5, 14, 5));
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		UImage* Img = MakeImage(WidgetTree, nullptr, FVector2D(50, 50), 25.f, Accent, 1.5f);
		H->AddChildToHorizontalBox(Sized(WidgetTree, Img, 50, 50))->SetPadding(FMargin(0, 0, 10, 0));
		UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
		R.Label = MakeText(WidgetTree, TEXT(""), 20, Text, true);
		V->AddChildToVerticalBox(R.Label);
		R.Value = MakeText(WidgetTree, TEXT(""), 14, TextDim);
		V->AddChildToVerticalBox(R.Value);
		H->AddChildToHorizontalBox(V)->SetVerticalAlignment(VAlign_Center);
		R.BG->SetContent(Sized(WidgetTree, H, 250, 0));
		R.Btn->SetFillContent(R.BG);
		PV->AddChildToVerticalBox(R.Btn)->SetPadding(FMargin(0, 2));
		PartyRows.Add(R);
		PartyImages.Add(Img);
	}
	PartyPanel->SetContent(PV);
	Place(Root, PartyPanel, FAnchors(0, 0), FVector2D(36, 110), FVector2D(0, 0));

	// Colonne 1
	UBorder* P1 = MakePanel(WidgetTree, PanelBG, 14.f, FMargin(10, 10), PanelBorder, 1.2f);
	UVerticalBox* V1 = WidgetTree->ConstructWidget<UVerticalBox>();
	Col1Title = MakeText(WidgetTree, TEXT(""), 22, Accent, true, 40);
	V1->AddChildToVerticalBox(Col1Title)->SetPadding(FMargin(10, 0, 0, 6));
	MakeRows(V1, 15, 1, Rows1, 19.f);
	P1->SetContent(Sized(WidgetTree, V1, 390, 0));
	Place(Root, P1, FAnchors(0, 0), FVector2D(322, 110), FVector2D(0, 0));

	// Colonne 2
	Col2Panel = MakePanel(WidgetTree, PanelBG, 14.f, FMargin(10, 10), PanelBorder, 1.2f);
	UVerticalBox* V2 = WidgetTree->ConstructWidget<UVerticalBox>();
	Col2Title = MakeText(WidgetTree, TEXT(""), 22, Accent, true, 40);
	V2->AddChildToVerticalBox(Col2Title)->SetPadding(FMargin(10, 0, 0, 6));
	MakeRows(V2, 17, 2, Rows2, 17.f);
	Col2Panel->SetContent(Sized(WidgetTree, V2, 400, 0));
	Place(Root, Col2Panel, FAnchors(0, 0), FVector2D(734, 110), FVector2D(0, 0));

	// Détail
	UBorder* DP = MakePanel(WidgetTree, PanelBG, 14.f, FMargin(20, 16), PanelBorder, 1.2f);
	UVerticalBox* DV = WidgetTree->ConstructWidget<UVerticalBox>();
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>();
	DetailImage = MakeImage(WidgetTree, nullptr, FVector2D(150, 200), 10.f);
	Head->AddChildToHorizontalBox(Sized(WidgetTree, DetailImage, 150, 200))->SetPadding(FMargin(0, 0, 16, 0));
	UVerticalBox* HV = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailTitle = MakeText(WidgetTree, TEXT(""), 28, Text, true, 30);
	DetailTitle->SetAutoWrapText(true);
	HV->AddChildToVerticalBox(DetailTitle);
	DetailSub = MakeText(WidgetTree, TEXT(""), 16, TextDim);
	DetailSub->SetAutoWrapText(true);
	HV->AddChildToVerticalBox(DetailSub)->SetPadding(FMargin(0, 4));
	Head->AddChildToHorizontalBox(Sized(WidgetTree, HV, 480, 0))->SetVerticalAlignment(VAlign_Top);
	DV->AddChildToVerticalBox(Head);
	UVerticalBox* Grid = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 i = 0; i < 14; ++i)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* L = MakeText(WidgetTree, TEXT(""), 16, TextDim, true);
		UTextBlock* B = MakeText(WidgetTree, TEXT(""), 16, Text);
		UTextBlock* A = MakeText(WidgetTree, TEXT(""), 16, Good, true);
		Row->AddChildToHorizontalBox(Sized(WidgetTree, L, 130, 0));
		Row->AddChildToHorizontalBox(Sized(WidgetTree, B, 120, 0));
		Row->AddChildToHorizontalBox(Sized(WidgetTree, A, 220, 0));
		Grid->AddChildToVerticalBox(Row);
		CmpLabel.Add(L); CmpBefore.Add(B); CmpAfter.Add(A);
	}
	DV->AddChildToVerticalBox(Grid)->SetPadding(FMargin(0, 10, 0, 4));
	ResistText = MakeText(WidgetTree, TEXT(""), 15, Text);
	ResistText->SetAutoWrapText(true);
	DV->AddChildToVerticalBox(Sized(WidgetTree, ResistText, 650, 0))->SetPadding(FMargin(0, 4));
	DetailBody = MakeText(WidgetTree, TEXT(""), 16, Text);
	DetailBody->SetAutoWrapText(true);
	DV->AddChildToVerticalBox(Sized(WidgetTree, DetailBody, 650, 0))->SetPadding(FMargin(0, 6));
	DP->SetContent(Sized(WidgetTree, DV, 690, 0));
	DetailPanel = DP;
	Place(Root, DP, FAnchors(1, 0), FVector2D(-26, 110), FVector2D(1, 0));

	FooterText = MakeText(WidgetTree, TEXT(""), 16, TextDim);
	Place(Root, FooterText, FAnchors(0.5f, 1.f), FVector2D(0, -18), FVector2D(0.5f, 1.f));
	ToastText = MakeText(WidgetTree, TEXT(""), 20, Gold, true);
	Place(Root, ToastText, FAnchors(0.5f, 1.f), FVector2D(0, -52), FVector2D(0.5f, 1.f));
	Refresh();
}

void UZMenuWidget::OpenAt(int32 InTab)
{
	Tab = FMath::Clamp(InTab, 0, NumTabs - 1);
	Column = Tab >= 4 ? 1 : 0;
	Sel1 = Sel2 = Scroll1 = Scroll2 = 0;
	if (WidgetTree && WidgetTree->RootWidget) Refresh();
}

FZCharacterState* UZMenuWidget::Member()
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	if (!GI || GI->Party.Num() == 0) return nullptr;
	MemberSel = FMath::Clamp(MemberSel, 0, GI->Party.Num() - 1);
	return &GI->Party[MemberSel];
}

void UZMenuWidget::BuildColumns()
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	const UZGameData& DB = UZGameData::Get();
	Col1.Reset();
	Col2.Reset();
	FZCharacterState* M = Member();
	if (!GI) return;

	auto Row = [](const FString& Icon, const FString& Label, const FString& Value, const FString& Key, bool bEnabled = true, FLinearColor Color = FLinearColor::White, int32 Data = 0)
	{
		FZMenuRow R; R.Icon = Icon; R.Label = Label; R.Value = Value; R.Key = Key; R.bEnabled = bEnabled; R.Color = Color; R.Data = Data;
		return R;
	};

	switch (Tab)
	{
	case 0:
		Col1Title->SetText(FText::FromString(TEXT("")));
		break;
	case 1: // Équipement
	{
		Col1Title->SetText(FText::FromString(TEXT("Emplacements")));
		if (!M) break;
		for (int32 s = 0; s < ZEquipSlotCount; ++s)
		{
			const FZItemDef* It = DB.FindItem(M->GetEquip((EZEquipSlot)s));
			const bool bOutfitRepeat = It && It->Slot == EZSlot::Outfit && (s == (int32)EZEquipSlot::Head || s == (int32)EZEquipSlot::Legs);
			Col1.Add(Row(ZNames::SlotName((EZEquipSlot)s).Left(1), ZNames::SlotName((EZEquipSlot)s),
				It ? (bOutfitRepeat ? TEXT("(tenue complète)") : It->DisplayName()) : TEXT("—"), FString::FromInt(s)));
		}
		if (Column == 2 && Col1.IsValidIndex(Sel1))
		{
			const EZEquipSlot Slot = (EZEquipSlot)Sel1;
			Col2Title->SetText(FText::FromString(ZNames::SlotName(Slot)));
			Col2.Add(Row(TEXT("×"), TEXT("— Retirer —"), FString(), TEXT(""), true, TextDim));
			TArray<const FZItemDef*> Cands;
			for (const TPair<FString, int32>& KV : GI->OwnedItems)
			{
				const FZItemDef* It = DB.FindItem(KV.Key);
				if (!It || !ZProgression::SlotAccepts(Slot, It->Slot)) continue;
				Cands.Add(It);
			}
			Cands.Sort([](const FZItemDef& A, const FZItemDef& B) { return A.Rank != B.Rank ? A.Rank > B.Rank : A.DisplayName() < B.DisplayName(); });
			for (const FZItemDef* It : Cands)
			{
				FString Why;
				const bool bOk = ZProgression::CanEquip(*M, Slot, *It, Why);
				const int32 Owned = GI->OwnedItems.FindRef(It->Id);
				const int32 Used = GI->CountEquipped(It->Id) - (M->Equip.Contains(It->Id) ? 1 : 0);
				const bool bAvail = Owned > Used;
				Col2.Add(Row(FString::Printf(TEXT("%d"), It->Rank), It->DisplayName(), It->Game, It->Id, bOk && bAvail, bOk ? Text : TextDisabled));
			}
		}
		break;
	}
	case 2: // Techniques
	{
		Col1Title->SetText(FText::FromString(TEXT("Techniques préparées")));
		if (!M) break;
		for (int32 i = 0; i < 6; ++i)
		{
			const FName A = M->Prepared.IsValidIndex(i) ? M->Prepared[i] : NAME_None;
			const FZAbilityDef* Ab = DB.FindAbility(A);
			Col1.Add(Row(FString::FromInt(i + 1), Ab ? Ab->Name : TEXT("— vide —"), Ab && Ab->MP > 0 ? FString::Printf(TEXT("%d PM"), Ab->MP) : FString(), TEXT("slot"), true, Ab ? Text : TextDim, i));
		}
		const int32 Cap = ZProgression::PassiveCapacity(M->Level);
		const int32 Used = ZProgression::PassiveCostUsed(*M);
		for (const FZPassiveDef& P : DB.Passives)
		{
			const bool bMastered = ZProgression::IsMastered(*M, P.Id);
			const bool bOn = M->Passives.Contains(P.Id);
			const int32 Have = M->AbilityAP.FindRef(P.Id);
			Col1.Add(Row(bOn ? TEXT("●") : TEXT("○"), P.Name, bMastered ? FString::Printf(TEXT("%d pts"), P.Cost) : FString::Printf(TEXT("%d/%d PA"), Have, P.AP),
				TEXT("passive:") + P.Id.ToString(), bMastered, bMastered ? Text : TextDim));
		}
		Col1Title->SetText(FText::FromString(FString::Printf(TEXT("Techniques · passifs %d/%d"), Used, Cap)));
		if (Column == 2 && Col1.IsValidIndex(Sel1) && Col1[Sel1].Key == TEXT("slot"))
		{
			Col2Title->SetText(FText::FromString(TEXT("Disponibles")));
			Col2.Add(Row(TEXT("×"), TEXT("— Vider l'emplacement —"), FString(), TEXT(""), true, TextDim));
			for (FName A : ZProgression::AvailableAbilities(*M))
			{
				const FZAbilityDef* Ab = DB.FindAbility(A);
				if (!Ab || Ab->School == TEXT("Relic") || Ab->School == TEXT("Form")) continue;
				const bool bPrepared = M->Prepared.Contains(A);
				const bool bMastered = ZProgression::IsMastered(*M, A);
				Col2.Add(Row(bMastered ? TEXT("★") : TEXT("◇"), Ab->Name, bPrepared ? TEXT("préparée") : (Ab->MP > 0 ? FString::Printf(TEXT("%d PM"), Ab->MP) : FString()),
					A.ToString(), !bPrepared, bPrepared ? TextDim : Text));
			}
		}
		break;
	}
	case 3: // Formes & Esprits
	{
		Col1Title->SetText(FText::FromString(TEXT("Formes et esprits")));
		const TArray<FName> Unlocked = GI->UnlockedForms();
		for (const FZFormDef& F : DB.Forms)
		{
			const bool bOk = Unlocked.Contains(F.Id);
			Col1.Add(Row(TEXT("▲"), bOk ? F.Name : FString::Printf(TEXT("%s (verrouillée)"), *F.Name), FString::Printf(TEXT("%d R"), F.Resonance), TEXT("form:") + F.Id.ToString(), true, bOk ? Text : TextDim));
		}
		for (const FZSpiritDef& S : DB.Spirits)
		{
			FString Holder;
			for (const FZCharacterState& C : GI->Party) { if (C.Spirit == S.Id) { if (const FZCharacterDef* D = DB.FindCharacter(C.Id)) Holder = D->Name; } }
			Col1.Add(Row(TEXT("✧"), S.Name, Holder.IsEmpty() ? FString::Printf(TEXT("%d R"), S.Resonance) : TEXT("lié : ") + Holder, TEXT("spirit:") + S.Id.ToString(), true, Holder.IsEmpty() ? Text : Accent));
		}
		break;
	}
	case 4: // Collection
	{
		const int32 Total = DB.Items.Num();
		Col1Title->SetText(FText::FromString(FString::Printf(TEXT("Archives · %d / %d"), GI->CountOwned(), Total)));
		{
			int32 Own = 0, All = 0;
			for (const FZItemDef& It : DB.Items) { if (!MatchType(It, TypeFilter)) continue; ++All; if (GI->Owns(It.Id)) ++Own; }
			Col1.Add(Row(TEXT("◎"), TEXT("Tous les jeux"), FString::Printf(TEXT("%d/%d"), Own, All), TEXT("ALL")));
		}
		for (const FString& G : DB.GameOrder)
		{
			int32 Own = 0, All = 0;
			FString Name = G;
			for (const FZItemDef& It : DB.Items)
			{
				if (It.Game != G || !MatchType(It, TypeFilter)) continue;
				++All;
				if (GI->Owns(It.Id)) ++Own;
				Name = It.GameName;
			}
			if (All == 0) continue;
			Col1.Add(Row(TEXT("◆"), Name, FString::Printf(TEXT("%d/%d"), Own, All), G, true, Own == All ? Gold : Text));
		}
		if (Column == 2 && Col1.IsValidIndex(Sel1))
		{
			const FString G = Col1[Sel1].Key;
			Col2Title->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *Col1[Sel1].Label, TypeNames[TypeFilter])));
			for (const FZItemDef& It : DB.Items)
			{
				if ((G != TEXT("ALL") && It.Game != G) || !MatchType(It, TypeFilter)) continue;
				const bool bOwn = GI->Owns(It.Id);
				Col2.Add(Row(bOwn ? TEXT("✓") : TEXT("·"), It.DisplayName(), ZNames::ItemSlotName(It.Slot), It.Id, true, bOwn ? Text : TextDim));
			}
		}
		break;
	}
	case 5: // Journal
		Col1Title->SetText(FText::FromString(TEXT("Quêtes et sources")));
		for (const FZQuestDef& Q : DB.Quests)
		{
			const bool bDone = GI->HasFlag(Q.Id) || GI->ClaimedRewards.Contains(Q.Id);
			Col1.Add(Row(bDone ? TEXT("✓") : TEXT("◇"), Q.Name, FString::Printf(TEXT("C%d"), Q.Chapter), Q.Id.ToString(), true, bDone ? Good : Text));
		}
		break;
	case 6: // Système
	{
		Col1Title->SetText(FText::FromString(TEXT("Système")));
		static const TCHAR* Diff[] = { TEXT("Histoire (×0,7)"), TEXT("Standard"), TEXT("Héroïque (×1,2)") };
		const int32 DiffIdx = GI->DifficultyDamage < 0.8f ? 0 : (GI->DifficultyDamage > 1.1f ? 2 : 1);
		Col1.Add(Row(TEXT("◆"), TEXT("Sauvegarder"), GI->HasSave() ? TEXT("écrase l'emplacement 1") : FString(), TEXT("save")));
		Col1.Add(Row(TEXT("◆"), TEXT("Charger"), GI->HasSave() ? FString() : TEXT("aucune sauvegarde"), TEXT("load"), GI->HasSave()));
		Col1.Add(Row(TEXT("◆"), TEXT("Vitesse des jauges ATB"), FString::Printf(TEXT("%.1f×"), GI->BattleSpeed), TEXT("atb")));
		Col1.Add(Row(TEXT("◆"), TEXT("Vitesse des animations"), FString::Printf(TEXT("%.0f×"), GI->AnimSpeed), TEXT("anim")));
		Col1.Add(Row(TEXT("◆"), TEXT("Difficulté"), Diff[DiffIdx], TEXT("diff")));
		Col1.Add(Row(TEXT("◆"), TEXT("Taille du texte"), FString::Printf(TEXT("%d %%"), GI->TextScale), TEXT("text")));
		Col1.Add(Row(TEXT("◆"), TEXT("Mode Archiviste (test)"), GI->bArchivistMode ? TEXT("actif") : TEXT("tout débloquer"), TEXT("archivist")));
		Col1.Add(Row(TEXT("◆"), TEXT("Retour à l'écran titre"), FString(), TEXT("title")));
		Col1.Add(Row(TEXT("◆"), TEXT("Fermer le menu"), FString(), TEXT("close")));
		break;
	}
	}
	if (Column < 2)
	{
		Col2Title->SetText(FText::GetEmpty());
	}
}

void UZMenuWidget::RefreshRows(const TArray<FZMenuRow>& Data, TArray<FRowW>& Rows, int32 SelIdx, int32& Scroll, bool bActive)
{
	const int32 Visible = Rows.Num();
	if (SelIdx < Scroll) Scroll = SelIdx;
	if (SelIdx >= Scroll + Visible) Scroll = SelIdx - Visible + 1;
	Scroll = FMath::Clamp(Scroll, 0, FMath::Max(0, Data.Num() - Visible));
	for (int32 i = 0; i < Visible; ++i)
	{
		const int32 D = i + Scroll;
		const bool bUsed = Data.IsValidIndex(D);
		Rows[i].Btn->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bUsed) continue;
		const FZMenuRow& R = Data[D];
		const bool bSel = D == SelIdx;
		Rows[i].Icon->SetText(FText::FromString(R.Icon));
		Rows[i].Label->SetText(FText::FromString(R.Label));
		Rows[i].Value->SetText(FText::FromString(R.Value));
		const FLinearColor Dark(0.01f, 0.1f, 0.1f);
		const FLinearColor LabelCol = (bSel && bActive) ? Dark : (R.bEnabled ? R.Color : TextDisabled);
		Rows[i].Label->SetColorAndOpacity(FSlateColor(LabelCol));
		Rows[i].Icon->SetColorAndOpacity(FSlateColor(LabelCol));
		Rows[i].Value->SetColorAndOpacity(FSlateColor((bSel && bActive) ? Dark : TextDim));
		Rows[i].BG->SetBrush(RoundBrush(bSel ? (bActive ? Accent : FLinearColor(0.1f, 0.3f, 0.3f, 0.8f)) : FLinearColor::Transparent, 14.f));
	}
}

void UZMenuWidget::Refresh()
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	const UZGameData& DB = UZGameData::Get();
	if (!GI) return;
	for (int32 i = 0; i < NumTabs; ++i)
	{
		const bool bSel = i == Tab;
		TabBG[i]->SetBrush(RoundBrush(bSel ? Accent : PanelBG, 18.f, PanelBorder, bSel ? 0.f : 1.f));
		TabText[i]->SetColorAndOpacity(FSlateColor(bSel ? FLinearColor(0.01f, 0.1f, 0.1f) : Text));
	}
	HeaderInfo->SetText(FText::FromString(FString::Printf(TEXT("◆ %d rubis   ·   Chapitre %d   ·   %d / %d objets"), GI->Rupees, GI->Chapter, GI->CountOwned(), DB.Items.Num())));

	// Équipe
	const bool bParty = Tab <= 3;
	for (int32 i = 0; i < PartyRows.Num(); ++i)
	{
		const bool bUsed = bParty && GI->Party.IsValidIndex(i);
		PartyRows[i].Btn->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bUsed) continue;
		const FZCharacterState& S = GI->Party[i];
		const FZCharacterDef* D = DB.FindCharacter(S.Id);
		PartyRows[i].Label->SetText(FText::FromString(D ? D->Name : S.Id.ToString()));
		PartyRows[i].Value->SetText(FText::FromString(FString::Printf(TEXT("Niveau %d%s"), S.Level, GI->ActiveParty.Contains(S.Id) ? TEXT(" · actif") : TEXT(""))));
		const bool bSel = i == MemberSel;
		PartyRows[i].BG->SetBrush(RoundBrush(bSel ? (Column == 0 ? Accent : FLinearColor(0.1f, 0.3f, 0.3f, 0.8f)) : FLinearColor::Transparent, 30.f));
		PartyRows[i].Label->SetColorAndOpacity(FSlateColor(bSel && Column == 0 ? FLinearColor(0.01f, 0.1f, 0.1f) : Text));
		if (UTexture2D* T = Portrait(S.Id)) PartyImages[i]->SetBrush(ImageBrush(T, FVector2D(50, 50), 25.f, Accent, 1.5f));
	}

	BuildColumns();
	Sel1 = FMath::Clamp(Sel1, 0, FMath::Max(0, Col1.Num() - 1));
	Sel2 = FMath::Clamp(Sel2, 0, FMath::Max(0, Col2.Num() - 1));
	RefreshRows(Col1, Rows1, Sel1, Scroll1, Column == 1);
	RefreshRows(Col2, Rows2, Sel2, Scroll2, Column == 2);
	Col2Panel->SetVisibility(Col2.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UpdateDetail();

	static const TCHAR* Footer = TEXT("↑↓ choisir · Entrée valider · Échap retour · Q/E ou PgPréc/PgSuiv : onglets · Tab : fermer");
	FooterText->SetText(FText::FromString(Tab == 4 ? FString(Footer) + TEXT(" · T : type (") + TypeNames[TypeFilter] + TEXT(")") : FString(Footer)));
}

void UZMenuWidget::ClearCompare()
{
	for (int32 i = 0; i < CmpLabel.Num(); ++i)
	{
		CmpLabel[i]->SetText(FText::GetEmpty());
		CmpBefore[i]->SetText(FText::GetEmpty());
		CmpAfter[i]->SetText(FText::GetEmpty());
	}
	ResistText->SetText(FText::GetEmpty());
}

void UZMenuWidget::SetCompare(const TArray<FString>& Labels, const TArray<FString>& Before, const TArray<FString>& After, const TArray<int32>& Delta)
{
	ClearCompare();
	for (int32 i = 0; i < FMath::Min(Labels.Num(), CmpLabel.Num()); ++i)
	{
		CmpLabel[i]->SetText(FText::FromString(Labels[i]));
		CmpBefore[i]->SetText(FText::FromString(Before.IsValidIndex(i) ? Before[i] : FString()));
		CmpAfter[i]->SetText(FText::FromString(After.IsValidIndex(i) ? After[i] : FString()));
		const int32 D = Delta.IsValidIndex(i) ? Delta[i] : 0;
		CmpAfter[i]->SetColorAndOpacity(FSlateColor(D > 0 ? Good : (D < 0 ? Bad : TextDim)));
	}
}

static void SetImage(UImage* Img, FString& Shown, const FString& File, const FVector2D& Size)
{
	if (Shown == File) return;
	Shown = File;
	UTexture2D* T = File.IsEmpty() ? nullptr : LoadUITexture(File);
	if (T) Img->SetBrush(ImageBrush(T, Size, 10.f, Accent, 1.5f));
	Img->SetVisibility(T ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UZMenuWidget::ShowStatus()
{
	FZCharacterState* M = Member();
	if (!M) return;
	const UZGameData& DB = UZGameData::Get();
	const FZCharacterDef* Def = DB.FindCharacter(M->Id);
	const FZDerivedStats D = ZProgression::ComputeDerived(*M);
	SetImage(DetailImage, ShownImage, M->Id == FName(TEXT("link")) ? TEXT("T_Link_FullBody.png") : FString::Printf(TEXT("T_Portrait_%s.png"), *M->Id.ToString()), FVector2D(150, 200));
	DetailTitle->SetText(FText::FromString(Def ? Def->Name : M->Id.ToString()));
	const int32 Next = M->Level < 99 ? ZProgression::TotalXPForLevel(M->Level + 1) - M->XP : 0;
	DetailSub->SetText(FText::FromString(FString::Printf(TEXT("%s · Niveau %d\nXP %s · prochain niveau dans %s\nPassifs : %d / %d points"),
		Def ? *Def->Role : TEXT(""), M->Level, *ZNames::FormatInt(M->XP), *ZNames::FormatInt(Next), ZProgression::PassiveCostUsed(*M), ZProgression::PassiveCapacity(M->Level))));
	const int32 HPv = M->CurrentHP < 0 ? D.MaxHP : M->CurrentHP;
	const int32 MPv = M->CurrentMP < 0 ? D.MaxMP : M->CurrentMP;
	TArray<FString> L = { TEXT("PV"), TEXT("PM"), TEXT("FOR / MAG"), TEXT("VIT / SPR"), TEXT("AGI / CHC"), TEXT("ATQ"), TEXT("AMAG"), TEXT("DEF"), TEXT("DFM"), TEXT("Critique"), TEXT("Esquive") };
	TArray<FString> B = {
		FString::Printf(TEXT("%d / %d"), HPv, D.MaxHP), FString::Printf(TEXT("%d / %d"), MPv, D.MaxMP),
		FString::Printf(TEXT("%d / %d"), (int32)D.STR, (int32)D.MAG), FString::Printf(TEXT("%d / %d"), (int32)D.VIT, (int32)D.SPR),
		FString::Printf(TEXT("%d / %d"), (int32)D.AGI, (int32)D.LCK), FString::FromInt((int32)D.ATQ), FString::FromInt((int32)D.AMAG),
		FString::FromInt((int32)D.DEF), FString::FromInt((int32)D.DFM),
		FString::Printf(TEXT("%d %%"), (int32)FMath::Min(30.f, 5.f + 0.25f * D.LCK + D.Crit)), FString::Printf(TEXT("%d pts"), (int32)D.Evade) };
	TArray<FString> A;
	A.Add(FString::Printf(TEXT("arme %d · focus %d"), (int32)D.WeaponPower, (int32)D.Focus));
	SetCompare(L, B, TArray<FString>(), TArray<int32>());
	CmpAfter[5]->SetText(FText::FromString(FString::Printf(TEXT("= 2×FOR + arme %d"), (int32)D.WeaponPower)));
	CmpAfter[7]->SetText(FText::FromString(FString::Printf(TEXT("= VIT + équipement %d"), (int32)D.EquipDef)));
	for (UTextBlock* T : CmpAfter) T->SetColorAndOpacity(FSlateColor(TextDim));
	TArray<FString> Res;
	for (int32 e = 1; e < ZElementCount; ++e)
	{
		Res.Add(FString::Printf(TEXT("%s %s %s"), *ZNames::ElementIcon((EZElement)e), *ZNames::ElementName((EZElement)e), *Pct(D.Resist[e])));
	}
	ResistText->SetText(FText::FromString(TEXT("Résistances : ") + FString::Join(Res, TEXT("   "))));
	FString Body;
	if (const FZSpiritDef* S = DB.FindSpirit(M->Spirit)) Body += FString::Printf(TEXT("Esprit lié : %s (%d R) — %s\n"), *S->Name, S->Resonance, *S->Desc);
	for (const FString& N : D.Notes) Body += TEXT("• ") + N + TEXT("\n");
	if (Def) Body += TEXT("\n") + Def->Lore;
	DetailBody->SetText(FText::FromString(Body));
}

void UZMenuWidget::ShowItemCard(const FString& ItemId, const FString& CurrentId, bool bCompare, EZEquipSlot Slot)
{
	const UZGameData& DB = UZGameData::Get();
	UZGameInstance* GI = UZGameInstance::Get(this);
	const FZItemDef* It = DB.FindItem(ItemId);
	ClearCompare();
	if (!It)
	{
		DetailTitle->SetText(FText::FromString(bCompare ? TEXT("Retirer") : TEXT("—")));
		DetailSub->SetText(FText::GetEmpty());
		DetailBody->SetText(FText::GetEmpty());
		if (!bCompare) return;
	}
	else
	{
		DetailTitle->SetText(FText::FromString(It->DisplayName()));
		const bool bOwned = GI && GI->Owns(It->Id);
		DetailSub->SetText(FText::FromString(FString::Printf(TEXT("%s%s\n%s · %s · Rang %d · profil %s · %s"),
			It->NameFR.IsEmpty() ? TEXT("") : *(It->Name + TEXT(" — ")), *It->GameName, *ZNames::ItemSlotName(It->Slot), *It->Category, It->Rank, *It->Profile,
			bOwned ? TEXT("possédé") : TEXT("verrouillé"))));
		FString Body;
		TArray<FString> Stats;
		if (It->Power) Stats.Add(FString::Printf(TEXT("Puissance %d"), It->Power));
		if (It->Focus) Stats.Add(FString::Printf(TEXT("Focus %d"), It->Focus));
		if (It->Def) Stats.Add(FString::Printf(TEXT("DEF +%d"), It->Def));
		if (It->MDef) Stats.Add(FString::Printf(TEXT("DFM +%d"), It->MDef));
		if (It->Hands == 2) Stats.Add(TEXT("deux mains"));
		if (It->Element != EZElement::Neutral) Stats.Add(ZNames::ElementIcon(It->Element) + TEXT(" ") + ZNames::ElementName(It->Element));
		if (Stats.Num()) Body += FString::Join(Stats, TEXT(" · ")) + TEXT("\n");
		Body += TEXT("Effet : ") + It->Effect + (It->EffectModeled ? TEXT("") : TEXT("  (descriptif, à modéliser)")) + TEXT("\n");
		if (!It->Special.IsEmpty()) Body += It->Special + TEXT("\n");
		if (!It->Teaches.IsNone())
		{
			FString Name = It->Teaches.ToString();
			int32 Need = 0;
			if (const FZAbilityDef* Ab = DB.FindAbility(It->Teaches)) { Name = Ab->Name; Need = Ab->AP; }
			else if (const FZPassiveDef* P = DB.FindPassive(It->Teaches)) { Name = P->Name; Need = P->AP; }
			FZCharacterState* M = Member();
			const int32 Have = M ? M->AbilityAP.FindRef(It->Teaches) : 0;
			Body += Need > 0 ? FString::Printf(TEXT("Enseigne « %s » : %d / %d PA%s\n"), *Name, FMath::Min(Have, Need), Need, Have >= Need ? TEXT(" — maîtrisée") : TEXT(""))
				: FString::Printf(TEXT("Donne la commande « %s » tant qu'il est porté\n"), *Name);
		}
		Body += TEXT("Déblocage : ") + (It->Quest.IsEmpty() ? It->UnlockText : It->Quest) + FString::Printf(TEXT(" (archive ouverte au chapitre %d)\n"), It->GateChapter);
		if (!It->CanonNote.IsEmpty()) Body += TEXT("Note canon : ") + It->CanonNote + TEXT("\n");
		Body += FString::Printf(TEXT("Référence : %s · identifiant %s"), *It->Source, *It->Id);
		DetailBody->SetText(FText::FromString(Body));
	}
	SetImage(DetailImage, ShownImage, FString(), FVector2D(150, 200));

	if (bCompare)
	{
		FZCharacterState* M = Member();
		if (!M) return;
		const FZDerivedStats Before = ZProgression::ComputeDerived(*M);
		FZCharacterState Copy = *M;
		FString Why;
		if (ItemId.IsEmpty()) ZProgression::Unequip(Copy, Slot);
		else ZProgression::Equip(Copy, Slot, ItemId, Why);
		const FZDerivedStats After = ZProgression::ComputeDerived(Copy);
		TArray<FString> L = { TEXT("PV max"), TEXT("PM max"), TEXT("ATQ"), TEXT("AMAG"), TEXT("DEF"), TEXT("DFM"), TEXT("AGI"), TEXT("FOR"), TEXT("SPR") };
		const float BV[] = { (float)Before.MaxHP, (float)Before.MaxMP, Before.ATQ, Before.AMAG, Before.DEF, Before.DFM, Before.AGI, Before.STR, Before.SPR };
		const float AV[] = { (float)After.MaxHP, (float)After.MaxMP, After.ATQ, After.AMAG, After.DEF, After.DFM, After.AGI, After.STR, After.SPR };
		TArray<FString> B, A;
		TArray<int32> Dl;
		for (int32 i = 0; i < L.Num(); ++i)
		{
			const int32 b = (int32)BV[i], a = (int32)AV[i];
			B.Add(FString::FromInt(b));
			A.Add(a == b ? FString::Printf(TEXT("→ %d"), a) : FString::Printf(TEXT("→ %d  (%+d)"), a, a - b));
			Dl.Add(a - b);
		}
		SetCompare(L, B, A, Dl);
		TArray<FString> Res;
		for (int32 e = 1; e < ZElementCount; ++e)
		{
			const float b = Before.Resist[e], a = After.Resist[e];
			if (FMath::IsNearlyZero(a) && FMath::IsNearlyZero(b)) continue;
			Res.Add(FString::Printf(TEXT("%s %s %s → %s"), *ZNames::ElementIcon((EZElement)e), *ZNames::ElementName((EZElement)e), *Pct(b), *Pct(a)));
		}
		ResistText->SetText(FText::FromString(Res.Num() ? TEXT("Résistances : ") + FString::Join(Res, TEXT("   ")) : FString(TEXT("Aucune résistance élémentaire en jeu"))));
	}
}

void UZMenuWidget::ShowAbilityCard(FName AbilityId)
{
	const UZGameData& DB = UZGameData::Get();
	ClearCompare();
	SetImage(DetailImage, ShownImage, FString(), FVector2D(150, 200));
	const FZAbilityDef* Ab = DB.FindAbility(AbilityId);
	if (!Ab)
	{
		DetailTitle->SetText(FText::FromString(TEXT("Emplacement libre")));
		DetailSub->SetText(FText::FromString(TEXT("Attaque, Garde, Objets, Relais, Forme et outils n'occupent pas ces six places.")));
		DetailBody->SetText(FText::GetEmpty());
		return;
	}
	FZCharacterState* M = Member();
	const int32 Have = M ? M->AbilityAP.FindRef(AbilityId) : 0;
	DetailTitle->SetText(FText::FromString(Ab->Name));
	DetailSub->SetText(FText::FromString(FString::Printf(TEXT("%s · %s%s"), *Ab->School, Ab->Tier.IsEmpty() ? TEXT("") : *Ab->Tier, Ab->Source.IsEmpty() ? TEXT("") : *(TEXT(" · ") + Ab->Source))));
	TArray<FString> L = { TEXT("Coût"), TEXT("Puissance"), TEXT("Élément"), TEXT("Cible"), TEXT("Brèche"), TEXT("Apprentissage") };
	static const TCHAR* Targets[] = { TEXT("Un ennemi"), TEXT("Tous les ennemis"), TEXT("Un allié"), TEXT("Tout le groupe"), TEXT("Soi"), TEXT("Allié KO") };
	TArray<FString> B = {
		FString::Printf(TEXT("%d PM%s"), Ab->MP, Ab->Resonance ? *FString::Printf(TEXT(" + %d R"), Ab->Resonance) : TEXT("")),
		Ab->Power > 0 ? FString::Printf(TEXT("P%.2f%s"), Ab->Power, Ab->Hits > 1 ? *FString::Printf(TEXT(" ×%d"), Ab->Hits) : TEXT("")) : TEXT("—"),
		ZNames::ElementName(Ab->Element), Targets[(int32)Ab->Target], Ab->Breach ? FString::Printf(TEXT("+%d"), Ab->Breach) : TEXT("—"),
		Ab->AP > 0 ? FString::Printf(TEXT("%d / %d PA"), FMath::Min(Have, Ab->AP), Ab->AP) : TEXT("kit") };
	SetCompare(L, B, TArray<FString>(), TArray<int32>());
	FString Body = Ab->Description;
	if (M && ZProgression::IsMastered(*M, AbilityId)) Body += TEXT("\n★ Maîtrisée : reste disponible sans l'objet source.");
	else if (Ab->AP > 0) Body += TEXT("\nPorter l'équipement source et gagner des combats remplit les PA (3 ordinaire, 8 élite, 20 boss).");
	DetailBody->SetText(FText::FromString(Body));
}

void UZMenuWidget::UpdateDetail()
{
	const UZGameData& DB = UZGameData::Get();
	UZGameInstance* GI = UZGameInstance::Get(this);
	FZCharacterState* M = Member();
	switch (Tab)
	{
	case 0:
		ShowStatus();
		break;
	case 1:
		if (Column == 2 && Col2.IsValidIndex(Sel2) && M) ShowItemCard(Col2[Sel2].Key, M->GetEquip((EZEquipSlot)Sel1), true, (EZEquipSlot)Sel1);
		else if (Column == 1 && M && Col1.IsValidIndex(Sel1)) ShowItemCard(M->GetEquip((EZEquipSlot)Sel1), FString(), false, (EZEquipSlot)Sel1);
		else ShowStatus();
		break;
	case 2:
		if (Column == 2 && Col2.IsValidIndex(Sel2)) ShowAbilityCard(FName(*Col2[Sel2].Key));
		else if (Column == 1 && Col1.IsValidIndex(Sel1))
		{
			if (Col1[Sel1].Key == TEXT("slot")) ShowAbilityCard(M && M->Prepared.IsValidIndex(Sel1) ? M->Prepared[Sel1] : NAME_None);
			else if (const FZPassiveDef* P = DB.FindPassive(FName(*Col1[Sel1].Key.Mid(8))))
			{
				ClearCompare();
				DetailTitle->SetText(FText::FromString(P->Name));
				DetailSub->SetText(FText::FromString(FString::Printf(TEXT("Passif · coût %d points · %d PA"), P->Cost, P->AP)));
				DetailBody->SetText(FText::FromString(P->Desc + TEXT("\nLes bonus d'une pièce portée et son passif maîtrisé identique ne se cumulent pas.")));
			}
		}
		else ShowStatus();
		break;
	case 3:
		if (Column == 1 && Col1.IsValidIndex(Sel1))
		{
			ClearCompare();
			const FString K = Col1[Sel1].Key;
			if (K.StartsWith(TEXT("form:")))
			{
				const FZFormDef* F = DB.FindForm(FName(*K.Mid(5)));
				SetImage(DetailImage, ShownImage, FString::Printf(TEXT("T_Form_%s.png"), *K.Mid(5)), FVector2D(150, 200));
				DetailTitle->SetText(FText::FromString(F->Name));
				TArray<FString> Cmds;
				for (FName C : F->Commands) { if (const FZAbilityDef* Ab = DB.FindAbility(C)) Cmds.Add(FString::Printf(TEXT("%s (%s)"), *Ab->Name, *Ab->Description)); }
				DetailSub->SetText(FText::FromString(FString::Printf(TEXT("Coût %d Résonance · 3 activations · ATQ ×%.2f · AMAG ×%.2f · DEF ×%.2f · AGI ×%.2f"), F->Resonance, F->ATQ, F->AMAG, F->DEF, F->AGI)));
				const FZItemDef* Mask = DB.FindItem(F->MaskItem);
				DetailBody->SetText(FText::FromString(FString::Printf(TEXT("Commandes : %s\n\n%s\n\n%s"), *FString::Join(Cmds, TEXT(" · ")), *F->Weakness,
					Mask ? *FString::Printf(TEXT("Débloquée par : %s — %s"), *Mask->DisplayName(), Mask->Quest.IsEmpty() ? *Mask->UnlockText : *Mask->Quest) : TEXT("Débloquée par un lien narratif (voir le Journal)."))));
			}
			else
			{
				const FZSpiritDef* S = DB.FindSpirit(FName(*K.Mid(7)));
				SetImage(DetailImage, ShownImage, FString(), FVector2D(150, 200));
				DetailTitle->SetText(FText::FromString(S->Name));
				DetailSub->SetText(FText::FromString(FString::Printf(TEXT("Invocation · %d Résonance · une fois par combat"), S->Resonance)));
				DetailBody->SetText(FText::FromString(FString::Printf(TEXT("%s\n\nAcquisition : %s\nPassif du lien : %s\n\nEntrée : lier cet esprit au personnage sélectionné (un seul lien par personnage)."), *S->Desc, *S->Acquire, S->Passive.IsEmpty() ? TEXT("—") : *S->Passive)));
			}
		}
		else ShowStatus();
		break;
	case 4:
		if (Column == 2 && Col2.IsValidIndex(Sel2)) ShowItemCard(Col2[Sel2].Key, FString(), false, EZEquipSlot::Weapon);
		else
		{
			ClearCompare();
			SetImage(DetailImage, ShownImage, FString(), FVector2D(150, 200));
			DetailTitle->SetText(FText::FromString(TEXT("Archives d'Hyrule")));
			DetailSub->SetText(FText::FromString(TEXT("21 jeux principaux · 1 257 références documentées + prêts de démonstration")));
			DetailBody->SetText(FText::FromString(TEXT("Chaque objet a un identifiant stable, un jeu d'origine, un profil, un rang et une voie de déblocage garantie (histoire, boutique, forge, contrat, quête, anneaux de Vasu, atelier TFH, sanctuaires EOW). Aucun équipement permanent n'est manquable.\n\nT : filtrer par type. Mode Archiviste (Système) : tout posséder pour tester.")));
		}
		break;
	case 5:
		if (Col1.IsValidIndex(Sel1))
		{
			ClearCompare();
			for (const FZQuestDef& Q : DB.Quests)
			{
				if (Q.Id.ToString() != Col1[Sel1].Key) continue;
				DetailTitle->SetText(FText::FromString(Q.Name));
				DetailSub->SetText(FText::FromString(FString::Printf(TEXT("%s · chapitre %d"), *Q.Id.ToString(), Q.Chapter)));
				FString Body;
				for (const FString& S : Q.Steps) Body += TEXT("◇ ") + S + TEXT("\n");
				Body += TEXT("\nRécompense : ") + Q.Reward;
				DetailBody->SetText(FText::FromString(Body));
			}
		}
		break;
	case 6:
		ClearCompare();
		DetailTitle->SetText(FText::FromString(Col1.IsValidIndex(Sel1) ? Col1[Sel1].Label : FString()));
		DetailSub->SetText(FText::GetEmpty());
		DetailBody->SetText(FText::FromString(TEXT("Options d'accessibilité du chapitre 21 : texte 100/125/150 %, vitesse des animations 1×/2×, difficulté Histoire / Standard / Héroïque. Aucun timing manuel n'est obligatoire.\n\nLe Mode Archiviste donne tous les objets du catalogue et toutes les formes : pratique pour tester l'équipement et les transformations.")));
		break;
	}
}

void UZMenuWidget::Toast(const FString& S)
{
	ToastMsg = S;
	ToastT = 0.f;
	ToastText->SetText(FText::FromString(S));
}

void UZMenuWidget::Confirm()
{
	UZGameInstance* GI = UZGameInstance::Get(this);
	const UZGameData& DB = UZGameData::Get();
	FZCharacterState* M = Member();
	if (!GI) return;
	if (Column == 0)
	{
		if (Tab >= 1 && Tab <= 3) { Column = 1; Sel1 = 0; Scroll1 = 0; }
		Refresh();
		return;
	}
	if (Column == 1)
	{
		if (!Col1.IsValidIndex(Sel1)) return;
		const FZMenuRow& R = Col1[Sel1];
		switch (Tab)
		{
		case 1: Column = 2; Sel2 = 0; Scroll2 = 0; break;
		case 2:
			if (R.Key == TEXT("slot")) { Column = 2; Sel2 = 0; Scroll2 = 0; }
			else if (R.Key.StartsWith(TEXT("passive:")) && M)
			{
				const FName P(*R.Key.Mid(8));
				if (!R.bEnabled) { Toast(TEXT("Passif non maîtrisé : gagne des PA avec l'équipement qui l'enseigne")); break; }
				if (M->Passives.Contains(P)) M->Passives.Remove(P);
				else
				{
					const FZPassiveDef* PD = DB.FindPassive(P);
					if (ZProgression::PassiveCostUsed(*M) + (PD ? PD->Cost : 0) > ZProgression::PassiveCapacity(M->Level)) Toast(TEXT("Capacité de passifs insuffisante"));
					else M->Passives.Add(P);
				}
			}
			break;
		case 3:
			if (R.Key.StartsWith(TEXT("spirit:")) && M)
			{
				const FName S(*R.Key.Mid(7));
				for (FZCharacterState& C : GI->Party) { if (C.Spirit == S) C.Spirit = NAME_None; }
				M->Spirit = S;
				Toast(FString::Printf(TEXT("Esprit lié à %s"), *DB.FindCharacter(M->Id)->Name));
			}
			break;
		case 4: Column = 2; Sel2 = 0; Scroll2 = 0; break;
		case 6:
			if (R.Key == TEXT("save")) Toast(GI->SaveToSlot() ? TEXT("Partie sauvegardée") : TEXT("Échec de la sauvegarde"));
			else if (R.Key == TEXT("load")) { Toast(GI->LoadFromSlot() ? TEXT("Partie chargée") : TEXT("Chargement impossible")); if (OnEquipmentChanged) OnEquipmentChanged(); }
			else if (R.Key == TEXT("atb")) GI->BattleSpeed = GI->BattleSpeed >= 2.f ? 1.f : GI->BattleSpeed + 0.5f;
			else if (R.Key == TEXT("anim")) GI->AnimSpeed = GI->AnimSpeed >= 2.f ? 1.f : 2.f;
			else if (R.Key == TEXT("diff")) GI->DifficultyDamage = GI->DifficultyDamage < 0.8f ? 1.f : (GI->DifficultyDamage < 1.1f ? 1.2f : 0.7f);
			else if (R.Key == TEXT("text")) { GI->TextScale = GI->TextScale >= 150 ? 100 : GI->TextScale + 25; Toast(TEXT("Taille du texte appliquée à la réouverture des écrans")); }
			else if (R.Key == TEXT("archivist")) { GI->UnlockEverything(); Toast(TEXT("Mode Archiviste : 1 258 objets et toutes les formes débloqués")); if (OnEquipmentChanged) OnEquipmentChanged(); }
			else if (R.Key == TEXT("title")) { if (OnReturnToTitle) OnReturnToTitle(); return; }
			else if (R.Key == TEXT("close")) { if (OnClose) OnClose(); return; }
			break;
		default: break;
		}
		Refresh();
		return;
	}
	// Colonne 2
	if (!Col2.IsValidIndex(Sel2) || !M) return;
	const FZMenuRow& R = Col2[Sel2];
	if (Tab == 1)
	{
		const EZEquipSlot Slot = (EZEquipSlot)Sel1;
		if (R.Key.IsEmpty())
		{
			ZProgression::Unequip(*M, Slot);
		}
		else
		{
			if (!R.bEnabled)
			{
				FString Why;
				const FZItemDef* It = DB.FindItem(R.Key);
				if (It && !ZProgression::CanEquip(*M, Slot, *It, Why)) Toast(Why);
				else Toast(TEXT("Déjà porté par un autre personnage"));
				return;
			}
			FString Why;
			if (!ZProgression::Equip(*M, Slot, R.Key, Why)) { Toast(Why); return; }
			Toast(FString::Printf(TEXT("%s équipé"), *DB.FindItem(R.Key)->DisplayName()));
		}
		ZProgression::AutoPrepare(*M);
		if (OnEquipmentChanged) OnEquipmentChanged();
		Column = 1;
	}
	else if (Tab == 2)
	{
		if (M->Prepared.Num() < 6) M->Prepared.SetNum(6);
		M->Prepared[Sel1] = R.Key.IsEmpty() ? NAME_None : FName(*R.Key);
		M->Prepared.RemoveAll([](FName N) { return N.IsNone(); });
		Column = 1;
	}
	else if (Tab == 4)
	{
		// Rien à faire : la fiche s'affiche au survol
	}
	Refresh();
}

void UZMenuWidget::Back()
{
	if (Column == 2) { Column = 1; Refresh(); return; }
	if (Column == 1 && Tab <= 3) { Column = 0; Refresh(); return; }
	if (OnClose) OnClose();
}

void UZMenuWidget::Move(int32 D)
{
	int32& S = Column == 0 ? MemberSel : (Column == 1 ? Sel1 : Sel2);
	const int32 N = Column == 0 ? (UZGameInstance::Get(this) ? UZGameInstance::Get(this)->Party.Num() : 1) : (Column == 1 ? Col1.Num() : Col2.Num());
	if (N <= 0) return;
	S = (S + D + N) % N;
	Refresh();
}

void UZMenuWidget::SwitchTab(int32 D)
{
	Tab = (Tab + D + NumTabs) % NumTabs;
	Column = Tab >= 4 ? 1 : 0;
	Sel1 = Sel2 = Scroll1 = Scroll2 = 0;
	Refresh();
}

void UZMenuWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	ToastT += Dt;
	if (ToastText) ToastText->SetRenderOpacity(ToastT < 2.5f ? 1.f : FMath::Max(0.f, 1.f - (ToastT - 2.5f) / 0.5f));
	if (!HasKeyboardFocus() && IsVisible()) SetKeyboardFocus();
}

FReply UZMenuWidget::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& KeyEvent)
{
	const FKey K = KeyEvent.GetKey();
	if (K == EKeys::Up || K == EKeys::W || K == EKeys::Z || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_LeftStick_Up) { Move(-1); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Down) { Move(1); return FReply::Handled(); }
	if (K == EKeys::PageUp || K == EKeys::Q || K == EKeys::Gamepad_LeftShoulder) { SwitchTab(-1); return FReply::Handled(); }
	if (K == EKeys::PageDown || K == EKeys::E || K == EKeys::Gamepad_RightShoulder) { SwitchTab(1); return FReply::Handled(); }
	if (K == EKeys::Right || K == EKeys::D || K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::Gamepad_FaceButton_Bottom || K == EKeys::Gamepad_DPad_Right) { Confirm(); return FReply::Handled(); }
	if (K == EKeys::Left || K == EKeys::A || K == EKeys::Escape || K == EKeys::BackSpace || K == EKeys::Gamepad_FaceButton_Right || K == EKeys::Gamepad_DPad_Left) { Back(); return FReply::Handled(); }
	if (K == EKeys::Tab || K == EKeys::Gamepad_Special_Right) { if (OnClose) OnClose(); return FReply::Handled(); }
	if (K == EKeys::T && Tab == 4) { TypeFilter = (TypeFilter + 1) % NumTypes; Sel2 = Scroll2 = 0; Refresh(); return FReply::Handled(); }
	if (K == EKeys::PageUp) { Move(-10); return FReply::Handled(); }
	for (int32 i = 0; i < NumTabs; ++i)
	{
		static const FKey Nums[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven };
		if (K == Nums[i]) { SwitchTab(i - Tab); return FReply::Handled(); }
	}
	return FReply::Handled();
}

FReply UZMenuWidget::NativeOnMouseWheel(const FGeometry& Geo, const FPointerEvent& Mouse)
{
	Move(Mouse.GetWheelDelta() > 0 ? -1 : 1);
	return FReply::Handled();
}
