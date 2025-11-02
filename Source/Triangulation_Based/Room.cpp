#include "Room.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ARoom::ARoom()
{
    PrimaryActorTick.bCanEverTick = false;

    VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
    SetRootComponent(VisualMesh);
    VisualMesh->SetMobility(EComponentMobility::Movable);
    VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        VisualMesh->SetStaticMesh(CubeMesh.Object);
    }
}

void ARoom::OnConstruction(const FTransform& Transform)
{
    SyncVisual();
}

void ARoom::SyncVisual()
{
    const float ScaleX = FMath::Max(SizeXY.X, 1.f) / 100.f;
    const float ScaleY = FMath::Max(SizeXY.Y, 1.f) / 100.f;
    const float ScaleZ = FMath::Max(Thickness, 1.f) / 100.f;
    VisualMesh->SetWorldScale3D(FVector(ScaleX, ScaleY, ScaleZ));
}
