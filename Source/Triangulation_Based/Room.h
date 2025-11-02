#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Room.generated.h"

UCLASS()
class TRIANGULATION_BASED_API ARoom : public AActor
{
	GENERATED_BODY()

public:
	ARoom();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room")
	TObjectPtr<class UStaticMeshComponent> VisualMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room")
	FVector2D SizeXY = FVector2D(600.f, 600.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Room")
	float Thickness = 2000.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room")
	bool bIsMain = false;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UFUNCTION() void SyncVisual();
	UFUNCTION(BlueprintCallable, Category="Room")
	float GetArea() const { return FMath::Max(1.f, SizeXY.X) * FMath::Max(1.f, SizeXY.Y); }
};
