#include "ZBattleHUD.h"
#include "ZUI.h"
#include "ZBattleDirector.h"
#include "ZBattlePawn.h"
#include "ZGameData.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

using namespace ZUI;

namespace
{
	USizeBox* Sized(UWidgetTree* T, UWidget* Content, float W, float H)
	{
		USizeBox* S = T->ConstructWidget<USizeBox>();
		if (W > 0) S->SetWidthOverride(W);
		if (H > 0) S->SetHeightOverride(H);
		S->SetContent(Content);
		return S;
	}

	UVerticalBoxSlot* VAdd(UVerticalBox* Box, UWidget* W, const FMargin& Pad = FMargin(0), EHorizontalAlignment H = HAlign_Fill)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
		S->SetPadding(Pad);
		S->SetHorizontalAlignment(H);
		return S;
	}

	UHorizontalBoxSlot* HAdd(UHorizontalBox* Box, UWidget* W, const FMargin& Pad = FMargin(0), EVerticalAlignment V = VAlign_Center, bool bFill = false)
	{
		UHorizontalBoxSlot* S = Box->AddChildToHorizontalBox(W);
		S->SetPadding(Pad);
		S->SetVerticalAlignment(V);
		if (bFill) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return S;
	}

	bool IsMagicAbility(const FZAbilityDef& A)
	{
		return A.Kind == EZAbilityKind::Magical || A.Kind == EZAbilityKind::Heal || A.Kind == EZAbilityKind::Revive
			|| A.School == TEXT("Chant") || A.School == TEXT("Magic") || A.School == TEXT("Mask");
	}

	FString StatusLine(const FZBattler& B)
	{
		TArray<FString> Parts;
		if (!B.Form.IsNone())
		{
			if (const FZFormDef* F = UZGameData::Get().FindForm(B.Form)) Parts.Add(FString::Printf(TEXT("%s (%d)"), *F->Name, B.FormLeft));
		}
		for (const FZStatusInst& S : B.Statuses)
		{
			if (S.Status == EZStatus::Stability || S.Status == EZStatus::BreachStability) continue;
			Parts.Add(ZNames::StatusName(S.Status));
		}
		return FString::Join(Parts, TEXT(" · "));
	}
}

TSharedRef<SWidget> UZBattleHUD::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { Build(); }
	return Super::RebuildWidget();
}

