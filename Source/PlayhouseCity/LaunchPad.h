#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "LaunchPad.generated.h"

UCLASS()
class PLAYHOUSECITY_API ALaunchPad : public AActor
{
	GENERATED_BODY()

public:
	ALaunchPad();

	UPROPERTY(VisibleAnywhere)
	UBoxComponent* TriggerBox;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* PadMesh;

	UPROPERTY(EditAnywhere, Category = "Launch")
	float LaunchStrength = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Launch")
	float ForwardBoost = 400.0f;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);
};
