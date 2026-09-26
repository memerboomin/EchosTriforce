#include "ZUI.h"
#include "ZGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Engine/Engine.h"
#include "Components/ButtonSlot.h"
#include "Fonts/CompositeFont.h"
#include "Misc/CommandLine.h"

namespace ZUI
{
	const FLinearColor PanelBG(0.010f, 0.040f, 0.055f, 0.80f);
	const FLinearColor PanelBGLight(0.030f, 0.090f, 0.110f, 0.82f);
	const FLinearColor PanelBorder(0.35f, 0.85f, 0.80f, 0.45f);
	const FLinearColor Accent(0.16f, 0.88f, 0.78f, 1.f);
	const FLinearColor AccentDark(0.02f, 0.30f, 0.30f, 1.f);
	const FLinearColor Text(0.93f, 0.97f, 0.96f, 1.f);
	const FLinearColor TextDim(0.62f, 0.76f, 0.76f, 1.f);
	const FLinearColor TextDisabled(0.35f, 0.42f, 0.44f, 1.f);
	const FLinearColor HP(0.20f, 0.85f, 0.55f, 1.f);
	const FLinearColor MP(0.20f, 0.52f, 1.00f, 1.f);
	const FLinearColor ATB(1.00f, 0.80f, 0.15f, 1.f);
	const FLinearColor BossHP(0.88f, 0.12f, 0.12f, 1.f);
	const FLinearColor Gold(1.00f, 0.84f, 0.40f, 1.f);
	const FLinearColor Good(0.35f, 1.00f, 0.55f, 1.f);
	const FLinearColor Bad(1.00f, 0.38f, 0.35f, 1.f);
	const FLinearColor Breach(1.00f, 0.55f, 0.15f, 1.f);

	float TextScale()
	{
		const UWorld* W = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		if (const UZGameInstance* GI = W ? Cast<UZGameInstance>(W->GetGameInstance()) : nullptr)
		{
			return GI->TextScale / 100.f;
		}
		return 1.f;
	}

