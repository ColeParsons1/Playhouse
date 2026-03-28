#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"
#include "TrainingDummy.generated.h"

UCLASS()
class PLAYHOUSECITY_API ATrainingDummy : public AActor
{
	GENERATED_BODY()

public:
	ATrainingDummy();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* BodyMesh;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* HeadMesh;

	UPROPERTY(VisibleAnywhere)
	UCapsuleComponent* HitZone;

	UPROPERTY(EditAnywhere, Category = "Dummy")
	float KnockbackForce = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Dummy")
	float RecoverSpeed = 2.0f;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	FVector OriginalLocation;
	bool bKnockedBack;
	float KnockbackTimer;
};
