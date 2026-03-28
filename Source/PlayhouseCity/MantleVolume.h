#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "MantleVolume.generated.h"

UCLASS()
class PLAYHOUSECITY_API AMantleVolume : public AActor
{
	GENERATED_BODY()

public:
	AMantleVolume();

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* WallMesh;

	UPROPERTY(EditAnywhere, Category = "Mantle")
	float WallHeight = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Mantle")
	float MantleSpeed = 5.0f;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
