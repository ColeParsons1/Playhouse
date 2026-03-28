#include "MantleVolume.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AMantleVolume::AMantleVolume()
{
	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	RootComponent = WallMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		WallMesh->SetStaticMesh(CubeMesh.Object);
	}
	WallMesh->SetWorldScale3D(FVector(3.0f, 0.3f, 1.5f));

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(RootComponent);
	TriggerBox->SetBoxExtent(FVector(200.0f, 100.0f, 100.0f));
	TriggerBox->SetRelativeLocation(FVector(0.0f, -200.0f, 0.0f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AMantleVolume::OnOverlap);
}

void AMantleVolume::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* Character = Cast<ACharacter>(OtherActor);
	if (Character && Character->GetCharacterMovement())
	{
		// Launch the character up and slightly forward over the wall
		FVector WallForward = GetActorForwardVector();
		float UpForce = WallHeight * 5.0f;
		float ForwardForce = 300.0f;

		FVector LaunchVelocity = FVector(WallForward.X * ForwardForce, WallForward.Y * ForwardForce, UpForce);
		Character->LaunchCharacter(LaunchVelocity, false, true);
	}
}
