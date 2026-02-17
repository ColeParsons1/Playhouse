// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/KitchenChaos/KitchenGameMode.h"
#include "Games/KitchenChaos/KitchenHUD.h"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AKitchenGameMode::AKitchenGameMode()
{
	GameType = EPHCGameType::KitchenChaos;
	GameHUDClass = UKitchenHUD::StaticClass();
	MaxGameTime = 180.f; // 3-minute rounds.
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AKitchenGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void AKitchenGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// -- Update order timers --------------------------------------------------

	for (int32 i = ActiveOrders.Num() - 1; i >= 0; --i)
	{
		FPHCOrder& Order = ActiveOrders[i];
		if (Order.bComplete)
		{
			continue;
		}

		Order.TimeRemaining -= DeltaTime;
		if (Order.TimeRemaining <= 0.f)
		{
			OnOrderExpired(i);
		}
	}

	// -- Spawn new orders on timer --------------------------------------------

	OrderSpawnTimer += DeltaTime;
	if (OrderSpawnTimer >= CurrentOrderInterval && ActiveOrders.Num() < MaxActiveOrders)
	{
		SpawnOrder();
		OrderSpawnTimer = 0.f;

		// Gradually decrease spawn interval as the game progresses.
		CurrentOrderInterval = FMath::Max(
			MinOrderInterval,
			BaseOrderInterval - (GameTimer * 0.05f));
	}
}

// ---------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------

void AKitchenGameMode::StartGame()
{
	Super::StartGame();

	// Reset state.
	ActiveOrders.Empty();
	CurrentPlate.Empty();
	HeldItem = FPHCHeldItem();
	OrdersCompleted = 0;
	OrdersFailed = 0;
	OrderSpawnTimer = 0.f;
	CurrentOrderInterval = BaseOrderInterval;
	AvailableRecipes.Empty();

	// Initialize recipe definitions.
	InitializeRecipeBook();

	// Start with simpler recipes, unlock more over time.
	AvailableRecipes.Add(EPHCRecipe::Burger);
	AvailableRecipes.Add(EPHCRecipe::Salad);

	// Spawn the first batch of orders.
	const int32 InitialOrders = FMath::Min(2, MaxActiveOrders);
	for (int32 i = 0; i < InitialOrders; ++i)
	{
		SpawnOrder();
	}
}

// ---------------------------------------------------------------------------
// Recipe book
// ---------------------------------------------------------------------------

void AKitchenGameMode::InitializeRecipeBook()
{
	RecipeBook.Empty();

	// Burger: Bun + Patty (cooked)
	RecipeBook.Add(EPHCRecipe::Burger, {
		EPHCIngredient::Bun,
		EPHCIngredient::Patty
	});

	// Cheeseburger: Bun + Patty (cooked) + Cheese
	RecipeBook.Add(EPHCRecipe::Cheeseburger, {
		EPHCIngredient::Bun,
		EPHCIngredient::Patty,
		EPHCIngredient::Cheese
	});

	// Salad: Lettuce (chopped) + Tomato (chopped) + Onion (chopped)
	RecipeBook.Add(EPHCRecipe::Salad, {
		EPHCIngredient::Lettuce,
		EPHCIngredient::Tomato,
		EPHCIngredient::Onion
	});

	// Fish Taco: Bun + Fish (cooked) + Lettuce (chopped)
	RecipeBook.Add(EPHCRecipe::FishTaco, {
		EPHCIngredient::Bun,
		EPHCIngredient::Fish,
		EPHCIngredient::Lettuce
	});

	// Sushi: Rice + Fish + Nori
	RecipeBook.Add(EPHCRecipe::Sushi, {
		EPHCIngredient::Rice,
		EPHCIngredient::Fish,
		EPHCIngredient::Nori
	});

	// Deluxe Burger: Bun + Patty (cooked) + Cheese + Lettuce (chopped) + Tomato (chopped)
	RecipeBook.Add(EPHCRecipe::DeluxeBurger, {
		EPHCIngredient::Bun,
		EPHCIngredient::Patty,
		EPHCIngredient::Cheese,
		EPHCIngredient::Lettuce,
		EPHCIngredient::Tomato
	});
}

