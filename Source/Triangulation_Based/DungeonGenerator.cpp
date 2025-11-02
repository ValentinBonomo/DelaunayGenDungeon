#include "DungeonGenerator.h"
#include "DrawDebugHelpers.h"
#include "Algo/Sort.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include <cfloat>

ADungeonGenerator::ADungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CorridorISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CorridorISM"));
	CorridorISM->SetupAttachment(RootComponent);
	CorridorISM->SetMobility(EComponentMobility::Movable);
}

void ADungeonGenerator::BeginPlay()
{
	Super::BeginPlay();

	DungeonCenter = GetActorLocation();

	CorridorSegments.Reset();
	if (CorridorISM) CorridorISM->ClearInstances();

	for (ARoom* R : SpawnedRooms)
		if (IsValid(R)) R->Destroy();
	SpawnedRooms.Reset();
	
	GenerateDungeon();
	
	TArray<FRoomRef> Refs;
	BuildRoomRefs(Refs);
	for (int32 it = 0; it < MaxRelaxIterations; ++it)
	{
		const int32 overlaps = RelaxOnce(Refs);
		ApplyRefs(Refs);
		if (overlaps == 0) break;
	}

	if (bEnableCulling) StartDelayedCulling();
	else SelectMainRooms();
}

void ADungeonGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CullingTimerHandle);

	if (CorridorISM) CorridorISM->ClearInstances();

	for (ARoom* R : SpawnedRooms)
		if (IsValid(R)) R->Destroy();

	SpawnedRooms.Reset();
	MainCenters.Reset();
	Points2D.Reset();
	CorridorSegments.Reset();

	Super::EndPlay(EndPlayReason);
}

FVector2D ADungeonGenerator::RandomPointInDisk(float Radius, FRandomStream& Rng) const
{
	const float Angle = Rng.FRandRange(0.f, 2.f * PI);
	const float r = Radius * FMath::Sqrt(Rng.FRand());
	return FVector2D(r * FMath::Cos(Angle), r * FMath::Sin(Angle));
}

void ADungeonGenerator::GenerateDungeon()
{
	UWorld* W = GetWorld(); if (!W) return;

	const uint64 Ticks = FDateTime::Now().GetTicks();
	int32 SeedG = static_cast<int32>(Ticks ^ (Ticks >> 32));
	FRandomStream Rng(SeedG == 0 ? 1 : SeedG);

	UClass* ClassToSpawn = RoomClass ? RoomClass.Get() : ARoom::StaticClass();

	for (int32 i = 0; i < RoomsNbr; ++i)
	{
		const FVector2D Off2D = RandomPointInDisk(SpawnRadius, Rng);
		const FVector SpawnLoc = DungeonCenter + FVector(Off2D.X, Off2D.Y, 0.f);

		const float SX = Rng.FRandRange(RoomSizeMin.X, RoomSizeMax.X);
		const float SY = Rng.FRandRange(RoomSizeMin.Y, RoomSizeMax.Y);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ARoom* Room = W->SpawnActor<ARoom>(ClassToSpawn, SpawnLoc, FRotator::ZeroRotator, Params);
		if (IsValid(Room))
		{
			Room->SizeXY = FVector2D(SX, SY);
			Room->Thickness = 2000.f;
			Room->SyncVisual();
			SpawnedRooms.Add(Room);
		}
	}
}

void ADungeonGenerator::BuildRoomRefs(TArray<FRoomRef>& Out) const
{
	Out.Reset();
	Out.Reserve(SpawnedRooms.Num());
	for (ARoom* A : SpawnedRooms)
	{
		if (!IsValid(A)) continue;
		const FVector L = A->GetActorLocation();
		FRoomRef R;
		R.Actor  = A;
		R.Center = FVector2D(L.X, L.Y);
		R.Half   = A->SizeXY * 0.5f;
		Out.Add(R);
	}
}

bool ADungeonGenerator::Overlap(const FRoomRef& A, const FRoomRef& B, float Padding)
{
	const FVector2D d = (A.Center - B.Center).GetAbs();
	return (d.X < (A.Half.X + B.Half.X - Padding)) && (d.Y < (A.Half.Y + B.Half.Y - Padding));
}