void UZBattleHUD::Build()
{
	SetIsFocusable(true);
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// --- Mode Attente (haut gauche)
	UBorder* Mode = MakePanel(WidgetTree, PanelBG, 6.f, FMargin(18, 8), PanelBorder, 1.2f);
	Mode->SetContent(MakeText(WidgetTree, Upper(TEXT("Mode Attente")), 20, Text, true, 180));
	Place(Root, Mode, FAnchors(0, 0), FVector2D(28, 24), FVector2D(0, 0));

	// --- Ennemi (haut centre)
	UVerticalBox* EnemyBox = WidgetTree->ConstructWidget<UVerticalBox>();
	EnemyName = MakeText(WidgetTree, TEXT(""), 40, Text, true, 80);
	VAdd(EnemyBox, EnemyName, FMargin(0, 0, 0, 2), HAlign_Center);
	EnemyHP = MakeBar(WidgetTree, BossHP);
	VAdd(EnemyBox, Sized(WidgetTree, EnemyHP, 560, 16), FMargin(0, 2), HAlign_Center);
	EnemyHPText = MakeText(WidgetTree, TEXT(""), 22, Text, true, 40);
	VAdd(EnemyBox, EnemyHPText, FMargin(0, 2), HAlign_Center);
	EnemyBreach = MakeBar(WidgetTree, ZUI::Breach);
	VAdd(EnemyBox, Sized(WidgetTree, EnemyBreach, 420, 6), FMargin(0, 2), HAlign_Center);
	EnemyBreachText = MakeText(WidgetTree, TEXT(""), 14, TextDim, false, 40);
	VAdd(EnemyBox, EnemyBreachText, FMargin(0, 0), HAlign_Center);
	EnemyInfo = MakeText(WidgetTree, TEXT(""), 15, TextDim);
	VAdd(EnemyBox, EnemyInfo, FMargin(0, 2), HAlign_Center);
	PreparePlate = MakePanel(WidgetTree, PanelBG, 6.f, FMargin(26, 6), PanelBorder, 1.f);
	PrepareText = MakeText(WidgetTree, TEXT(""), 22, Text, false, 20);
	PreparePlate->SetContent(PrepareText);
	VAdd(EnemyBox, PreparePlate, FMargin(0, 6, 0, 0), HAlign_Center);
	ActionPlate = MakePanel(WidgetTree, PanelBGLight, 8.f, FMargin(30, 8), Accent, 1.5f);
	ActionText = MakeText(WidgetTree, TEXT(""), 26, Text, true, 40);
	ActionPlate->SetContent(ActionText);
	VAdd(EnemyBox, ActionPlate, FMargin(0, 14, 0, 0), HAlign_Center);
	Place(Root, EnemyBox, FAnchors(0.5f, 0.f), FVector2D(0, 18), FVector2D(0.5f, 0.f));

	// --- Résonance (haut droite)
	UOverlay* RingBox = WidgetTree->ConstructWidget<UOverlay>();
	UBorder* RingBG = MakePanel(WidgetTree, PanelBG, 80.f, FMargin(0));
	RingBox->AddChildToOverlay(Sized(WidgetTree, RingBG, 156, 156));
	Ring = CreateWidget<UZRingWidget>(this, UZRingWidget::StaticClass());
	UOverlaySlot* RS = RingBox->AddChildToOverlay(Sized(WidgetTree, Ring, 156, 156));
	RS->SetHorizontalAlignment(HAlign_Center);
	UVerticalBox* RingTxt = WidgetTree->ConstructWidget<UVerticalBox>();
	VAdd(RingTxt, MakeText(WidgetTree, Upper(TEXT("Résonance")), 15, Text, true, 60), FMargin(0), HAlign_Center);
	RingValue = MakeText(WidgetTree, TEXT("0 / 100"), 26, Text, true);
	VAdd(RingTxt, RingValue, FMargin(0), HAlign_Center);
	UOverlaySlot* TS = RingBox->AddChildToOverlay(RingTxt);
	TS->SetHorizontalAlignment(HAlign_Center);
	TS->SetVerticalAlignment(VAlign_Center);
	Place(Root, RingBox, FAnchors(1.f, 0.f), FVector2D(-30, 18), FVector2D(1.f, 0.f));
	NextText = MakeText(WidgetTree, TEXT(""), 15, TextDim);
	Place(Root, NextText, FAnchors(1.f, 0.f), FVector2D(-32, 184), FVector2D(1.f, 0.f));

	// --- Commandes (bas gauche)
	CommandPanel = MakePanel(WidgetTree, PanelBG, 12.f, FMargin(10, 10, 10, 14), PanelBorder, 1.5f);
	UVerticalBox* CmdBox = WidgetTree->ConstructWidget<UVerticalBox>();
	CommandTitle = MakeText(WidgetTree, TEXT("Link"), 28, Text, true, 40);
	VAdd(CmdBox, CommandTitle, FMargin(14, 2, 14, 8));
	CommandList = WidgetTree->ConstructWidget<UVerticalBox>();
	VAdd(CmdBox, CommandList);
	for (int32 i = 0; i < 12; ++i)
	{
		UZEntryButton* Btn = WidgetTree->ConstructWidget<UZEntryButton>(UZEntryButton::StaticClass());
		Btn->Index = i;
		Btn->OnHover = [this](int32 I) { if (I < Entries.Num() && I != Sel) { Sel = I; RefreshList(); } };
		Btn->OnPick = [this](int32 I) { if (I < Entries.Num()) { Sel = I; RefreshList(); Confirm(); } };
		Btn->Bind();
		UBorder* BG = MakePanel(WidgetTree, FLinearColor::Transparent, 18.f, FMargin(14, 5, 16, 5));
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* Icon = MakeText(WidgetTree, TEXT(""), 20, Text);
		HAdd(H, Sized(WidgetTree, Icon, 30, 0), FMargin(0, 0, 6, 0));
		UTextBlock* Label = MakeText(WidgetTree, TEXT(""), 24, Text);
		Label->SetClipping(EWidgetClipping::ClipToBounds);
		HAdd(H, Label, FMargin(0), VAlign_Center, true);
		UTextBlock* Cost = MakeText(WidgetTree, TEXT(""), 17, TextDim);
		Cost->SetJustification(ETextJustify::Right);
		HAdd(H, Sized(WidgetTree, Cost, 74, 0), FMargin(8, 0, 0, 0));
		BG->SetContent(H);
		Btn->SetFillContent(BG);
		VAdd(CommandList, Btn, FMargin(0, 1));
		RowButton.Add(Btn); RowBG.Add(BG); RowIcon.Add(Icon); RowLabel.Add(Label); RowCost.Add(Cost);
	}
	CommandPanel->SetContent(CmdBox);
	UCanvasPanelSlot* CS = Place(Root, Sized(WidgetTree, CommandPanel, 480, 0), FAnchors(0.f, 1.f), FVector2D(30, -30), FVector2D(0.f, 1.f));
	CS->SetAutoSize(true);

	// --- Aide et journal (bas centre)
	HelpPlate = MakePanel(WidgetTree, PanelBG, 8.f, FMargin(20, 8), PanelBorder, 1.f);
	HelpText = MakeText(WidgetTree, TEXT(""), 18, Text);
	HelpText->SetAutoWrapText(true);
	HelpPlate->SetContent(Sized(WidgetTree, HelpText, 520, 0));
	Place(Root, HelpPlate, FAnchors(0.5f, 1.f), FVector2D(-90, -30), FVector2D(0.5f, 1.f));
	LogText = MakeText(WidgetTree, TEXT(""), 15, TextDim);
	Place(Root, LogText, FAnchors(0.f, 0.f), FVector2D(30, 74), FVector2D(0.f, 0.f));

	// --- Groupe (bas droite)
	UVerticalBox* PartyBox = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 i = 0; i < 3; ++i)
	{
		UBorder* Row = MakePanel(WidgetTree, PanelBG, 14.f, FMargin(10, 8, 18, 8), PanelBorder, 1.2f);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		UImage* Portrait = MakeImage(WidgetTree, nullptr, FVector2D(64, 64), 32.f, Accent, 2.f);
		HAdd(H, Sized(WidgetTree, Portrait, 64, 64), FMargin(0, 0, 12, 0));
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* NameLine = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* Name = MakeText(WidgetTree, TEXT(""), 22, Text, true, 30);
		HAdd(NameLine, Name, FMargin(0, 0, 10, 0));
		UTextBlock* Status = MakeText(WidgetTree, TEXT(""), 13, Gold);
		HAdd(NameLine, Status);
		VAdd(Info, NameLine);
		auto StatLine = [&](const TCHAR* Label, const FLinearColor& Col, UTextBlock*& OutText, UProgressBar*& OutBar)
		{
			UHorizontalBox* L = WidgetTree->ConstructWidget<UHorizontalBox>();
			HAdd(L, Sized(WidgetTree, MakeText(WidgetTree, Label, 14, Col, true), 36, 0));
			OutText = MakeText(WidgetTree, TEXT(""), 17, Text, true);
			HAdd(L, Sized(WidgetTree, OutText, 118, 0));
			OutBar = MakeBar(WidgetTree, Col);
			HAdd(L, Sized(WidgetTree, OutBar, 142, 8), FMargin(0), VAlign_Center);
			VAdd(Info, L, FMargin(0, 1));
		};
		UTextBlock* HPT; UProgressBar* HPB; UTextBlock* MPT; UProgressBar* MPB;
		StatLine(TEXT("PV"), HP, HPT, HPB);
		StatLine(TEXT("PM"), MP, MPT, MPB);
		HAdd(H, Info);
		UVerticalBox* AtbBox = WidgetTree->ConstructWidget<UVerticalBox>();
		VAdd(AtbBox, MakeText(WidgetTree, TEXT("ATB"), 14, ATB, true, 40));
		UProgressBar* Atb = MakeBar(WidgetTree, ATB);
		VAdd(AtbBox, Sized(WidgetTree, Atb, 120, 10), FMargin(0, 4));
		HAdd(H, AtbBox, FMargin(18, 0, 0, 0));
		Row->SetContent(H);
		VAdd(PartyBox, Row, FMargin(0, 4));
		PartyRow.Add(Row); PartyPortrait.Add(Portrait); PartyName.Add(Name); PartyStatus.Add(Status);
		PartyHPText.Add(HPT); PartyHP.Add(HPB); PartyMPText.Add(MPT); PartyMP.Add(MPB); PartyATB.Add(Atb);
	}
	Place(Root, PartyBox, FAnchors(1.f, 1.f), FVector2D(-24, -24), FVector2D(1.f, 1.f));

	// --- Grande bannière (centre)
	BigBannerPlate = MakePanel(WidgetTree, FLinearColor(0.01f, 0.03f, 0.04f, 0.7f), 14.f, FMargin(30, 14), PanelBorder, 1.5f);
	UHorizontalBox* BB = WidgetTree->ConstructWidget<UHorizontalBox>();
	BigBannerImage = MakeImage(WidgetTree, nullptr, FVector2D(170, 205), 10.f);
	HAdd(BB, Sized(WidgetTree, BigBannerImage, 170, 205), FMargin(0, 0, 24, 0));
	BigBannerText = MakeText(WidgetTree, TEXT(""), 46, Text, true, 60);
	HAdd(BB, BigBannerText);
	BigBannerPlate->SetContent(BB);
	Place(Root, BigBannerPlate, FAnchors(0.5f, 0.42f), FVector2D(0, 0), FVector2D(0.5f, 0.5f));
	BigBannerPlate->SetVisibility(ESlateVisibility::Collapsed);

	// --- Nombres flottants
	for (int32 i = 0; i < 28; ++i)
	{
		UTextBlock* T = MakeText(WidgetTree, TEXT(""), 30, Text, true);
		T->SetShadowOffset(FVector2D(2.f, 2.f));
		T->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.85f));
		UCanvasPanelSlot* PS = Place(Root, T, FAnchors(0, 0), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
		PS->SetZOrder(20);
		T->SetVisibility(ESlateVisibility::Collapsed);
		PopupPool.Add(T);
	}

	// --- Résultat
	ResultPanel = MakePanel(WidgetTree, FLinearColor(0.01f, 0.04f, 0.05f, 0.9f), 16.f, FMargin(40, 26), Accent, 2.f);
	UVerticalBox* RB = WidgetTree->ConstructWidget<UVerticalBox>();
	ResultTitle = MakeText(WidgetTree, TEXT(""), 48, Gold, true, 80);
	VAdd(RB, ResultTitle, FMargin(0, 0, 0, 12), HAlign_Center);
	ResultBody = MakeText(WidgetTree, TEXT(""), 21, Text);
	ResultBody->SetAutoWrapText(true);
	VAdd(RB, Sized(WidgetTree, ResultBody, 760, 0), FMargin(0, 0, 0, 16));
	ResultPrompt = MakeText(WidgetTree, TEXT(""), 20, Accent, true);
	VAdd(RB, ResultPrompt, FMargin(0), HAlign_Center);
	ResultPanel->SetContent(RB);
	UCanvasPanelSlot* ResS = Place(Root, ResultPanel, FAnchors(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));
	ResS->SetZOrder(30);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UZBattleHUD::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	if (!Director) return;
	UpdateEnemyPanel();
	UpdateParty();
	UpdateCommandPanel();
	UpdatePopups();
	UpdateBanners(Dt);
	UpdateResult();
	if (HelpOverrideT > 0.f) HelpOverrideT -= Dt;
	if (!HasKeyboardFocus() && IsVisible()) SetKeyboardFocus();
}