TArray<EPHCIngredient> AKitchenGameMode::GetRecipeIngredients(EPHCRecipe Recipe) const
{
	if (const TArray<EPHCIngredient>* Found = RecipeBook.Find(Recipe))
	{
		return *Found;
	}
	return TArray<EPHCIngredient>();
}

// ---------------------------------------------------------------------------
// Player actions
// ---------------------------------------------------------------------------

void AKitchenGameMode::PickupIngredient(EPHCIngredient Ingredient)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Can only pick up if hands are empty.
	if (HeldItem.bValid)
	{
		return;
	}

	HeldItem.Ingredient = Ingredient;
	HeldItem.bChopped = false;
	HeldItem.bCooked = false;
	HeldItem.bBurned = false;
	HeldItem.bValid = true;
}

void AKitchenGameMode::UseStation(EPHCCookingStation Station)
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	// Must be holding something.
	if (!HeldItem.bValid)
	{
		return;
	}

	switch (Station)
	{
	case EPHCCookingStation::Grill:
	{
		// Grill cooks meat and fish.
		if (HeldItem.Ingredient == EPHCIngredient::Patty ||
			HeldItem.Ingredient == EPHCIngredient::Fish)
		{
			if (!HeldItem.bCooked)
			{
				HeldItem.bCooked = true;
			}
			else
			{
				// Cooking an already cooked item burns it.
				HeldItem.bBurned = true;
			}
		}
		break;
	}

	case EPHCCookingStation::ChoppingBoard:
	{
		// Chop vegetables and lettuce.
		if (HeldItem.Ingredient == EPHCIngredient::Lettuce ||
			HeldItem.Ingredient == EPHCIngredient::Tomato ||
			HeldItem.Ingredient == EPHCIngredient::Onion)
		{
			HeldItem.bChopped = true;
		}
		break;
	}

	case EPHCCookingStation::FryingPan:
	{
		// Frying pan cooks items (alternative to grill).
		if (HeldItem.Ingredient == EPHCIngredient::Fish ||
			HeldItem.Ingredient == EPHCIngredient::Rice ||
			HeldItem.Ingredient == EPHCIngredient::Onion)
		{
			if (!HeldItem.bCooked)
			{
				HeldItem.bCooked = true;
			}
			else
			{
				HeldItem.bBurned = true;
			}
		}
		break;
	}

	case EPHCCookingStation::ServingCounter:
	{
		// Serving counter - place item on plate.
		PlaceOnPlate();
		break;
	}
	}
}

void AKitchenGameMode::PlaceOnPlate()
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	if (!HeldItem.bValid)
	{
		return;
	}

	// Don't place burned items (they're ruined).
	if (HeldItem.bBurned)
	{
		return;
	}

	// Check plate capacity.
	if (CurrentPlate.Num() >= MaxPlateItems)
	{
		return;
	}

	CurrentPlate.Add(HeldItem);

	// Clear the held item.
	HeldItem = FPHCHeldItem();
}

