#include "ZExploreHUD.h"
#include "ZUI.h"
#include "ZGameInstance.h"
#include "ZProgression.h"
#include "ZGameData.h"
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

using namespace ZUI;

namespace
{
	USizeBox* Sized(UWidgetTree* T, UWidget* C, float W, float H)
	{
		USizeBox* S = T->ConstructWidget<USizeBox>();
		if (W > 0) S->SetWidthOverride(W);
		if (H > 0) S->SetHeightOverride(H);
		S->SetContent(C);
		return S;
	}
}

TSharedRef<SWidget> UZExploreHUD::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Build();
	return Super::RebuildWidget();
}

void UZExploreHUD::Build()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Salle et objectif (haut gauche)
	UBorder* RoomPlate = MakePanel(WidgetTree, FLinearColor(0.01f, 0.04f, 0.05f, 0.62f), 6.f, FMargin(26, 12, 34, 14));
	UVerticalBox* RV = WidgetTree->ConstructWidget<UVerticalBox>();
	RoomText = MakeText(WidgetTree, TEXT(""), 38, Text, true, 60);
	RV->AddChildToVerticalBox(RoomText);
	UHorizontalBox* ObjLine = WidgetTree->ConstructWidget<UHorizontalBox>();
	UTextBlock* Diamond = MakeText(WidgetTree, TEXT("◇"), 20, Accent, true);
	ObjLine->AddChildToHorizontalBox(Diamond)->SetPadding(FMargin(0, 0, 10, 0));
	ObjectiveText = MakeText(WidgetTree, TEXT(""), 20, Text);
	ObjLine->AddChildToHorizontalBox(ObjectiveText);
	RV->AddChildToVerticalBox(ObjLine)->SetPadding(FMargin(0, 4, 0, 0));
	RoomPlate->SetContent(RV);
	Place(Root, RoomPlate, FAnchors(0, 0), FVector2D(40, 28), FVector2D(0, 0));

	// Mini-carte (haut droite)
	Minimap = CreateWidget<UZMinimapWidget>(this, UZMinimapWidget::StaticClass());
	Place(Root, Sized(WidgetTree, Minimap, 210, 210), FAnchors(1, 0), FVector2D(-36, 24), FVector2D(1, 0));
	UTextBlock* North = MakeText(WidgetTree, TEXT("N"), 16, Text, true);
	Place(Root, North, FAnchors(1, 0), FVector2D(-136, 16), FVector2D(0.5f, 0));
	RupeeText = MakeText(WidgetTree, TEXT(""), 18, Gold, true);
	Place(Root, RupeeText, FAnchors(1, 0), FVector2D(-40, 244), FVector2D(1, 0));

	// Équipe (bas gauche) : portrait, nom, cœurs
	UVerticalBox* Party = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 i = 0; i < 3; ++i)
	{
		UBorder* Row = MakePanel(WidgetTree, FLinearColor(0.01f, 0.04f, 0.05f, 0.62f), 30.f, FMargin(6, 6, 26, 6));
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		UImage* P = MakeImage(WidgetTree, nullptr, FVector2D(52, 52), 26.f, Accent, 1.5f);
		H->AddChildToHorizontalBox(Sized(WidgetTree, P, 52, 52))->SetPadding(FMargin(0, 0, 12, 0));
		UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* N = MakeText(WidgetTree, TEXT(""), 19, Text, true, 60);
		V->AddChildToVerticalBox(N);
		UTextBlock* Hearts = MakeText(WidgetTree, TEXT(""), 17, FLinearColor(0.25f, 0.95f, 0.75f), true, 30);
		V->AddChildToVerticalBox(Hearts);
		UHorizontalBoxSlot* VS = H->AddChildToHorizontalBox(V);
		VS->SetVerticalAlignment(VAlign_Center);
		Row->SetContent(Sized(WidgetTree, H, 300, 0));
		Party->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 5));
		MemberRow.Add(Row); MemberPortrait.Add(P); MemberName.Add(N); MemberHearts.Add(Hearts);
	}
	Place(Root, Party, FAnchors(0, 1), FVector2D(36, -30), FVector2D(0, 1));

	// Invite d'interaction (bas centre)
	PromptPlate = MakePanel(WidgetTree, PanelBG, 20.f, FMargin(24, 8), Accent, 1.5f);
	PromptText = MakeText(WidgetTree, TEXT(""), 22, Text, true);
	PromptPlate->SetContent(PromptText);
	Place(Root, PromptPlate, FAnchors(0.5f, 1.f), FVector2D(0, -60), FVector2D(0.5f, 1.f));
	PromptPlate->SetVisibility(ESlateVisibility::Collapsed);

	// Dialogue
	DialogPlate = MakePanel(WidgetTree, FLinearColor(0.01f, 0.035f, 0.045f, 0.9f), 14.f, FMargin(34, 20), Accent, 1.5f);
	UVerticalBox* DV = WidgetTree->ConstructWidget<UVerticalBox>();
	DialogSpeaker = MakeText(WidgetTree, TEXT(""), 24, Accent, true, 40);
	DV->AddChildToVerticalBox(DialogSpeaker)->SetPadding(FMargin(0, 0, 0, 8));
	DialogText = MakeText(WidgetTree, TEXT(""), 21, Text);
	DialogText->SetAutoWrapText(true);
	DV->AddChildToVerticalBox(Sized(WidgetTree, DialogText, 900, 0));
	UTextBlock* Hint = MakeText(WidgetTree, TEXT("E / Entrée — continuer"), 15, TextDim);
	DV->AddChildToVerticalBox(Hint)->SetHorizontalAlignment(HAlign_Right);
	DialogPlate->SetContent(DV);
	Place(Root, DialogPlate, FAnchors(0.5f, 1.f), FVector2D(0, -40), FVector2D(0.5f, 1.f));
	DialogPlate->SetVisibility(ESlateVisibility::Collapsed);

	// Notification
	ToastPlate = MakePanel(WidgetTree, PanelBGLight, 10.f, FMargin(26, 10), Gold, 1.5f);
	ToastText = MakeText(WidgetTree, TEXT(""), 22, Gold, true);
	ToastPlate->SetContent(ToastText);
	Place(Root, ToastPlate, FAnchors(0.5f, 0.f), FVector2D(0, 120), FVector2D(0.5f, 0.f));
	ToastPlate->SetVisibility(ESlateVisibility::Collapsed);

	UTextBlock* Keys = MakeText(WidgetTree, TEXT("ZQSD/WASD : se déplacer · Souris : caméra · E : interagir · Espace : sauter · F : frapper · Tab : menu"), 14, TextDim);
	Place(Root, Keys, FAnchors(1, 1), FVector2D(-30, -14), FVector2D(1, 1));
}

