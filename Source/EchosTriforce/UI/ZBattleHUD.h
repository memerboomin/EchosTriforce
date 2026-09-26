// HUD de combat (chapitre 21) : ennemi et préparation en haut, groupe en bas à droite, commandes en bas à gauche,
// Résonance en haut à droite, trois prochains acteurs, nombres flottants, écran de résultat.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZCombat.h"
#include "ZBattleHUD.generated.h"

class AZBattleDirector;
class UCanvasPanel;
class UTextBlock;
class UProgressBar;
class UBorder;
class UImage;
class UVerticalBox;
class UZRingWidget;
class UZEntryButton;

enum class EZMenuAction : uint8
{
	None, Attack, OpenTech, OpenMagic, OpenTools, OpenItems, OpenForms, OpenDuos, Ability, Item, Transform, Revert, Summon, Duo, Guard, Valve, Flee
};

struct FZMenuEntry
{
	FString Label;
	FString Icon;
	FString Cost;
	FString Help;
	bool bEnabled = true;
	FString Reason;
	EZMenuAction Action = EZMenuAction::None;
	FName Id;
	EZTarget Target = EZTarget::Self;
};

UCLASS()
class ECHOSTRIFORCE_API UZBattleHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY() AZBattleDirector* Director = nullptr;

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Mouse) override;

private:
	void Build();
	void UpdateEnemyPanel();
	void UpdateParty();
	void UpdateCommandPanel();
	void UpdatePopups();
	void UpdateBanners(float Dt);
	void UpdateResult();

	void BuildRoot();
	void BuildSub(EZMenuAction Kind);
	void BuildTargets(const FZMenuEntry& For);
	void RefreshList();
	void MoveSelection(int32 Delta);
	void Confirm();
	void Cancel();
	void Execute(const FZMenuEntry& E, const TArray<int32>& Targets);
	void SetHelp(const FString& S, bool bWarn = false);

	enum class ELevel : uint8 { Root, Sub, Target };
	ELevel Level = ELevel::Root;
	TArray<FZMenuEntry> Entries;
	TArray<FZMenuEntry> RootEntries;
	int32 RootSel = 0;
	int32 Sel = 0;
	FZMenuEntry Pending;
	TArray<int32> TargetCandidates;
	bool bTargetAll = false;
	int32 MenuActor = -1;
	int32 ResultSel = 0;

	UPROPERTY() UCanvasPanel* Root = nullptr;
	// Ennemi
	UPROPERTY() UTextBlock* EnemyName = nullptr;
	UPROPERTY() UProgressBar* EnemyHP = nullptr;
	UPROPERTY() UTextBlock* EnemyHPText = nullptr;
	UPROPERTY() UProgressBar* EnemyBreach = nullptr;
	UPROPERTY() UTextBlock* EnemyBreachText = nullptr;
	UPROPERTY() UTextBlock* EnemyInfo = nullptr;
	UPROPERTY() UBorder* PreparePlate = nullptr;
	UPROPERTY() UTextBlock* PrepareText = nullptr;
	UPROPERTY() UBorder* ActionPlate = nullptr;
	UPROPERTY() UTextBlock* ActionText = nullptr;
	// Résonance et ordre
	UPROPERTY() UZRingWidget* Ring = nullptr;
	UPROPERTY() UTextBlock* RingValue = nullptr;
	UPROPERTY() UTextBlock* NextText = nullptr;
	// Commandes
	UPROPERTY() UBorder* CommandPanel = nullptr;
	UPROPERTY() UTextBlock* CommandTitle = nullptr;
	UPROPERTY() UVerticalBox* CommandList = nullptr;
	UPROPERTY() TArray<UBorder*> RowBG;
	UPROPERTY() TArray<UTextBlock*> RowLabel;
	UPROPERTY() TArray<UTextBlock*> RowIcon;
	UPROPERTY() TArray<UTextBlock*> RowCost;
	UPROPERTY() TArray<UZEntryButton*> RowButton;
	UPROPERTY() UBorder* HelpPlate = nullptr;
	UPROPERTY() UTextBlock* HelpText = nullptr;
	UPROPERTY() UTextBlock* LogText = nullptr;
	// Groupe
	UPROPERTY() TArray<UBorder*> PartyRow;
	UPROPERTY() TArray<UImage*> PartyPortrait;
	UPROPERTY() TArray<UTextBlock*> PartyName;
	UPROPERTY() TArray<UTextBlock*> PartyHPText;
	UPROPERTY() TArray<UProgressBar*> PartyHP;
	UPROPERTY() TArray<UTextBlock*> PartyMPText;
	UPROPERTY() TArray<UProgressBar*> PartyMP;
	UPROPERTY() TArray<UProgressBar*> PartyATB;
	UPROPERTY() TArray<UTextBlock*> PartyStatus;
	TArray<int32> PartyIds;
	// Bannières
	UPROPERTY() UTextBlock* BigBannerText = nullptr;
	UPROPERTY() UImage* BigBannerImage = nullptr;
	UPROPERTY() UBorder* BigBannerPlate = nullptr;
	FName ShownBannerImage;
	// Nombres flottants
	UPROPERTY() TArray<UTextBlock*> PopupPool;
	// Résultat
	UPROPERTY() UBorder* ResultPanel = nullptr;
	UPROPERTY() UTextBlock* ResultTitle = nullptr;
	UPROPERTY() UTextBlock* ResultBody = nullptr;
	UPROPERTY() UTextBlock* ResultPrompt = nullptr;
	FString HelpOverride;
	float HelpOverrideT = 0.f;
	TMap<int32, FName> LoadedPortraits;
	FName LastMenuForm;
};
