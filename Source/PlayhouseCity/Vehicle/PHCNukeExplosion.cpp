// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCNukeExplosion.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APHCNukeExplosion::APHCNukeExplosion()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UStaticMesh* SphereSM = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
	UStaticMesh* CylSM = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakeSphere = [&](const TCHAR* Name, const FVector& Loc, float Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		M->SetupAttachment(Root);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (SphereSM) M->SetStaticMesh(SphereSM);
		M->SetRelativeScale3D(FVector(Scale));
		M->SetRelativeLocation(Loc);
		M->SetVisibility(false);
		M->SetBoundsScale(10.f);
		return M;
	};

	auto MakeCylinder = [&](const TCHAR* Name, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* M = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		M->SetupAttachment(Root);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (CylSM) M->SetStaticMesh(CylSM);
		M->SetRelativeScale3D(Scale);
		M->SetRelativeLocation(Loc);
		M->SetVisibility(false);
		M->SetBoundsScale(10.f);
		return M;
	};

	// -- Core fireball (central) --
	FireballCore = MakeSphere(TEXT("FireballCore"), FVector(0.f, 0.f, 80.f), 1.f);

	// -- Flame burst lobes (offset fireballs for rolling fire look) --
	FlameBurstA = MakeSphere(TEXT("FlameBurstA"), FVector(120.f, 60.f, 50.f), 1.f);
	FlameBurstB = MakeSphere(TEXT("FlameBurstB"), FVector(-80.f, 150.f, 60.f), 1.f);
	FlameBurstC = MakeSphere(TEXT("FlameBurstC"), FVector(-60.f, -130.f, 70.f), 1.f);

	// -- Smoke plume (dark cylinder rising from center) --
	SmokePlume = MakeCylinder(TEXT("SmokePlume"), FVector(0.f, 0.f, 50.f), FVector(4.f, 4.f, 0.5f));

	// -- Smoke clouds (dark billowing spheres at top) --
	SmokeCloudA = MakeSphere(TEXT("SmokeCloudA"), FVector(50.f, 30.f, 300.f), 1.f);
	SmokeCloudB = MakeSphere(TEXT("SmokeCloudB"), FVector(-40.f, -50.f, 350.f), 1.f);

	// -- Debris ring (flat brown cylinder at ground level) --
	DebrisRing = MakeCylinder(TEXT("DebrisRing"), FVector(0.f, 0.f, 10.f), FVector(3.f, 3.f, 0.15f));

	// -- Shockwave ring (fast thin white ring) --
	ShockwaveRing = MakeCylinder(TEXT("ShockwaveRing"), FVector(0.f, 0.f, 5.f), FVector(2.f, 2.f, 0.02f));

	// -- Explosion light --
	ExplosionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExplosionLight"));
	ExplosionLight->SetupAttachment(Root);
	ExplosionLight->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	ExplosionLight->SetIntensity(0.f);
	ExplosionLight->SetAttenuationRadius(12000.f);
	ExplosionLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.15f));
	ExplosionLight->SetCastShadows(false);
}

