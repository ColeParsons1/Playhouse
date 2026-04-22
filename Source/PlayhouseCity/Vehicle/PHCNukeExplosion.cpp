// Copyright PlayhouseCity. All Rights Reserved.

#include "Vehicle/PHCNukeExplosion.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "EngineUtils.h"
#include "Player/PHCCityCharacter.h"
#include "Core/PHCCitySubsystem.h"

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
		M->SetBoundsScale(50.f);
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
		M->SetBoundsScale(50.f);
		return M;
	};

	// Legacy primitive pyrotechnics (FireballCore, FlameBurstA/B/C, SmokePlume,
	// SmokeCloudA/B, DebrisRing, ShockwaveRing) have been removed — the Fab
	// NS_NuclearExplosion Niagara is a complete replacement. Leaving these as
	// nullptrs keeps Tick's null-guards a no-op for the legacy animation code.

	// -- Explosion light --
	ExplosionLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ExplosionLight"));
	ExplosionLight->SetupAttachment(Root);
	ExplosionLight->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	ExplosionLight->SetIntensity(0.f);
	ExplosionLight->SetAttenuationRadius(12000.f);
	ExplosionLight->SetLightColor(FLinearColor(1.0f, 0.6f, 0.15f));
	ExplosionLight->SetCastShadows(false);

	// HeatHazeDome (translucent orange sphere) removed — it was a custom mesh.
	// The post-process shimmer below gives the heat-distortion look without any mesh.

	// -- Unbound post-process for heat shimmer + chromatic aberration --
	HeatPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("HeatPostProcess"));
	HeatPostProcess->SetupAttachment(Root);
	HeatPostProcess->bUnbound = true;
	HeatPostProcess->Priority = 5.f;
}