void AKitchenGameMode::SubmitOrder()
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	if (CurrentPlate.Num() == 0)
	{
		return;
	}

	// Try to match plate against active orders (first match wins).
	for (int32 i = 0; i < ActiveOrders.Num(); ++i)
	{
		FPHCOrder& Order = ActiveOrders[i];
		if (Order.bComplete)
		{
			continue;
		}

		if (CheckRecipeMatch(CurrentPlate, Order))
		{
			// Order fulfilled.
			Order.bComplete = true;
			OrdersCompleted++;

			// Calculate score: base score + time bonus.
			const float TimeRatio = FMath::Clamp(Order.TimeRemaining / Order.TimeLimit, 0.f, 1.f);
			const int32 TimeBonus = FMath::FloorToInt32(Order.BaseScore * TimeRatio * 0.5f);
			const int32 TotalScore = Order.BaseScore + TimeBonus;
			AddScore(TotalScore);

			// Broadcast order completion.
			OnOrderCompleted.Broadcast(TotalScore);

			// Remove the completed order.
			ActiveOrders.RemoveAt(i);

			// Clear plate.
			CurrentPlate.Empty();

			// Unlock more recipes based on orders completed.
			if (OrdersCompleted >= 2 && !AvailableRecipes.Contains(EPHCRecipe::Cheeseburger))
			{
				AvailableRecipes.Add(EPHCRecipe::Cheeseburger);
			}
			if (OrdersCompleted >= 4 && !AvailableRecipes.Contains(EPHCRecipe::FishTaco))
			{
				AvailableRecipes.Add(EPHCRecipe::FishTaco);
			}
			if (OrdersCompleted >= 6 && !AvailableRecipes.Contains(EPHCRecipe::Sushi))
			{
				AvailableRecipes.Add(EPHCRecipe::Sushi);
			}
			if (OrdersCompleted >= 8 && !AvailableRecipes.Contains(EPHCRecipe::DeluxeBurger))
			{
				AvailableRecipes.Add(EPHCRecipe::DeluxeBurger);
			}

			return;
		}
	}

	// No matching order found - plate doesn't match anything.
	// Player keeps the plate to try again or clear it.
}

void AKitchenGameMode::DropItem()
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	HeldItem = FPHCHeldItem();
}

void AKitchenGameMode::ClearPlate()
{
	if (!bGameActive || bGamePaused)
	{
		return;
	}

	CurrentPlate.Empty();
}

// ---------------------------------------------------------------------------
// Order management
// ---------------------------------------------------------------------------

void AKitchenGameMode::SpawnOrder()
{
	if (ActiveOrders.Num() >= MaxActiveOrders)
	{
		return;
	}

	if (AvailableRecipes.Num() == 0)
	{
		return;
	}

	// Pick a random recipe from available ones.
	const int32 RecipeIndex = FMath::RandRange(0, AvailableRecipes.Num() - 1);
	const EPHCRecipe Recipe = AvailableRecipes[RecipeIndex];

	FPHCOrder NewOrder;
	NewOrder.Recipe = Recipe;
	NewOrder.RequiredIngredients = GetRecipeIngredients(Recipe);
	NewOrder.bComplete = false;

	// Set time limit and score based on recipe complexity.
	switch (Recipe)
	{
	case EPHCRecipe::Burger:
		NewOrder.TimeLimit = 45.f;
		NewOrder.BaseScore = 40;
		break;
	case EPHCRecipe::Cheeseburger:
		NewOrder.TimeLimit = 50.f;
		NewOrder.BaseScore = 55;
		break;
	case EPHCRecipe::Salad:
		NewOrder.TimeLimit = 40.f;
		NewOrder.BaseScore = 45;
		break;
	case EPHCRecipe::FishTaco:
		NewOrder.TimeLimit = 55.f;
		NewOrder.BaseScore = 60;
		break;
	case EPHCRecipe::Sushi:
		NewOrder.TimeLimit = 50.f;
		NewOrder.BaseScore = 65;
		break;
	case EPHCRecipe::DeluxeBurger:
		NewOrder.TimeLimit = 65.f;
		NewOrder.BaseScore = 80;
		break;
	}

	// Reduce time limit slightly as the game progresses for extra pressure.
	const float TimePressure = FMath::Max(0.6f, 1.f - GameTimer * 0.002f);
	NewOrder.TimeLimit *= TimePressure;
	NewOrder.TimeRemaining = NewOrder.TimeLimit;

	ActiveOrders.Add(MoveTemp(NewOrder));
}

