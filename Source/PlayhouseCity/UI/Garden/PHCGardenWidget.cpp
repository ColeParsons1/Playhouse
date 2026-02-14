// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Garden/PHCGardenWidget.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "TimerManager.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Static data
// ---------------------------------------------------------------------------

const TArray<FString> UPHCGardenWidget::DefaultPlantTypes = {
	TEXT("Tomato"),
	TEXT("Sunflower"),
	TEXT("Carrot"),
	TEXT("Blueberry"),
	TEXT("Pumpkin"),
	TEXT("Rose")
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCGardenWidget::UPHCGardenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize 9 empty plots.
	Plots.SetNum(NumPlots);
	for (int32 i = 0; i < NumPlots; ++i)
	{
		Plots[i].PlotIndex = i;
		Plots[i].State = EPHCPlantState::Empty;
	}

	AvailablePlantTypes = DefaultPlantTypes;
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCGardenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button callbacks.
	if (WaterAllButton)
	{
		WaterAllButton->OnClicked.AddDynamic(this, &UPHCGardenWidget::OnWaterAllClicked);
	}
	if (HarvestAllButton)
	{
		HarvestAllButton->OnClicked.AddDynamic(this, &UPHCGardenWidget::OnHarvestAllClicked);
	}

	// Build the UI grid and plant selector.
	BuildGardenGrid();
	BuildPlantSelector();

	// Load garden state from the backend.
	LoadGarden();

	// Start a periodic timer to update growth visuals.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			GrowthTimerHandle,
			FTimerDelegate::CreateUObject(this, &UPHCGardenWidget::OnGrowthTimerTick),
			GrowthTickInterval,
			/*bLoop=*/ true
		);
	}
}

void UPHCGardenWidget::NativeDestruct()
{
	// Clear the timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrowthTimerHandle);
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------

void UPHCGardenWidget::LoadGarden()
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCGarden: HttpClient unavailable."));
		return;
	}

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCGardenWidget::OnGardenDataReceived);
	Http->GetGarden(Callback);
}

void UPHCGardenWidget::OnPlotClicked(int32 PlotIndex)
{
	if (PlotIndex < 0 || PlotIndex >= Plots.Num())
	{
		return;
	}

	const FPHCGardenPlot& Plot = Plots[PlotIndex];

	switch (Plot.State)
	{
		case EPHCPlantState::Empty:
		{
			if (SelectedPlantType.IsEmpty())
			{
				UE_LOG(LogTemp, Log, TEXT("PHCGarden: No plant type selected. Select one first."));
				if (GardenInfoText)
				{
					GardenInfoText->SetText(FText::FromString(TEXT("Select a plant type first!")));
				}
				return;
			}
			PlantSeed(PlotIndex, SelectedPlantType);
			break;
		}
		case EPHCPlantState::Planted:
		case EPHCPlantState::Growing:
		{
			WaterPlant(PlotIndex);
			break;
		}
		case EPHCPlantState::Ready:
		{
			HarvestPlant(PlotIndex);
			break;
		}
		case EPHCPlantState::Wilted:
		{
			// Clear the wilted plot so it can be replanted.
			Plots[PlotIndex].State = EPHCPlantState::Empty;
			Plots[PlotIndex].PlantType.Empty();
			Plots[PlotIndex].GrowthProgress = 0.f;
			UpdatePlotVisual(PlotIndex, Plots[PlotIndex]);
			break;
		}
	}
}

void UPHCGardenWidget::PlantSeed(int32 PlotIndex, const FString& PlantType)
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		return;
	}

	PendingPlotIndex = PlotIndex;

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCGardenWidget::OnPlantSeedResult);
	Http->PlantSeed(PlotIndex, PlantType, Callback);
}

void UPHCGardenWidget::WaterPlant(int32 PlotIndex)
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		return;
	}

	PendingPlotIndex = PlotIndex;

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCGardenWidget::OnWaterPlantResult);
	Http->WaterPlant(PlotIndex, Callback);
}

void UPHCGardenWidget::HarvestPlant(int32 PlotIndex)
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		return;
	}

	PendingPlotIndex = PlotIndex;

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCGardenWidget::OnHarvestPlantResult);
	Http->HarvestPlant(PlotIndex, Callback);
}

