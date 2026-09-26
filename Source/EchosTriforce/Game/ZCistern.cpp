#include "ZCistern.h"
#include "ZVisuals.h"
#include "ZGameInstance.h"
#include "ZGameData.h"
#include "EchosTriforce.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	constexpr float WallH = 480.f;

	UStaticMesh* PropMesh(const TCHAR* Name) { return ZVis::Mesh(FString(TEXT("/Game/Art/Meshes/")) + Name); }

	/** Matériaux du kit par nom d'emplacement (Body = pierre claire sculptée). */
	void PropMaterials(UStaticMeshComponent* C)
	{
		const TArray<FName> Slots = C->GetMaterialSlotNames();
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			FString S = Slots[i].ToString();
			if (S == TEXT("Body")) S = TEXT("Arcade");
			const FString Path = FString::Printf(TEXT("/Game/Art/Materials/MI_Kit_%s.MI_Kit_%s"), *S, *S);
			if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path))) continue;
			if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn)) C->SetMaterial(i, M);
		}
	}
	constexpr float WallT = 90.f;
	constexpr float CorridorW = 520.f;

	struct FOpening { int32 Room; int32 Side; float At; }; // Side 0=Sud 1=Nord 2=Ouest 3=Est ; At = coordonnée le long du mur
	struct FCorridor { FVector2D A; FVector2D B; FName Gate; };

	const TArray<FOpening>& Openings()
	{
		static const TArray<FOpening> O = {
			{ 0, 1, 0 },
			{ 1, 0, 0 }, { 1, 2, 2500 }, { 1, 1, 0 },
			{ 2, 3, 2500 },
			{ 3, 0, 0 }, { 3, 3, 4900 },
			{ 4, 2, 4900 }, { 4, 1, 2500 },
			{ 5, 0, 2500 }, { 5, 2, 7300 },
			{ 6, 3, 7300 }, { 6, 1, 0 },
			{ 7, 0, 0 }, { 7, 1, 0 },
			{ 8, 0, 0 }, { 8, 1, 0 },
			{ 9, 0, 0 },
		};
		return O;
	}

	const TArray<FCorridor>& Corridors()
	{
		static const TArray<FCorridor> C = {
			{ FVector2D(900, 0), FVector2D(1500, 0), NAME_None },
			{ FVector2D(2500, -1100), FVector2D(2500, -1800), NAME_None },
			{ FVector2D(3500, 0), FVector2D(4100, 0), NAME_None },
			{ FVector2D(4900, 800), FVector2D(4900, 1750), TEXT("G_VALVE") },
			{ FVector2D(5650, 2500), FVector2D(6500, 2500), NAME_None },
			{ FVector2D(7300, 1750), FVector2D(7300, 900), TEXT("G_PUZZLE") },
			{ FVector2D(8200, 0), FVector2D(8900, 0), NAME_None },
			{ FVector2D(10500, 0), FVector2D(11100, 0), NAME_None },
			{ FVector2D(12300, 0), FVector2D(12800, 0), TEXT("G_BOSS") },
		};
		return C;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

const TArray<FZRoomDef>& AZCistern::Rooms()
{
	static TArray<FZRoomDef> R;
	if (R.Num() == 0)
	{
		auto Add = [](FName Id, const TCHAR* Name, const TCHAR* Obj, FVector2D C, FVector2D H, bool bPool, int32 N, bool bRound = false)
		{
			FZRoomDef D; D.Id = Id; D.Name = Name; D.Objective = Obj; D.Center = C; D.Half = H; D.bPool = bPool; D.Number = N; D.bRound = bRound;
			R.Add(D);
		};
		Add(TEXT("R01"), TEXT("Vestibule"), TEXT("Garde-toi de l'Octorok, puis ouvre le coffre"), FVector2D(0, 0), FVector2D(900, 900), false, 1);
		Add(TEXT("R02"), TEXT("Bassin"), TEXT("Compare ta tenue face aux Chuchus aqueux"), FVector2D(2500, 0), FVector2D(1000, 1100), true, 2);
		Add(TEXT("R03"), TEXT("Atelier"), TEXT("Récupère la tunique Zora auprès du mécanicien"), FVector2D(2500, -2500), FVector2D(700, 700), false, 3);
		Add(TEXT("R04"), TEXT("Conduite"), TEXT("Actionne la vanne avec le grappin"), FVector2D(4900, 0), FVector2D(800, 800), false, 4);
		Add(TEXT("R05"), TEXT("Chapelle"), TEXT("Rejoins Mipha près de la fontaine"), FVector2D(4900, 2500), FVector2D(750, 750), false, 5);
		Add(TEXT("R06"), TEXT("Archives"), TEXT("Active les trois symboles dans le bon ordre"), FVector2D(7300, 2500), FVector2D(800, 750), false, 6);
		Add(TEXT("R07"), TEXT("Écluse"), TEXT("Vaincs la sentinelle électrique — attention à Mouillé"), FVector2D(7300, 0), FVector2D(900, 900), true, 7);
		Add(TEXT("R08"), TEXT("Fontaine"), TEXT("Repose-toi et obtiens le masque Zora"), FVector2D(9700, 0), FVector2D(800, 800), true, 8);
		Add(TEXT("R09"), TEXT("Antichambre"), TEXT("Sauvegarde et prépare tes passifs"), FVector2D(11700, 0), FVector2D(600, 600), false, 9);
		Add(TEXT("R10"), TEXT("Sanctuaire de la Néréide"), TEXT("Ouvrir le sceau des trois sources"), FVector2D(14400, 0), FVector2D(1600, 1600), false, 10, true);
	}
	return R;
}

int32 AZCistern::RoomAt(const FVector& W)
{
	const TArray<FZRoomDef>& R = Rooms();
	for (int32 i = 0; i < R.Num(); ++i)
	{
		const FVector2D D = FVector2D(W.X, W.Y) - R[i].Center;
		if (FMath::Abs(D.X) <= R[i].Half.X + 50.f && FMath::Abs(D.Y) <= R[i].Half.Y + 50.f) return i;
	}
	return INDEX_NONE;
}

FVector AZCistern::RoomSpawn(FName RoomId)
{
	for (const FZRoomDef& R : Rooms())
	{
		if (R.Id == RoomId) return FVector(R.Center.X - R.Half.X * 0.6f, R.Center.Y, 120.f);
	}
	return FVector(-500, 0, 120);
}

FVector AZCistern::BattleCenter(const FVector& Near, FRotator& OutFacing)
{
	OutFacing = FRotator(0, 0, 0);
	const int32 I = RoomAt(Near);
	if (I == INDEX_NONE) return Near;
	const FZRoomDef& R = Rooms()[I];
	FVector C(R.Center.X, R.Center.Y, 0.f);
	if (R.bPool) C.X -= R.Half.X * 0.35f;
	if (R.Id == FName(TEXT("R10"))) C.X += 150.f;
	return C;
}

// ---------------------------------------------------------------------------------------------------------------------

AZCistern::AZCistern()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* R = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	R->SetMobility(EComponentMobility::Static);
	SetRootComponent(R);
}

void AZCistern::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	bBuilt = false;
	Build();
}

void AZCistern::BeginPlay()
{
	Super::BeginPlay();
	bBuilt = false;
	Build();
	SpawnGameplay();
}