void AKitchenGameMode::OnOrderExpired(int32 OrderIndex)
{
	if (!ActiveOrders.IsValidIndex(OrderIndex))
	{
		return;
	}

	OrdersFailed++;
	ActiveOrders.RemoveAt(OrderIndex);

	// Broadcast failure.
	OnOrderFailed.Broadcast();
}

// ---------------------------------------------------------------------------
// Recipe matching
// ---------------------------------------------------------------------------

bool AKitchenGameMode::CheckRecipeMatch(const TArray<FPHCHeldItem>& Plate,
                                         const FPHCOrder& Order) const
{
	const TArray<EPHCIngredient>& Required = Order.RequiredIngredients;

	// Must have exactly the right number of ingredients.
	if (Plate.Num() != Required.Num())
	{
		return false;
	}

	// Track which required ingredients have been matched.
	TArray<bool> Matched;
	Matched.SetNumZeroed(Required.Num());

	for (const FPHCHeldItem& PlateItem : Plate)
	{
		// Burned items never match.
		if (PlateItem.bBurned)
		{
			return false;
		}

		bool bFoundMatch = false;

		for (int32 i = 0; i < Required.Num(); ++i)
		{
			if (Matched[i])
			{
				continue;
			}

			if (PlateItem.Ingredient != Required[i])
			{
				continue;
			}

			// Check processing requirements.
			if (NeedsChopping(Required[i]) && !PlateItem.bChopped)
			{
				continue;
			}
			if (NeedsCooking(Required[i]) && !PlateItem.bCooked)
			{
				continue;
			}

			Matched[i] = true;
			bFoundMatch = true;
			break;
		}

		if (!bFoundMatch)
		{
			return false;
		}
	}

	// All required ingredients must be matched.
	for (bool bWasMatched : Matched)
	{
		if (!bWasMatched)
		{
			return false;
		}
	}

	return true;
}

bool AKitchenGameMode::NeedsChopping(EPHCIngredient Ingredient) const
{
	switch (Ingredient)
	{
	case EPHCIngredient::Lettuce:
	case EPHCIngredient::Tomato:
	case EPHCIngredient::Onion:
		return true;
	default:
		return false;
	}
}

bool AKitchenGameMode::NeedsCooking(EPHCIngredient Ingredient) const
{
	switch (Ingredient)
	{
	case EPHCIngredient::Patty:
	case EPHCIngredient::Fish:
		return true;
	default:
		return false;
	}
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

FString AKitchenGameMode::GetIngredientName(EPHCIngredient Ingredient)
{
	switch (Ingredient)
	{
	case EPHCIngredient::Bun:		return TEXT("Bun");
	case EPHCIngredient::Patty:		return TEXT("Patty");
	case EPHCIngredient::Lettuce:	return TEXT("Lettuce");
	case EPHCIngredient::Tomato:	return TEXT("Tomato");
	case EPHCIngredient::Cheese:	return TEXT("Cheese");
	case EPHCIngredient::Onion:		return TEXT("Onion");
	case EPHCIngredient::Fish:		return TEXT("Fish");
	case EPHCIngredient::Rice:		return TEXT("Rice");
	case EPHCIngredient::Nori:		return TEXT("Nori");
	default:						return TEXT("Unknown");
	}
}

FString AKitchenGameMode::GetRecipeName(EPHCRecipe Recipe)
{
	switch (Recipe)
	{
	case EPHCRecipe::Burger:		return TEXT("Burger");
	case EPHCRecipe::Cheeseburger:	return TEXT("Cheeseburger");
	case EPHCRecipe::Salad:			return TEXT("Salad");
	case EPHCRecipe::FishTaco:		return TEXT("Fish Taco");
	case EPHCRecipe::Sushi:			return TEXT("Sushi");
	case EPHCRecipe::DeluxeBurger:	return TEXT("Deluxe Burger");
	default:						return TEXT("Unknown");
	}
}