void APHCNukeExplosion::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(TotalDuration);

	// Helper: create emissive MID on a mesh component
	auto MakeEmissiveMat = [](UStaticMeshComponent* Comp,
		const FLinearColor& Color) -> UMaterialInstanceDynamic*
	{
		if (!Comp || !Comp->GetStaticMesh()) return nullptr;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(
			Comp->GetMaterial(0), Comp);
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), Color);
			M->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			M->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			M->SetScalarParameterValue(TEXT("Roughness"), 1.f);
			Comp->SetMaterial(0, M);
		}
		return M;
	};

	// Initial colors
	const FLinearColor WhiteHot(15.f, 12.f, 8.f);
	const FLinearColor BrightOrange(8.f, 3.f, 0.4f);
	const FLinearColor DarkSmoke(0.08f, 0.06f, 0.05f);
	const FLinearColor BrownDirt(0.35f, 0.22f, 0.1f);
	const FLinearColor WhiteFlash(6.f, 5.f, 3.f);

	FireballCoreMat = MakeEmissiveMat(FireballCore, WhiteHot);
	FlameMatA = MakeEmissiveMat(FlameBurstA, BrightOrange);
	FlameMatB = MakeEmissiveMat(FlameBurstB, BrightOrange);
	FlameMatC = MakeEmissiveMat(FlameBurstC, BrightOrange);
	SmokePlumeMat = MakeEmissiveMat(SmokePlume, DarkSmoke);
	SmokeCloudAMat = MakeEmissiveMat(SmokeCloudA, DarkSmoke);
	SmokeCloudBMat = MakeEmissiveMat(SmokeCloudB, DarkSmoke);
	DebrisRingMat = MakeEmissiveMat(DebrisRing, BrownDirt);
	ShockwaveRingMat = MakeEmissiveMat(ShockwaveRing, WhiteFlash);

	// Make all meshes visible at start
	if (FireballCore) FireballCore->SetVisibility(true);
	if (ShockwaveRing) ShockwaveRing->SetVisibility(true);
	if (DebrisRing) DebrisRing->SetVisibility(true);

	// Initial light flash
	if (ExplosionLight) ExplosionLight->SetIntensity(200000.f);

	// -- Spawn ember particles at runtime --
	UStaticMesh* SphereSM = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Embers.Reserve(NumEmbers);
	EmberMats.Reserve(NumEmbers);
	EmberVelocities.Reserve(NumEmbers);
	EmberPositions.Reserve(NumEmbers);

	FRandomStream Rand(FMath::Rand());
	for (int32 i = 0; i < NumEmbers; ++i)
	{
		UStaticMeshComponent* E = NewObject<UStaticMeshComponent>(this);
		if (!E) continue;
		E->SetupAttachment(GetRootComponent());
		if (SphereSM) E->SetStaticMesh(SphereSM);
		E->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		E->SetRelativeScale3D(FVector(FMath::RandRange(0.15f, 0.4f)));
		E->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
		E->SetBoundsScale(10.f);
		E->RegisterComponent();

		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(
			E->GetMaterial(0), E);
		if (M)
		{
			const FLinearColor EmberColor(
				FMath::RandRange(5.f, 10.f),
				FMath::RandRange(1.5f, 3.f),
				FMath::RandRange(0.05f, 0.3f));
			M->SetVectorParameterValue(TEXT("BaseColor"), EmberColor);
			M->SetVectorParameterValue(TEXT("EmissiveColor"), EmberColor);
			M->SetScalarParameterValue(TEXT("Metallic"), 0.f);
			M->SetScalarParameterValue(TEXT("Roughness"), 1.f);
			E->SetMaterial(0, M);
		}

		Embers.Add(E);
		EmberMats.Add(M);
		EmberPositions.Add(FVector(0.f, 0.f, 50.f));

		// Random outward + upward velocity
		const float Angle = Rand.FRandRange(0.f, 360.f);
		const float HorizSpeed = Rand.FRandRange(800.f, 2500.f);
		const float VertSpeed = Rand.FRandRange(600.f, 2000.f);
		EmberVelocities.Add(FVector(
			FMath::Cos(FMath::DegreesToRadians(Angle)) * HorizSpeed,
			FMath::Sin(FMath::DegreesToRadians(Angle)) * HorizSpeed,
			VertSpeed));
	}
}