void UZBattleHUD::UpdateEnemyPanel()
{
	const FZCombatSim& Sim = Director->GetSim();
	const int32 E = Director->GetFocusEnemy();
	if (!Sim.Actors.IsValidIndex(E)) return;
	const FZBattler& B = Sim.Actors[E];
	EnemyName->SetText(FText::FromString(Upper(B.Name)));
	const int32 HPv = Director->GetDisplayHP(E);
	EnemyHP->SetPercent(B.MaxHP > 0 ? (float)HPv / B.MaxHP : 0.f);
	EnemyHPText->SetText(FText::FromString(FString::Printf(TEXT("PV  %s / %s"), *ZNames::FormatInt(HPv), *ZNames::FormatInt(B.MaxHP))));
	EnemyBreach->SetPercent(B.BreachMax > 0 ? B.Breach / B.BreachMax : 0.f);
	EnemyBreachText->SetText(FText::FromString(B.Has(EZStatus::BreachStability)
		? TEXT("Brèche : stabilisée")
		: FString::Printf(TEXT("Brèche %d / %d"), (int32)B.Breach, (int32)B.BreachMax)));

	TArray<FString> Weak, Res;
	for (int32 e = 1; e < ZElementCount; ++e)
	{
		const EZElement El = (EZElement)e;
		if (!B.bAnalyzed && !B.KnownElements.Contains(El)) continue;
		const float R = B.Resist[e];
		const FString Tag = ZNames::ElementIcon(El) + TEXT(" ") + ZNames::ElementName(El);
		if (R < -0.01f) Weak.Add(Tag);
		else if (R > 0.01f) Res.Add(FString::Printf(TEXT("%s %d %%"), *Tag, FMath::RoundToInt(R * 100.f)));
	}
	FString Info;
	if (Weak.Num()) Info += TEXT("Faiblesses : ") + FString::Join(Weak, TEXT("  "));
	if (Res.Num()) Info += (Info.IsEmpty() ? TEXT("") : TEXT("    ")) + FString(TEXT("Résiste : ")) + FString::Join(Res, TEXT("  "));
	if (Info.IsEmpty()) Info = B.bAnalyzed ? TEXT("Aucune affinité notable") : TEXT("Résistances inconnues — Analyse ou premier impact élémentaire");
	const FString St = StatusLine(B);
	if (!St.IsEmpty()) Info += TEXT("    [") + St + TEXT("]");
	EnemyInfo->SetText(FText::FromString(Info));

	FString Prep;
	for (const FZBattler& X : Sim.Actors)
	{
		if (X.bAlly || X.bKO || X.Prepared.IsNone()) continue;
		const FZAbilityDef* P = UZGameData::Get().FindAbility(X.Prepared);
		Prep = FString::Printf(TEXT("◆   Prépare : %s   ◆"), P ? *P->Name : *X.Prepared.ToString());
		if (X.Prepared == FName(TEXT("E_NER_DELUGE"))) Prep += FString::Printf(TEXT("   Vannes %d / 2"), X.Valves);
		if (X.Prepared == FName(TEXT("E_NER_CRUE"))) Prep += TEXT("   (grappin sur un conduit !)");
		break;
	}
	PreparePlate->SetVisibility(Prep.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	PrepareText->SetText(FText::FromString(Prep));

	RingValue->SetText(FText::FromString(FString::Printf(TEXT("%d / 100"), Director->GetDisplayResonance())));
	Ring->Percent = Director->GetDisplayResonance() / 100.f;

	TArray<FString> Names;
	for (int32 A : Sim.PredictNextActors(3)) { Names.Add(Sim.Actors[A].Name); }
	NextText->SetText(FText::FromString(TEXT("Prochains : ") + FString::Join(Names, TEXT("  ›  "))));
}

void UZBattleHUD::UpdateParty()
{
	const FZCombatSim& Sim = Director->GetSim();
	PartyIds.Reset();
	for (const FZBattler& B : Sim.Actors) { if (B.bAlly) PartyIds.Add(B.Id); }
	for (int32 r = 0; r < PartyRow.Num(); ++r)
	{
		if (!PartyIds.IsValidIndex(r)) { PartyRow[r]->SetVisibility(ESlateVisibility::Collapsed); continue; }
		PartyRow[r]->SetVisibility(ESlateVisibility::HitTestInvisible);
		const FZBattler& B = Sim.Actors[PartyIds[r]];
		const int32 H = Director->GetDisplayHP(B.Id);
		const int32 M = Director->GetDisplayMP(B.Id);
		PartyName[r]->SetText(FText::FromString(B.Name));
		PartyHPText[r]->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), H, B.MaxHP)));
		PartyHP[r]->SetPercent(B.MaxHP > 0 ? (float)H / B.MaxHP : 0.f);
		PartyHP[r]->SetFillColorAndOpacity(H < B.MaxHP * 0.25f ? Bad : ZUI::HP);
		PartyMPText[r]->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), M, B.MaxMP)));
		PartyMP[r]->SetPercent(B.MaxMP > 0 ? (float)M / B.MaxMP : 0.f);
		PartyATB[r]->SetPercent(B.bKO ? 0.f : B.J / 1000.f);
		PartyATB[r]->SetFillColorAndOpacity(B.J >= 1000.f ? FLinearColor(1.f, 0.95f, 0.55f) : ATB);
		PartyStatus[r]->SetText(FText::FromString(B.bKO ? FString(TEXT("KO")) : StatusLine(B)));
		const bool bCurrent = Sim.GetCurrentActor() == B.Id && Director->IsAwaitingInput();
		PartyRow[r]->SetBrush(RoundBrush(bCurrent ? PanelBGLight : PanelBG, 14.f, bCurrent ? Accent : PanelBorder, bCurrent ? 2.5f : 1.2f));
		PartyRow[r]->SetRenderOpacity(B.bKO ? 0.55f : 1.f);
		if (LoadedPortraits.FindRef(r) != B.DefId)
		{
			if (UTexture2D* T = Portrait(B.DefId)) PartyPortrait[r]->SetBrush(ImageBrush(T, FVector2D(64, 64), 32.f, Accent, 2.f));
			LoadedPortraits.Add(r, B.DefId);
		}
	}
}