UInstancedStaticMeshComponent* AZCistern::Kit(FName Key, const FString& MeshPath, const FString& Fallback, const FLinearColor& Color, float Emissive, bool bCollide)
{
	if (UInstancedStaticMeshComponent** Found = Kits.Find(Key)) return *Found;
	UStaticMesh* M = ZVis::Mesh(MeshPath);
	if (!M) M = ZVis::Mesh(Fallback);
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), Key));
	C->SetStaticMesh(M);
	C->SetupAttachment(GetRootComponent());
	C->SetMobility(EComponentMobility::Static);
	C->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	C->SetCastShadow(Key != FName(TEXT("Water")) && Key != FName(TEXT("Fall")));
	C->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	C->RegisterComponent();
	ZVis::Tint(C, Color, Emissive);
	// Matériau par emplacement : MI_Kit_<Clé>_<Emplacement>, sinon MI_Kit_<Emplacement>, sinon MI_Kit_<Clé>
	const TArray<FName> SlotNames = C->GetMaterialSlotNames();
	for (int32 i = 0; i < FMath::Max(1, C->GetNumMaterials()); ++i)
	{
		const FString Slot = SlotNames.IsValidIndex(i) ? SlotNames[i].ToString() : FString();
		TArray<FString> Names;
		if (!Slot.IsEmpty()) { Names.Add(FString::Printf(TEXT("MI_Kit_%s_%s"), *Key.ToString(), *Slot)); Names.Add(TEXT("MI_Kit_") + Slot); }
		Names.Add(TEXT("MI_Kit_") + Key.ToString());
		for (const FString& N : Names)
		{
			const FString Path = FString::Printf(TEXT("/Game/Art/Materials/%s.%s"), *N, *N);
			if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path))) continue;
			if (UMaterialInterface* Mi = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_Quiet | LOAD_NoWarn))
			{
				C->SetMaterial(i, Mi);
				break;
			}
		}
	}
	Kits.Add(Key, C);
	return C;
}

void AZCistern::Box(FName Key, const FVector& Center, const FVector& Size, float Yaw)
{
	if (UInstancedStaticMeshComponent** C = Kits.Find(Key))
	{
		(*C)->AddInstance(FTransform(FRotator(0, Yaw, 0), Center, Size / 100.f), true);
	}
}

void AZCistern::Put(FName Key, const FVector& Loc, float Yaw, float Scale)
{
	if (UInstancedStaticMeshComponent** C = Kits.Find(Key))
	{
		if ((*C)->GetStaticMesh()) (*C)->AddInstance(FTransform(FRotator(0, Yaw, 0), Loc, FVector(Scale)), true);
	}
}

void AZCistern::Cyl(FName Key, const FVector& Base, float Radius, float Height)
{
	if (UInstancedStaticMeshComponent** C = Kits.Find(Key))
	{
		(*C)->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, Height * 0.5f), FVector(Radius / 50.f, Radius / 50.f, Height / 100.f)), true);
	}
}

void AZCistern::Pillar(const FVector& Base, float Height)
{
	Box(TEXT("Trim"), Base + FVector(0, 0, 20), FVector(150, 150, 40));
	Cyl(TEXT("Pillar"), Base + FVector(0, 0, 40), 52.f, Height - 90.f);
	Box(TEXT("Trim"), Base + FVector(0, 0, Height - 25), FVector(160, 160, 50));
}

void AZCistern::Arch(const FVector& Center, float Yaw, float Width, float Height)
{
	const FRotator R(0, Yaw, 0);
	const FVector Side = R.RotateVector(FVector(0, Width * 0.5f + 70.f, 0));
	Pillar(Center + Side, Height);
	Pillar(Center - Side, Height);
	Box(TEXT("Trim"), Center + FVector(0, 0, Height + 35.f), FVector(160, Width + 300.f, 70), Yaw);
	Box(TEXT("Wall"), Center + FVector(0, 0, Height + 95.f), FVector(120, Width + 200.f, 50), Yaw);
	Box(TEXT("Glyph"), Center + FVector(0, 0, Height + 35.f) + R.RotateVector(FVector(-82, 0, 0)), FVector(6, 60, 40), Yaw);
}

void AZCistern::WallWithOpenings(const FVector2D& A, const FVector2D& B, const TArray<float>& Open, float Height, const FVector2D& Inside, int32 Seed)
{
	FRandomStream Rnd(Seed * 7919 + FMath::RoundToInt(A.X + A.Y * 3.f));
	// Mur axial de A à B, interrompu par des ouvertures (coordonnée le long du mur)
	const bool bAlongY = FMath::IsNearlyEqual(A.X, B.X);
	const float From = bAlongY ? FMath::Min(A.Y, B.Y) : FMath::Min(A.X, B.X);
	const float To = bAlongY ? FMath::Max(A.Y, B.Y) : FMath::Max(A.X, B.X);
	TArray<FVector2D> Segs;
	float Cur = From;
	TArray<float> Sorted = Open;
	Sorted.Sort();
	for (float O : Sorted)
	{
		const float S = O - CorridorW * 0.5f;
		if (S > Cur) Segs.Add(FVector2D(Cur, S));
		Cur = O + CorridorW * 0.5f;
	}
	if (Cur < To) Segs.Add(FVector2D(Cur, To));
	for (const FVector2D& S : Segs)
	{
		const float Mid = (S.X + S.Y) * 0.5f;
		const float Len = S.Y - S.X;
		if (Len < 10.f) continue;
		const FVector C = bAlongY ? FVector(A.X, Mid, Height * 0.5f) : FVector(Mid, A.Y, Height * 0.5f);
		const FVector Sz = bAlongY ? FVector(WallT, Len, Height) : FVector(Len, WallT, Height);
		Box(TEXT("Wall"), C, Sz);
		Box(TEXT("Trim"), C + FVector(0, 0, Height * 0.5f + 15.f), Sz + FVector(30, 30, -Height + 30.f));
		if (Inside.IsZero()) continue;
		// Habillage de la face intérieure : arcatures sculptées, lierre qui retombe, herbe au pied du mur
		const FVector Dir = bAlongY ? FVector(0, 1, 0) : FVector(1, 0, 0);
		const FVector N = bAlongY ? FVector(FMath::Sign(Inside.X - A.X), 0, 0) : FVector(0, FMath::Sign(Inside.Y - A.Y), 0);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(N.Y, N.X));
		const FVector Face = FVector(C.X, C.Y, 0) + N * (WallT * 0.5f);
		const float ArcScale = FMath::Min(1.f, (Height - 30.f) / 540.f);
		const int32 NArc = FMath::FloorToInt((Len - 120.f) / (460.f * ArcScale));
		for (int32 k = 0; k < NArc; ++k)
		{
			const float Along = (k - (NArc - 1) * 0.5f) * 460.f * ArcScale;
			Put(TEXT("Arcade"), Face + Dir * Along, Yaw, ArcScale);
			if (Rnd.FRand() < 0.45f)
			{
				Put(TEXT("Ivy"), Face + Dir * (Along + 230.f * ArcScale * (Rnd.FRand() < 0.5f ? -1.f : 1.f)) + FVector(0, 0, Height), Yaw, Rnd.FRandRange(0.8f, 1.25f));
			}
		}
		for (float T = 60.f; T < Len - 60.f; T += Rnd.FRandRange(70.f, 150.f))
		{
			if (Rnd.FRand() < 0.35f) continue;
			Put(Rnd.FRand() < 0.2f ? TEXT("GrassPatch") : TEXT("GrassTuft"), Face + Dir * (T - Len * 0.5f) + N * Rnd.FRandRange(15.f, 60.f), Rnd.FRandRange(0.f, 360.f), Rnd.FRandRange(0.7f, 1.4f));
		}
	}
}