FVector2D ADungeonGenerator::MTV(const FRoomRef& A, const FRoomRef& B)
{
	const FVector2D d = B.Center - A.Center;
	const float ox = (A.Half.X + B.Half.X) - FMath::Abs(d.X);
	const float oy = (A.Half.Y + B.Half.Y) - FMath::Abs(d.Y);
	if (ox < oy)
	{
		const float sx = (d.X >= 0.f) ? 1.f : -1.f;
		return FVector2D(ox * sx, 0.f);
	}
	else
	{
		const float sy = (d.Y >= 0.f) ? 1.f : -1.f;
		return FVector2D(0.f, oy * sy);
	}
}

int32 ADungeonGenerator::RelaxOnce(TArray<FRoomRef>& Refs)
{
	int32 overlaps = 0;
	for (int32 i = 0; i < Refs.Num(); ++i)
	{
		if (!Refs[i].IsValid()) continue;
		for (int32 j = i + 1; j < Refs.Num(); ++j)
		{
			if (!Refs[j].IsValid()) continue;

			if (Overlap(Refs[i], Refs[j], ContactPadding))
			{
				overlaps++;
				FVector2D mtv = MTV(Refs[i], Refs[j]);
				mtv.X = FMath::Clamp(mtv.X, -NudgeClamp, NudgeClamp);
				mtv.Y = FMath::Clamp(mtv.Y, -NudgeClamp, NudgeClamp);
				Refs[i].Center -= mtv * 0.5f;
				Refs[j].Center += mtv * 0.5f;
			}
		}
	}
	return overlaps;
}

void ADungeonGenerator::ApplyRefs(const TArray<FRoomRef>& Refs)
{
	for (const FRoomRef& R : Refs)
	{
		if (!R.IsValid()) continue;
		R.Actor->SetActorLocation(FVector(R.Center.X, R.Center.Y, DungeonCenter.Z));
	}
}

void ADungeonGenerator::StartDelayedCulling()
{
	if (!GetWorld()) return;
	GetWorld()->GetTimerManager().SetTimer(
		CullingTimerHandle, this, &ADungeonGenerator::DoFinalCulling, CullingDelaySeconds, false);
}

void ADungeonGenerator::DoFinalCulling()
{
	TArray<FRoomRef> Refs;
	BuildRoomRefs(Refs);

	for (int32 it = 0; it < 10; ++it)
	{
		const int32 overlaps = RelaxOnce(Refs);
		if (overlaps == 0) break;
	}
	CullResidualOverlaps(Refs);
	ApplyRefs(Refs);

	SelectMainRooms();
}

void ADungeonGenerator::CullResidualOverlaps(TArray<FRoomRef>& Refs)
{
	if (!bEnableCulling || MaxCulls <= 0) return;

	int32 culls = 0;
	bool changed = true;

	while (changed && culls < MaxCulls)
	{
		changed = false;
		for (int32 i = 0; i < Refs.Num(); ++i)
		{
			if (!Refs[i].IsValid()) continue;

			for (int32 j = i + 1; j < Refs.Num(); ++j)
			{
				if (!Refs[j].IsValid()) continue;

				if (Overlap(Refs[i], Refs[j], ContactPadding))
				{
					const FVector2D mtv = MTV(Refs[i], Refs[j]);
					const float pen = FMath::Max(FMath::Abs(mtv.X), FMath::Abs(mtv.Y));

					if (pen > CullPenetrationThreshold)
					{
						const int32 kill = (Refs[i].Area() <= Refs[j].Area()) ? i : j;
						if (IsValid(Refs[kill].Actor)) Refs[kill].Actor->Destroy();
						Refs[kill].Actor = nullptr;
						Refs.RemoveAll([](const FRoomRef& R){ return !R.IsValid(); });
						++culls; changed = true; break;
					}
				}
			}
			if (changed) break;
		}
	}
}