void UZBattleHUD::UpdateCommandPanel()
{
	const FZCombatSim& Sim = Director->GetSim();
	const int32 A = Sim.GetCurrentActor();
	const bool bShow = Director->IsAwaitingInput() && Sim.Actors.IsValidIndex(A) && Sim.Actors[A].bAlly;
	CommandPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	HelpPlate->SetVisibility(bShow || HelpOverrideT > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	TArray<FString> Log = Director->RecentLog;
	while (Log.Num() > 4) Log.RemoveAt(0);
	LogText->SetText(FText::FromString(FString::Join(Log, TEXT("\n"))));
	if (!bShow)
	{
		MenuActor = -1;
		return;
	}
	if (A != MenuActor || LastMenuForm != Sim.Actors[A].Form)
	{
		MenuActor = A;
		LastMenuForm = Sim.Actors[A].Form;
		Level = ELevel::Root;
		BuildRoot();
		Sel = 0;
		while (Sel < Entries.Num() - 1 && !Entries[Sel].bEnabled) ++Sel;
		RefreshList();
		Director->SetTargetHighlight({ A });
	}
}

void UZBattleHUD::BuildRoot()
{
	const FZCombatSim& Sim = Director->GetSim();
	const UZGameData& DB = UZGameData::Get();
	const FZBattler& B = Sim.Actors[MenuActor];
	Entries.Reset();
	FString Why;
	auto Add = [this](const FString& Label, const TCHAR* Icon, EZMenuAction Act, bool bOk, const FString& Reason, const FString& Help, EZTarget Tgt = EZTarget::Self, FName Id = NAME_None, const FString& Cost = FString())
	{
		FZMenuEntry E;
		E.Label = Label; E.Icon = Icon; E.Action = Act; E.bEnabled = bOk; E.Reason = Reason; E.Help = Help; E.Target = Tgt; E.Id = Id; E.Cost = Cost;
		Entries.Add(E);
	};

	const bool bLink = B.DefId == FName(TEXT("link"));
	int32 NTech = 0, NMagic = 0, NTools = 0;
	for (FName T : B.Techniques)
	{
		const FZAbilityDef* Ab = DB.FindAbility(T);
		if (!Ab || Ab->School == TEXT("Form")) continue;
		if (bLink && Ab->School == TEXT("Relic")) ++NTools;
		else if (IsMagicAbility(*Ab)) ++NMagic;
		else ++NTech;
	}
	int32 NItems = 0;
	for (const TPair<FName, int32>& KV : Sim.Items) { if (KV.Value > 0) ++NItems; }

	Add(TEXT("Attaquer"), TEXT("▶"), EZMenuAction::Attack, true, FString(), TEXT("Frappe P1 avec l'arme équipée. Brèche +10."), EZTarget::Enemy);
	if (B.Form.IsNone())
	{
		if (NTech) Add(TEXT("Techniques"), TEXT("✦"), EZMenuAction::OpenTech, true, FString(), TEXT("Six techniques préparées : lame, garde, tir, âmes."));
		if (NMagic) Add(TEXT("Magie"), TEXT("●"), EZMenuAction::OpenMagic, !B.Has(EZStatus::Silence), TEXT("Silence : magie et chants bloqués"), TEXT("Sorts, soins et chants (PM)."));
		if (NTools) Add(TEXT("Outils"), TEXT("◆"), EZMenuAction::OpenTools, true, FString(), TEXT("Grappin, boomerang, bombes : Brèche et interactions de terrain."));
		Add(TEXT("Objets"), TEXT("■"), EZMenuAction::OpenItems, NItems > 0, TEXT("Aucun objet"), TEXT("Potions, fées, remèdes. Consommés après validation."));
		if (B.Forms.Num() > 0) Add(TEXT("Transformation"), TEXT("▲"), EZMenuAction::OpenForms, true, FString(), TEXT("Masques de Majora's Mask et autres formes : 3 activations, coûte de la Résonance."));
		if (const FZSpiritDef* Sp = DB.FindSpirit(B.Spirit))
		{
			const bool bOk = Sim.CanSummon(MenuActor, Why);
			Add(Sp->Name, TEXT("✧"), EZMenuAction::Summon, bOk, Why, FString::Printf(TEXT("Invocation — %s"), *Sp->Desc), Sp->Target, Sp->Id, FString::Printf(TEXT("%d R"), Sp->Resonance));
		}
	}
	else if (const FZFormDef* F = DB.FindForm(B.Form))
	{
		for (FName C : F->Commands)
		{
			if (const FZAbilityDef* Ab = DB.FindAbility(C))
			{
				const bool bOk = Sim.CanUseAbility(MenuActor, C, Why);
				FString Cost = Ab->MP > 0 ? FString::Printf(TEXT("%d PM"), Sim.MPCost(MenuActor, *Ab)) : FString();
				Add(Ab->Name, TEXT("✦"), EZMenuAction::Ability, bOk, Why, Ab->Description, Ab->Target, C, Cost);
			}
		}
		if (!F->NoItems) Add(TEXT("Objets"), TEXT("■"), EZMenuAction::OpenItems, NItems > 0, TEXT("Aucun objet"), TEXT("Potions, fées, remèdes."));
		Add(TEXT("Retour"), TEXT("↺"), EZMenuAction::Revert, true, FString(), FString::Printf(TEXT("Reprendre la forme normale (gratuit, consomme les %d activations restantes)."), B.FormLeft));
	}
	TArray<FName> Duos = Sim.DuosFor(MenuActor);
	if (Duos.Num() > 0)
	{
		bool bAny = false;
		int32 P;
		for (FName D : Duos) { bAny |= Sim.CanDuo(MenuActor, D, P, Why); }
		Add(TEXT("Duo"), TEXT("◈"), EZMenuAction::OpenDuos, bAny, bAny ? FString() : Why, TEXT("Technique à deux : les deux jauges doivent être pleines."));
	}
	Add(TEXT("Garde"), TEXT("◇"), EZMenuAction::Guard, true, FString(), TEXT("Dégâts reçus ×0,5 jusqu'au début de ta prochaine activation."));
	if (Sim.CanInteractValve(MenuActor, Why)) Add(TEXT("Fermer une vanne"), TEXT("⊕"), EZMenuAction::Valve, true, FString(), TEXT("Deux vannes, partagées entre alliés, annulent le Déluge."));
	const bool bFlee = Sim.CanFlee(Why);
	Add(TEXT("Fuir"), TEXT("»"), EZMenuAction::Flee, bFlee, Why, TEXT("100 % hors boss ; aucune récompense."));
	RootEntries = Entries;
	CommandTitle->SetText(FText::FromString(B.Name));
}

void UZBattleHUD::BuildSub(EZMenuAction Kind)
{
	const FZCombatSim& Sim = Director->GetSim();
	const UZGameData& DB = UZGameData::Get();
	const FZBattler& B = Sim.Actors[MenuActor];
	const bool bLink = B.DefId == FName(TEXT("link"));
	Entries.Reset();
	FString Why;
	FString Title;
	if (Kind == EZMenuAction::OpenTech || Kind == EZMenuAction::OpenMagic || Kind == EZMenuAction::OpenTools)
	{
		Title = Kind == EZMenuAction::OpenTech ? TEXT("Techniques") : (Kind == EZMenuAction::OpenMagic ? TEXT("Magie") : TEXT("Outils"));
		for (FName T : B.Techniques)
		{
			const FZAbilityDef* Ab = DB.FindAbility(T);
			if (!Ab || Ab->School == TEXT("Form")) continue;
			const bool bTool = bLink && Ab->School == TEXT("Relic");
			const bool bMagic = !bTool && IsMagicAbility(*Ab);
			const bool bMatch = (Kind == EZMenuAction::OpenTools && bTool) || (Kind == EZMenuAction::OpenMagic && bMagic) || (Kind == EZMenuAction::OpenTech && !bTool && !bMagic);
			if (!bMatch) continue;
			FZMenuEntry E;
			E.Label = Ab->Name;
			E.Icon = ZNames::ElementIcon(Ab->Element);
			TArray<FString> Costs;
			if (Ab->MP > 0) Costs.Add(FString::Printf(TEXT("%d PM"), Sim.MPCost(MenuActor, *Ab)));
			if (Ab->Resonance > 0) Costs.Add(FString::Printf(TEXT("%d R"), Ab->Resonance));
			if (Ab->HasFlag(TEXT("ConsumesBomb"))) Costs.Add(FString::Printf(TEXT("×%d"), Sim.Bombs));
			E.Cost = FString::Join(Costs, TEXT(" "));
			E.bEnabled = Sim.CanUseAbility(MenuActor, T, Why);
			E.Reason = Why;
			E.Help = Ab->Description + (Ab->Source.IsEmpty() ? FString() : FString::Printf(TEXT("  (%s)"), *Ab->Source));
			E.Action = EZMenuAction::Ability;
			E.Id = T;
			E.Target = Ab->Target;
			Entries.Add(E);
		}
	}
	else if (Kind == EZMenuAction::OpenItems)
	{
		Title = TEXT("Objets");
		for (const FZConsumableDef& C : DB.Consumables)
		{
			const int32* N = Sim.Items.Find(C.Id);
			if (!N || *N <= 0) continue;
			FZMenuEntry E;
			E.Label = C.Name;
			E.Icon = TEXT("■");
			E.Cost = FString::Printf(TEXT("×%d"), *N);
			E.bEnabled = Sim.CanUseItem(MenuActor, C.Id, Why);
			E.Reason = Why;
			E.Help = C.Desc;
			E.Action = EZMenuAction::Item;
			E.Id = C.Id;
			E.Target = C.Target;
			Entries.Add(E);
		}
	}
	else if (Kind == EZMenuAction::OpenForms)
	{
		Title = TEXT("Transformation");
		for (FName F : B.Forms)
		{
			const FZFormDef* FD = DB.FindForm(F);
			if (!FD) continue;
			FZMenuEntry E;
			E.Label = FD->Name;
			E.Icon = TEXT("▲");
			E.Cost = FString::Printf(TEXT("%d R"), Sim.FormCost(MenuActor, *FD));
			E.bEnabled = Sim.CanTransform(MenuActor, F, Why);
			E.Reason = Why;
			TArray<FString> Cmds;
			for (FName C : FD->Commands) { if (const FZAbilityDef* Ab = DB.FindAbility(C)) Cmds.Add(Ab->Name); }
			E.Help = FString::Printf(TEXT("ATQ ×%.2f · AMAG ×%.2f · DEF ×%.2f · AGI ×%.2f — %s. %s"), FD->ATQ, FD->AMAG, FD->DEF, FD->AGI, *FString::Join(Cmds, TEXT(", ")), *FD->Weakness);
			E.Action = EZMenuAction::Transform;
			E.Id = F;
			Entries.Add(E);
		}
	}
	else if (Kind == EZMenuAction::OpenDuos)
	{
		Title = TEXT("Duo");
		for (FName D : Sim.DuosFor(MenuActor))
		{
			const FZDuoDef* DD = DB.FindDuo(D);
			int32 P;
			FZMenuEntry E;
			E.Label = DD->Name;
			E.Icon = TEXT("◈");
			E.Cost = FString::Printf(TEXT("%d R"), DD->Resonance);
			E.bEnabled = Sim.CanDuo(MenuActor, D, P, Why);
			E.Reason = Why;
			E.Help = DD->Desc;
			E.Action = EZMenuAction::Duo;
			E.Id = D;
			E.Target = DD->Target;
			Entries.Add(E);
		}
	}
	CommandTitle->SetText(FText::FromString(Title));
	Level = ELevel::Sub;
	Pending.Action = Kind; // mémorise le sous-menu d'origine pour Annuler
}

void UZBattleHUD::BuildTargets(const FZMenuEntry& For)
{
	const FZCombatSim& Sim = Director->GetSim();
	const EZMenuAction From = (Level == ELevel::Sub) ? Pending.Action : EZMenuAction::None;
	Pending = For;
	Pending.Help = For.Help;
	TargetCandidates.Reset();
	bTargetAll = false;
	switch (For.Target)
	{
	case EZTarget::Enemy: TargetCandidates = Sim.LivingEnemiesOf(MenuActor); break;
	case EZTarget::AllEnemies: TargetCandidates = Sim.LivingEnemiesOf(MenuActor); bTargetAll = true; break;
	case EZTarget::Ally: TargetCandidates = Sim.LivingAlliesOf(MenuActor); break;
	case EZTarget::AllAllies: TargetCandidates = Sim.LivingAlliesOf(MenuActor); bTargetAll = true; break;
	case EZTarget::DeadAlly:
		for (const FZBattler& B : Sim.Actors) { if (B.bAlly && B.bKO) TargetCandidates.Add(B.Id); }
		break;
	case EZTarget::Self: TargetCandidates = { MenuActor }; bTargetAll = true; break;
	}
	if (TargetCandidates.Num() == 0)
	{
		SetHelp(TEXT("Aucune cible valide"), true);
		return;
	}
	Entries.Reset();
	if (bTargetAll)
	{
		FZMenuEntry E;
		E.Label = For.Target == EZTarget::Self ? Sim.Actors[MenuActor].Name : (For.Target == EZTarget::AllEnemies ? TEXT("Tous les ennemis") : TEXT("Tout le groupe"));
		E.Icon = TEXT("◎");
		E.Help = For.Help;
		Entries.Add(E);
	}
	else
	{
		for (int32 T : TargetCandidates)
		{
			const FZBattler& B = Sim.Actors[T];
			FZMenuEntry E;
			E.Label = B.Name;
			E.Icon = B.bAlly ? TEXT("♥") : TEXT("◎");
			E.Cost = FString::Printf(TEXT("%d %%"), FMath::RoundToInt(B.HPPct() * 100.f));
			E.Help = For.Help;
			Entries.Add(E);
		}
	}
	Pending.Action = For.Action;
	Pending.Id = For.Id;
	Pending.Cost = For.Cost;
	Level = ELevel::Target;
	Sel = 0;
	// Cible par défaut : ennemi en focus ; soin : allié le plus blessé
	if (!bTargetAll)
	{
		if (For.Target == EZTarget::Enemy)
		{
			const int32 F = Director->GetFocusEnemy();
			const int32 Idx = TargetCandidates.IndexOfByKey(F);
			if (Idx != INDEX_NONE) Sel = Idx;
		}
		else if (For.Target == EZTarget::Ally)
		{
			float Low = 2.f;
			for (int32 i = 0; i < TargetCandidates.Num(); ++i)
			{
				const float P = Sim.Actors[TargetCandidates[i]].HPPct();
				if (P < Low) { Low = P; Sel = i; }
			}
		}
	}
	CommandTitle->SetText(FText::FromString(FString::Printf(TEXT("Cible — %s"), *For.Label)));
	// Mémorise le sous-menu d'origine dans Icon (champ libre) pour le retour
	Pending.Icon = FString::FromInt((int32)From);
	RefreshList();
}

void UZBattleHUD::RefreshList()
{
	for (int32 i = 0; i < RowButton.Num(); ++i)
	{
		const bool bUsed = i < Entries.Num();
		RowButton[i]->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bUsed) continue;
		const FZMenuEntry& E = Entries[i];
		const bool bSel = (i == Sel);
		RowLabel[i]->SetText(FText::FromString(E.Label));
		RowIcon[i]->SetText(FText::FromString(E.Icon));
		RowCost[i]->SetText(FText::FromString(E.Cost));
		const FLinearColor Col = !E.bEnabled ? TextDisabled : (bSel ? FLinearColor(0.01f, 0.1f, 0.1f) : Text);
		RowLabel[i]->SetColorAndOpacity(FSlateColor(Col));
		RowIcon[i]->SetColorAndOpacity(FSlateColor(Col));
		RowCost[i]->SetColorAndOpacity(FSlateColor(bSel ? FLinearColor(0.02f, 0.2f, 0.2f) : TextDim));
		RowLabel[i]->SetFont(Font(bSel ? 25.f : 24.f, bSel));
		RowBG[i]->SetBrush(RoundBrush(bSel ? (E.bEnabled ? Accent : FLinearColor(0.3f, 0.45f, 0.45f)) : FLinearColor::Transparent, 18.f));
	}
	if (Entries.IsValidIndex(Sel))
	{
		const FZMenuEntry& E = Entries[Sel];
		SetHelp(E.bEnabled ? E.Help : FString::Printf(TEXT("Indisponible : %s"), *E.Reason), !E.bEnabled);
	}
	if (Level == ELevel::Target)
	{
		Director->SetTargetHighlight(bTargetAll ? TargetCandidates : TArray<int32>{ TargetCandidates.IsValidIndex(Sel) ? TargetCandidates[Sel] : -1 });
	}
	else if (MenuActor >= 0)
	{
		Director->SetTargetHighlight({ MenuActor });
	}
}