void AZCistern::Waterfall(const FVector& Top, float Width, float Height, float Yaw)
{
	// Yaw = direction vers laquelle la cascade fait face (vers l'intérieur de la salle)
	const FRotator R(0, Yaw, 0);
	Box(TEXT("Fall"), Top - FVector(0, 0, Height * 0.5f), FVector(14, Width, Height), Yaw);
	Box(TEXT("Wall"), Top - FVector(0, 0, Height * 0.5f - 30.f) + R.RotateVector(FVector(-60, 0, 0)), FVector(80, Width + 160, Height + 60), Yaw);
	Box(TEXT("Trim"), Top + FVector(0, 0, 40.f) + R.RotateVector(FVector(-40, 0, 0)), FVector(140, Width + 200, 40), Yaw);
	Cyl(TEXT("Foam"), Top - FVector(0, 0, Height + 5.f) + R.RotateVector(FVector(45, 0, 0)), Width * 0.45f, 26.f);
}

void AZCistern::Banner(const FVector& Base, float Yaw)
{
	const FRotator R(0, Yaw, 0);
	if (UInstancedStaticMeshComponent** Cloth = Kits.Find(TEXT("BannerCloth")))
	{
		if ((*Cloth)->GetStaticMesh())
		{
			Put(TEXT("BannerCloth"), Base + FVector(0, 0, 40), Yaw, 1.f);
			return;
		}
	}
	Box(TEXT("Banner"), Base + FVector(0, 0, 150), FVector(8, 150, 300), Yaw);
	Box(TEXT("Glyph"), Base + FVector(0, 0, 190) + R.RotateVector(FVector(6, 0, 0)), FVector(6, 40, 70), Yaw);
	Box(TEXT("Glyph"), Base + FVector(0, 0, 105) + R.RotateVector(FVector(6, 0, 0)), FVector(6, 90, 10), Yaw);
	Box(TEXT("Glyph"), Base + FVector(0, 0, 80) + R.RotateVector(FVector(6, 0, 0)), FVector(6, 70, 10), Yaw);
}

void AZCistern::Gate(FName Id, const FVector& Center, float Yaw, float Width)
{
	UInstancedStaticMeshComponent* G = NewObject<UInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), FName(*(TEXT("Gate_") + Id.ToString()))));
	G->SetMobility(EComponentMobility::Static);
	G->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
	G->SetupAttachment(GetRootComponent());
	G->SetCollisionProfileName(TEXT("BlockAll"));
	G->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	G->RegisterComponent();
	ZVis::Tint(G, FLinearColor(0.28f, 0.33f, 0.36f), 0.f);
	const FRotator R(0, Yaw, 0);
	for (int32 i = 0; i < 9; ++i)
	{
		const float Off = -Width * 0.5f + Width * (i + 0.5f) / 9.f;
		G->AddInstance(FTransform(FRotator::ZeroRotator, Center + R.RotateVector(FVector(0, Off, 0)) + FVector(0, 0, 150), FVector(0.22f, 0.22f, 3.f)), true);
	}
	G->AddInstance(FTransform(R, Center + FVector(0, 0, 60), FVector(0.25f, Width / 50.f * 0.5f, 0.25f)), true);
	Gates.Add(Id, G);
}

