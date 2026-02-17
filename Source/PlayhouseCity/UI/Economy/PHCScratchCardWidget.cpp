// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Economy/PHCScratchCardWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"

#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCScratchCardWidget::UPHCScratchCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCScratchCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ClaimButton)
	{
		ClaimButton->OnClicked.AddDynamic(this, &UPHCScratchCardWidget::OnClaimClicked);
		ClaimButton->SetIsEnabled(false);
		ClaimButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PrizeText)
	{
		PrizeText->SetVisibility(ESlateVisibility::Collapsed);
	}

	InitializeScratchMask();
	FetchPrize();
}

void UPHCScratchCardWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Input overrides (scratch gesture)
// ---------------------------------------------------------------------------

FReply UPHCScratchCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bRevealed)
	{
		return FReply::Unhandled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsScratching = true;

		const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		ScratchAtPosition(LocalPos);

		return FReply::Handled().CaptureMouse(GetCachedWidget().ToSharedRef());
	}

	return FReply::Unhandled();
}

FReply UPHCScratchCardWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bRevealed || !bIsScratching)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	ScratchAtPosition(LocalPos);

	return FReply::Handled();
}

FReply UPHCScratchCardWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsScratching && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsScratching = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply UPHCScratchCardWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (bRevealed)
	{
		return FReply::Unhandled();
	}

	bIsScratching = true;

	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	ScratchAtPosition(LocalPos);

	return FReply::Handled();
}

FReply UPHCScratchCardWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (bRevealed || !bIsScratching)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	ScratchAtPosition(LocalPos);

	return FReply::Handled();
}

FReply UPHCScratchCardWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	bIsScratching = false;
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------

void UPHCScratchCardWidget::OnClaimClicked()
{
	if (bClaimInFlight || !bRevealed || PrizeAmount <= 0)
	{
		return;
	}

	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		return;
	}

	bClaimInFlight = true;

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCScratchCardWidget::OnClaimResult);
	Http->ScratchCard(Callback);
}

void UPHCScratchCardWidget::ResetCard()
{
	bRevealed = false;
	bIsScratching = false;
	bClaimInFlight = false;
	ScratchPercentage = 0.0f;
	ClearedPixels = 0;
	PrizeAmount = 0;

	InitializeScratchMask();

	if (ClaimButton)
	{
		ClaimButton->SetIsEnabled(false);
		ClaimButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PrizeText)
	{
		PrizeText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ScratchProgressBar)
	{
		ScratchProgressBar->SetPercent(0.0f);
	}
	if (ScratchCanvas)
	{
		ScratchCanvas->SetVisibility(ESlateVisibility::Visible);
	}

	FetchPrize();
}

// ---------------------------------------------------------------------------
// Private helpers - Scratch mask
// ---------------------------------------------------------------------------

void UPHCScratchCardWidget::InitializeScratchMask()
{
	TotalPixels = MaskWidth * MaskHeight;
	ClearedPixels = 0;

	// Allocate pixel buffer: BGRA format, fully opaque with a solid overlay color.
	const int32 ByteCount = MaskWidth * MaskHeight * 4;
	MaskPixels.SetNum(ByteCount);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		const int32 Offset = i * 4;
		MaskPixels[Offset + 0] = 180;  // B - silver/grey scratch-off color
		MaskPixels[Offset + 1] = 180;  // G
		MaskPixels[Offset + 2] = 180;  // R
		MaskPixels[Offset + 3] = 255;  // A - fully opaque
	}

	// Create or recreate the transient texture.
	ScratchMask = UTexture2D::CreateTransient(MaskWidth, MaskHeight, PF_B8G8R8A8);
	if (!ScratchMask)
	{
		UE_LOG(LogTemp, Error, TEXT("PHCScratchCard: Failed to create scratch mask texture."));
		return;
	}

	ScratchMask->Filter = TF_Bilinear;
	ScratchMask->SRGB = true;

	UpdateMaskTexture();

	if (ScratchCanvas)
	{
		ScratchCanvas->SetBrushFromTexture(ScratchMask);
	}
}

void UPHCScratchCardWidget::ScratchAtPosition(const FVector2D& LocalPosition)
{
	if (!ScratchCanvas || bRevealed)
	{
		return;
	}

	// Convert local widget position to mask texture coordinates.
	const FVector2D WidgetSize = GetCachedGeometry().GetLocalSize();
	if (WidgetSize.X <= 0.0f || WidgetSize.Y <= 0.0f)
	{
		return;
	}

	const float NormX = FMath::Clamp(LocalPosition.X / WidgetSize.X, 0.0f, 1.0f);
	const float NormY = FMath::Clamp(LocalPosition.Y / WidgetSize.Y, 0.0f, 1.0f);

	const int32 CenterX = FMath::RoundToInt(NormX * (MaskWidth - 1));
	const int32 CenterY = FMath::RoundToInt(NormY * (MaskHeight - 1));

	// Erase pixels within the brush radius.
	const int32 RadiusSq = BrushRadius * BrushRadius;

	for (int32 dy = -BrushRadius; dy <= BrushRadius; ++dy)
	{
		for (int32 dx = -BrushRadius; dx <= BrushRadius; ++dx)
		{
			if (dx * dx + dy * dy > RadiusSq)
			{
				continue;
			}

			const int32 px = CenterX + dx;
			const int32 py = CenterY + dy;

			if (px < 0 || px >= MaskWidth || py < 0 || py >= MaskHeight)
			{
				continue;
			}

			const int32 PixelIndex = (py * MaskWidth + px) * 4;
			const int32 AlphaIndex = PixelIndex + 3;

			if (MaskPixels[AlphaIndex] > 0)
			{
				// Set alpha to 0 (transparent).
				MaskPixels[AlphaIndex] = 0;
				MaskPixels[PixelIndex + 0] = 0; // B
				MaskPixels[PixelIndex + 1] = 0; // G
				MaskPixels[PixelIndex + 2] = 0; // R
				++ClearedPixels;
			}
		}
	}

	UpdateMaskTexture();
	UpdateScratchPercentage();
	CheckRevealThreshold();
}

