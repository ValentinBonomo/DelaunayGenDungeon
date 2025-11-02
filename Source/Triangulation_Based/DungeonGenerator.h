#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Room.h"
#include "DungeonGenerator.generated.h"

struct FDGEdge
{
	int32 A = INDEX_NONE;
	int32 B = INDEX_NONE;

	FDGEdge() = default;
	FDGEdge(int32 InA, int32 InB)
	{
		A = FMath::Min(InA, InB);
		B = FMath::Max(InA, InB);
	}
	bool operator==(const FDGEdge& Other) const { return A == Other.A && B == Other.B; }
	friend uint32 GetTypeHash(const FDGEdge& E) { return HashCombine(::GetTypeHash(E.A), ::GetTypeHash(E.B)); }
};

struct FDGTriangle
{
	int32 I = INDEX_NONE, J = INDEX_NONE, K = INDEX_NONE;
	FDGTriangle() = default;
	FDGTriangle(int32 InI, int32 InJ, int32 InK) : I(InI), J(InJ), K(InK) {}
};

struct FCorridorSeg
{
	FVector2D A, B;
	FCorridorSeg() {};
	FCorridorSeg(const FVector2D& InA, const FVector2D& InB) : A(InA), B(InB) {};
};

UCLASS()
class TRIANGULATION_BASED_API ADungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	ADungeonGenerator();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ================= Génération des rooms =================
	void GenerateDungeon();
	FVector2D RandomPointInDisk(float Radius, FRandomStream& Rng) const;

	// ================= Relaxation Rooms =================
	struct FRoomRef
	{
		ARoom* Actor = nullptr;
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D Half   = FVector2D::ZeroVector;
		float Area() const { return 4.f * Half.X * Half.Y; }
		bool IsValid() const { return ::IsValid(Actor); }
	};

	void BuildRoomRefs(TArray<FRoomRef>& Out) const;
	static bool Overlap(const FRoomRef& A, const FRoomRef& B, float Padding);
	static FVector2D MTV(const FRoomRef& A, const FRoomRef& B);
	int32 RelaxOnce(TArray<FRoomRef>& Refs);
	void ApplyRefs(const TArray<FRoomRef>& Refs);

	// ================= Culling =================
	void StartDelayedCulling();
	void DoFinalCulling();
	void CullResidualOverlaps(TArray<FRoomRef>& Refs);

	// ================= Main Rooms =================
	static bool TooCloseAABB(const ARoom* A, const ARoom* B, float ExtraGap);
	void RelaxMainRoomsPositions(const TArray<ARoom*>& Mains);

	// Main rooms
	void CollectAndStoreMainCenters();
	void DrawMainCenters() const;

	// ================= Delaunay & Prim =================
	bool PointInCircumcircle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C) const;
	void BuildDelaunay();
	void EdgesFromTriangles();
	void BuildMST_Prim();
	void DrawDebugViz();

	// ================= Corridors =================
	void BuildCorridorsFromMST();
	void DrawCorridorsDebug() const;
	void KeepMainAndCorridorRooms();
	void SpawnCorridorMeshes();
	static bool SegmentIntersectsAABB2D(const FVector2D& P0, const FVector2D& P1, const FVector2D& Center, const FVector2D& Half);

public:
	UFUNCTION(BlueprintCallable, Category="MainRooms")
	void SelectMainRooms();
	UFUNCTION(BlueprintPure, Category="MainRooms")
	void GetMainRoomCenters(TArray<FVector>& OutCenters) const { OutCenters = MainCenters; }

private:
	void RefreshMainRoomMaterials();