void AZCistern::Build()
{
	if (bBuilt) return;
	bBuilt = true;
	// Reconstruction propre (éditeur, PIE, rechargement) : on repart de zéro
	TArray<UInstancedStaticMeshComponent*> Old;
	GetComponents<UInstancedStaticMeshComponent>(Old);
	for (UInstancedStaticMeshComponent* C : Old) { if (C) C->DestroyComponent(); }
	Kits.Reset();
	Gates.Reset();
	const FString Art = TEXT("/Game/Art/Meshes/");
	Kit(TEXT("Floor"), Art + TEXT("SM_Kit_Floor"), TEXT("Cube"), FLinearColor(0.60f, 0.56f, 0.46f), 0.f, true);
	Kit(TEXT("Wall"), Art + TEXT("SM_Kit_Block"), TEXT("Cube"), FLinearColor(0.50f, 0.48f, 0.42f), 0.f, true);
	Kit(TEXT("Trim"), Art + TEXT("SM_Kit_Block"), TEXT("Cube"), FLinearColor(0.72f, 0.69f, 0.60f), 0.f, true);
	Kit(TEXT("Pillar"), Art + TEXT("SM_Kit_Column"), TEXT("Cylinder"), FLinearColor(0.68f, 0.65f, 0.56f), 0.f, true);
	Kit(TEXT("Moss"), TEXT(""), TEXT("Cube"), FLinearColor(0.22f, 0.40f, 0.16f), 0.f, false);
	Kit(TEXT("Water"), Art + TEXT("SM_Kit_Water"), TEXT("Plane"), FLinearColor(0.06f, 0.55f, 0.62f), 0.35f, false);
	Kit(TEXT("Fall"), TEXT(""), TEXT("Cube"), FLinearColor(0.60f, 0.88f, 0.96f), 1.4f, false);
	Kit(TEXT("RoundFloor"), Art + TEXT("SM_Kit_Disc"), TEXT("Cylinder"), FLinearColor(0.60f, 0.56f, 0.46f), 0.f, true);
	Kit(TEXT("RoundTrim"), TEXT(""), TEXT("Cylinder"), FLinearColor(0.72f, 0.69f, 0.60f), 0.f, true);
	Kit(TEXT("RoundGlyph"), TEXT(""), TEXT("Cylinder"), FLinearColor(0.25f, 1.0f, 0.92f), 6.f, false);
	Kit(TEXT("Foam"), TEXT(""), TEXT("Cylinder"), FLinearColor(0.85f, 0.95f, 1.f), 0.8f, false);
	Kit(TEXT("Banner"), TEXT(""), TEXT("Cube"), FLinearColor(0.04f, 0.20f, 0.24f), 0.f, false);
	Kit(TEXT("Glyph"), TEXT(""), TEXT("Cube"), FLinearColor(0.25f, 1.0f, 0.92f), 8.f, false);
	Kit(TEXT("Rock"), Art + TEXT("SM_Kit_Rock"), TEXT("Cube"), FLinearColor(0.46f, 0.43f, 0.38f), 0.f, false);
	Kit(TEXT("Tower"), TEXT(""), TEXT("Cube"), FLinearColor(0.62f, 0.68f, 0.74f), 0.f, false);
	Kit(TEXT("Grass"), TEXT(""), TEXT("Cube"), FLinearColor(0.30f, 0.52f, 0.20f), 0.f, false);
	// Habillage (Tools/blender/make_env_kit.py) : absent = simplement ignoré
	Kit(TEXT("GrassTuft"), Art + TEXT("SM_Kit_GrassTuft"), TEXT(""), FLinearColor::White, 0.f, false);
	Kit(TEXT("GrassPatch"), Art + TEXT("SM_Kit_GrassPatch"), TEXT(""), FLinearColor::White, 0.f, false);
	Kit(TEXT("Ivy"), Art + TEXT("SM_Kit_Ivy"), TEXT(""), FLinearColor::White, 0.f, false);
	Kit(TEXT("BannerCloth"), Art + TEXT("SM_Kit_BannerCloth"), TEXT(""), FLinearColor::White, 0.f, false);
	Kit(TEXT("Arcade"), Art + TEXT("SM_Kit_Arcade"), TEXT(""), FLinearColor::White, 0.f, false);
	Kit(TEXT("ColumnBroken"), Art + TEXT("SM_Kit_ColumnBroken"), TEXT(""), FLinearColor::White, 0.f, true);
	Kit(TEXT("Rubble"), Art + TEXT("SM_Kit_Rubble"), TEXT(""), FLinearColor::White, 0.f, false);
	Kit(TEXT("Lily"), Art + TEXT("SM_Kit_Lily"), TEXT(""), FLinearColor::White, 0.f, false);
	const bool bDressing = Kits[TEXT("GrassTuft")]->GetStaticMesh() != nullptr;
	// Murs invisibles au bord de l'eau : on ne peut pas tomber dans un bassin dont on ne pourrait pas ressortir.
	// Ils bloquent les personnages mais pas la caméra ni les tracés de sol.
	if (UInstancedStaticMeshComponent* Block = Kit(TEXT("Block"), TEXT(""), TEXT("Cube"), FLinearColor::White, 0.f, true))
	{
		Block->SetVisibility(false);
		Block->SetCastShadow(false);
		Block->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Block->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}

	const TArray<FZRoomDef>& Rs = Rooms();
	for (int32 i = 0; i < Rs.Num(); ++i)
	{
		const FZRoomDef& R = Rs[i];
		const FVector C(R.Center.X, R.Center.Y, 0.f);
		const FVector2D H = R.Half;
		// Sol (bassin central légèrement creusé et rempli d'eau)
		if (R.bPool)
		{
			const FVector2D PH(H.X * 0.45f, H.Y * 0.5f);
			Box(TEXT("Floor"), C + FVector(0, 0, -65), FVector(PH.X * 2, PH.Y * 2, 50));
			Box(TEXT("Floor"), C + FVector((H.X + PH.X) * 0.5f, 0, -25), FVector(H.X - PH.X, H.Y * 2, 50));
			Box(TEXT("Floor"), C - FVector((H.X + PH.X) * 0.5f, 0, 25), FVector(H.X - PH.X, H.Y * 2, 50));
			Box(TEXT("Floor"), C + FVector(0, (H.Y + PH.Y) * 0.5f, -25), FVector(PH.X * 2, H.Y - PH.Y, 50));
			Box(TEXT("Floor"), C - FVector(0, (H.Y + PH.Y) * 0.5f, 25), FVector(PH.X * 2, H.Y - PH.Y, 50));
			Box(TEXT("Water"), C + FVector(0, 0, -12), FVector(PH.X * 2, PH.Y * 2, 1));
			// Margelle
			Box(TEXT("Trim"), C + FVector(PH.X, 0, 4), FVector(40, PH.Y * 2 + 40, 20));
			Box(TEXT("Trim"), C + FVector(-PH.X, 0, 4), FVector(40, PH.Y * 2 + 40, 20));
			Box(TEXT("Trim"), C + FVector(0, PH.Y, 4), FVector(PH.X * 2, 40, 20));
			Box(TEXT("Trim"), C + FVector(0, -PH.Y, 4), FVector(PH.X * 2, 40, 20));
			Box(TEXT("Block"), C + FVector(PH.X, 0, 150), FVector(40, PH.Y * 2 + 40, 300));
			Box(TEXT("Block"), C + FVector(-PH.X, 0, 150), FVector(40, PH.Y * 2 + 40, 300));
			Box(TEXT("Block"), C + FVector(0, PH.Y, 150), FVector(PH.X * 2 + 40, 40, 300));
			Box(TEXT("Block"), C + FVector(0, -PH.Y, 150), FVector(PH.X * 2 + 40, 40, 300));
		}
		else if (R.bRound)
		{
			// Arène : plateforme ronde entourée d'un anneau d'eau (maquette de combat)
			Box(TEXT("Floor"), C + FVector(0, 0, -95), FVector(H.X * 2, H.Y * 2, 50));
			Box(TEXT("Water"), C + FVector(0, 0, -40), FVector(H.X * 2, H.Y * 2, 1));
			Cyl(TEXT("RoundFloor"), C + FVector(0, 0, -70), 1150.f, 70.f);
			Cyl(TEXT("RoundTrim"), C + FVector(0, 0, -4), 1100.f, 8.f);
			Cyl(TEXT("RoundFloor"), C + FVector(0, 0, 0), 1000.f, 6.f);
			for (int32 k = 0; k < 3; ++k)
			{
				Cyl(TEXT("RoundGlyph"), C + FVector(0, 0, 6.5f + k * 2.f), 820.f - k * 260.f, 1.f);
				Cyl(TEXT("RoundFloor"), C + FVector(0, 0, 7.f + k * 2.f), 790.f - k * 260.f, 1.f);
			}
			// Chaussée d'accès depuis le sud
			Box(TEXT("Floor"), C + FVector(-H.X + 250, 0, -25), FVector(700, CorridorW, 50));
			// Bord de la plateforme ronde (sauf l'arrivée de la chaussée) et bords de la chaussée
			for (int32 k = 0; k < 40; ++k)
			{
				const float A = 2.f * PI * k / 40.f;
				if (FMath::Abs(FMath::UnwindRadians(A - PI)) < 0.235f) continue;
				Box(TEXT("Block"), C + FVector(FMath::Cos(A) * 1160.f, FMath::Sin(A) * 1160.f, 150.f), FVector(40, 2.f * PI * 1160.f / 40.f + 30.f, 300), FMath::RadiansToDegrees(A));
			}
			for (int32 sy = -1; sy <= 1; sy += 2)
			{
				Box(TEXT("Block"), C + FVector(-H.X + 250, sy * (CorridorW * 0.5f + 20.f), 150), FVector(760, 40, 300));
			}
		}
		else
		{
			Box(TEXT("Floor"), C + FVector(0, 0, -25), FVector(H.X * 2, H.Y * 2, 50));
			// Dalles décoratives
			for (int32 k = -1; k <= 1; k += 2)
			{
				Box(TEXT("Trim"), C + FVector(k * H.X * 0.5f, 0, 1), FVector(20, H.Y * 1.6f, 2));
			}
		}

		// Murs avec ouvertures
		TArray<float> South, North, West, East;
		for (const FOpening& O : Openings())
		{
			if (O.Room != i) continue;
			(O.Side == 0 ? South : O.Side == 1 ? North : O.Side == 2 ? West : East).Add(O.At);
		}
		const float WH = R.bRound ? WallH + 140.f : WallH;
		const FVector2D In(C.X, C.Y);
		WallWithOpenings(FVector2D(C.X - H.X, C.Y - H.Y), FVector2D(C.X - H.X, C.Y + H.Y), South, WH, In, i * 4);
		WallWithOpenings(FVector2D(C.X + H.X, C.Y - H.Y), FVector2D(C.X + H.X, C.Y + H.Y), North, WH, In, i * 4 + 1);
		WallWithOpenings(FVector2D(C.X - H.X, C.Y - H.Y), FVector2D(C.X + H.X, C.Y - H.Y), West, WH, In, i * 4 + 2);
		WallWithOpenings(FVector2D(C.X - H.X, C.Y + H.Y), FVector2D(C.X + H.X, C.Y + H.Y), East, WH, In, i * 4 + 3);

		// Colonnes monumentales aux angles et le long des murs
		const float PillarH = R.bRound ? 1100.f : 720.f;
		for (int32 sx = -1; sx <= 1; sx += 2)
		{
			for (int32 sy = -1; sy <= 1; sy += 2)
			{
				Pillar(C + FVector(sx * (H.X - 140), sy * (H.Y - 140), 0), PillarH);
			}
		}
		if (R.bRound)
		{
			for (int32 k = 0; k < 10; ++k)
			{
				const float A = 2.f * PI * k / 10.f + 0.3f;
				Pillar(C + FVector(FMath::Cos(A) * 1320.f, FMath::Sin(A) * 1320.f, -70.f), PillarH);
			}
		}

		// Bannières à glyphe d'eau et cascades
		Banner(C + FVector(0, -H.Y + WallT * 0.5f + 6, 0) + FVector(H.X * 0.4f, 0, 0), 90.f);
		Banner(C + FVector(0, H.Y - WallT * 0.5f - 6, 0) + FVector(-H.X * 0.4f, 0, 0), -90.f);
		if (R.bPool || R.bRound || R.Number == 5 || R.Number == 8)
		{
			Waterfall(C + FVector(H.X * 0.2f, -H.Y + 10, WallH + 400), 230.f, WallH + 420.f, 90.f);
			Waterfall(C + FVector(-H.X * 0.3f, H.Y - 10, WallH + 380), 200.f, WallH + 400.f, -90.f);
		}
		// Végétation, ruines et nénuphars
		FRandomStream Rnd(i * 131 + 7);
		if (bDressing)
		{
			for (int32 sx = -1; sx <= 1; sx += 2)
			{
				for (int32 sy = -1; sy <= 1; sy += 2)
				{
					const FVector Corner = C + FVector(sx * (H.X - 150), sy * (H.Y - 150), 0);
					Put(TEXT("GrassPatch"), Corner + FVector(-sx * 90.f, -sy * 40.f, 0), Rnd.FRandRange(0.f, 360.f), Rnd.FRandRange(1.f, 1.5f));
					if (Rnd.FRand() < 0.4f) Put(TEXT("Rubble"), Corner + FVector(-sx * 60.f, -sy * 160.f, 0), Rnd.FRandRange(0.f, 360.f), Rnd.FRandRange(0.7f, 1.1f));
				}
			}
			if (!R.bRound && H.X >= 750.f)
			{
				const int32 sx = Rnd.FRand() < 0.5f ? -1 : 1;
				const int32 sy = Rnd.FRand() < 0.5f ? -1 : 1;
				Put(TEXT("ColumnBroken"), C + FVector(sx * (H.X - 380.f), sy * (H.Y - 330.f), 0), Rnd.FRandRange(0.f, 360.f), 1.f);
			}
			for (int32 k = 0; k < 10; ++k)
			{
				const FVector P = C + FVector(Rnd.FRandRange(-0.8f, 0.8f) * H.X, Rnd.FRandRange(-0.8f, 0.8f) * H.Y, 0);
				if (R.bPool && FMath::Abs(P.X - C.X) < H.X * 0.5f && FMath::Abs(P.Y - C.Y) < H.Y * 0.55f) continue;
				Put(TEXT("GrassTuft"), P, Rnd.FRandRange(0.f, 360.f), Rnd.FRandRange(0.6f, 1.1f));
			}
			if (R.bPool)
			{
				const FVector2D PH(H.X * 0.45f, H.Y * 0.5f);
				for (int32 k = 0; k < 5; ++k)
				{
					Put(TEXT("Lily"), C + FVector(Rnd.FRandRange(-0.8f, 0.8f) * PH.X, Rnd.FRandRange(-0.8f, 0.8f) * PH.Y, -11.f), Rnd.FRandRange(0.f, 360.f), Rnd.FRandRange(0.8f, 1.3f));
				}
			}
		}
		else
		{
			for (int32 k = 0; k < 6; ++k)
			{
				const float X = FMath::Sin(k * 12.9898f + i) * H.X * 0.8f;
				const float Y = FMath::Cos(k * 78.233f + i) * H.Y * 0.8f;
				Box(TEXT("Grass"), C + FVector(X, Y, 4), FVector(90, 70, 8), k * 37.f);
			}
		}
	}

	// Couloirs : dalles, murets et arches
	for (const FCorridor& Co : Corridors())
	{
		const FVector2D Mid = (Co.A + Co.B) * 0.5f;
		const bool bAlongX = FMath::IsNearlyEqual(Co.A.Y, Co.B.Y);
		const float Len = (Co.B - Co.A).Size() + 60.f;
		const FVector C(Mid.X, Mid.Y, 0.f);
		Box(TEXT("Floor"), C + FVector(0, 0, -25), bAlongX ? FVector(Len, CorridorW, 50) : FVector(CorridorW, Len, 50));
		// Eau de part et d'autre (chaussée sur l'eau, maquette d'exploration)
		Box(TEXT("Water"), C + FVector(0, 0, -45), bAlongX ? FVector(Len, CorridorW + 700, 1) : FVector(CorridorW + 700, Len, 1));
		Box(TEXT("Floor"), C + FVector(0, 0, -110), bAlongX ? FVector(Len, CorridorW + 700, 40) : FVector(CorridorW + 700, Len, 40));
		const float Side = CorridorW * 0.5f + 20.f;
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector Off = bAlongX ? FVector(0, s * Side, 0) : FVector(s * Side, 0, 0);
			Box(TEXT("Trim"), C + Off + FVector(0, 0, 30), bAlongX ? FVector(Len, 40, 60) : FVector(40, Len, 60));
			Box(TEXT("Block"), C + Off + FVector(0, 0, 150), bAlongX ? FVector(Len, 40, 300) : FVector(40, Len, 300));
		}
		// Murs latéraux lointains pour fermer l'espace
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector Off = bAlongX ? FVector(0, s * (CorridorW * 0.5f + 350.f), 0) : FVector(s * (CorridorW * 0.5f + 350.f), 0, 0);
			Box(TEXT("Wall"), C + Off + FVector(0, 0, WallH * 0.5f - 60.f), bAlongX ? FVector(Len, WallT, WallH + 120) : FVector(WallT, Len, WallH + 120));
		}
		Arch(C, bAlongX ? 0.f : 90.f, CorridorW, 520.f);
		if (!Co.Gate.IsNone()) Gate(Co.Gate, C + (bAlongX ? FVector(Len * 0.25f, 0, 0) : FVector(0, (Co.B.Y > Co.A.Y ? 1 : -1) * Len * 0.25f, 0)), bAlongX ? 0.f : 90.f, CorridorW);
	}

	// Îles flottantes et tours lointaines (silhouettes dans la brume)
	for (int32 k = 0; k < 14; ++k)
	{
		const float A = k * 2.399963f;
		const float Rd = 9000.f + (k % 5) * 2200.f;
		const FVector P(7000 + FMath::Cos(A) * Rd, FMath::Sin(A) * Rd, 1800.f + (k % 4) * 900.f);
		const float S = 500.f + (k % 3) * 350.f;
		Box(TEXT("Rock"), P, FVector(S, S * 0.8f, S * 0.45f), k * 23.f);
		Box(TEXT("Rock"), P - FVector(0, 0, S * 0.45f), FVector(S * 0.6f, S * 0.5f, S * 0.5f), k * 41.f);
		Box(TEXT("Grass"), P + FVector(0, 0, S * 0.23f), FVector(S * 0.95f, S * 0.75f, 20.f), k * 23.f);
	}
	for (int32 k = 0; k < 18; ++k)
	{
		const float A = k * 0.349f + 0.2f;
		const float Rd = 16000.f + (k % 3) * 3000.f;
		const FVector P(7000 + FMath::Cos(A) * Rd, FMath::Sin(A) * Rd, 0);
		const float Hgt = 3000.f + (k % 4) * 1400.f;
		Box(TEXT("Tower"), P + FVector(0, 0, Hgt * 0.5f - 600.f), FVector(700, 700, Hgt));
		Box(TEXT("Tower"), P + FVector(0, 0, Hgt - 400.f), FVector(420, 420, 900));
	}
	// Grand plan d'eau sous l'ensemble
	Box(TEXT("Water"), FVector(7000, 0, -140), FVector(60000, 60000, 1));
}