	/** Polices à empattements proches de la maquette. Priorité aux fichiers du projet (Content/UI/Fonts :
	 *  Title.ttf, Regular.ttf, Bold.ttf, sous licence libre), sinon polices système macOS chargées sur place
	 *  (non copiées dans le projet) : Iowan Old Style pour le texte, Copperplate (petites capitales) pour les titres.
	 *  Les symboles absents (icônes d'éléments, cœurs…) retombent sur les polices de secours du moteur. */
	const TSharedPtr<const FCompositeFont>& SerifFont()
	{
		static TSharedPtr<const FCompositeFont> Font;
		static bool bTried = false;
		if (bTried) return Font;
		bTried = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("ZNoSerif"))) return Font;
		struct FFace { const TCHAR* Name; const TCHAR* ProjectFile; const TCHAR* SystemFile; int32 SubFace; };
		static const FFace Faces[] = {
			{ TEXT("Regular"), TEXT("Regular.ttf"), TEXT("/System/Library/Fonts/Supplemental/Iowan Old Style.ttc"), 0 },
			{ TEXT("Bold"), TEXT("Bold.ttf"), TEXT("/System/Library/Fonts/Supplemental/Iowan Old Style.ttc"), 1 },
			{ TEXT("Title"), TEXT("Title.ttf"), TEXT("/System/Library/Fonts/Supplemental/Copperplate.ttc"), 0 },
		};
		TSharedRef<FStandaloneCompositeFont> C = MakeShared<FStandaloneCompositeFont>();
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("UI/Fonts"));
		for (const FFace& F : Faces)
		{
			const FString Local = Dir / F.ProjectFile;
			const bool bLocal = FPaths::FileExists(Local);
			if (!bLocal && !FPaths::FileExists(F.SystemFile)) continue;
			FTypefaceEntry& E = C->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(F.Name));
			E.Font = FFontData(bLocal ? Local : FString(F.SystemFile), EFontHinting::Default, EFontLoadingPolicy::LazyLoad, bLocal ? 0 : F.SubFace);
		}
		if (C->DefaultTypeface.Fonts.Num() < 2) return Font;
		const FSlateFontInfo Def = FCoreStyle::GetDefaultFontStyle("Regular", 12);
		if (const FCompositeFont* D = Def.GetCompositeFont())
		{
			// Symboles (cœurs, flèches, formes, icônes d'éléments) : police du moteur, teintable, avant tout émoji couleur du système
			FCompositeSubFont Symbols;
			Symbols.Typeface = D->DefaultTypeface;
			Symbols.CharacterRanges.Add(FInt32Range(0x2190, 0x2800));
			C->SubTypefaces.Add(Symbols);
			C->SubTypefaces.Append(D->SubTypefaces);
			C->FallbackTypeface = D->FallbackTypeface;
		}
		Font = C;
		return Font;
	}

	bool HasTitleFont()
	{
		const TSharedPtr<const FCompositeFont>& F = SerifFont();
		return F.IsValid() && F->DefaultTypeface.Fonts.ContainsByPredicate([](const FTypefaceEntry& E) { return E.Name == TEXT("Title"); });
	}

	FSlateFontInfo Font(float Size, bool bBold, int32 LetterSpacing)
	{
		const int32 Px = FMath::RoundToInt(Size * TextScale());
		if (!SerifFont().IsValid())
		{
			FSlateFontInfo F = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Px);
			F.LetterSpacing = LetterSpacing;
			return F;
		}
		// Titres (gras espacé) : petites capitales ; le reste en romain à empattements, un peu plus grand (œil plus petit)
		const bool bTitle = bBold && LetterSpacing >= 30 && HasTitleFont();
		FSlateFontInfo F(SerifFont(), bTitle ? Px : FMath::RoundToInt(Px * 1.06f), bTitle ? FName(TEXT("Title")) : (bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"))));
		F.LetterSpacing = bTitle ? LetterSpacing / 2 : LetterSpacing;
		return F;
	}

	FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}

	FSlateBrush ImageBrush(UTexture2D* Tex, const FVector2D& Size, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		FSlateBrush B;
		B.SetResourceObject(Tex);
		B.ImageSize = Size;
		if (Radius > 0.f || OutlineWidth > 0.f)
		{
			B.DrawAs = ESlateBrushDrawType::RoundedBox;
			B.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
			B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		}
		else
		{
			B.DrawAs = ESlateBrushDrawType::Image;
		}
		return B;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Str, float Size, const FLinearColor& Color, bool bBold, int32 Spacing)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Str));
		T->SetFont(Font(Size, bBold, Spacing));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(FVector2D(1.f, 1.5f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		return T;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Fill, float Radius, const FMargin& Padding, const FLinearColor& Outline, float OutlineWidth)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>();
		B->SetBrush(RoundBrush(Fill, Radius, Outline, OutlineWidth));
		B->SetPadding(Padding);
		return B;
	}

	UProgressBar* MakeBar(UWidgetTree* Tree, const FLinearColor& Fill, const FLinearColor& Background)
	{
		UProgressBar* P = Tree->ConstructWidget<UProgressBar>();
		FProgressBarStyle S;
		S.BackgroundImage = RoundBrush(Background, 3.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 1.f);
		S.FillImage = RoundBrush(FLinearColor::White, 3.f);
		S.MarqueeImage = RoundBrush(FLinearColor::White, 3.f);
		P->SetWidgetStyle(S);
		P->SetFillColorAndOpacity(Fill);
		P->SetPercent(1.f);
		return P;
	}

	UImage* MakeImage(UWidgetTree* Tree, UTexture2D* Tex, const FVector2D& Size, float Radius, const FLinearColor& Outline, float OutlineWidth)
	{
		UImage* I = Tree->ConstructWidget<UImage>();
		if (Tex) { I->SetBrush(ImageBrush(Tex, Size, Radius, Outline, OutlineWidth)); }
		else { I->SetBrush(RoundBrush(FLinearColor(0.1f, 0.2f, 0.22f, 1.f), Radius)); I->SetDesiredSizeOverride(Size); }
		return I;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* W, const FAnchors& Anchors, const FVector2D& Pos, const FVector2D& Alignment, const FVector2D& Size)
	{
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(W);
		S->SetAnchors(Anchors);
		S->SetPosition(Pos);
		S->SetAlignment(Alignment);
		if (Size.IsZero()) { S->SetAutoSize(true); }
		else { S->SetSize(Size); }
		return S;
	}

	UTexture2D* LoadUITexture(const FString& FileName)
	{
		static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
		if (TWeakObjectPtr<UTexture2D>* C = Cache.Find(FileName))
		{
			if (C->IsValid()) return C->Get();
		}
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI"), TEXT("Portraits"), FileName);
		UTexture2D* T = FImageUtils::ImportFileAsTexture2D(Path);
		if (T)
		{
			T->AddToRoot();
			Cache.Add(FileName, T);
		}
		return T;
	}

	UTexture2D* Portrait(FName CharacterId)
	{
		return LoadUITexture(FString::Printf(TEXT("T_Portrait_%s.png"), *CharacterId.ToString()));
	}

	FString Upper(const FString& In)
	{
		if (HasTitleFont()) return In; // Copperplate : les minuscules s'affichent déjà en petites capitales
		FString S = In.ToUpper();
		static const TCHAR* From = TEXT("éèêëàâäîïôöùûüçœæ");
		static const TCHAR* To = TEXT("ÉÈÊËÀÂÄÎÏÔÖÙÛÜÇŒÆ");
		for (TCHAR& C : S)
		{
			for (int32 i = 0; From[i]; ++i) { if (C == From[i]) { C = To[i]; break; } }
		}
		return S;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

UZEntryButton::UZEntryButton(const FObjectInitializer& OI) : Super(OI)
{
	InitIsFocusable(false);
	FButtonStyle S;
	FSlateBrush None;
	None.DrawAs = ESlateBrushDrawType::NoDrawType;
	S.SetNormal(None);
	S.SetHovered(None);
	S.SetPressed(None);
	S.SetDisabled(None);
	S.SetNormalPadding(FMargin(0));
	S.SetPressedPadding(FMargin(0));
	SetStyle(S);
}

void UZEntryButton::SetFillContent(UWidget* Content)
{
	if (UButtonSlot* S = Cast<UButtonSlot>(SetContent(Content)))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
		S->SetPadding(FMargin(0));
	}
}

