// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCGardenWidget.generated.h"

class UUniformGridPanel;
class UScrollBox;
class UButton;
class UTextBlock;
class UImage;
class UBorder;
class UPHCHttpClient;

/**
 * UPHCGardenWidget
 *
 * A 3x3 plot garden mini-game. Players can plant seeds, water growing plants,
 * and harvest ready crops. Growth progresses in real time and the UI updates
 * via a periodic timer to reflect progress visually.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCGardenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCGardenWidget(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	// -- Bind Widgets ----------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> GardenGrid;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> PlantSelector;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> WaterAllButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> HarvestAllButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> GardenInfoText;

	// -- Data ------------------------------------------------------------------

	/** The 9 garden plots (indexed 0-8, row-major). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Garden")
	TArray<FPHCGardenPlot> Plots;

	/** Available plant types the player can choose from. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Garden")
	TArray<FString> AvailablePlantTypes;

	/** Currently selected plant type for planting into an empty plot. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Garden")
	FString SelectedPlantType;

	// -- Methods ---------------------------------------------------------------

	/** Fetch garden state from the backend and populate the grid. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void LoadGarden();

	/** Handle a click on a specific plot (context-sensitive action). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void OnPlotClicked(int32 PlotIndex);

	/** Plant a seed in the specified plot. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void PlantSeed(int32 PlotIndex, const FString& PlantType);

	/** Water a growing plant. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void WaterPlant(int32 PlotIndex);

	/** Harvest a ready plant and receive rewards. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void HarvestPlant(int32 PlotIndex);

	/** Update the visual appearance of a single plot. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void UpdatePlotVisual(int32 PlotIndex, const FPHCGardenPlot& Plot);

	/** Water all currently growing plants. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void OnWaterAllClicked();

	/** Harvest all plants that are ready. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Garden")
	void OnHarvestAllClicked();

private:
	// -- Internal helpers ------------------------------------------------------

	/** Build the 3x3 grid of plot buttons. */
	void BuildGardenGrid();

	/** Build the plant-type selector scroll box. */
	void BuildPlantSelector();

	/** Periodic timer callback to update growth progress visuals. */
	void OnGrowthTimerTick();

	/** Callback after the GetGarden API returns. */
	UFUNCTION()
	void OnGardenDataReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Callback after PlantSeed API returns. */
	UFUNCTION()
	void OnPlantSeedResult(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Callback after WaterPlant API returns. */
	UFUNCTION()
	void OnWaterPlantResult(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Callback after HarvestPlant API returns. */
	UFUNCTION()
	void OnHarvestPlantResult(bool bSuccess, int32 StatusCode, const FString& JsonBody);

	/** Convenience accessor for the HttpClient via GameInstance. */
	UPHCHttpClient* GetHttpClient() const;

	/** Get a color representing the plant state for visual display. */
	static FLinearColor GetPlotColor(const FPHCGardenPlot& Plot);

	/** Get a label string representing the plot state. */
	static FString GetPlotLabel(const FPHCGardenPlot& Plot);

	/** Handle for the growth progress update timer. */
	FTimerHandle GrowthTimerHandle;

	/** Plot buttons indexed by plot index for direct access. */
	UPROPERTY()
	TArray<TObjectPtr<UButton>> PlotButtons;

	/** Text labels inside plot buttons. */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> PlotLabels;

	/** Border wrappers around plot buttons for coloring. */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> PlotBorders;

	/** Track which plot index a pending API call is for. */
	int32 PendingPlotIndex = -1;

	/** Number of garden plots (always 9). */
	static constexpr int32 NumPlots = 9;

	/** Number of columns in the grid. */
	static constexpr int32 GridColumns = 3;

	/** Growth timer interval in seconds. */
	static constexpr float GrowthTickInterval = 5.0f;

	/** Default available plant types. */
	static const TArray<FString> DefaultPlantTypes;
};