void APHCNukeExplosion::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(TotalDuration);

	// === Fab nuclear explosion Niagara — single system replaces the legacy
	// 8-sphere / 100+ flame-jet custom effect. ===
	{
		UNiagaraSystem* NukeFX = LoadObject<UNiagaraSystem>(nullptr,
			TEXT("/Game/NuclearExplosion/Effects/NS_NuclearExplosion.NS_NuclearExplosion"));
		if (NukeFX)
		{
			UNiagaraComponent* FX = NewObject<UNiagaraComponent>(this, TEXT("NuclearExplosionFX"));
			FX->SetAsset(NukeFX);
			FX->SetupAttachment(GetRootComponent());
			FX->SetRelativeLocation(FVector::ZeroVector);
			FX->SetRelativeRotation(FRotator::ZeroRotator);
			FX->SetAutoActivate(true);
			FX->SetAutoDestroy(false);
			FX->RegisterComponent();
			UE_LOG(LogPlayhouseCity, Log, TEXT("NS_NuclearExplosion spawned"));
		}
		else
		{
			UE_LOG(LogPlayhouseCity, Warning, TEXT("NS_NuclearExplosion FAILED to load"));
		}
	}

	// Legacy mesh visuals (FireballCore, FlameBurstA/B/C, SmokePlume, clouds, debris,
	// shockwave, embers, flame-jet rings) are intentionally left hidden — the Niagara
	// system above handles all the pyrotechnic visuals now. The components still exist
	// (constructor-created) but no materials/visibility are ever applied to them.

	// Initial light flash — preserved for world-lighting pop on detonation.
	if (ExplosionLight) ExplosionLight->SetIntensity(200000.f);

	// -- Heat haze dome material (translucent orange, emissive) --
	if (HeatHazeDome && HeatHazeDome->GetStaticMesh())
	{
		UMaterial* GlassMat = LoadObject<UMaterial>(nullptr,
			TEXT("/Game/Materials/M_PHCGlass.M_PHCGlass"));
		if (GlassMat)
		{
			HeatHazeMat = UMaterialInstanceDynamic::Create(GlassMat, HeatHazeDome);
			if (HeatHazeMat)
			{
				HeatHazeMat->SetVectorParameterValue(TEXT("BaseColor"),
					FLinearColor(1.0f, 0.4f, 0.1f));
				HeatHazeMat->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(5.0f, 2.0f, 0.5f));
				HeatHazeMat->SetScalarParameterValue(TEXT("Opacity"), 0.25f);
				HeatHazeMat->SetScalarParameterValue(TEXT("Metallic"), 0.f);
				HeatHazeMat->SetScalarParameterValue(TEXT("Roughness"), 1.f);
				HeatHazeDome->SetMaterial(0, HeatHazeMat);
				HeatHazeDome->SetVisibility(true);
			}
		}
	}

	// -- Post-process heat-shimmer initial settings (scaled up in Tick) --
	if (HeatPostProcess)
	{
		FPostProcessSettings& PP = HeatPostProcess->Settings;
		PP.bOverride_BloomIntensity = true;
		PP.BloomIntensity = 1.0f;
		PP.bOverride_SceneFringeIntensity = true;
		PP.SceneFringeIntensity = 0.f;
		PP.bOverride_FilmGrainIntensity = true;
		PP.FilmGrainIntensity = 0.06f;
		PP.bOverride_VignetteIntensity = true;
		PP.VignetteIntensity = 0.4f;
		PP.bOverride_MotionBlurAmount = true;
		PP.MotionBlurAmount = 0.f;
		PP.bOverride_ColorSaturation = true;
		PP.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.f);
		PP.bOverride_ColorGamma = true;
		PP.ColorGamma = FVector4(1.f, 1.f, 1.f, 1.f);
		PP.bOverride_WhiteTemp = true;
		PP.WhiteTemp = 6500.f;
	}

	// -- Kick off slight global time distortion (tracked in real seconds) --
	RealStartTime = FPlatformTime::Seconds();
	if (UWorld* W = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(this, TimeDilationPeak);
		bTimeDilationRestored = false;
	}

	// -- Damage pass: kill all players, destroy NPC helicopters --
	// Only the server has authority over health state. In Standalone/ListenServer each
	// character's Role == ROLE_Authority locally; on clients, HasAuthority() is false so
	// we skip (the server-side nuke spawn handles it).
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<APHCCityCharacter> It(W); It; ++It)
		{
			APHCCityCharacter* Ch = *It;
			if (!Ch || !Ch->HasAuthority()) continue;
			Ch->Health = 0.f;
			Ch->MulticastDamageFlash();
			Ch->ClientOnKilled();
			Ch->Health = Ch->MaxHealth;
			Ch->SetActorLocation(FVector(0.f, 0.f, 100.f));
		}

		if (UPHCCitySubsystem* Sub = W->GetSubsystem<UPHCCitySubsystem>())
		{
			if (Sub->NPCHelicopter && Sub->bNPCHeliVisible && !Sub->bNPCHeliCrashing)
			{
				Sub->NPCHeliHealth = 0.f;
				Sub->OnNPCHeliDestroyed();
			}
			if (Sub->NPCHelicopter2 && Sub->bNPCHeliVisible2 && !Sub->bNPCHeliCrashing2)
			{
				Sub->NPCHeliHealth2 = 0.f;
				Sub->OnNPCHeli2Destroyed();
			}
		}
	}

	// Ember particles and the F_FreeFlameFx_Pack flame-jet rings have been removed —
	// the Fab NS_NuclearExplosion Niagara system above is a full replacement.
}

void APHCNukeExplosion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Always restore normal time — don't leave the world in slow-mo if we die early.
	if (!bTimeDilationRestored)
	{
		if (GetWorld())
		{
			UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
		}
		bTimeDilationRestored = true;
	}
	Super::EndPlay(EndPlayReason);
}