bool ADungeonGenerator::TooCloseAABB(const ARoom* A, const ARoom* B, float ExtraGap)
{
	if (!IsValid(A) || !IsValid(B)) return false;

	const FVector2D CA(A->GetActorLocation().X, A->GetActorLocation().Y);
	const FVector2D CB(B->GetActorLocation().X, B->GetActorLocation().Y);

	const FVector2D HA = A->SizeXY * 0.5f + FVector2D(ExtraGap, ExtraGap);
	const FVector2D HB = B->SizeXY * 0.5f + FVector2D(ExtraGap, ExtraGap);

	const FVector2D d = (CA - CB).GetAbs();
	return (d.X < (HA.X + HB.X)) && (d.Y < (HA.Y + HB.Y));
}

void ADungeonGenerator::RelaxMainRoomsPositions(const TArray<ARoom*>& Mains)
{
	for (int32 it = 0; it < 10; ++it)
	{
		bool moved = false;
		for (int32 i = 0; i < Mains.Num(); ++i)
		{
			for (int32 j = i + 1; j < Mains.Num(); ++j)
			{
				if (!TooCloseAABB(Mains[i], Mains[j], MinMainGap)) continue;

				FVector2D CA(Mains[i]->GetActorLocation().X, Mains[i]->GetActorLocation().Y);
				FVector2D CB(Mains[j]->GetActorLocation().X, Mains[j]->GetActorLocation().Y);
				FVector2D push = (CB - CA).GetSafeNormal() * 40.f;

				Mains[i]->AddActorWorldOffset(FVector(-push.X, -push.Y, 0));
				Mains[j]->AddActorWorldOffset(FVector( push.X,  push.Y, 0));
				moved = true;
			}
		}
		if (!moved) break;
	}
}

void ADungeonGenerator::CollectAndStoreMainCenters()
{
	MainCenters.Reset();
	Points2D.Reset();

	for (ARoom* R : SpawnedRooms)
	{
		if (!IsValid(R) || !R->bIsMain) continue;

		FVector Origin, Extent;
		R->GetActorBounds(false, Origin, Extent);

		FVector C = Origin;
		C.Z = Origin.Z + Extent.Z + MainCenterZOffset;

		MainCenters.Add(C);
		Points2D.Add(FVector2D(Origin.X, Origin.Y));
	}
}

void ADungeonGenerator::DrawMainCenters() const
{
	if (!bDrawMainCenters) return;
	UWorld* W = GetWorld(); if (!W) return;

	for (int32 i = 0; i < MainCenters.Num(); ++i)
	{
		const FVector P = MainCenters[i];

		DrawDebugSolidBox(W, P, CenterMarkerHalfExtent, FColor::Cyan, true, DebugDuration);
		DrawDebugLine(W, P - FVector(0,0,CenterPillarHeight), P, FColor::Cyan, true, DebugDuration, 0, 8.f);

		if (bDrawCenterLabel)
		{
			DrawDebugString(W, P + FVector(0,0,CenterMarkerHalfExtent.Z + 20.f),
				FString::Printf(TEXT("#%d"), i), nullptr, FColor::Cyan, DebugDuration, true, 1.6f);
		}
	}
}
static void CircumCenterRadius(const FVector2D& A, const FVector2D& B, const FVector2D& C, FVector2D& O, float& R2)
{
	const float D = 2.f*(A.X*(B.Y-C.Y)+B.X*(C.Y-A.Y)+C.X*(A.Y-B.Y));
	if (FMath::IsNearlyZero(D)) { O = FVector2D(FLT_MAX, FLT_MAX); R2 = FLT_MAX; return; }

	const float A2 = A.SizeSquared(), B2 = B.SizeSquared(), C2 = C.SizeSquared();
	O.X = (A2*(B.Y-C.Y) + B2*(C.Y-A.Y) + C2*(A.Y-B.Y)) / D;
	O.Y = (A2*(C.X-B.X) + B2*(A.X-C.X) + C2*(B.X-A.X)) / D;
	R2  = (O - A).SizeSquared();
}

bool ADungeonGenerator::PointInCircumcircle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C) const
{
	FVector2D O; float R2; CircumCenterRadius(A,B,C,O,R2);
	if (R2 == FLT_MAX) return false;
	return (P - O).SizeSquared() <= R2;
}

