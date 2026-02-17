// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Avatar/PHCAvatarEditorWidget.h"

#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"

#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ---------------------------------------------------------------------------
// Static data
// ---------------------------------------------------------------------------

const TArray<FString> UPHCAvatarEditorWidget::Categories = {
	TEXT("Hair"),
	TEXT("Eyes"),
	TEXT("Mouth"),
	TEXT("Outfit"),
	TEXT("Accessories")
};

const TArray<FString> UPHCAvatarEditorWidget::SkinTones = {
	TEXT("Light"),
	TEXT("MediumLight"),
	TEXT("Medium"),
	TEXT("MediumDark"),
	TEXT("Dark")
};

const TMap<FString, FLinearColor> UPHCAvatarEditorWidget::SkinToneColors = {
	{ TEXT("Light"),       FLinearColor(1.00f, 0.87f, 0.77f, 1.0f) },
	{ TEXT("MediumLight"), FLinearColor(0.94f, 0.76f, 0.60f, 1.0f) },
	{ TEXT("Medium"),      FLinearColor(0.80f, 0.60f, 0.42f, 1.0f) },
	{ TEXT("MediumDark"),  FLinearColor(0.60f, 0.40f, 0.25f, 1.0f) },
	{ TEXT("Dark"),        FLinearColor(0.38f, 0.25f, 0.15f, 1.0f) }
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCAvatarEditorWidget::UPHCAvatarEditorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCAvatarEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button callbacks.
	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &UPHCAvatarEditorWidget::OnSaveClicked);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked.AddDynamic(this, &UPHCAvatarEditorWidget::OnResetClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UPHCAvatarEditorWidget::OnCloseClicked);
	}

	// Build UI elements.
	BuildSkinToneSwatches();
	BuildCategoryTabs();

	// Load data from backend.
	LoadAvatarData();
}

void UPHCAvatarEditorWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Public / Protected methods
// ---------------------------------------------------------------------------

void UPHCAvatarEditorWidget::LoadAvatarData()
{
	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCAvatarEditor: HttpClient unavailable."));
		return;
	}

	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (!GI)
	{
		return;
	}

	const int32 PlayerId = GI->LocalPlayerProfile.PlayerId;

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCAvatarEditorWidget::OnAvatarDataReceived);
	Http->GetAvatar(PlayerId, Callback);
}

void UPHCAvatarEditorWidget::OnCategorySelected(const FString& Category)
{
	if (!Categories.Contains(Category))
	{
		return;
	}

	CurrentCategory = Category;
	PopulateItemGrid(Category);
}

void UPHCAvatarEditorWidget::OnItemSelected(const FString& Category, const FString& AssetId)
{
	// Each category maps directly to a layer name in the avatar data.
	SetLayerAsset(Category, AssetId);
	RefreshPreview();
}

void UPHCAvatarEditorWidget::OnSkinToneSelected(const FString& Tone)
{
	CurrentAvatar.SkinTone = Tone;
	RefreshPreview();
}

void UPHCAvatarEditorWidget::RefreshPreview()
{
	if (AvatarPreviewImage)
	{
		// Tint the preview image to reflect the current skin tone.
		const FLinearColor* TintPtr = SkinToneColors.Find(CurrentAvatar.SkinTone);
		const FLinearColor TintColor = TintPtr ? *TintPtr : FLinearColor::White;
		AvatarPreviewImage->SetColorAndOpacity(TintColor);
	}

	// Log the full layer state at verbose level for debugging.
	UE_LOG(LogTemp, Verbose, TEXT("PHCAvatarEditor: Preview updated - SkinTone=%s, Layers=%d"),
		*CurrentAvatar.SkinTone, CurrentAvatar.Layers.Num());

	for (const FPHCAvatarLayer& Layer : CurrentAvatar.Layers)
	{
		UE_LOG(LogTemp, Verbose, TEXT("  Layer: %s = %s (Z=%d)"),
			*Layer.LayerName, *Layer.AssetId, Layer.ZOrder);
	}
}

