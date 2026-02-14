// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "KitchenGameMode.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Ingredient types available in the kitchen. */
UENUM(BlueprintType)
enum class EPHCIngredient : uint8
{
	Bun		UMETA(DisplayName = "Bun"),
	Patty	UMETA(DisplayName = "Patty"),
	Lettuce	UMETA(DisplayName = "Lettuce"),
	Tomato	UMETA(DisplayName = "Tomato"),
	Cheese	UMETA(DisplayName = "Cheese"),
	Onion	UMETA(DisplayName = "Onion"),
	Fish	UMETA(DisplayName = "Fish"),
	Rice	UMETA(DisplayName = "Rice"),
	Nori	UMETA(DisplayName = "Nori")
};

/** Cooking station types. */
UENUM(BlueprintType)
enum class EPHCCookingStation : uint8
{
	Grill			UMETA(DisplayName = "Grill"),
	ChoppingBoard	UMETA(DisplayName = "Chopping Board"),
	FryingPan		UMETA(DisplayName = "Frying Pan"),
	ServingCounter	UMETA(DisplayName = "Serving Counter")
};

/** Recipe types that can be ordered. */
UENUM(BlueprintType)
enum class EPHCRecipe : uint8
{
	Burger			UMETA(DisplayName = "Burger"),
	Cheeseburger	UMETA(DisplayName = "Cheeseburger"),
	Salad			UMETA(DisplayName = "Salad"),
	FishTaco		UMETA(DisplayName = "Fish Taco"),
	Sushi			UMETA(DisplayName = "Sushi"),
	DeluxeBurger	UMETA(DisplayName = "Deluxe Burger")
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** A customer order to be fulfilled. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCOrder
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	EPHCRecipe Recipe = EPHCRecipe::Burger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	TArray<EPHCIngredient> RequiredIngredients;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	float TimeLimit = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	float TimeRemaining = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	bool bComplete = false;

	/** Bonus score multiplier for fast completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	int32 BaseScore = 50;
};

/** An ingredient the player is holding or has placed on a plate. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCHeldItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	EPHCIngredient Ingredient = EPHCIngredient::Bun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	bool bChopped = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	bool bCooked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	bool bBurned = false;

	/** Whether this slot is occupied (used for HeldItem to distinguish empty hand). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Kitchen")
	bool bValid = false;
};

// ---------------------------------------------------------------------------
// AKitchenGameMode
// ---------------------------------------------------------------------------

/**
 * Overcooked-style cooking mini-game.
 *
 * Customer orders appear with countdown timers. The player must pick up
 * raw ingredients, process them at the correct stations (chop, grill, fry),
 * assemble them on a plate, and submit the plate before the order expires.
 *
 * Completing orders earns score; letting orders expire costs potential points.
 * The game runs for a fixed time; the goal is to complete as many orders as
 * possible.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API AKitchenGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	AKitchenGameMode();

	// -- Lifecycle ------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Game flow ------------------------------------------------------------

	virtual void StartGame() override;

	// -- Player actions -------------------------------------------------------

	/** Pick up a raw ingredient from the pantry. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Kitchen")
	void PickupIngredient(EPHCIngredient Ingredient);

	/** Use the held ingredient at a cooking station. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Kitchen")
	void UseStation(EPHCCookingStation Station);

	/** Place the held item onto the current plate. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Kitchen")
	void PlaceOnPlate();

	/** Submit the current plate to fulfill an order. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Kitchen")
	void SubmitOrder();

	/** Drop / discard the currently held item. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Kitchen")
	void DropItem();

	/** Clear all items from the current plate. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Kitchen")
	void ClearPlate();

	// -- Queries --------------------------------------------------------------

	/** Get the list of ingredients required for a recipe. */
	UFUNCTION(BlueprintPure, Category = "PHC|Kitchen")
	TArray<EPHCIngredient> GetRecipeIngredients(EPHCRecipe Recipe) const;

	/** Get the display name for an ingredient. */
	UFUNCTION(BlueprintPure, Category = "PHC|Kitchen")
	static FString GetIngredientName(EPHCIngredient Ingredient);

	/** Get the display name for a recipe. */
	UFUNCTION(BlueprintPure, Category = "PHC|Kitchen")
	static FString GetRecipeName(EPHCRecipe Recipe);

	// -- Configuration --------------------------------------------------------

	/** Maximum simultaneous active orders. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Kitchen")
	int32 MaxActiveOrders = 4;

	/** Maximum items that can be placed on a single plate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Kitchen")
	int32 MaxPlateItems = 6;

	/** Base interval between new order spawns (decreases over time). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Kitchen")
	float BaseOrderInterval = 12.f;

	/** Minimum interval between orders at peak difficulty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Kitchen")
	float MinOrderInterval = 5.f;

	// -- Runtime state --------------------------------------------------------

	/** Active customer orders awaiting fulfillment. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Kitchen")
	TArray<FPHCOrder> ActiveOrders;

	/** Items currently assembled on the plate. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Kitchen")
	TArray<FPHCHeldItem> CurrentPlate;

	/** The ingredient currently held by the player. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Kitchen")
	FPHCHeldItem HeldItem;

	/** Total orders completed this round. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Kitchen")
	int32 OrdersCompleted = 0;

	/** Total orders that expired (failed). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Kitchen")
	int32 OrdersFailed = 0;

	// -- Delegates ------------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOrderCompleted, int32, ScoreEarned);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOrderFailed);

	UPROPERTY(BlueprintAssignable, Category = "PHC|Kitchen")
	FOnOrderCompleted OnOrderCompleted;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Kitchen")
	FOnOrderFailed OnOrderFailed;

protected:
	// -- Internal helpers -----------------------------------------------------

	/** Initialize the recipe book mapping recipes to their ingredients. */
	void InitializeRecipeBook();

	/** Spawn a new random order and add it to the active list. */
	void SpawnOrder();

	/** Handle an expired order. */
	void OnOrderExpired(int32 OrderIndex);

	/**
	 * Check whether a plate's contents match an order.
	 * Validates ingredient types and their processing state.
	 */
	bool CheckRecipeMatch(const TArray<FPHCHeldItem>& Plate, const FPHCOrder& Order) const;

	/** Determine whether an ingredient needs to be chopped for a given recipe. */
	bool NeedsChopping(EPHCIngredient Ingredient) const;

	/** Determine whether an ingredient needs to be cooked for a given recipe. */
	bool NeedsCooking(EPHCIngredient Ingredient) const;

private:
	/** Maps each recipe to its required ingredients. */
	TMap<EPHCRecipe, TArray<EPHCIngredient>> RecipeBook;

	/** Timer tracking when to spawn the next order. */
	float OrderSpawnTimer = 0.f;

	/** Current order spawn interval (decreases over time). */
	float CurrentOrderInterval = 12.f;

	/** Available recipes that can be ordered (unlocked based on game progress). */
	TArray<EPHCRecipe> AvailableRecipes;
};