void APHCNukeExplosion::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;
	const float t = ElapsedTime;

	// Real-seconds clock is immune to our own time dilation — use it for
	// the time-dilation restore ramp and the heat-shimmer curve.
	const float RealT = static_cast<float>(FPlatformTime::Seconds() - RealStartTime);

	// Time dilation: hold peak slowdown, then smoothly ramp back to 1.0x.
	if (!bTimeDilationRestored)
	{
		if (GetWorld())
		{
			float Dilation;
			if (RealT < TimeDilationHold)
			{
				Dilation = TimeDilationPeak;
			}
			else
			{
				const float Alpha = FMath::Clamp(
					(RealT - TimeDilationHold) / TimeDilationRamp, 0.f, 1.f);
				Dilation = FMath::Lerp(TimeDilationPeak, 1.f, Alpha);
				if (Alpha >= 1.f) bTimeDilationRestored = true;
			}
			UGameplayStatics::SetGlobalTimeDilation(this, Dilation);
		}
	}

	// Heat-distortion post-process curve — rises to peak fast, fades slowly.
	if (HeatPostProcess)
	{
		const float RiseA = FMath::Clamp(RealT / 0.15f, 0.f, 1.f);
		const float FallA = FMath::Clamp((RealT - 1.0f) / 3.0f, 0.f, 1.f);
		const float I = FMath::Clamp(RiseA - FallA, 0.f, 1.f);

		FPostProcessSettings& PP = HeatPostProcess->Settings;
		PP.SceneFringeIntensity = FMath::Lerp(0.f, 6.5f, I);
		PP.FilmGrainIntensity = FMath::Lerp(0.06f, 0.45f, I);
		PP.VignetteIntensity = FMath::Lerp(0.4f, 1.4f, I);
		PP.MotionBlurAmount = FMath::Lerp(0.f, 0.9f, I);
		PP.BloomIntensity = FMath::Lerp(1.0f, 4.5f, I);
		PP.ColorSaturation = FVector4(
			FMath::Lerp(1.f, 1.4f, I),
			FMath::Lerp(1.f, 0.85f, I),
			FMath::Lerp(1.f, 0.6f, I),
			1.f);
		PP.ColorGamma = FVector4(
			FMath::Lerp(1.f, 0.85f, I),
			FMath::Lerp(1.f, 0.95f, I),
			FMath::Lerp(1.f, 1.05f, I),
			1.f);
		PP.WhiteTemp = FMath::Lerp(6500.f, 8500.f, I);
	}

	// Heat-haze dome — translucent orange sphere swelling outward, fading out.
	if (HeatHazeDome && HeatHazeMat)
	{
		const float ExpandA = FMath::Clamp(RealT / 1.2f, 0.f, 1.f);
		const float Eased = 1.f - FMath::Pow(1.f - ExpandA, 3.f);
		const float Scale = FMath::Lerp(3.f, 140.f, Eased);
		HeatHazeDome->SetRelativeScale3D(FVector(Scale));

		const float FadeA = FMath::Clamp((RealT - 0.5f) / 2.5f, 0.f, 1.f);
		HeatHazeMat->SetScalarParameterValue(TEXT("Opacity"),
			FMath::Lerp(0.25f, 0.f, FadeA));

		const float CoolA = FMath::Clamp(RealT / 0.4f, 0.f, 1.f);
		FLinearColor Emissive = FMath::Lerp(
			FLinearColor(5.f, 2.f, 0.5f),
			FLinearColor(1.5f, 0.6f, 0.15f), CoolA);
		Emissive = Emissive * FMath::Lerp(1.f, 0.f, FadeA);
		HeatHazeMat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);

		if (FadeA >= 1.f) HeatHazeDome->SetVisibility(false);
	}

	// Force destroy when animation is complete
	if (t >= TotalDuration)
	{
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		Destroy();
		return;
	}

	// Deactivate flame jets in last 1.5 seconds for fade-out
	if (t > TotalDuration - 1.5f)
	{
		for (UNiagaraComponent* F : FlameJets)
		{
			if (F && F->IsActive())
				F->Deactivate();
		}
	}

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

			if (SmokePlume && FadeA >= 1.f) SmokePlume->SetVisibility(false);

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