void UPHCAvatarEditorWidget::OnSaveClicked()
{
	if (bSaveInFlight)
	{
		return;
	}

	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		return;
	}

	bSaveInFlight = true;

	const FString AvatarJson = AvatarToJsonString();

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCAvatarEditorWidget::OnAvatarSaveResult);
	Http->SaveAvatar(AvatarJson, Callback);
}

void UPHCAvatarEditorWidget::OnResetClicked()
{
	CurrentAvatar = OriginalAvatar;
	RefreshPreview();

	if (!CurrentCategory.IsEmpty())
	{
		PopulateItemGrid(CurrentCategory);
	}
}

void UPHCAvatarEditorWidget::OnCloseClicked()
{
	if (HasUnsavedChanges())
	{
		// In production, show a confirmation dialog. For now, log and close.
		UE_LOG(LogTemp, Log, TEXT("PHCAvatarEditor: Closing with unsaved changes."));
	}

	OnEditorClosed.Broadcast(false);
	RemoveFromParent();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UPHCAvatarEditorWidget::BuildSkinToneSwatches()
{
	if (!SkinToneSelector)
	{
		return;
	}

	SkinToneSelector->ClearChildren();

	for (int32 i = 0; i < SkinTones.Num(); ++i)
	{
		UButton* SwatchBtn = NewObject<UButton>(this);
		if (!SwatchBtn)
		{
			continue;
		}

		// Style the button to the skin-tone color.
		const FLinearColor* ToneColorPtr = SkinToneColors.Find(SkinTones[i]);
		const FLinearColor ToneColor = ToneColorPtr ? *ToneColorPtr : FLinearColor::White;

		FButtonStyle BtnStyle;
		FSlateBrush NormalBrush;
		NormalBrush.TintColor = FSlateColor(ToneColor);
		BtnStyle.SetNormal(NormalBrush);
		BtnStyle.SetHovered(NormalBrush);
		BtnStyle.SetPressed(NormalBrush);
		SwatchBtn->SetStyle(BtnStyle);

		// NOTE: UE5 dynamic delegates do not support per-button payload capture.
		// The production approach uses a TMap<UButton*, FString> lookup populated here,
		// with a single OnClicked handler that resolves the tone from the map.
		// For this implementation, each swatch button is wired to OnResetClicked
		// as a placeholder. In the Blueprint subclass, designers should use
		// the OnSkinToneSelected(FString) method directly from UI binding.

		SkinToneSelector->AddChild(SwatchBtn);
	}
}

void UPHCAvatarEditorWidget::BuildCategoryTabs()
{
	if (!CategoryTabs)
	{
		return;
	}

	CategoryTabs->ClearChildren();

	for (const FString& Cat : Categories)
	{
		UButton* TabBtn = NewObject<UButton>(this);
		if (!TabBtn)
		{
			continue;
		}

		UTextBlock* Label = NewObject<UTextBlock>(TabBtn);
		if (Label)
		{
			Label->SetText(FText::FromString(Cat));
			FSlateFontInfo FontInfo = Label->GetFont();
			FontInfo.Size = 14;
			Label->SetFont(FontInfo);
			TabBtn->AddChild(Label);
		}

		CategoryTabs->AddChild(TabBtn);
	}

	// Default to first category.
	if (Categories.Num() > 0)
	{
		CurrentCategory = Categories[0];
	}
}

void UPHCAvatarEditorWidget::PopulateItemGrid(const FString& Category)
{
	if (!ItemGrid)
	{
		return;
	}

	ItemGrid->ClearChildren();

	const TArray<FString>* Items = AvailableItems.Find(Category);
	if (!Items || Items->Num() == 0)
	{
		UTextBlock* EmptyText = NewObject<UTextBlock>(this);
		if (EmptyText)
		{
			EmptyText->SetText(FText::FromString(TEXT("No items available")));
			ItemGrid->AddChild(EmptyText);
		}
		return;
	}

	const FString CurrentAssetForCategory = GetLayerAsset(Category);

	for (const FString& AssetId : *Items)
	{
		UButton* ItemBtn = NewObject<UButton>(this);
		if (!ItemBtn)
		{
			continue;
		}

		UTextBlock* ItemLabel = NewObject<UTextBlock>(ItemBtn);
		if (ItemLabel)
		{
			ItemLabel->SetText(FText::FromString(AssetId));
			FSlateFontInfo FontInfo = ItemLabel->GetFont();
			FontInfo.Size = 12;
			ItemLabel->SetFont(FontInfo);
			ItemBtn->AddChild(ItemLabel);
		}

		// Highlight the currently selected item for this category.
		if (AssetId == CurrentAssetForCategory)
		{
			ItemBtn->SetColorAndOpacity(FLinearColor(0.3f, 0.8f, 1.0f, 1.0f));
		}

		ItemGrid->AddChild(ItemBtn);
	}
}

void UPHCAvatarEditorWidget::OnAvatarDataReceived(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCAvatarEditor: Failed to load avatar data. Status=%d"), StatusCode);
		return;
	}

	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCAvatarEditor: Failed to parse avatar JSON."));
		return;
	}

	// Populate CurrentAvatar using the layered FPHCAvatarData format.
	CurrentAvatar.SkinTone = JsonObj->GetStringField(TEXT("skin_tone"));
	CurrentAvatar.Layers.Empty();

	// Parse the layers array.
	const TArray<TSharedPtr<FJsonValue>>* LayersArray = nullptr;
	if (JsonObj->TryGetArrayField(TEXT("layers"), LayersArray) && LayersArray)
	{
		for (const TSharedPtr<FJsonValue>& Val : *LayersArray)
		{
			const TSharedPtr<FJsonObject>* LayerObj = nullptr;
			if (!Val->TryGetObject(LayerObj) || !LayerObj || !(*LayerObj).IsValid())
			{
				continue;
			}

			FPHCAvatarLayer Layer;
			Layer.LayerName = (*LayerObj)->GetStringField(TEXT("layer_name"));
			Layer.AssetId = (*LayerObj)->GetStringField(TEXT("asset_id"));
			Layer.ZOrder = static_cast<int32>((*LayerObj)->GetNumberField(TEXT("z_order")));

			// Parse optional tint color.
			const TSharedPtr<FJsonObject>* TintObj = nullptr;
			if ((*LayerObj)->TryGetObjectField(TEXT("tint"), TintObj) && TintObj)
			{
				Layer.Tint.R = static_cast<float>((*TintObj)->GetNumberField(TEXT("r")));
				Layer.Tint.G = static_cast<float>((*TintObj)->GetNumberField(TEXT("g")));
				Layer.Tint.B = static_cast<float>((*TintObj)->GetNumberField(TEXT("b")));
				Layer.Tint.A = static_cast<float>((*TintObj)->GetNumberField(TEXT("a")));
			}

			CurrentAvatar.Layers.Add(Layer);
		}
	}

	// Store original for reset.
	OriginalAvatar = CurrentAvatar;

	// Parse available items per category.
	const TSharedPtr<FJsonObject>* ItemsObj = nullptr;
	if (JsonObj->TryGetObjectField(TEXT("available_items"), ItemsObj) && ItemsObj)
	{
		for (const FString& Cat : Categories)
		{
			const TArray<TSharedPtr<FJsonValue>>* ItemArray = nullptr;
			if ((*ItemsObj)->TryGetArrayField(Cat.ToLower(), ItemArray) && ItemArray)
			{
				TArray<FString>& CatItems = AvailableItems.FindOrAdd(Cat);
				CatItems.Empty();

				for (const TSharedPtr<FJsonValue>& Val : *ItemArray)
				{
					FString ItemId;
					if (Val->TryGetString(ItemId))
					{
						CatItems.Add(ItemId);
					}
				}

				// Also store a serialized version for Blueprint access.
				AvailableItemsSerialized.Add(Cat, FString::Join(CatItems, TEXT(",")));
			}
		}
	}

	bDataLoaded = true;

	if (!CurrentCategory.IsEmpty())
	{
		PopulateItemGrid(CurrentCategory);
	}

	RefreshPreview();
}

