// Boîte à outils UMG construite en C++ : palette « bleu-sarcelle » inspirée de la maquette de combat.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "ZUI.generated.h"

class UWidgetTree;
class UTextBlock;
class UBorder;
class UProgressBar;
class UImage;
class UCanvasPanel;
class UCanvasPanelSlot;
class UTexture2D;

namespace ZUI
{
	// Palette (chapitre 21 : couleurs doublées de symboles et libellés)
	extern ECHOSTRIFORCE_API const FLinearColor PanelBG;
	extern ECHOSTRIFORCE_API const FLinearColor PanelBGLight;
	extern ECHOSTRIFORCE_API const FLinearColor PanelBorder;
	extern ECHOSTRIFORCE_API const FLinearColor Accent;
	extern ECHOSTRIFORCE_API const FLinearColor AccentDark;
	extern ECHOSTRIFORCE_API const FLinearColor Text;
	extern ECHOSTRIFORCE_API const FLinearColor TextDim;
	extern ECHOSTRIFORCE_API const FLinearColor TextDisabled;
	extern ECHOSTRIFORCE_API const FLinearColor HP;
	extern ECHOSTRIFORCE_API const FLinearColor MP;
	extern ECHOSTRIFORCE_API const FLinearColor ATB;
	extern ECHOSTRIFORCE_API const FLinearColor BossHP;
	extern ECHOSTRIFORCE_API const FLinearColor Gold;
	extern ECHOSTRIFORCE_API const FLinearColor Good;
	extern ECHOSTRIFORCE_API const FLinearColor Bad;
	extern ECHOSTRIFORCE_API const FLinearColor Breach;

	ECHOSTRIFORCE_API float TextScale();
	ECHOSTRIFORCE_API FSlateFontInfo Font(float Size, bool bBold = false, int32 LetterSpacing = 0);
	ECHOSTRIFORCE_API FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);
	ECHOSTRIFORCE_API FSlateBrush ImageBrush(UTexture2D* Tex, const FVector2D& Size, float Radius = 0.f, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);

	ECHOSTRIFORCE_API UTextBlock* MakeText(UWidgetTree* Tree, const FString& Str, float Size, const FLinearColor& Color, bool bBold = false, int32 Spacing = 0);
	ECHOSTRIFORCE_API UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Fill, float Radius, const FMargin& Padding, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);
	ECHOSTRIFORCE_API UProgressBar* MakeBar(UWidgetTree* Tree, const FLinearColor& Fill, const FLinearColor& Background = FLinearColor(0.02f, 0.05f, 0.07f, 0.85f));
	ECHOSTRIFORCE_API UImage* MakeImage(UWidgetTree* Tree, UTexture2D* Tex, const FVector2D& Size, float Radius = 0.f, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);
	ECHOSTRIFORCE_API UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* W, const FAnchors& Anchors, const FVector2D& Pos, const FVector2D& Alignment, const FVector2D& Size = FVector2D::ZeroVector);

	/** Charge un PNG de Content/UI/Portraits (mis en cache). */
	ECHOSTRIFORCE_API UTexture2D* LoadUITexture(const FString& FileName);
	ECHOSTRIFORCE_API UTexture2D* Portrait(FName CharacterId);
	/** Majuscules françaises (FString::ToUpper ignore les accents). */
	ECHOSTRIFORCE_API FString Upper(const FString& S);
}

/** Bouton transparent qui transmet son index au propriétaire (clic et survol à la souris). */
UCLASS()
class ECHOSTRIFORCE_API UZEntryButton : public UButton
{
	GENERATED_BODY()
public:
	UZEntryButton(const FObjectInitializer& OI);
	int32 Index = 0;
	/** Place le contenu en remplissage (un UButton centre son contenu par défaut). */
	void SetFillContent(UWidget* Content);
	TFunction<void(int32)> OnPick;
	TFunction<void(int32)> OnHover;
	void Bind();
private:
	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
};

/** Anneau de jauge (Résonance) dessiné en NativePaint. */
UCLASS()
class ECHOSTRIFORCE_API UZRingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	float Percent = 0.f;
	FLinearColor Color = FLinearColor(0.2f, 0.9f, 0.8f);
	FLinearColor Track = FLinearColor(0.05f, 0.15f, 0.18f, 0.9f);
	float Thickness = 7.f;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
};

/** Mini-carte de la citerne : salles et position du joueur. */
UCLASS()
class ECHOSTRIFORCE_API UZMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	TArray<FBox2D> Rooms;      // en unités monde (X,Y)
	TArray<bool> Visited;
	FVector2D Player = FVector2D::ZeroVector;
	float PlayerYaw = 0.f;
	FBox2D Bounds = FBox2D(FVector2D(-1000, -1000), FVector2D(1000, 1000));
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
};