// ---------------------------------------------------------------------------------------------------------------------

void AZCistern::SpawnGameplay()
{
	UWorld* W = GetWorld();
	if (!W || Interactables.Num() > 0) return;
	UZGameInstance* GI = UZGameInstance::Get(this);

	auto Spawn = [&](EZInteractKind Kind, const FVector& Loc, float Yaw, FName Reward, const FString& Item, const FString& Label, const FString& Text) -> AZInteractable*
	{
		AZInteractable* I = W->SpawnActor<AZInteractable>(AZInteractable::StaticClass(), FTransform(FRotator(0, Yaw, 0), Loc));
		I->Setup(Kind, Reward, Item, Label, Text);
		Interactables.Add(I);
		return I;
	};
	Spawn(EZInteractKind::Sign, FVector(-600, 350, 0), 0.f, NAME_None, FString(), TEXT("Stèle"),
		TEXT("Citerne des mémoires. « L'eau se souvient de chaque pas. » Les ennemis visibles engagent le combat au contact ; frappe-les d'abord (F / clic) pour prendre l'initiative."));
	Spawn(EZInteractKind::Chest, FVector(550, -600, 0), 180.f, TEXT("chest.r01"), TEXT("POTION_RED"), TEXT("Coffre"), TEXT("3 potions rouges"));
	Spawn(EZInteractKind::Atelier, FVector(2700, -2750, 0), 90.f, TEXT("Q_WATER_01"), TEXT("OOT_010"), TEXT("Mécanicien zora"),
		TEXT("« Tu viens pour les vannes ? Prends cette tunique : elle occupe tête, torse et jambes, et divise par deux les dégâts d'eau. Essaie-la dans le menu Équipement (Tab). »"));
	Spawn(EZInteractKind::Valve, FVector(5450, 650, 0), -90.f, TEXT("valve.r04"), TEXT("OOT_023"), TEXT("Vanne"), TEXT("Le grappin s'accroche à la vanne : la grille de la chapelle se lève."));
	Spawn(EZInteractKind::Companion, FVector(5000, 2750, 0), 180.f, TEXT("join.mipha"), FString(), TEXT("Mipha"),
		TEXT("Mipha : « Link… c'est bien toi ? Cette citerne garde mes souvenirs. Je t'accompagne. » (Mipha rejoint l'équipe ; la Grande Fée prête son soin de groupe.)"));
	Spawn(EZInteractKind::Fountain, FVector(4500, 2900, 0), 0.f, TEXT("fountain.r05"), FString(), TEXT("Fontaine de la Grande Fée"), TEXT("Une douce lumière soigne l'équipe."));
	Spawn(EZInteractKind::Sign, FVector(7650, 2150, 0), 180.f, NAME_None, FString(), TEXT("Registre"), TEXT("« La goutte naît, la vague grandit, le courant s'en va. »"));
	for (int32 k = 0; k < 3; ++k)
	{
		static const TCHAR* Names[] = { TEXT("Symbole : Vague"), TEXT("Symbole : Goutte"), TEXT("Symbole : Courant") };
		AZInteractable* P = Spawn(EZInteractKind::Puzzle, FVector(7000 + k * 300, 2900, 0), 0.f, TEXT("puzzle.r06"), FString(), Names[k], FString());
		P->PuzzleIndex = k == 0 ? 1 : (k == 1 ? 0 : 2); // ordre attendu : Goutte (0), Vague (1), Courant (2)
	}
	Spawn(EZInteractKind::Chest, FVector(7850, 3000, 0), 180.f, TEXT("chest.r06"), TEXT("OOS/OOA_022"), TEXT("Coffre latéral"), TEXT("Anneau du nageur"));
	Spawn(EZInteractKind::Fountain, FVector(9950, 300, 0), 180.f, TEXT("fountain.r08"), TEXT("MM_022"), TEXT("Fontaine des mémoires"),
		TEXT("Tu te reposes. Au fond du bassin repose le Masque Zora : en combat, « Transformation » coûte 60 de Résonance et dure trois activations."));
	Spawn(EZInteractKind::SavePoint, FVector(11500, -350, 0), 90.f, NAME_None, FString(), TEXT("Cristal de sauvegarde"), TEXT("Partie sauvegardée."));
	Spawn(EZInteractKind::BossDoor, FVector(12150, 0, 0), 0.f, TEXT("door.boss"), FString(), TEXT("Sceau des trois sources"), TEXT("Le sceau réagit aux trois sources : la voie du gardien s'ouvre."));

	auto Enc = [&](FName Id, const TArray<FName>& Enemies, const FVector& Loc, float Radius)
	{
		if (GI && GI->HasFlag(FName(*(TEXT("done.") + Id.ToString())))) return;
		AZEncounterActor* E = W->SpawnActor<AZEncounterActor>(AZEncounterActor::StaticClass(), FTransform(FRotator(0, 180, 0), Loc));
		E->Setup(Id, Enemies, Radius);
		Encounters.Add(E);
	};
	Enc(TEXT("ENC_VESTIBULE"), { TEXT("octorok") }, FVector(450, 350, 0), 250.f);
	Enc(TEXT("ENC_BASSIN"), { TEXT("chuchu_water"), TEXT("chuchu_water") }, FVector(2700, 450, 0), 300.f);
	Enc(TEXT("ENC_CONDUITE"), { TEXT("bokoblin"), TEXT("keese") }, FVector(5100, -350, 0), 250.f);
	Enc(TEXT("ENC_ECLUSE"), { TEXT("sentinel_elec") }, FVector(7450, -300, 0), 150.f);
	Enc(TEXT("ENC_NEREIDE"), { TEXT("nereide") }, FVector(14700, 0, 0), 0.f);
	RefreshDoors();
}