void UZBattleHUD::SetHelp(const FString& S, bool bWarn)
{
	HelpText->SetText(FText::FromString(S));
	HelpText->SetColorAndOpacity(FSlateColor(bWarn ? FLinearColor(1.f, 0.7f, 0.55f) : Text));
	if (bWarn) HelpOverrideT = 2.f;
}

void UZBattleHUD::MoveSelection(int32 Delta)
{
	if (Entries.Num() == 0) return;
	Sel = (Sel + Delta + Entries.Num()) % Entries.Num();
	RefreshList();
}

void UZBattleHUD::Confirm()
{
	if (!Entries.IsValidIndex(Sel)) return;
	if (Level == ELevel::Target)
	{
		const TArray<int32> T = bTargetAll ? TargetCandidates : TArray<int32>{ TargetCandidates[Sel] };
		Execute(Pending, T);
		return;
	}
	const FZMenuEntry E = Entries[Sel];
	if (!E.bEnabled)
	{
		SetHelp(FString::Printf(TEXT("Indisponible : %s"), *E.Reason), true);
		return;
	}
	switch (E.Action)
	{
	case EZMenuAction::OpenTech:
	case EZMenuAction::OpenMagic:
	case EZMenuAction::OpenTools:
	case EZMenuAction::OpenItems:
	case EZMenuAction::OpenForms:
	case EZMenuAction::OpenDuos:
		RootSel = Sel;
		BuildSub(E.Action);
		Sel = 0;
		while (Sel < Entries.Num() - 1 && !Entries[Sel].bEnabled) ++Sel;
		RefreshList();
		break;
	case EZMenuAction::Attack:
	case EZMenuAction::Ability:
	case EZMenuAction::Item:
	case EZMenuAction::Duo:
		if (Level == ELevel::Root) RootSel = Sel;
		BuildTargets(E);
		break;
	default:
		Execute(E, {});
		break;
	}
}