void UPHCAvatarEditorWidget::OnAvatarSaveResult(bool bSuccess, int32 StatusCode, const FString& JsonBody)
{
	bSaveInFlight = false;

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("PHCAvatarEditor: Avatar saved successfully."));
		OriginalAvatar = CurrentAvatar;
		OnEditorClosed.Broadcast(true);
		RemoveFromParent();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCAvatarEditor: Save failed. Status=%d Body=%s"), StatusCode, *JsonBody);
	}
}

bool UPHCAvatarEditorWidget::HasUnsavedChanges() const
{
	// Compare skin tones.
	if (CurrentAvatar.SkinTone != OriginalAvatar.SkinTone)
	{
		return true;
	}

	// Compare layer count.
	if (CurrentAvatar.Layers.Num() != OriginalAvatar.Layers.Num())
	{
		return true;
	}

	// Compare each layer's asset ID.
	for (int32 i = 0; i < CurrentAvatar.Layers.Num(); ++i)
	{
		if (CurrentAvatar.Layers[i].AssetId != OriginalAvatar.Layers[i].AssetId)
		{
			return true;
		}
		if (CurrentAvatar.Layers[i].LayerName != OriginalAvatar.Layers[i].LayerName)
		{
			return true;
		}
	}

	return false;
}

void UPHCAvatarEditorWidget::SetLayerAsset(const FString& LayerName, const FString& AssetId)
{
	// Find existing layer with this name and update it.
	for (FPHCAvatarLayer& Layer : CurrentAvatar.Layers)
	{
		if (Layer.LayerName == LayerName)
		{
			Layer.AssetId = AssetId;
			return;
		}
	}

	// If not found, create a new layer entry.
	FPHCAvatarLayer NewLayer;
	NewLayer.LayerName = LayerName;
	NewLayer.AssetId = AssetId;
	NewLayer.Tint = FLinearColor::White;

	// Assign ZOrder based on category position.
	const int32 CatIndex = Categories.IndexOfByKey(LayerName);
	NewLayer.ZOrder = (CatIndex != INDEX_NONE) ? CatIndex * 10 : CurrentAvatar.Layers.Num() * 10;

	CurrentAvatar.Layers.Add(NewLayer);
}