void APHCNukeExplosion::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;
	const float t = ElapsedTime;

	// Phase helper: 0..1 ramp between Start and End
	auto Phase = [](float Current, float Start, float End) -> float
	{
		if (Current < Start) return 0.f;
		if (Current > End) return 1.f;
		return (Current - Start) / (End - Start);
	};

	// Ease-out: fast start, slow finish
	auto EaseOut = [](float A) -> float
	{
		return 1.f - FMath::Pow(1.f - A, 3.f);
	};

	// ================================================================
	// CORE FIREBALL (t=0 to t=1.5) — bright flash, expands, fades
	// ================================================================
	{
		const float ExpandA = EaseOut(Phase(t, 0.f, 0.6f));
		const float Scale = FMath::Lerp(1.f, 22.f, ExpandA);
		const float FadeA = Phase(t, 0.8f, 1.5f);

		if (FireballCore)
		{
			FireballCore->SetRelativeScale3D(FVector(Scale * FMath::Lerp(1.f, 0.6f, FadeA)));
			FireballCore->SetRelativeLocation(FVector(0.f, 0.f, 80.f + ExpandA * 200.f));
			if (FadeA >= 1.f) FireballCore->SetVisibility(false);
		}

		if (FireballCoreMat)
		{
			FLinearColor Color;
			if (t < 0.15f)
				Color = FLinearColor(15.f, 12.f, 8.f); // white-hot flash
			else if (t < 0.5f)
				Color = FMath::Lerp(FLinearColor(15.f, 12.f, 8.f),
					FLinearColor(8.f, 3.f, 0.4f), Phase(t, 0.15f, 0.5f));
			else
				Color = FMath::Lerp(FLinearColor(8.f, 3.f, 0.4f),
					FLinearColor(1.5f, 0.3f, 0.02f), Phase(t, 0.5f, 1.5f));
			Color = Color * FMath::Lerp(1.f, 0.f, FadeA);
			FireballCoreMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			FireballCoreMat->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
		}
	}

	// ================================================================
	// FLAME BURSTS (t=0.05 to t=1.8) — rolling fire lobes
	// ================================================================
	{
		struct FlameLobe
		{
			UStaticMeshComponent* Mesh;
			UMaterialInstanceDynamic* Mat;
			float Delay;
			FVector Dir; // outward direction
			float MaxScale;
		};
		FlameLobe Lobes[] = {
			{ FlameBurstA, FlameMatA, 0.05f, FVector(1.0f, 0.5f, 0.8f), 16.f },
			{ FlameBurstB, FlameMatB, 0.10f, FVector(-0.6f, 1.0f, 0.7f), 13.f },
			{ FlameBurstC, FlameMatC, 0.08f, FVector(-0.4f, -0.9f, 0.9f), 14.f },
		};

		for (auto& L : Lobes)
		{
			const float lt = t - L.Delay;
			if (lt < 0.f) continue;

			if (L.Mesh && !L.Mesh->IsVisible()) L.Mesh->SetVisibility(true);

			const float ExpandA = EaseOut(Phase(lt, 0.f, 0.7f));
			const float FadeA = Phase(lt, 0.9f, 1.7f);
			const float Scale = FMath::Lerp(1.5f, L.MaxScale, ExpandA) * FMath::Lerp(1.f, 0.3f, FadeA);

			if (L.Mesh)
			{
				L.Mesh->SetRelativeScale3D(FVector(Scale));
				const FVector BasePos(L.Dir * 120.f + FVector(0.f, 0.f, 50.f));
				L.Mesh->SetRelativeLocation(BasePos + L.Dir * (ExpandA * 500.f));
				if (FadeA >= 1.f) L.Mesh->SetVisibility(false);
			}

			if (L.Mat)
			{
				FLinearColor Color;
				if (lt < 0.3f)
					Color = FMath::Lerp(FLinearColor(10.f, 6.f, 1.f),
						FLinearColor(6.f, 2.f, 0.2f), Phase(lt, 0.f, 0.3f));
				else
					Color = FMath::Lerp(FLinearColor(6.f, 2.f, 0.2f),
						FLinearColor(1.2f, 0.2f, 0.01f), Phase(lt, 0.3f, 1.5f));
				Color = Color * FMath::Lerp(1.f, 0.f, FadeA);
				L.Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
				L.Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			}
		}
	}

	// ================================================================
	// SMOKE PLUME (t=0.3 to t=5) — dark column rising from center
	// ================================================================
	{
		const float st = t - 0.3f;
		if (st >= 0.f)
		{
			if (SmokePlume && !SmokePlume->IsVisible()) SmokePlume->SetVisibility(true);

			const float RiseA = EaseOut(Phase(st, 0.f, 2.5f));
			const float FadeA = Phase(st, 3.0f, 4.5f);
			const float Height = FMath::Lerp(80.f, 1800.f, RiseA);
			const float ScaleZ = Height / 100.f;
			const float Width = FMath::Lerp(4.f, 14.f, RiseA);

			if (SmokePlume)
			{
				SmokePlume->SetRelativeScale3D(FVector(Width, Width, ScaleZ));
				SmokePlume->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
			}

			if (SmokePlumeMat)
			{
				// Dark gray smoke, gradually lightens as it dissipates
				FLinearColor Color = FMath::Lerp(
					FLinearColor(0.06f, 0.05f, 0.04f),
					FLinearColor(0.15f, 0.13f, 0.11f), RiseA);
				Color = Color * FMath::Lerp(1.f, 0.f, FadeA);
				SmokePlumeMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
				SmokePlumeMat->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(0.f, 0.f, 0.f)); // smoke doesn't glow
			}
		}
	}

	// ================================================================
	// SMOKE CLOUDS (t=0.5/0.8 to t=5) — billowing dark tops
	// ================================================================
	{
		struct SmokeCloud
		{
			UStaticMeshComponent* Mesh;
			UMaterialInstanceDynamic* Mat;
			float Delay;
			FVector Drift;
		};
		SmokeCloud Clouds[] = {
			{ SmokeCloudA, SmokeCloudAMat, 0.5f, FVector(60.f, 40.f, 0.f) },
			{ SmokeCloudB, SmokeCloudBMat, 0.8f, FVector(-50.f, -60.f, 0.f) },
		};

		for (auto& C : Clouds)
		{
			const float ct = t - C.Delay;
			if (ct < 0.f) continue;

			if (C.Mesh && !C.Mesh->IsVisible()) C.Mesh->SetVisibility(true);

			const float ExpandA = EaseOut(Phase(ct, 0.f, 2.0f));
			const float FadeA = Phase(ct, 2.5f, 4.0f);
			const float Scale = FMath::Lerp(2.f, 20.f, ExpandA);

			if (C.Mesh)
			{
				C.Mesh->SetRelativeScale3D(FVector(Scale, Scale, Scale * 0.7f));
				// Rise with the smoke plume, drift sideways
				const float Z = FMath::Lerp(200.f, 1600.f, ExpandA);
				C.Mesh->SetRelativeLocation(FVector(
					C.Drift.X * ExpandA, C.Drift.Y * ExpandA, Z));
				if (FadeA >= 1.f) C.Mesh->SetVisibility(false);
			}

			if (C.Mat)
			{
				FLinearColor Color = FMath::Lerp(
					FLinearColor(0.1f, 0.08f, 0.06f),
					FLinearColor(0.2f, 0.18f, 0.15f), ExpandA);
				Color = Color * FMath::Lerp(1.f, 0.f, FadeA);
				C.Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
				C.Mat->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(0.f, 0.f, 0.f));
			}
		}
	}

	// ================================================================
	// DEBRIS RING (t=0 to t=3) — brown dirt kicked up at ground level
	// ================================================================
	{
		const float ExpandA = EaseOut(Phase(t, 0.f, 1.5f));
		const float FadeA = Phase(t, 1.5f, 3.0f);
		const float RingScale = FMath::Lerp(3.f, 55.f, ExpandA);
		const float RingHeight = FMath::Lerp(0.15f, 1.5f, Phase(t, 0.f, 0.5f))
			* FMath::Lerp(1.f, 0.3f, Phase(t, 0.5f, 2.5f));

		if (DebrisRing)
		{
			DebrisRing->SetRelativeScale3D(FVector(RingScale, RingScale, RingHeight));
			DebrisRing->SetRelativeLocation(FVector(0.f, 0.f, 10.f + Phase(t, 0.f, 0.5f) * 50.f));
			if (FadeA >= 1.f) DebrisRing->SetVisibility(false);
		}

		if (DebrisRingMat)
		{
			FLinearColor Color = FMath::Lerp(
				FLinearColor(0.4f, 0.25f, 0.1f),
				FLinearColor(0.25f, 0.15f, 0.08f), ExpandA);
			Color = Color * FMath::Lerp(1.f, 0.f, FadeA);
			DebrisRingMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			DebrisRingMat->SetVectorParameterValue(TEXT("EmissiveColor"),
				Color * 0.3f); // slight warm glow from fireball light
		}
	}

	// ================================================================
	// SHOCKWAVE RING (t=0 to t=1.0) — fast expanding white dust ring
	// ================================================================
	{
		const float WaveA = EaseOut(Phase(t, 0.f, 0.8f));
		const float FadeA = Phase(t, 0.3f, 1.0f);
		const float WaveScale = FMath::Lerp(2.f, 80.f, WaveA);

		if (ShockwaveRing)
		{
			ShockwaveRing->SetRelativeScale3D(FVector(WaveScale, WaveScale, 0.02f));
			if (FadeA >= 1.f) ShockwaveRing->SetVisibility(false);
		}

		if (ShockwaveRingMat)
		{
			FLinearColor Color = FMath::Lerp(
				FLinearColor(6.f, 5.f, 3.f),
				FLinearColor(0.f, 0.f, 0.f), FadeA);
			ShockwaveRingMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			ShockwaveRingMat->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
		}
	}

	// ================================================================
	// EMBER PARTICLES — fly outward with gravity, fade out
	// ================================================================
	{
		const float EmberFade = Phase(t, 1.0f, 2.5f);

		for (int32 i = 0; i < Embers.Num(); ++i)
		{
			// Apply gravity to velocity
			EmberVelocities[i].Z -= 980.f * DeltaTime;

			// Update position
			EmberPositions[i] += EmberVelocities[i] * DeltaTime;

			if (Embers[i])
			{
				Embers[i]->SetRelativeLocation(EmberPositions[i]);
				if (EmberFade >= 1.f) Embers[i]->SetVisibility(false);
			}

			if (EmberMats[i])
			{
				// Fade embers from bright to dark
				FLinearColor Color(
					FMath::Lerp(8.f, 0.f, EmberFade),
					FMath::Lerp(2.5f, 0.f, EmberFade),
					FMath::Lerp(0.2f, 0.f, EmberFade));
				EmberMats[i]->SetVectorParameterValue(TEXT("BaseColor"), Color);
				EmberMats[i]->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
			}
		}
	}

	// ================================================================
	// EXPLOSION LIGHT — bright flash, fades over 2 seconds
	// ================================================================
	if (ExplosionLight)
	{
		float Intensity;
		if (t < 0.08f)
			Intensity = 200000.f;
		else if (t < 0.4f)
			Intensity = FMath::Lerp(200000.f, 40000.f, Phase(t, 0.08f, 0.4f));
		else
			Intensity = FMath::Lerp(40000.f, 0.f, Phase(t, 0.4f, 2.5f));

		ExplosionLight->SetIntensity(Intensity);
		// Light rises slightly with the fireball
		ExplosionLight->SetRelativeLocation(
			FVector(0.f, 0.f, 200.f + Phase(t, 0.f, 1.f) * 300.f));
	}
}