void AZCistern::RefreshDoors()
{
	const UZGameInstance* GI = UZGameInstance::Get(this);
	auto Open = [&](FName Gate, bool bOpen)
	{
		if (UInstancedStaticMeshComponent** G = Gates.Find(Gate))
		{
			(*G)->SetVisibility(!bOpen);
			(*G)->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		}
	};
	Open(TEXT("G_VALVE"), GI && GI->HasFlag(TEXT("valve.r04")));
	Open(TEXT("G_PUZZLE"), GI && GI->HasFlag(TEXT("puzzle.r06")));
	Open(TEXT("G_BOSS"), GI && GI->HasFlag(TEXT("door.boss")));
	for (AZInteractable* I : Interactables) { if (I) I->RefreshState(); }
}

// ---------------------------------------------------------------------------------------------------------------------

AZInteractable::AZInteractable()
{
	PrimaryActorTick.bCanEverTick = true;
	Zone = CreateDefaultSubobject<USphereComponent>(TEXT("Zone"));
	Zone->InitSphereRadius(220.f);
	Zone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Zone->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SetRootComponent(Zone);
	MeshA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshA"));
	MeshA->SetupAttachment(Zone);
	MeshA->SetCollisionProfileName(TEXT("BlockAll"));
	MeshA->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	MeshB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshB"));
	MeshB->SetupAttachment(Zone);
	MeshB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Zone);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(600.f);
	Light->SetCastShadows(false);
	FigureRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FigureRoot"));
	FigureRoot->SetupAttachment(Zone);
	Figure = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Figure"));
	Figure->SetupAttachment(FigureRoot);
	Figure->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->SetAnimationMode(EAnimationMode::AnimationSingleNode);
}