void UZExploreHUD::SetRoom(const FString& Name, const FString& Objective)
{
	if (!RoomText) return;
	RoomText->SetText(FText::FromString(Name));
	ObjectiveText->SetText(FText::FromString(Objective));
	RoomT = 0.f;
}

void UZExploreHUD::SetPrompt(const FString& Prompt)
{
	if (!PromptPlate) return;
	PromptPlate->SetVisibility(Prompt.IsEmpty() || bDialog ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	PromptText->SetText(FText::FromString(TEXT("E  ·  ") + Prompt));
}

void UZExploreHUD::ShowDialog(const FString& Speaker, const FString& InText)
{
	bDialog = true;
	DialogSpeaker->SetText(FText::FromString(Speaker));
	DialogText->SetText(FText::FromString(InText));
	DialogPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	PromptPlate->SetVisibility(ESlateVisibility::Collapsed);
}

void UZExploreHUD::CloseDialog()
{
	bDialog = false;
	DialogPlate->SetVisibility(ESlateVisibility::Collapsed);
}

void UZExploreHUD::Toast(const FString& InText)
{
	ToastText->SetText(FText::FromString(InText));
	ToastT = 0.f;
}

void UZExploreHUD::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	ToastT += Dt;
	RoomT += Dt;
	ToastPlate->SetVisibility(ToastT < 3.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ToastPlate->SetRenderOpacity(ToastT < 2.5f ? 1.f : 1.f - (ToastT - 2.5f) / 0.5f);

	UZGameInstance* GI = UZGameInstance::Get(this);
	if (!GI) return;
	RupeeText->SetText(FText::FromString(FString::Printf(TEXT("◆ %d rubis"), GI->Rupees)));
	const TArray<FZCharacterState*> Party = GI->GetActiveParty();
	for (int32 i = 0; i < MemberRow.Num(); ++i)
	{
		if (!Party.IsValidIndex(i)) { MemberRow[i]->SetVisibility(ESlateVisibility::Collapsed); continue; }
		MemberRow[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
		const FZCharacterState& S = *Party[i];
		const FZDerivedStats D = ZProgression::ComputeDerived(S);
		const int32 HPv = S.CurrentHP < 0 ? D.MaxHP : S.CurrentHP;
		const int32 Full = FMath::Clamp(FMath::CeilToInt(7.f * HPv / FMath::Max(1, D.MaxHP)), 0, 7);
		FString Hearts;
		for (int32 h = 0; h < 7; ++h) Hearts += h < Full ? TEXT("♥") : TEXT("♡");
		MemberHearts[i]->SetText(FText::FromString(Hearts));
		const FZCharacterDef* Def = UZGameData::Get().FindCharacter(S.Id);
		MemberName[i]->SetText(FText::FromString(FString::Printf(TEXT("%s  N%d"), Def ? *Def->Name : *S.Id.ToString(), S.Level)));
		if (!ShownIds.IsValidIndex(i) || ShownIds[i] != S.Id)
		{
			if (ShownIds.Num() <= i) ShownIds.SetNum(i + 1);
			ShownIds[i] = S.Id;
			if (UTexture2D* T = Portrait(S.Id)) MemberPortrait[i]->SetBrush(ImageBrush(T, FVector2D(52, 52), 26.f, Accent, 1.5f));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UZTitleWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) Build();
	return Super::RebuildWidget();
}

void UZTitleWidget::Build()
{
	SetIsFocusable(true);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* Zelda = MakeText(WidgetTree, TEXT("ZELDA"), 86, Gold, true, 400);
	V->AddChildToVerticalBox(Zelda)->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Sub = MakeText(WidgetTree, TEXT("Les Échos de la Triforce"), 40, Text, true, 80);
	V->AddChildToVerticalBox(Sub)->SetHorizontalAlignment(HAlign_Center);
	Subtitle = MakeText(WidgetTree, TEXT("RPG à commandes · ATB en mode Attente"), 20, TextDim, false, 60);
	V->AddChildToVerticalBox(Subtitle)->SetHorizontalAlignment(HAlign_Center);
	Place(Root, V, FAnchors(0.5f, 0.2f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));

	UBorder* Panel = MakePanel(WidgetTree, PanelBG, 14.f, FMargin(16, 14), PanelBorder, 1.5f);
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>();
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		UZEntryButton* B = WidgetTree->ConstructWidget<UZEntryButton>(UZEntryButton::StaticClass());
		B->Index = i;
		B->OnHover = [this](int32 I) { Sel = I; Refresh(); };
		B->OnPick = [this](int32 I) { Sel = I; if (OnChoose) OnChoose(I); };
		B->Bind();
		UBorder* BG = MakePanel(WidgetTree, FLinearColor::Transparent, 18.f, FMargin(28, 7));
		UTextBlock* T = MakeText(WidgetTree, Options[i], 24, Text, false, 20);
		BG->SetContent(T);
		B->SetFillContent(BG);
		List->AddChildToVerticalBox(B)->SetPadding(FMargin(0, 2));
		RowBG.Add(BG); RowText.Add(T); RowBtn.Add(B);
	}
	// Largeur minimale, mais la liste s'élargit avec son texte (police à empattements plus large)
	USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>();
	ListBox->SetMinDesiredWidth(560.f);
	ListBox->SetContent(List);
	Panel->SetContent(ListBox);
	Place(Root, Panel, FAnchors(0.5f, 0.62f), FVector2D::ZeroVector, FVector2D(0.5f, 0.5f));

	UTextBlock* Legal = MakeText(WidgetTree, TEXT("Projet de fan non officiel, sans but commercial. The Legend of Zelda et ses personnages appartiennent à Nintendo."), 14, TextDim);
	Place(Root, Legal, FAnchors(0.5f, 1.f), FVector2D(0, -18), FVector2D(0.5f, 1.f));
	Refresh();
}

void UZTitleWidget::Refresh()
{
	for (int32 i = 0; i < RowBG.Num(); ++i)
	{
		const bool bSel = i == Sel;
		RowBG[i]->SetBrush(RoundBrush(bSel ? Accent : FLinearColor::Transparent, 18.f));
		RowText[i]->SetColorAndOpacity(FSlateColor(bSel ? FLinearColor(0.01f, 0.1f, 0.1f) : Text));
		RowText[i]->SetFont(Font(24, bSel, 20));
	}
}

void UZTitleWidget::NativeTick(const FGeometry& Geo, float Dt)
{
	Super::NativeTick(Geo, Dt);
	Time += Dt;
	if (Subtitle) Subtitle->SetRenderOpacity(0.6f + 0.4f * FMath::Sin(Time * 1.5f));
	if (!HasKeyboardFocus() && IsVisible()) SetKeyboardFocus();
}

FReply UZTitleWidget::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& KeyEvent)
{
	const FKey K = KeyEvent.GetKey();
	if (K == EKeys::Up || K == EKeys::W || K == EKeys::Z || K == EKeys::Gamepad_DPad_Up) { Sel = (Sel + Options.Num() - 1) % Options.Num(); Refresh(); return FReply::Handled(); }
	if (K == EKeys::Down || K == EKeys::S || K == EKeys::Gamepad_DPad_Down) { Sel = (Sel + 1) % Options.Num(); Refresh(); return FReply::Handled(); }
	if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::E || K == EKeys::Gamepad_FaceButton_Bottom) { if (OnChoose) OnChoose(Sel); return FReply::Handled(); }
	return FReply::Handled();
}