void UPHCGardenWidget::UpdatePlotVisual(int32 PlotIndex, const FPHCGardenPlot& Plot)
{
	if (PlotIndex < 0 || PlotIndex >= PlotBorders.Num() || PlotIndex >= PlotLabels.Num())
	{
		return;
	}

	// Update border color based on state.
	if (PlotBorders[PlotIndex])
	{
		PlotBorders[PlotIndex]->SetBrushColor(GetPlotColor(Plot));
	}

	// Update label text.
	if (PlotLabels[PlotIndex])
	{
		PlotLabels[PlotIndex]->SetText(FText::FromString(GetPlotLabel(Plot)));
	}
}

void UPHCGardenWidget::OnWaterAllClicked()
{
	for (int32 i = 0; i < Plots.Num(); ++i)
	{
		if (Plots[i].State == EPHCPlantState::Planted || Plots[i].State == EPHCPlantState::Growing)
		{
			WaterPlant(i);
		}
	}

	if (GardenInfoText)
	{
		GardenInfoText->SetText(FText::FromString(TEXT("Watered all growing plants!")));
	}
}

void UPHCGardenWidget::OnHarvestAllClicked()
{
	for (int32 i = 0; i < Plots.Num(); ++i)
	{
		if (Plots[i].State == EPHCPlantState::Ready)
		{
			HarvestPlant(i);
		}
	}

	if (GardenInfoText)
	{
		GardenInfoText->SetText(FText::FromString(TEXT("Harvested all ready plants!")));
	}
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UPHCGardenWidget::BuildGardenGrid()
{
	if (!GardenGrid)
	{
		return;
	}

	GardenGrid->ClearChildren();
	PlotButtons.Empty(NumPlots);
	PlotLabels.Empty(NumPlots);
	PlotBorders.Empty(NumPlots);

	for (int32 i = 0; i < NumPlots; ++i)
	{
		const int32 Row = i / GridColumns;
		const int32 Col = i % GridColumns;

		// Create a border to serve as the plot background.
		UBorder* PlotBorder = NewObject<UBorder>(this);
		if (!PlotBorder)
		{
			continue;
		}

		PlotBorder->SetBrushColor(FLinearColor(0.3f, 0.2f, 0.1f, 1.0f)); // Soil color for empty.
		PlotBorder->SetPadding(FMargin(4.f));

		// Create button inside the border.
		UButton* PlotBtn = NewObject<UButton>(PlotBorder);
		if (!PlotBtn)
		{
			continue;
		}

		// Create label inside the button.
		UTextBlock* Label = NewObject<UTextBlock>(PlotBtn);
		if (Label)
		{
			Label->SetText(FText::FromString(TEXT("Empty")));
			FSlateFontInfo FontInfo = Label->GetFont();
			FontInfo.Size = 11;
			Label->SetFont(FontInfo);
			Label->SetJustification(ETextJustify::Center);
			PlotBtn->AddChild(Label);
		}

		PlotBorder->AddChild(PlotBtn);

		// Add to the uniform grid at the correct row/column.
		UUniformGridSlot* GridSlot = GardenGrid->AddChildToUniformGrid(PlotBorder, Row, Col);
		if (GridSlot)
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Store references for later updates.
		PlotButtons.Add(PlotBtn);
		PlotLabels.Add(Label);
		PlotBorders.Add(PlotBorder);
	}
}

void UPHCGardenWidget::BuildPlantSelector()
{
	if (!PlantSelector)
	{
		return;
	}

	PlantSelector->ClearChildren();

	for (const FString& PlantType : AvailablePlantTypes)
	{
		UButton* PlantBtn = NewObject<UButton>(this);
		if (!PlantBtn)
		{
			continue;
		}

		UTextBlock* Label = NewObject<UTextBlock>(PlantBtn);
		if (Label)
		{
			Label->SetText(FText::FromString(PlantType));
			FSlateFontInfo FontInfo = Label->GetFont();
			FontInfo.Size = 12;
			Label->SetFont(FontInfo);
			PlantBtn->AddChild(Label);
		}

		PlantSelector->AddChild(PlantBtn);
	}

	// Default selection.
	if (AvailablePlantTypes.Num() > 0)
	{
		SelectedPlantType = AvailablePlantTypes[0];
	}
}

void UPHCGardenWidget::OnGrowthTimerTick()
{
	// Update growth progress visuals for all active plots.
	const FDateTime Now = FDateTime::UtcNow();

	for (int32 i = 0; i < Plots.Num(); ++i)
	{
		FPHCGardenPlot& Plot = Plots[i];

		if (Plot.State == EPHCPlantState::Planted || Plot.State == EPHCPlantState::Growing)
		{
			// Estimate growth progress based on time elapsed since planting.
			const FTimespan Elapsed = Now - Plot.PlantedTime;
			const float ElapsedMinutes = static_cast<float>(Elapsed.GetTotalMinutes());

			// Assume a base growth time of 30 minutes for full maturity.
			static constexpr float BaseGrowthMinutes = 30.0f;
			Plot.GrowthProgress = FMath::Clamp(ElapsedMinutes / BaseGrowthMinutes, 0.0f, 1.0f);

			if (Plot.GrowthProgress >= 1.0f)
			{
				Plot.State = EPHCPlantState::Ready;
			}
			else if (Plot.GrowthProgress > 0.0f)
			{
				Plot.State = EPHCPlantState::Growing;
			}

			UpdatePlotVisual(i, Plot);
		}
	}
}

void UPHCGardenWidget::OnGardenDataReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCGarden: Failed to load garden. Status=%d"), StatusCode);
		if (GardenInfoText)
		{
			GardenInfoText->SetText(FText::FromString(TEXT("Failed to load garden data.")));
		}
		return;
	}

	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCGarden: Failed to parse garden JSON."));
		return;
	}

	// Parse plots array.
	const TArray<TSharedPtr<FJsonValue>>* PlotsArray = nullptr;
	if (RootObj->TryGetArrayField(TEXT("plots"), PlotsArray) && PlotsArray)
	{
		for (const TSharedPtr<FJsonValue>& Val : *PlotsArray)
		{
			const TSharedPtr<FJsonObject>* PlotObj = nullptr;
			if (!Val->TryGetObject(PlotObj) || !PlotObj || !(*PlotObj).IsValid())
			{
				continue;
			}

			const int32 Index = static_cast<int32>((*PlotObj)->GetNumberField(TEXT("plot_index")));
			if (Index < 0 || Index >= NumPlots)
			{
				continue;
			}

			FPHCGardenPlot& Plot = Plots[Index];
			Plot.PlotIndex = Index;
			Plot.PlantType = (*PlotObj)->GetStringField(TEXT("plant_type"));
			Plot.GrowthProgress = static_cast<float>((*PlotObj)->GetNumberField(TEXT("growth_progress")));

			// Parse plant state.
			const FString StateStr = (*PlotObj)->GetStringField(TEXT("state"));
			if (StateStr == TEXT("planted"))
			{
				Plot.State = EPHCPlantState::Planted;
			}
			else if (StateStr == TEXT("growing"))
			{
				Plot.State = EPHCPlantState::Growing;
			}
			else if (StateStr == TEXT("ready"))
			{
				Plot.State = EPHCPlantState::Ready;
			}
			else if (StateStr == TEXT("wilted"))
			{
				Plot.State = EPHCPlantState::Wilted;
			}
			else
			{
				Plot.State = EPHCPlantState::Empty;
			}

			// Parse planted time.
			FString PlantedTimeStr;
			if ((*PlotObj)->TryGetStringField(TEXT("planted_time"), PlantedTimeStr) && !PlantedTimeStr.IsEmpty())
			{
				FDateTime::ParseIso8601(*PlantedTimeStr, Plot.PlantedTime);
			}

			UpdatePlotVisual(Index, Plot);
		}
	}

	// Parse available plant types if the server provides them.
	const TArray<TSharedPtr<FJsonValue>>* PlantTypesArray = nullptr;
	if (RootObj->TryGetArrayField(TEXT("available_plants"), PlantTypesArray) && PlantTypesArray)
	{
		AvailablePlantTypes.Empty();
		for (const TSharedPtr<FJsonValue>& Val : *PlantTypesArray)
		{
			FString TypeName;
			if (Val->TryGetString(TypeName))
			{
				AvailablePlantTypes.Add(TypeName);
			}
		}
		BuildPlantSelector();
	}

	if (GardenInfoText)
	{
		GardenInfoText->SetText(FText::FromString(TEXT("Garden loaded. Tap a plot to interact.")));
	}
}

