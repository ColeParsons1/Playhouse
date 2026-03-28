#include "TrainingDummy.h"
#include "GameFramework/Character.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ATrainingDummy::ATrainingDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	bKnockedBack = false;
	KnockbackTimer = 0.0f;

	// Body - tall cylinder-ish shape
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
	}
	BodyMesh->SetWorldScale3D(FVector(0.6f, 0.6f, 1.8f));
	BodyMesh->SetSimulatePhysics(true);
	BodyMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	BodyMesh->SetMassOverrideInKg(NAME_None, 50.0f, true);
	BodyMesh->SetLinearDamping(2.0f);
	BodyMesh->SetAngularDamping(5.0f);
	BodyMesh->BodyInstance.bLockXRotation = true;
	BodyMesh->BodyInstance.bLockYRotation = true;
	BodyMesh->OnComponentHit.AddDynamic(this, &ATrainingDummy::OnHit);

	// Head - smaller cube on top
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(BodyMesh);
	if (CubeMesh.Succeeded())
	{
		HeadMesh->SetStaticMesh(CubeMesh.Object);
	}
	HeadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HeadMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Hit zone - overlap trigger for when player runs into it
	HitZone = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitZone"));
	HitZone->SetupAttachment(BodyMesh);
	HitZone->SetCapsuleHalfHeight(120.0f);
	HitZone->SetCapsuleRadius(80.0f);
	HitZone->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	HitZone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	HitZone->OnComponentBeginOverlap.AddDynamic(this, &ATrainingDummy::OnOverlap);
}

void ATrainingDummy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bKnockedBack)
	{
		KnockbackTimer -= DeltaTime;
		if (KnockbackTimer <= 0.0f)
		{
			bKnockedBack = false;
		}
	}
}

void ATrainingDummy::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character && !bKnockedBack)
	{
		FVector Direction = GetActorLocation() - Character->GetActorLocation();
		Direction.Z = 0.2f;
		Direction.Normalize();

		BodyMesh->AddImpulse(Direction * KnockbackForce, NAME_None, true);
		bKnockedBack = true;
		KnockbackTimer = 1.0f;
	}
}

void ATrainingDummy::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character && !bKnockedBack)
	{
		FVector Direction = GetActorLocation() - Character->GetActorLocation();
		Direction.Z = 0.3f;
		Direction.Normalize();

		BodyMesh->AddImpulse(Direction * KnockbackForce * 1.5f, NAME_None, true);
		bKnockedBack = true;
		KnockbackTimer = 1.0f;
	}
}
