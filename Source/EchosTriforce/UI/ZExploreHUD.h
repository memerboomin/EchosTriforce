// HUD d'exploration (maquette « Temple des Marées ») : salle + objectif, mini-carte, équipe et cœurs,
// invite d'interaction, dialogues et notifications. Contient aussi l'écran titre.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZExploreHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UBorder;
class UImage;
class UVerticalBox;
class UZMinimapWidget;
class UZEntryButton;

UCLASS()
class ECHOSTRIFORCE_API UZExploreHUD : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;

	void SetRoom(const FString& Name, const FString& Objective);
	void SetPrompt(const FString& Prompt);
	void ShowDialog(const FString& Speaker, const FString& Text);
	bool IsDialogOpen() const { return bDialog; }
	void CloseDialog();
	void Toast(const FString& Text);

	UPROPERTY() UZMinimapWidget* Minimap = nullptr;

private:
	void Build();
	UPROPERTY() UCanvasPanel* Root = nullptr;
	UPROPERTY() UTextBlock* RoomText = nullptr;
	UPROPERTY() UTextBlock* ObjectiveText = nullptr;
	UPROPERTY() UBorder* PromptPlate = nullptr;
	UPROPERTY() UTextBlock* PromptText = nullptr;
	UPROPERTY() UBorder* DialogPlate = nullptr;
	UPROPERTY() UTextBlock* DialogSpeaker = nullptr;
	UPROPERTY() UTextBlock* DialogText = nullptr;
	UPROPERTY() UBorder* ToastPlate = nullptr;
	UPROPERTY() UTextBlock* ToastText = nullptr;
	UPROPERTY() UTextBlock* RupeeText = nullptr;
	UPROPERTY() TArray<UBorder*> MemberRow;
	UPROPERTY() TArray<UImage*> MemberPortrait;
	UPROPERTY() TArray<UTextBlock*> MemberName;
	UPROPERTY() TArray<UTextBlock*> MemberHearts;
	TArray<FName> ShownIds;
	bool bDialog = false;
	float ToastT = 99.f;
	float RoomT = 0.f;
};

UCLASS()
class ECHOSTRIFORCE_API UZTitleWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;

	TArray<FString> Options;
	TFunction<void(int32)> OnChoose;
	int32 Sel = 0;

private:
	void Build();
	void Refresh();
	UPROPERTY() TArray<UBorder*> RowBG;
	UPROPERTY() TArray<UTextBlock*> RowText;
	UPROPERTY() TArray<UZEntryButton*> RowBtn;
	UPROPERTY() UTextBlock* Subtitle = nullptr;
	float Time = 0.f;
};