void UPHCScratchCardWidget::UpdateScratchPercentage()
{
	if (TotalPixels > 0)
	{
		ScratchPercentage = static_cast<float>(ClearedPixels) / static_cast<float>(TotalPixels);
	}
	else
	{
		ScratchPercentage = 0.0f;
	}

	if (ScratchProgressBar)
	{
		ScratchProgressBar->SetPercent(ScratchPercentage);
	}
}

void UPHCScratchCardWidget::CheckRevealThreshold()
{
	if (!bRevealed && ScratchPercentage >= RevealThreshold)
	{
		AutoReveal();
	}
}

void UPHCScratchCardWidget::AutoReveal()
{
	bRevealed = true;
	bIsScratching = false;

	// Make the entire mask transparent.
	FMemory::Memzero(MaskPixels.GetData(), MaskPixels.Num());
	ClearedPixels = TotalPixels;
	ScratchPercentage = 1.0f;

	UpdateMaskTexture();

	if (ScratchProgressBar)
	{
		ScratchProgressBar->SetPercent(1.0f);
	}

	// Hide the canvas entirely to fully reveal the prize.
	if (ScratchCanvas)
	{
		ScratchCanvas->SetVisibility(ESlateVisibility::Hidden);
	}

	// Show the prize and claim button.
	if (PrizeText)
	{
		PrizeText->SetText(FText::FromString(
			FString::Printf(TEXT("You won %d coins!"), PrizeAmount)));
		PrizeText->SetVisibility(ESlateVisibility::Visible);
	}

	if (ClaimButton)
	{
		ClaimButton->SetIsEnabled(true);
		ClaimButton->SetVisibility(ESlateVisibility::Visible);
	}

	UE_LOG(LogTemp, Log, TEXT("PHCScratchCard: Revealed! Prize = %d coins."), PrizeAmount);
}

void UPHCScratchCardWidget::UpdateMaskTexture()
{
	if (!ScratchMask || MaskPixels.Num() == 0)
	{
		return;
	}

	// Lock the texture mip data and copy in our modified pixel buffer.
	FTexture2DMipMap& Mip = ScratchMask->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (TextureData)
	{
		FMemory::Memcpy(TextureData, MaskPixels.GetData(), MaskPixels.Num());
		Mip.BulkData.Unlock();
		ScratchMask->UpdateResource();
	}
}

void UPHCScratchCardWidget::FetchPrize()
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		// Fallback: random local prize.
		PrizeAmount = FMath::RandRange(10, 500);
		UE_LOG(LogTemp, Log, TEXT("PHCScratchCard: No HTTP client. Using local prize = %d."), PrizeAmount);
		return;
	}

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCScratchCardWidget::OnPrizeReceived);
	Http->ScratchCard(Callback);
}

void UPHCScratchCardWidget::OnPrizeReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (!bSuccess)
	{
		// Fallback on failure.
		PrizeAmount = FMath::RandRange(10, 500);
		UE_LOG(LogTemp, Warning, TEXT("PHCScratchCard: Failed to fetch prize. Using fallback = %d."), PrizeAmount);
		return;
	}

	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
	{
		PrizeAmount = static_cast<int32>(JsonObj->GetNumberField(TEXT("prize_amount")));
	}
	else
	{
		PrizeAmount = FMath::RandRange(10, 500);
	}

	UE_LOG(LogTemp, Log, TEXT("PHCScratchCard: Prize loaded = %d coins."), PrizeAmount);
}

void UPHCScratchCardWidget::OnClaimResult(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	bClaimInFlight = false;

	if (bSuccess)
	{
		TSharedPtr<FJsonObject> JsonObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			const int32 AwardedCoins = static_cast<int32>(JsonObj->GetNumberField(TEXT("coins_awarded")));

			// Update GameInstance balance.
			UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
			if (GI && JsonObj->HasField(TEXT("total_coins")))
			{
				GI->LocalPlayerProfile.Coins = static_cast<int32>(JsonObj->GetNumberField(TEXT("total_coins")));
			}

			if (PrizeText)
			{
				PrizeText->SetText(FText::FromString(
					FString::Printf(TEXT("Claimed %d coins!"), AwardedCoins)));
			}

			OnPrizeClaimed.Broadcast(AwardedCoins);

			UE_LOG(LogTemp, Log, TEXT("PHCScratchCard: Claimed %d coins successfully."), AwardedCoins);
		}

		if (ClaimButton)
		{
			ClaimButton->SetIsEnabled(false);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCScratchCard: Claim failed. Status=%d"), StatusCode);

		if (PrizeText)
		{
			PrizeText->SetText(FText::FromString(TEXT("Claim failed. Try again.")));
		}
	}
}

UPHCHttpClient* UPHCScratchCardWidget::GetHttpClient() const
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	return GI ? GI->GetHttpClient() : nullptr;
}