void UZBattleHUD::Cancel()
{
	if (Level == ELevel::Target)
	{
		const EZMenuAction From = (EZMenuAction)FCString::Atoi(*Pending.Icon);
		if (From != EZMenuAction::None)
		{
			BuildSub(From);
			Sel = 0;
		}
		else
		{
			Level = ELevel::Root;
			Entries = RootEntries;
			CommandTitle->SetText(FText::FromString(Director->GetSim().Actors[MenuActor].Name));
			Sel = RootSel;
		}
		RefreshList();
		return;
	}
	if (Level == ELevel::Sub)
	{
		Level = ELevel::Root;
		BuildRoot();
		Sel = RootSel;
		RefreshList();
	}
}

void UZBattleHUD::Execute(const FZMenuEntry& E, const TArray<int32>& Targets)
{
	FZCommand C;
	C.Actor = MenuActor;
	C.Targets = Targets;
	C.Id = E.Id;
	switch (E.Action)
	{
	case EZMenuAction::Attack: C.Type = EZCmd::Attack; break;
	case EZMenuAction::Ability: C.Type = EZCmd::Ability; break;
	case EZMenuAction::Item: C.Type = EZCmd::Item; break;
	case EZMenuAction::Transform: C.Type = EZCmd::Transform; break;
	case EZMenuAction::Revert: C.Type = EZCmd::Revert; break;
	case EZMenuAction::Summon: C.Type = EZCmd::Summon; break;
	case EZMenuAction::Duo: C.Type = EZCmd::Duo; break;
	case EZMenuAction::Guard: C.Type = EZCmd::Guard; break;
	case EZMenuAction::Valve: C.Type = EZCmd::Interact; break;
	case EZMenuAction::Flee: C.Type = EZCmd::Flee; break;
	default: return;
	}
	FString Why;
	if (!Director->SubmitCommand(C, Why))
	{
		SetHelp(Why, true);
		return;
	}
	Level = ELevel::Root;
	MenuActor = -1;
	Entries.Reset();
}

