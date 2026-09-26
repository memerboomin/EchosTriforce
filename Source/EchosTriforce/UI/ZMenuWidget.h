// Menu principal (chapitres 10 et 21) : Statut, Équipement, Techniques, Formes & Esprits, Collection, Journal, Système.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZTypes.h"
#include "ZMenuWidget.generated.h"

class UTextBlock;
class UBorder;
class UImage;
class UVerticalBox;
class UZEntryButton;

struct FZMenuRow
{
	FString Icon;
	FString Label;
	FString Value;
	FLinearColor Color = FLinearColor::White;
	bool bEnabled = true;
	FString Key;
	int32 Data = 0;
};

UCLASS()
class ECHOSTRIFORCE_API UZMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geo, const FPointerEvent& Mouse) override;

	TFunction<void()> OnClose;
	TFunction<void()> OnEquipmentChanged;
	TFunction<void()> OnReturnToTitle;
	void OpenAt(int32 InTab);

private:
	struct FRowW { UZEntryButton* Btn = nullptr; UBorder* BG = nullptr; UTextBlock* Icon = nullptr; UTextBlock* Label = nullptr; UTextBlock* Value = nullptr; };

	void Build();
	void MakeRows(UVerticalBox* Box, int32 Count, int32 Column, TArray<FRowW>& Out, float LabelSize);
	void Refresh();
	void RefreshRows(const TArray<FZMenuRow>& Data, TArray<FRowW>& Rows, int32 SelIdx, int32& Scroll, bool bActive);
	void BuildColumns();
	void UpdateDetail();
	void ShowItemCard(const FString& ItemId, const FString& CompareSlotItem, bool bCompare, EZEquipSlot Slot);
	void ShowAbilityCard(FName AbilityId);
	void ShowStatus();
	void SetCompare(const TArray<FString>& Labels, const TArray<FString>& Before, const TArray<FString>& After, const TArray<int32>& Delta);
	void ClearCompare();
	void Confirm();
	void Back();
	void Move(int32 D);
	void SwitchTab(int32 D);
	void Toast(const FString& S);
	struct FZCharacterState* Member();

	int32 Tab = 0;
	int32 Column = 0; // 0 = équipe (ou liste principale), 1, 2
	int32 MemberSel = 0;
	int32 Sel1 = 0, Sel2 = 0, Scroll1 = 0, Scroll2 = 0;
	int32 TypeFilter = 0;
	TArray<FZMenuRow> Col1, Col2;
	FString ToastMsg;
	float ToastT = 99.f;

	UPROPERTY() TArray<UBorder*> TabBG;
	UPROPERTY() TArray<UTextBlock*> TabText;
	TArray<FRowW> PartyRows, Rows1, Rows2;
	UPROPERTY() TArray<UImage*> PartyImages;
	UPROPERTY() UTextBlock* Col1Title = nullptr;
	UPROPERTY() UTextBlock* Col2Title = nullptr;
	UPROPERTY() UBorder* Col2Panel = nullptr;
	UPROPERTY() UBorder* DetailPanel = nullptr;
	UPROPERTY() UImage* DetailImage = nullptr;
	UPROPERTY() UTextBlock* DetailTitle = nullptr;
	UPROPERTY() UTextBlock* DetailSub = nullptr;
	UPROPERTY() UTextBlock* DetailBody = nullptr;
	UPROPERTY() UTextBlock* ResistText = nullptr;
	UPROPERTY() TArray<UTextBlock*> CmpLabel;
	UPROPERTY() TArray<UTextBlock*> CmpBefore;
	UPROPERTY() TArray<UTextBlock*> CmpAfter;
	UPROPERTY() UTextBlock* FooterText = nullptr;
	UPROPERTY() UTextBlock* ToastText = nullptr;
	UPROPERTY() UTextBlock* HeaderInfo = nullptr;
	FString ShownImage;
};