void ADungeonGenerator::BuildDelaunay()
{
	DelaunayTriangles.Reset();
	if (Points2D.Num() < 3) return;

	FBox2D BB(Points2D[0], Points2D[0]); for (const auto& P : Points2D) BB += P;
	const float delta = FMath::Max(BB.Max.X - BB.Min.X, BB.Max.Y - BB.Min.Y) * 10.f;
	const FVector2D mid = (BB.Min + BB.Max) * 0.5f;

	const FVector2D S1(mid.X - 2*delta, mid.Y - delta);
	const FVector2D S2(mid.X,            mid.Y + 2*delta);
	const FVector2D S3(mid.X + 2*delta,  mid.Y - delta);

	TArray<FVector2D> Pts = Points2D;
	const int32 iS1 = Pts.Add(S1), iS2 = Pts.Add(S2), iS3 = Pts.Add(S3);

	TArray<FDGTriangle> Tris;
	Tris.Add(FDGTriangle(iS1, iS2, iS3));

	for (int32 idx = 0; idx < Points2D.Num(); ++idx)
	{
		const FVector2D& P = Points2D[idx];

		TArray<int32> Bad;
		for (int32 t = 0; t < Tris.Num(); ++t)
		{
			const FDGTriangle& T = Tris[t];
			if (PointInCircumcircle(P, Pts[T.I], Pts[T.J], Pts[T.K])) Bad.Add(t);
		}

		TMap<FDGEdge, int32> EdgeCount;
		for (int32 tIndex : Bad)
		{
			const FDGTriangle& T = Tris[tIndex];
			EdgeCount.FindOrAdd(FDGEdge(T.I,T.J))++;
			EdgeCount.FindOrAdd(FDGEdge(T.J,T.K))++;
			EdgeCount.FindOrAdd(FDGEdge(T.K,T.I))++;
		}

		Bad.Sort();
		for (int32 i = Bad.Num()-1; i >= 0; --i) Tris.RemoveAt(Bad[i]);

		for (const auto& KV : EdgeCount)
			if (KV.Value == 1) Tris.Add(FDGTriangle(KV.Key.A, KV.Key.B, idx));
	}

	for (int32 i = Tris.Num()-1; i >= 0; --i)
	{
		const FDGTriangle& T = Tris[i];
		if (T.I >= Points2D.Num() || T.J >= Points2D.Num() || T.K >= Points2D.Num())
			Tris.RemoveAt(i);
	}

	DelaunayTriangles = Tris;
}

void ADungeonGenerator::EdgesFromTriangles()
{
	GraphEdges.Reset();
	for (const FDGTriangle& T : DelaunayTriangles)
	{
		GraphEdges.Add(FDGEdge(T.I, T.J));
		GraphEdges.Add(FDGEdge(T.J, T.K));
		GraphEdges.Add(FDGEdge(T.K, T.I));
	}
}

void ADungeonGenerator::BuildMST_Prim()
{
	MSTEdges.Reset();
	const int32 N = Points2D.Num();
	if (N <= 1) return;

	TSet<int32> Visited; Visited.Add(0);

	while (Visited.Num() < N)
	{
		float Best = TNumericLimits<float>::Max();
		FDGEdge BestE; int32 Next = INDEX_NONE;

		for (const FDGEdge& E : GraphEdges)
		{
			const bool aIn = Visited.Contains(E.A);
			const bool bIn = Visited.Contains(E.B);
			if (aIn == bIn) continue;

			const float d = (Points2D[E.A] - Points2D[E.B]).Size();
			if (d < Best) { Best = d; BestE = E; Next = aIn ? E.B : E.A; }
		}

		if (Next == INDEX_NONE) break;
		MSTEdges.Add(BestE);
		Visited.Add(Next);
	}
}