void UZBattleHUD::UpdatePopups()
{
	APlayerController* PC = GetOwningPlayer();
	int32 i = 0;
	for (const FZPopup& P : Director->Popups)
	{
		if (i >= PopupPool.Num()) break;
		FVector2D Screen;
		if (!PC || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, P.World, Screen, false)) continue;
		UTextBlock* T = PopupPool[i++];
		T->SetVisibility(ESlateVisibility::HitTestInvisible);
		T->SetText(FText::FromString(P.Text));
		T->SetFont(Font(P.Size, true));
		const float K = P.Age / P.Life;
		const float Alpha = K < 0.7f ? 1.f : 1.f - (K - 0.7f) / 0.3f;
		T->SetColorAndOpacity(FSlateColor(FLinearColor(P.Color.R, P.Color.G, P.Color.B, Alpha)));
		const float Rise = FMath::Min(P.Age, 0.35f) / 0.35f * 40.f + P.Age * 18.f;
		const float Pop = P.Age < 0.12f ? 1.f + (0.12f - P.Age) * 3.f : 1.f;
		T->SetRenderScale(FVector2D(Pop));
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(T->Slot)) S->SetPosition(Screen - FVector2D(0, Rise));
	}
	for (; i < PopupPool.Num(); ++i) PopupPool[i]->SetVisibility(ESlateVisibility::Collapsed);
}