void AZInteractable::Setup(EZInteractKind InKind, FName InReward, const FString& InItem, const FString& InLabel, const FString& InText)
{
	Kind = InKind; RewardId = InReward; ItemId = InItem; Label = InLabel; Text = InText;
	const FString Art = TEXT("/Game/Art/Meshes/");
	// Modèle d'art s'il existe (MeshA = objet, MeshB = pièce animée éventuelle), sinon formes de base
	auto UseProp = [this](const TCHAR* NameA, const TCHAR* NameB) -> bool
	{
		UStaticMesh* A = PropMesh(NameA);
		if (!A) return false;
		MeshA->SetStaticMesh(A);
		MeshA->SetRelativeScale3D(FVector::OneVector);
		MeshA->SetRelativeLocation(FVector::ZeroVector);
		MeshA->SetRelativeRotation(FRotator::ZeroRotator);
		PropMaterials(MeshA);
		UStaticMesh* B = NameB ? PropMesh(NameB) : nullptr;
		MeshB->SetStaticMesh(B);
		MeshB->SetVisibility(B != nullptr);
		if (B)
		{
			MeshB->SetRelativeScale3D(FVector::OneVector);
			MeshB->SetRelativeRotation(FRotator::ZeroRotator);
			PropMaterials(MeshB);
		}
		bArt = true;
		return true;
	};
	switch (Kind)
	{
	case EZInteractKind::Chest:
		MeshA->SetStaticMesh(ZVis::Mesh(Art + TEXT("SM_Chest")) ? ZVis::Mesh(Art + TEXT("SM_Chest")) : ZVis::Mesh(TEXT("Cube")));
		if (!ZVis::Mesh(Art + TEXT("SM_Chest"))) { MeshA->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.55f)); MeshA->SetRelativeLocation(FVector(0, 0, 28)); }
		ZVis::Tint(MeshA, FLinearColor(0.42f, 0.26f, 0.12f));
		MeshB->SetStaticMesh(ZVis::Mesh(TEXT("Cube")));
		MeshB->SetRelativeScale3D(FVector(0.95f, 0.65f, 0.12f));
		MeshB->SetRelativeLocation(FVector(0, 0, 62));
		ZVis::Tint(MeshB, FLinearColor(0.95f, 0.72f, 0.25f), 0.4f);
		MeshB->SetVisibility(ZVis::Mesh(Art + TEXT("SM_Chest")) == nullptr); // le coffre modélisé a déjà son couvercle
		Light->SetLightColor(FLinearColor(1.f, 0.8f, 0.4f));
		Light->SetIntensity(1200.f);
		Light->SetRelativeLocation(FVector(0, 0, 90));
		break;
	case EZInteractKind::SavePoint:
		if (UseProp(TEXT("SM_Prop_SavePlinth"), TEXT("SM_Prop_SaveGem")))
		{
			MeshB->SetRelativeLocation(FVector(0, 0, 150));
		}
		else
		{
			MeshA->SetStaticMesh(ZVis::Mesh(TEXT("Cone")));
			MeshA->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.4f));
			MeshA->SetRelativeLocation(FVector(0, 0, 130));
			MeshA->SetRelativeRotation(FRotator(180, 0, 0));
			ZVis::Tint(MeshA, FLinearColor(0.35f, 0.9f, 1.f), 6.f);
			MeshB->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
			MeshB->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.15f));
			MeshB->SetRelativeLocation(FVector(0, 0, 8));
			ZVis::Tint(MeshB, FLinearColor(0.7f, 0.68f, 0.6f));
		}
		Light->SetLightColor(FLinearColor(0.35f, 0.9f, 1.f));
		Light->SetIntensity(6000.f);
		Light->SetRelativeLocation(FVector(0, 0, 150));
		break;
	case EZInteractKind::Fountain:
		if (!UseProp(TEXT("SM_Prop_Fountain"), nullptr))
		{
			MeshA->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
			MeshA->SetRelativeScale3D(FVector(3.f, 3.f, 0.5f));
			MeshA->SetRelativeLocation(FVector(0, 0, 25));
			ZVis::Tint(MeshA, FLinearColor(0.72f, 0.68f, 0.58f));
			MeshB->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
			MeshB->SetRelativeScale3D(FVector(2.6f, 2.6f, 0.05f));
			MeshB->SetRelativeLocation(FVector(0, 0, 50));
			ZVis::Tint(MeshB, FLinearColor(0.2f, 0.85f, 0.95f), 3.f);
		}
		Light->SetLightColor(FLinearColor(0.6f, 0.9f, 1.f));
		Light->SetIntensity(5000.f);
		Light->SetRelativeLocation(FVector(0, 0, 120));
		Zone->SetSphereRadius(260.f);
		break;
	case EZInteractKind::Valve:
		if (!UseProp(TEXT("SM_Prop_Valve"), nullptr))
		{
			MeshA->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
			MeshA->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.2f));
			MeshA->SetRelativeLocation(FVector(0, 0, 160));
			MeshA->SetRelativeRotation(FRotator(90, 0, 0));
			ZVis::Tint(MeshA, FLinearColor(0.55f, 0.45f, 0.25f), 0.2f);
			MeshB->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
			MeshB->SetRelativeScale3D(FVector(0.25f, 0.25f, 1.6f));
			MeshB->SetRelativeLocation(FVector(0, 0, 80));
			ZVis::Tint(MeshB, FLinearColor(0.35f, 0.35f, 0.38f));
		}
		break;
	case EZInteractKind::BossDoor:
		if (!UseProp(TEXT("SM_Prop_Seal"), nullptr))
		{
			MeshA->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
			MeshA->SetRelativeScale3D(FVector(3.2f, 3.2f, 0.3f));
			MeshA->SetRelativeRotation(FRotator(90, 0, 0));
			MeshA->SetRelativeLocation(FVector(40, 0, 200));
			ZVis::Tint(MeshA, FLinearColor(0.5f, 0.52f, 0.5f));
			MeshB->SetStaticMesh(ZVis::Mesh(TEXT("Sphere")));
			MeshB->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.6f));
			MeshB->SetRelativeLocation(FVector(20, 0, 200));
			ZVis::Tint(MeshB, FLinearColor(0.2f, 1.f, 0.95f), 10.f);
		}
		MeshA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Light->SetLightColor(FLinearColor(0.3f, 1.f, 0.95f));
		Light->SetIntensity(4000.f);
		Light->SetRelativeLocation(FVector(-60, 0, 200));
		Zone->SetSphereRadius(320.f);
		break;
	case EZInteractKind::Puzzle:
		if (!UseProp(TEXT("SM_Prop_Pedestal"), nullptr))
		{
			MeshA->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
			MeshA->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.1f));
			MeshA->SetRelativeLocation(FVector(0, 0, 55));
			ZVis::Tint(MeshA, FLinearColor(0.62f, 0.6f, 0.52f));
		}
		// Orbe du symbole (s'illumine quand l'énigme est résolue)
		MeshB->SetStaticMesh(ZVis::Mesh(TEXT("Sphere")));
		MeshB->SetVisibility(true);
		MeshB->SetRelativeScale3D(FVector(bArt ? 0.28f : 0.35f));
		MeshB->SetRelativeLocation(FVector(0, 0, bArt ? 128 : 135));
		ZVis::Tint(MeshB, FLinearColor(0.2f, 0.4f, 0.45f), 0.5f);
		Zone->SetSphereRadius(160.f);
		break;
	case EZInteractKind::Atelier:
	case EZInteractKind::Companion:
	{
		const bool bMipha = Kind == EZInteractKind::Companion;
		// Personnage animé ; un cylindre invisible garde la collision
		MeshA->SetStaticMesh(ZVis::Mesh(TEXT("Cylinder")));
		MeshA->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.6f));
		MeshA->SetRelativeLocation(FVector(0, 0, 80));
		MeshB->SetVisibility(false);
		const FZLook L = ZVis::CharacterLook(bMipha ? FName(TEXT("mipha")) : FName(TEXT("zora_npc")), NAME_None, FString(), FString(), FString());
		ZVis::Build(this, FigureRoot, Figure, L, FigureParts);
		if (Figure->GetSkeletalMeshAsset())
		{
			MeshA->SetVisibility(false);
			if (UAnimSequence* Idle = ZVis::Anim(TEXT("Idle"))) Figure->PlayAnimation(Idle, true);
		}
		else
		{
			ZVis::Tint(MeshA, bMipha ? FLinearColor(0.75f, 0.15f, 0.18f) : FLinearColor(0.35f, 0.55f, 0.7f), bMipha ? 0.3f : 0.6f);
		}
		Light->SetLightColor(FLinearColor(0.6f, 0.85f, 1.f));
		Light->SetIntensity(1500.f);
		Light->SetRelativeLocation(FVector(0, 0, 220));
		break;
	}
	case EZInteractKind::Sign:
	default:
		if (!UseProp(TEXT("SM_Prop_Stele"), nullptr))
		{
			MeshA->SetStaticMesh(ZVis::Mesh(TEXT("Cube")));
			MeshA->SetRelativeScale3D(FVector(0.25f, 1.1f, 1.4f));
			MeshA->SetRelativeLocation(FVector(0, 0, 70));
			ZVis::Tint(MeshA, FLinearColor(0.58f, 0.56f, 0.5f));
			MeshB->SetStaticMesh(ZVis::Mesh(TEXT("Cube")));
			MeshB->SetRelativeScale3D(FVector(0.05f, 0.5f, 0.5f));
			MeshB->SetRelativeLocation(FVector(14, 0, 95));
			ZVis::Tint(MeshB, FLinearColor(0.25f, 1.f, 0.92f), 5.f);
		}
		break;
	}
	RefreshState();
}