void UZEntryButton::Bind()
{
	OnClicked.AddUniqueDynamic(this, &UZEntryButton::HandleClicked);
	OnHovered.AddUniqueDynamic(this, &UZEntryButton::HandleHovered);
}

void UZEntryButton::HandleClicked() { if (OnPick) OnPick(Index); }
void UZEntryButton::HandleHovered() { if (OnHover) OnHover(Index); }

// ---------------------------------------------------------------------------------------------------------------------

static void DrawArc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geo, const FVector2D& Center, float Radius, float From, float To, const FLinearColor& Color, float Thickness)
{
	TArray<FVector2D> Pts;
	const int32 Steps = FMath::Max(2, FMath::CeilToInt(FMath::Abs(To - From) / 4.f));
	for (int32 i = 0; i <= Steps; ++i)
	{
		const float A = FMath::DegreesToRadians(FMath::Lerp(From, To, (float)i / Steps) - 90.f);
		Pts.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
	}
	FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Pts, ESlateDrawEffect::None, Color, true, Thickness);
}

int32 UZRingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const FVector2D C = Size * 0.5f;
	const float R = FMath::Min(Size.X, Size.Y) * 0.5f - Thickness;
	DrawArc(Out, Layer, Geo, C, R, 0.f, 360.f, Track, Thickness);
	if (Percent > 0.f)
	{
		DrawArc(Out, Layer + 1, Geo, C, R, 0.f, 360.f * FMath::Clamp(Percent, 0.f, 1.f), Color, Thickness);
	}
	DrawArc(Out, Layer + 1, Geo, C, R + Thickness * 0.9f, 0.f, 360.f, FLinearColor(Color.R, Color.G, Color.B, 0.25f), 1.f);
	return Super::NativePaint(Args, Geo, Cull, Out, Layer + 2, Style, bParentEnabled);
}

int32 UZMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Size = Geo.GetLocalSize();
	const FVector2D C = Size * 0.5f;
	const float R = FMath::Min(Size.X, Size.Y) * 0.5f - 3.f;
	// Disque de fond
	for (int32 k = 0; k < 18; ++k)
	{
		DrawArc(Out, Layer, Geo, C, R * (k + 1) / 18.f, 0.f, 360.f, FLinearColor(0.02f, 0.08f, 0.1f, 0.85f), R / 18.f + 1.5f);
	}
	DrawArc(Out, Layer + 1, Geo, C, R, 0.f, 360.f, FLinearColor(0.4f, 0.9f, 0.85f, 0.7f), 2.f);
	// Salles, centrées sur le joueur
	const float Scale = R / 2600.f;
	auto ToLocal = [&](const FVector2D& W)
	{
		// Nord en haut : X monde → haut
		const FVector2D D = (W - Player) * Scale;
		return C + FVector2D(D.Y, -D.X);
	};
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const FVector2D A = ToLocal(Rooms[i].Min);
		const FVector2D B = ToLocal(Rooms[i].Max);
		const FVector2D Mn(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y));
		const FVector2D Mx(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y));
		if (FVector2D::Distance((Mn + Mx) * 0.5f, C) > R + 60.f) continue;
		const bool bSeen = Visited.IsValidIndex(i) && Visited[i];
		const FLinearColor Col = bSeen ? FLinearColor(0.25f, 0.75f, 0.72f, 0.75f) : FLinearColor(0.12f, 0.3f, 0.32f, 0.5f);
		TArray<FVector2D> Box = { Mn, FVector2D(Mx.X, Mn.Y), Mx, FVector2D(Mn.X, Mx.Y), Mn };
		FSlateDrawElement::MakeLines(Out, Layer + 2, Geo.ToPaintGeometry(), Box, ESlateDrawEffect::None, Col, true, 2.f);
	}
	// Flèche du joueur
	const float Yaw = FMath::DegreesToRadians(PlayerYaw);
	const FVector2D Fwd(FMath::Sin(Yaw), -FMath::Cos(Yaw));
	const FVector2D Side(-Fwd.Y, Fwd.X);
	TArray<FVector2D> Arrow = { C + Fwd * 9.f, C - Fwd * 6.f + Side * 6.f, C - Fwd * 3.f, C - Fwd * 6.f - Side * 6.f, C + Fwd * 9.f };
	FSlateDrawElement::MakeLines(Out, Layer + 3, Geo.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, FLinearColor(1.f, 0.85f, 0.3f, 1.f), true, 2.5f);
	return Super::NativePaint(Args, Geo, Cull, Out, Layer + 4, Style, bParentEnabled);
}