void ADungeonGenerator::DrawDebugViz()
{
	if (!bDrawDelaunay) return;
	UWorld* W = GetWorld(); if (!W) return;

	const float Z_Del = DungeonCenter.Z + DelaunayZDebugOffset;
	const float Z_MST = DungeonCenter.Z + MSTZDebugOffset;

	for (const FDGTriangle& T : DelaunayTriangles)
	{
		const FVector A(Points2D[T.I].X, Points2D[T.I].Y, Z_Del);
		const FVector B(Points2D[T.J].X, Points2D[T.J].Y, Z_Del);
		const FVector C(Points2D[T.K].X, Points2D[T.K].Y, Z_Del);
		DrawDebugLine(W, A, B, FColor::Blue,  true, DebugDuration, 0, 6.f);
		DrawDebugLine(W, B, C, FColor::Blue,  true, DebugDuration, 0, 6.f);
		DrawDebugLine(W, C, A, FColor::Blue,  true, DebugDuration, 0, 6.f);
	}

	for (const FDGEdge& E : MSTEdges)
	{
		const FVector A(Points2D[E.A].X, Points2D[E.A].Y, Z_MST);
		const FVector B(Points2D[E.B].X, Points2D[E.B].Y, Z_MST);
		DrawDebugLine(W, A, B, FColor::Green, true, DebugDuration, 0, 12.f);
	}
}

bool ADungeonGenerator::SegmentIntersectsAABB2D(
	const FVector2D& P0, const FVector2D& P1,
	const FVector2D& Center, const FVector2D& Half)
{
	const double minX = static_cast<double>(Center.X - Half.X);
	const double maxX = static_cast<double>(Center.X + Half.X);
	const double minY = static_cast<double>(Center.Y - Half.Y);
	const double maxY = static_cast<double>(Center.Y + Half.Y);

	const double dx = static_cast<double>(P1.X - P0.X);
	const double dy = static_cast<double>(P1.Y - P0.Y);

	double p[4] = { -dx, dx, -dy, dy };
	double q[4] = {
		static_cast<double>(P0.X) - minX,
		maxX - static_cast<double>(P0.X),
		static_cast<double>(P0.Y) - minY,
		maxY - static_cast<double>(P0.Y)
	};

	double u0 = 0.0, u1 = 1.0;
	for (int i = 0; i < 4; ++i)
	{
		if (FMath::IsNearlyZero(p[i]))
		{
			if (q[i] < 0.0) return false;
		}
		else
		{
			const double t = q[i] / p[i];
			if (p[i] < 0.0) { if (t > u1) return false; if (t > u0) u0 = t; }
			else            { if (t < u0) return false; if (t < u1) u1 = t; }
		}
	}
	return true;
}

static bool ClipSegmentAgainstAABB2D(
	const FVector2D& P0, const FVector2D& P1,
	const FVector2D& Center, const FVector2D& Half,
	double& OutU0, double& OutU1)
{
	const double minX = Center.X - Half.X;
	const double maxX = Center.X + Half.X;
	const double minY = Center.Y - Half.Y;
	const double maxY = Center.Y + Half.Y;

	const double dx = static_cast<double>(P1.X - P0.X);
	const double dy = static_cast<double>(P1.Y - P0.Y);

	double p[4] = { -dx, dx, -dy, dy };
	double q[4] = {
		static_cast<double>(P0.X) - minX,
		maxX - static_cast<double>(P0.X),
		static_cast<double>(P0.Y) - minY,
		maxY - static_cast<double>(P0.Y)
	};

	double u0 = 0.0, u1 = 1.0;
	for (int i = 0; i < 4; ++i)
	{
		if (FMath::IsNearlyZero(p[i]))
		{
			if (q[i] < 0.0) return false;
		}
		else
		{
			const double t = q[i] / p[i];
			if (p[i] < 0.0) { if (t > u1) return false; if (t > u0) u0 = t; }
			else            { if (t < u0) return false; if (t < u1) u1 = t; }
		}
	}
	OutU0 = u0; OutU1 = u1;
	return true;
}