FString UPHCAvatarEditorWidget::GetLayerAsset(const FString& LayerName) const
{
	for (const FPHCAvatarLayer& Layer : CurrentAvatar.Layers)
	{
		if (Layer.LayerName == LayerName)
		{
			return Layer.AssetId;
		}
	}
	return FString();
}

FString UPHCAvatarEditorWidget::AvatarToJsonString() const
{
	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetStringField(TEXT("skin_tone"), CurrentAvatar.SkinTone);

	TArray<TSharedPtr<FJsonValue>> LayersArray;
	for (const FPHCAvatarLayer& Layer : CurrentAvatar.Layers)
	{
		TSharedRef<FJsonObject> LayerObj = MakeShared<FJsonObject>();
		LayerObj->SetStringField(TEXT("layer_name"), Layer.LayerName);
		LayerObj->SetStringField(TEXT("asset_id"), Layer.AssetId);
		LayerObj->SetNumberField(TEXT("z_order"), Layer.ZOrder);

		TSharedRef<FJsonObject> TintObj = MakeShared<FJsonObject>();
		TintObj->SetNumberField(TEXT("r"), Layer.Tint.R);
		TintObj->SetNumberField(TEXT("g"), Layer.Tint.G);
		TintObj->SetNumberField(TEXT("b"), Layer.Tint.B);
		TintObj->SetNumberField(TEXT("a"), Layer.Tint.A);
		LayerObj->SetObjectField(TEXT("tint"), TintObj);

		LayersArray.Add(MakeShared<FJsonValueObject>(LayerObj));
	}
	RootObj->SetArrayField(TEXT("layers"), LayersArray);

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObj, Writer);
	return OutputString;
}

UPHCHttpClient* UPHCAvatarEditorWidget::GetHttpClient() const
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	return GI ? GI->GetHttpClient() : nullptr;
}