void UZBattleHUD::UpdateBanners(float Dt)
{
	const float BA = Director->BannerAge;
	ActionPlate->SetVisibility(BA < 1.5f && !Director->Banner.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ActionText->SetText(FText::FromString(Director->Banner));
	ActionPlate->SetRenderOpacity(BA < 1.1f ? 1.f : FMath::Max(0.f, 1.f - (BA - 1.1f) / 0.4f));

	const float G = Director->BigBannerAge;
	const bool bShow = G < 1.8f && !Director->BigBanner.IsEmpty();
	BigBannerPlate->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		BigBannerText->SetText(FText::FromString(Director->BigBanner));
		BigBannerText->SetColorAndOpacity(FSlateColor(Director->BigBannerColor));
		const float In = FMath::Clamp(G / 0.15f, 0.f, 1.f);
		const float Out = G > 1.4f ? 1.f - (G - 1.4f) / 0.4f : 1.f;
		BigBannerPlate->SetRenderOpacity(In * Out);
		BigBannerPlate->SetRenderScale(FVector2D(0.9f + 0.1f * In));
		if (ShownBannerImage != Director->BigBannerImage)
		{
			ShownBannerImage = Director->BigBannerImage;
			UTexture2D* Tex = ShownBannerImage.IsNone() ? nullptr : LoadUITexture(FString::Printf(TEXT("T_Form_%s.png"), *ShownBannerImage.ToString()));
			if (Tex) BigBannerImage->SetBrush(ImageBrush(Tex, FVector2D(170, 205), 10.f, Accent, 1.5f));
			BigBannerImage->SetVisibility(Tex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UZBattleHUD::UpdateResult()
{
	if (!Director->bShowingResult)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	ResultPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FZVictoryReport& R = Director->Report;
	if (Director->Outcome == EZSimState::Victory)
	{
		ResultTitle->SetText(FText::FromString(Upper(TEXT("Victoire"))));
		ResultTitle->SetColorAndOpacity(FSlateColor(Gold));
		FString Body = FString::Printf(TEXT("XP +%s      ·      PA +%d      ·      Rubis +%d\n"), *ZNames::FormatInt(R.XP), R.AP, R.Rupees);
		for (const FString& L : R.Lines) Body += TEXT("\n• ") + L;
		if (R.Lines.Num() == 0) Body += TEXT("\nLes PA nourrissent les techniques de l'équipement porté.");
		ResultBody->SetText(FText::FromString(Body));
		ResultPrompt->SetText(FText::FromString(TEXT("Entrée — continuer")));
	}
	else if (Director->Outcome == EZSimState::Defeat)
	{
		ResultTitle->SetText(FText::FromString(Upper(TEXT("Défaite"))));
		ResultTitle->SetColorAndOpacity(FSlateColor(Bad));
		ResultBody->SetText(FText::FromString(TEXT("Aucune XP ni objet permanent n'est perdu. Tu peux réessayer, ou revenir au point de contrôle pour changer d'équipement.")));
		ResultPrompt->SetText(FText::FromString(ResultSel == 0 ? TEXT("▶ Réessayer        Point de contrôle") : TEXT("Réessayer        ▶ Point de contrôle")));
	}
	else
	{
		ResultTitle->SetText(FText::FromString(Upper(TEXT("Fuite"))));
		ResultTitle->SetColorAndOpacity(FSlateColor(Text));
		ResultBody->SetText(FText::FromString(TEXT("L'équipe s'éloigne. Aucune récompense de combat.")));
		ResultPrompt->SetText(FText::FromString(TEXT("Entrée — continuer")));
	}
}

FReply UZBattleHUD::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& KeyEvent)
{
	const FKey K = KeyEvent.GetKey();
	const bool bUp = K == EKeys::Up || K == EKeys::W || K == EKeys::Z || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_LeftStick_Up;
	const bool bDown = K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Down;
	const bool bLeft = K == EKeys::Left || K == EKeys::A || K == EKeys::Q || K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_LeftStick_Left;
	const bool bRight = K == EKeys::Right || K == EKeys::D || K == EKeys::Gamepad_DPad_Right || K == EKeys::Gamepad_LeftStick_Right;
	const bool bOk = K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::E || K == EKeys::Gamepad_FaceButton_Bottom;
	const bool bBack = K == EKeys::Escape || K == EKeys::BackSpace || K == EKeys::Gamepad_FaceButton_Right;

	if (!Director) return FReply::Unhandled();
	if (Director->bShowingResult)
	{
		if (Director->Outcome == EZSimState::Defeat && (bLeft || bRight || bUp || bDown)) { ResultSel = 1 - ResultSel; return FReply::Handled(); }
		if (bOk)
		{
			if (Director->Outcome == EZSimState::Defeat && ResultSel == 0) { Director->RetryBattle(); }
			else { Director->ConfirmResult(); }
			ResultSel = 0;
		}
		return FReply::Handled();
	}
	if (K == EKeys::Tab || K == EKeys::Gamepad_RightShoulder)
	{
		// Vitesse des jauges : 1× → 1,5× → 2×
		if (UZGameInstance* GI = UZGameInstance::Get(this))
		{
			GI->BattleSpeed = GI->BattleSpeed >= 2.f ? 1.f : GI->BattleSpeed + 0.5f;
			Director->GetSimMutable().SpeedScale = GI->BattleSpeed;
			SetHelp(FString::Printf(TEXT("Vitesse des jauges : %.1f×"), GI->BattleSpeed), false);
			HelpOverrideT = 1.5f;
		}
		return FReply::Handled();
	}
	if (CommandPanel->GetVisibility() == ESlateVisibility::Collapsed) return FReply::Handled();
	if (bUp) { MoveSelection(-1); return FReply::Handled(); }
	if (bDown) { MoveSelection(1); return FReply::Handled(); }
	if (Level == ELevel::Target && (bLeft || bRight)) { MoveSelection(bLeft ? -1 : 1); return FReply::Handled(); }
	if (bOk) { Confirm(); return FReply::Handled(); }
	if (bBack) { Cancel(); return FReply::Handled(); }
	return FReply::Handled();
}

FReply UZBattleHUD::NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Mouse)
{
	if (Director && Director->bShowingResult)
	{
		if (Director->Outcome == EZSimState::Defeat && ResultSel == 0) Director->RetryBattle();
		else Director->ConfirmResult();
		return FReply::Handled();
	}
	if (Mouse.GetEffectingButton() == EKeys::RightMouseButton) { Cancel(); return FReply::Handled(); }
	return FReply::Unhandled();
}