void ADungeonGenerator::BuildCorridorsFromMST()
{
    CorridorSegments.Reset();

    auto FindMainRoomByCenter = [&](const FVector2D& C)->ARoom*
    {
        ARoom* Best = nullptr; double BestD2 = DBL_MAX;
        for (ARoom* R : SpawnedRooms)
        {
            if (!IsValid(R) || !R->bIsMain) continue;
            FVector O,E; R->GetActorBounds(false, O, E);
            const double d2 = (FVector2D(O.X,O.Y) - C).SizeSquared();
            if (d2 < BestD2) { BestD2 = d2; Best = R; }
        }
        return (BestD2 < 1.0) ? Best : nullptr;
    };
	
    auto ExitPointFromRoom = [&](const FVector2D& Start, const FVector2D& Toward,
                                 const ARoom* Room, float Inset)->FVector2D
    {
        if (!IsValid(Room)) return Start;

        FVector O,E; Room->GetActorBounds(false,O,E);
        const FVector2D C(O.X,O.Y);
        FVector2D H = Room->SizeXY * 0.5f;
        H.X = FMath::Max(0.f, H.X - Inset);
        H.Y = FMath::Max(0.f, H.Y - Inset);
    	
        const double minX = C.X - H.X, maxX = C.X + H.X;
        const double minY = C.Y - H.Y, maxY = C.Y + H.Y;
        const double dx = (double)Toward.X - (double)Start.X;
        const double dy = (double)Toward.Y - (double)Start.Y;

        double p[4] = { -dx, dx, -dy, dy };
        double q[4] = { (double)Start.X - minX, maxX - (double)Start.X,
                        (double)Start.Y - minY, maxY - (double)Start.Y };
        double u0=0.0, u1=1.0;
        for (int i=0;i<4;++i){
            if (FMath::IsNearlyZero(p[i])) { if (q[i] < 0.0) return Start; }
            else {
                const double t = q[i]/p[i];
                if (p[i] < 0.0) { if (t > u1) return Start; if (t > u0) u0 = t; }
                else            { if (t < u0) return Start; if (t < u1) u1 = t; }
            }
        }
        return FMath::Lerp(Start, Toward, (float)u1);
    };

    const float EdgeInset = 10.f;
    const float EpsAlign  = 1e-2f;
    for (const FDGEdge& E : MSTEdges)
    {
        const FVector2D Acenter = Points2D[E.A];
        const FVector2D Bcenter = Points2D[E.B];

        ARoom* AR = FindMainRoomByCenter(Acenter);
        ARoom* BR = FindMainRoomByCenter(Bcenter);
    	
        const FVector2D Aedge = ExitPointFromRoom(Acenter, Bcenter, AR, EdgeInset);
        const FVector2D Bedge = ExitPointFromRoom(Bcenter, Acenter, BR, EdgeInset);

        if (bCorridorFollowMSTExact)
        {
            CorridorSegments.Emplace(Aedge, Bedge);
        }
        else
        {
            const bool AlignedH = FMath::IsNearlyEqual(Acenter.Y, Bcenter.Y, EpsAlign);
            const bool AlignedV = FMath::IsNearlyEqual(Acenter.X, Bcenter.X, EpsAlign);

            if (AlignedH || AlignedV)
            {
                CorridorSegments.Emplace(Aedge, Bedge);
            }
            else
            {
                FVector2D Corner(Acenter.X, Bcenter.Y);
                const FVector2D A_to_Corner = ExitPointFromRoom(Acenter, Corner, AR, EdgeInset);
                const FVector2D Corner_to_B = ExitPointFromRoom(Bcenter, Corner, BR, EdgeInset);

                CorridorSegments.Emplace(A_to_Corner, Corner);
                CorridorSegments.Emplace(Corner, Corner_to_B);
            }
        }
    }
}


void ADungeonGenerator::DrawCorridorsDebug() const
{
	UWorld* W = GetWorld(); if (!W) return;

	const float Z = DungeonCenter.Z + CorridorZOffset;

	for (const FCorridorSeg& S : CorridorSegments)
	{
		const FVector P0(S.A.X, S.A.Y, Z);
		const FVector P1(S.B.X, S.B.Y, Z);
		DrawDebugLine(W, P0, P1, FColor(0, 160, 255), true, DebugDuration, 0, CorridorThickness);

		DrawDebugBox(W, P0, FVector(30,30,30), FColor(0,160,255), true, DebugDuration, 0, 6.f);
		DrawDebugBox(W, P1, FVector(30,30,30), FColor(0,160,255), true, DebugDuration, 0, 6.f);
	}
}