void UPHCGardenWidget::OnPlantSeedResult(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (PendingPlotIndex < 0 || PendingPlotIndex >= Plots.Num())
	{
		PendingPlotIndex = -1;
		return;
	}

	if (bSuccess)
	{
		FPHCGardenPlot& Plot = Plots[PendingPlotIndex];
		Plot.State = EPHCPlantState::Planted;
		Plot.PlantType = SelectedPlantType;
		Plot.GrowthProgress = 0.0f;
		Plot.PlantedTime = FDateTime::UtcNow();
		UpdatePlotVisual(PendingPlotIndex, Plot);

		if (GardenInfoText)
		{
			GardenInfoText->SetText(FText::FromString(
				FString::Printf(TEXT("Planted %s in plot %d!"), *SelectedPlantType, PendingPlotIndex + 1)));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCGarden: PlantSeed failed for plot %d. Status=%d"), PendingPlotIndex, StatusCode);
	}

	PendingPlotIndex = -1;
}

void UPHCGardenWidget::OnWaterPlantResult(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (PendingPlotIndex < 0 || PendingPlotIndex >= Plots.Num())
	{
		PendingPlotIndex = -1;
		return;
	}

	if (bSuccess)
	{
		FPHCGardenPlot& Plot = Plots[PendingPlotIndex];
		Plot.State = EPHCPlantState::Growing;

		// Parse updated growth progress from response if available.
		TSharedPtr<FJsonObject> JsonObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			if (JsonObj->HasField(TEXT("growth_progress")))
			{
				Plot.GrowthProgress = static_cast<float>(JsonObj->GetNumberField(TEXT("growth_progress")));
			}
		}

		UpdatePlotVisual(PendingPlotIndex, Plot);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCGarden: WaterPlant failed for plot %d. Status=%d"), PendingPlotIndex, StatusCode);
	}

	PendingPlotIndex = -1;
}