void AZInteractable::RefreshState()
{
	const UZGameInstance* GI = UZGameInstance::Get(this);
	bUsed = GI && !RewardId.IsNone() && (GI->ClaimedRewards.Contains(RewardId) || GI->HasFlag(RewardId));
	if (Kind == EZInteractKind::Chest && bUsed)
	{
		MeshB->SetRelativeRotation(FRotator(0, 0, -70));
		MeshB->SetRelativeLocation(FVector(0, -25, 80));
		Light->SetIntensity(0.f);
		ZVis::Tint(MeshA, FLinearColor(0.3f, 0.2f, 0.12f)); // coffre ouvert : assombri
	}
	if (Kind == EZInteractKind::Companion)
	{
		const bool bJoined = GI && GI->FindMember(TEXT("mipha")) && bUsed;
		SetActorHiddenInGame(bJoined);
		SetActorEnableCollision(!bJoined);
	}
	if (Kind == EZInteractKind::BossDoor)
	{
		MeshA->SetVisibility(!bUsed);
		if (bArt) MeshB->SetVisibility(false);
		Light->SetIntensity(bUsed ? 0.f : 4000.f);
	}
	if (Kind == EZInteractKind::Puzzle)
	{
		const bool bSolved = GI && GI->HasFlag(TEXT("puzzle.r06"));
		if (bSolved) ZVis::Tint(MeshB, FLinearColor(0.25f, 1.f, 0.95f), 8.f);
	}
}

FString AZInteractable::PromptText() const
{
	switch (Kind)
	{
	case EZInteractKind::Chest: return bUsed ? FString() : TEXT("Ouvrir le coffre");
	case EZInteractKind::SavePoint: return TEXT("Sauvegarder");
	case EZInteractKind::Fountain: return TEXT("Se reposer");
	case EZInteractKind::Atelier: return TEXT("Parler au mécanicien");
	case EZInteractKind::Valve: return bUsed ? FString() : TEXT("Grappin sur la vanne");
	case EZInteractKind::BossDoor: return bUsed ? FString() : TEXT("Toucher le sceau");
	case EZInteractKind::Puzzle: return TEXT("Activer : ") + Label.Mid(10);
	case EZInteractKind::Companion: return TEXT("Parler à Mipha");
	default: return TEXT("Lire");
	}
}

void AZInteractable::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	if (Kind == EZInteractKind::SavePoint)
	{
		UStaticMeshComponent* Gem = bArt ? MeshB : MeshA;
		Gem->SetRelativeRotation(FRotator(bArt ? 0.f : 180.f, Time * 40.f, 0));
		Gem->SetRelativeLocation(FVector(0, 0, (bArt ? 150.f : 130.f) + FMath::Sin(Time * 2.f) * 10.f));
	}
	if (Kind == EZInteractKind::BossDoor && !bUsed && !bArt)
	{
		MeshB->SetRelativeScale3D(FVector(0.4f + 0.05f * FMath::Sin(Time * 3.f)));
	}
}

// ---------------------------------------------------------------------------------------------------------------------

AZEncounterActor::AZEncounterActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Zone = CreateDefaultSubobject<USphereComponent>(TEXT("Zone"));
	Zone->InitSphereRadius(170.f);
	Zone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Zone->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SetRootComponent(Zone);
	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Zone);
}

void AZEncounterActor::Setup(FName InEncounter, const TArray<FName>& InEnemies, float WanderRadius)
{
	EncounterId = InEncounter;
	Enemies = InEnemies;
	Radius = WanderRadius;
	HomeLoc = GetActorLocation();
	WanderGoal = HomeLoc;
	if (Enemies.Num() > 0)
	{
		if (const FZEnemyDef* D = UZGameData::Get().FindEnemy(Enemies[0]))
		{
			const float Scale = D->Boss ? D->Scale : D->Scale * 0.85f;
			FZLook Look = ZVis::EnemyLook(D->Mesh, ZNames::HexColor(D->Color, FLinearColor::Gray), Scale);
			ZVis::Build(this, Visual, nullptr, Look, Parts);
			if (D->Boss) Zone->SetSphereRadius(900.f);
		}
	}
}

void AZEncounterActor::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	if (Cooldown > 0.f) Cooldown -= Dt;
	if (Radius <= 0.f) return;
	FVector P = GetActorLocation();
	if (FVector::Dist2D(P, WanderGoal) < 30.f)
	{
		const float A = FMath::FRandRange(0.f, 2.f * PI);
		WanderGoal = HomeLoc + FVector(FMath::Cos(A), FMath::Sin(A), 0) * FMath::FRandRange(0.f, Radius);
	}
	const FVector Dir = (WanderGoal - P).GetSafeNormal2D();
	SetActorLocation(P + Dir * 90.f * Dt);
	if (!Dir.IsNearlyZero()) SetActorRotation(FMath::RInterpTo(GetActorRotation(), Dir.Rotation(), Dt, 3.f));
	Visual->SetRelativeLocation(FVector(0, 0, FMath::Abs(FMath::Sin(Time * 5.f)) * 12.f));
}