void ADungeonGenerator::KeepMainAndCorridorRooms()
{
	if (!bKeepOnlyMainAndPath) return;

	TSet<ARoom*> KeepSet;
	
	for (ARoom* R : SpawnedRooms)
		if (IsValid(R) && R->bIsMain)
			KeepSet.Add(R);
	
	for (ARoom* R : SpawnedRooms)
	{
		if (!IsValid(R)) continue;

		FVector Origin, Extent;
		R->GetActorBounds(false, Origin, Extent);

		const FVector2D Center(Origin.X, Origin.Y);
		FVector2D Half(R->SizeXY.X * 0.5f, R->SizeXY.Y * 0.5f);
		Half.X += CorridorKeepDistance;
		Half.Y += CorridorKeepDistance;

		for (const FCorridorSeg& S : CorridorSegments)
		{
			if (SegmentIntersectsAABB2D(S.A, S.B, Center, Half))
			{
				KeepSet.Add(R);
				break;
			}
		}
	}

	for (int32 i = SpawnedRooms.Num()-1; i >= 0; --i)
	{
		ARoom* R = SpawnedRooms[i];
		if (!IsValid(R)) { SpawnedRooms.RemoveAt(i); continue; }
		if (!KeepSet.Contains(R))
		{
			R->Destroy();
			SpawnedRooms.RemoveAt(i);
		}
	}
}

void ADungeonGenerator::SpawnCorridorMeshes()
{
	if (!CorridorISM) return;

	CorridorISM->ClearInstances();

	if (CorridorMesh)
	{
		CorridorISM->SetStaticMesh(CorridorMesh);
	}
	if (CorridorMaterial && CorridorISM->GetMaterial(0) != CorridorMaterial)
	{
		CorridorISM->SetMaterial(0, CorridorMaterial);
	}
	
	const float Base = 100.f;

	for (const FCorridorSeg& S : CorridorSegments)
	{
		const FVector2D AB = S.B - S.A;
		const float Len = AB.Size();
		if (Len <= KINDA_SMALL_NUMBER) continue;

		const FVector Mid(
			(S.A.X + S.B.X) * 0.5f,
			(S.A.Y + S.B.Y) * 0.5f,
			DungeonCenter.Z + CorridorZOffset + (CorridorHeight * 0.5f));

		const float YawDeg = FMath::RadiansToDegrees(FMath::Atan2(AB.Y, AB.X));

		const FTransform Xform(
			FRotator(0.f, YawDeg, 0.f),
			Mid,
			FVector(Len / Base, CorridorWidth / Base, CorridorHeight / Base)
		);

		CorridorISM->AddInstance(Xform, true);
	}
}

void ADungeonGenerator::SelectMainRooms()
{
	for (ARoom* R : SpawnedRooms)
		if (IsValid(R)) R->bIsMain = false;
	TArray<ARoom*> Sorted = SpawnedRooms;
	Sorted.Sort([](const ARoom& A, const ARoom& B){ return A.GetArea() > B.GetArea(); });
	
	TArray<ARoom*> Picked; Picked.Reserve(MainCount);
	for (ARoom* Candidate : Sorted)
	{
		bool ok = true;
		for (ARoom* P : Picked) if (TooCloseAABB(Candidate, P, MinMainGap)) { ok = false; break; }
		if (ok) { Picked.Add(Candidate); if (Picked.Num() >= MainCount) break; }
	}

	RelaxMainRoomsPositions(Picked);

	for (ARoom* R : Picked) if (IsValid(R)) R->bIsMain = true;

	RefreshMainRoomMaterials();
	CollectAndStoreMainCenters();
	DrawMainCenters();
	BuildDelaunay();
	EdgesFromTriangles();
	BuildMST_Prim();
	DrawDebugViz();
	
	if (bBuildCorridors)
	{
		BuildCorridorsFromMST();
		DrawCorridorsDebug();
		KeepMainAndCorridorRooms();
		SpawnCorridorMeshes();
	}
}

void ADungeonGenerator::RefreshMainRoomMaterials()
{
	if (!MainRoomMaterial) return;
	for (ARoom* R : SpawnedRooms)
	{
		if (!IsValid(R) || !IsValid(R->VisualMesh)) continue;
		if (R->bIsMain) R->VisualMesh->SetMaterial(0, MainRoomMaterial);
	}
}