void UPHCGardenWidget::OnHarvestPlantResult(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (PendingPlotIndex < 0 || PendingPlotIndex >= Plots.Num())
	{
		PendingPlotIndex = -1;
		return;
	}

	if (bSuccess)
	{
		FPHCGardenPlot& Plot = Plots[PendingPlotIndex];

		// Parse reward from response.
		int32 CoinsEarned = 0;
		TSharedPtr<FJsonObject> JsonObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			CoinsEarned = static_cast<int32>(JsonObj->GetNumberField(TEXT("coins_earned")));

			// Update the GameInstance balance.
			UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
			if (GI && JsonObj->HasField(TEXT("total_coins")))
			{
				GI->LocalPlayerProfile.Coins = static_cast<int32>(JsonObj->GetNumberField(TEXT("total_coins")));
			}
		}

		// Reset the plot to empty.
		Plot.State = EPHCPlantState::Empty;
		Plot.PlantType.Empty();
		Plot.GrowthProgress = 0.0f;
		UpdatePlotVisual(PendingPlotIndex, Plot);

		if (GardenInfoText)
		{
			GardenInfoText->SetText(FText::FromString(
				FString::Printf(TEXT("Harvested plot %d! Earned %d coins."), PendingPlotIndex + 1, CoinsEarned)));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCGarden: HarvestPlant failed for plot %d. Status=%d"), PendingPlotIndex, StatusCode);
	}

	PendingPlotIndex = -1;
}

UPHCHttpClient* UPHCGardenWidget::GetHttpClient() const
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	return GI ? GI->GetHttpClient() : nullptr;
}

FLinearColor UPHCGardenWidget::GetPlotColor(const FPHCGardenPlot& Plot)
{
	switch (Plot.State)
	{
		case EPHCPlantState::Empty:
			return FLinearColor(0.3f, 0.2f, 0.1f, 1.0f);   // Brown soil
		case EPHCPlantState::Planted:
			return FLinearColor(0.2f, 0.5f, 0.2f, 1.0f);   // Light green
		case EPHCPlantState::Growing:
			return FLinearColor(0.1f, 0.7f, 0.1f, 1.0f);   // Vibrant green
		case EPHCPlantState::Ready:
			return FLinearColor(1.0f, 0.84f, 0.0f, 1.0f);  // Gold (harvest)
		case EPHCPlantState::Wilted:
			return FLinearColor(0.5f, 0.3f, 0.2f, 1.0f);   // Brownish red
		default:
			return FLinearColor(0.3f, 0.2f, 0.1f, 1.0f);
	}
}

FString UPHCGardenWidget::GetPlotLabel(const FPHCGardenPlot& Plot)
{
	switch (Plot.State)
	{
		case EPHCPlantState::Empty:
			return TEXT("Empty");
		case EPHCPlantState::Planted:
			return FString::Printf(TEXT("%s\nPlanted"), *Plot.PlantType);
		case EPHCPlantState::Growing:
			return FString::Printf(TEXT("%s\n%d%%"), *Plot.PlantType,
				FMath::RoundToInt(Plot.GrowthProgress * 100.0f));
		case EPHCPlantState::Ready:
			return FString::Printf(TEXT("%s\nReady!"), *Plot.PlantType);
		case EPHCPlantState::Wilted:
			return TEXT("Wilted");
		default:
			return TEXT("---");
	}
}