public:
	// Rooms
	UPROPERTY(EditAnywhere, Category="Rooms") int32    RoomsNbr = 32;
	UPROPERTY(EditAnywhere, Category="Rooms") FVector2D RoomSizeMin = FVector2D(250, 250);
	UPROPERTY(EditAnywhere, Category="Rooms") FVector2D RoomSizeMax = FVector2D(950, 950);
	UPROPERTY(EditAnywhere, Category="Rooms") TSubclassOf<ARoom> RoomClass;

	UPROPERTY(EditAnywhere, Category="Generation") float SpawnRadius = 1600.f;

	// Relax
	UPROPERTY(EditAnywhere, Category="Relax") int32 MaxRelaxIterations = 80;
	UPROPERTY(EditAnywhere, Category="Relax") float NudgeClamp = 100.f;
	UPROPERTY(EditAnywhere, Category="Relax") float ContactPadding = 2.f;

	// Culling
	UPROPERTY(EditAnywhere, Category="Culling") bool  bEnableCulling = true;
	UPROPERTY(EditAnywhere, Category="Culling") float CullingDelaySeconds = 2.0f;
	UPROPERTY(EditAnywhere, Category="Culling") float CullPenetrationThreshold = 60.f;
	UPROPERTY(EditAnywhere, Category="Culling") int32 MaxCulls = 2;

	// Main Rooms
	UPROPERTY(EditAnywhere, Category="MainRooms") int32 MainCount = 7;
	UPROPERTY(EditAnywhere, Category="MainRooms") float MinMainGap = 120.f;
	UPROPERTY(EditAnywhere, Category="MainRooms") TObjectPtr<UMaterialInterface> MainRoomMaterial = nullptr;

	// Debug Centres
	UPROPERTY(EditAnywhere, Category="MainRooms|Debug") bool  bDrawMainCenters = true;
	UPROPERTY(EditAnywhere, Category="MainRooms|Debug") float MainCenterZOffset = 80.f;
	UPROPERTY(EditAnywhere, Category="MainRooms|Debug") float MainCenterPointSize = 50.f;
	UPROPERTY(EditAnywhere, Category="MainRooms|Debug") FVector CenterMarkerHalfExtent = FVector(120,120,120);
	UPROPERTY(EditAnywhere, Category="MainRooms|Debug") float CenterPillarHeight = 400.f;
	UPROPERTY(EditAnywhere, Category="MainRooms|Debug") bool  bDrawCenterLabel = true;

	// Debug Delaunay/MST
	UPROPERTY(EditAnywhere, Category="Graph|Debug") bool  bDrawDelaunay = true;
	UPROPERTY(EditAnywhere, Category="Graph|Debug") float DebugDuration = 40.f;
	UPROPERTY(EditAnywhere, Category="Graph|Debug") float DelaunayZDebugOffset = 1200.f;
	UPROPERTY(EditAnywhere, Category="Graph|Debug") float MSTZDebugOffset      = 1250.f;

	// Corridors debug
	UPROPERTY(EditAnywhere, Category="Corridors") bool  bBuildCorridors = true;
	UPROPERTY(EditAnywhere, Category="Corridors") bool  bKeepOnlyMainAndPath = true;
	UPROPERTY(EditAnywhere, Category="Corridors") float CorridorKeepDistance = 150.f; 
	UPROPERTY(EditAnywhere, Category="Corridors|Debug") float CorridorZOffset = 260.f;
	UPROPERTY(EditAnywhere, Category="Corridors|Debug") float CorridorThickness = 16.f;
	UPROPERTY(EditAnywhere, Category="Corridors")
	bool bCorridorFollowMSTExact = false;
	UPROPERTY(EditAnywhere, Category="Corridors|Mesh") TObjectPtr<class UStaticMesh> CorridorMesh = nullptr;
	UPROPERTY(EditAnywhere, Category="Corridors|Mesh") TObjectPtr<class UMaterialInterface> CorridorMaterial = nullptr;
	UPROPERTY(EditAnywhere, Category="Corridors|Mesh") float CorridorWidth  = 250.f;
	UPROPERTY(EditAnywhere, Category="Corridors|Mesh") float CorridorHeight = 150.f;

private:
	TArray<TObjectPtr<ARoom>> SpawnedRooms;
	TArray<FVector>   MainCenters;
	TArray<FVector2D> Points2D;
	TArray<FDGTriangle> DelaunayTriangles;
	TSet<FDGEdge>       GraphEdges;
	TArray<FDGEdge>     MSTEdges;
	TArray<FCorridorSeg> CorridorSegments;
	UPROPERTY(Transient) TObjectPtr<class UInstancedStaticMeshComponent> CorridorISM;
	FVector DungeonCenter = FVector::ZeroVector;
	FTimerHandle CullingTimerHandle;
};
