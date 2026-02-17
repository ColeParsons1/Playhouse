// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/PHCTypes.h"
#include "PHCRoomThemes.generated.h"

// ---------------------------------------------------------------------------
// FPHCRoomTheme
// ---------------------------------------------------------------------------

/**
 * FPHCRoomTheme
 *
 * Data-only struct describing a visual / auditory theme that can be applied
 * to a PlayhouseCity voice room. Each theme defines a background color,
 * accent color, optional background image, and optional ambient sound loop.
 */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCRoomTheme
{
	GENERATED_BODY()

	/** Unique machine-readable identifier (e.g. "cozy_lounge"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|RoomTheme")
	FString ThemeId;

	/** Human-readable display name shown in the UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|RoomTheme")
	FString DisplayName;

	/** Primary background color for the room environment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|RoomTheme")
	FLinearColor BackgroundColor = FLinearColor::Black;

	/** Accent color used for highlights, borders, and interactive elements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|RoomTheme")
	FLinearColor AccentColor = FLinearColor::White;

	/** Asset path to the background image (Texture2D). Empty if no image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|RoomTheme")
	FString BackgroundImagePath;

	/** Asset path to the ambient sound loop (SoundWave). Empty if silent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|RoomTheme")
	FString AmbientSoundPath;

	/** Default playback volume for the ambient sound [0.0 - 1.0]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|RoomTheme", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbientVolume = 0.5f;
};

// ---------------------------------------------------------------------------
// PHCRoomThemes namespace -- 15 built-in themes
// ---------------------------------------------------------------------------

namespace PHCRoomThemes
{
	/**
	 * Returns an array containing all 15 built-in room themes.
	 * Each theme has unique colors and placeholder asset paths that should
	 * be replaced with actual content paths once art assets are authored.
	 */
	inline TArray<FPHCRoomTheme> GetAllRoomThemes()
	{
		TArray<FPHCRoomTheme> Themes;
		Themes.Reserve(15);

		// ----------------------------------------------------------
		// 1. Cozy Lounge
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("cozy_lounge");
			Theme.DisplayName        = TEXT("Cozy Lounge");
			Theme.BackgroundColor    = FLinearColor(0.18f, 0.12f, 0.08f, 1.0f);   // Warm brown
			Theme.AccentColor        = FLinearColor(0.95f, 0.65f, 0.20f, 1.0f);   // Amber gold
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/CozyLounge/T_BG_CozyLounge");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/CozyLounge/S_Ambient_Fireplace");
			Theme.AmbientVolume      = 0.4f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 2. Neon Club
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("neon_club");
			Theme.DisplayName        = TEXT("Neon Club");
			Theme.BackgroundColor    = FLinearColor(0.05f, 0.02f, 0.15f, 1.0f);   // Deep purple-black
			Theme.AccentColor        = FLinearColor(0.0f, 1.0f, 0.85f, 1.0f);     // Electric cyan
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/NeonClub/T_BG_NeonClub");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/NeonClub/S_Ambient_BassLoop");
			Theme.AmbientVolume      = 0.6f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 3. Beach House
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("beach_house");
			Theme.DisplayName        = TEXT("Beach House");
			Theme.BackgroundColor    = FLinearColor(0.55f, 0.82f, 0.95f, 1.0f);   // Sky blue
			Theme.AccentColor        = FLinearColor(1.0f, 0.85f, 0.45f, 1.0f);    // Sandy gold
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/BeachHouse/T_BG_BeachHouse");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/BeachHouse/S_Ambient_Waves");
			Theme.AmbientVolume      = 0.5f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 4. Space Station
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("space_station");
			Theme.DisplayName        = TEXT("Space Station");
			Theme.BackgroundColor    = FLinearColor(0.02f, 0.02f, 0.06f, 1.0f);   // Near-black space
			Theme.AccentColor        = FLinearColor(0.30f, 0.70f, 1.0f, 1.0f);    // Hologram blue
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/SpaceStation/T_BG_SpaceStation");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/SpaceStation/S_Ambient_ShipHum");
			Theme.AmbientVolume      = 0.35f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 5. Forest Cabin
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("forest_cabin");
			Theme.DisplayName        = TEXT("Forest Cabin");
			Theme.BackgroundColor    = FLinearColor(0.10f, 0.18f, 0.08f, 1.0f);   // Dark forest green
			Theme.AccentColor        = FLinearColor(0.70f, 0.50f, 0.25f, 1.0f);   // Warm wood
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/ForestCabin/T_BG_ForestCabin");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/ForestCabin/S_Ambient_Birds");
			Theme.AmbientVolume      = 0.45f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 6. Underwater Cave
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("underwater_cave");
			Theme.DisplayName        = TEXT("Underwater Cave");
			Theme.BackgroundColor    = FLinearColor(0.0f, 0.08f, 0.20f, 1.0f);    // Deep ocean blue
			Theme.AccentColor        = FLinearColor(0.20f, 0.90f, 0.70f, 1.0f);   // Bioluminescent teal
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/UnderwaterCave/T_BG_UnderwaterCave");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/UnderwaterCave/S_Ambient_Bubbles");
			Theme.AmbientVolume      = 0.4f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 7. Rooftop Garden
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("rooftop_garden");
			Theme.DisplayName        = TEXT("Rooftop Garden");
			Theme.BackgroundColor    = FLinearColor(0.40f, 0.55f, 0.75f, 1.0f);   // Twilight blue
			Theme.AccentColor        = FLinearColor(0.85f, 0.40f, 0.55f, 1.0f);   // Rose pink
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/RooftopGarden/T_BG_RooftopGarden");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/RooftopGarden/S_Ambient_CityBreeze");
			Theme.AmbientVolume      = 0.35f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 8. Retro Diner
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("retro_diner");
			Theme.DisplayName        = TEXT("Retro Diner");
			Theme.BackgroundColor    = FLinearColor(0.85f, 0.20f, 0.20f, 1.0f);   // Cherry red
			Theme.AccentColor        = FLinearColor(0.95f, 0.90f, 0.60f, 1.0f);   // Cream yellow
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/RetroDiner/T_BG_RetroDiner");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/RetroDiner/S_Ambient_Jukebox");
			Theme.AmbientVolume      = 0.5f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 9. Library Study
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("library_study");
			Theme.DisplayName        = TEXT("Library Study");
			Theme.BackgroundColor    = FLinearColor(0.15f, 0.10f, 0.05f, 1.0f);   // Dark mahogany
			Theme.AccentColor        = FLinearColor(0.80f, 0.70f, 0.45f, 1.0f);   // Antique gold
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/LibraryStudy/T_BG_LibraryStudy");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/LibraryStudy/S_Ambient_PageTurn");
			Theme.AmbientVolume      = 0.25f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 10. Campfire Circle
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("campfire_circle");
			Theme.DisplayName        = TEXT("Campfire Circle");
			Theme.BackgroundColor    = FLinearColor(0.05f, 0.05f, 0.12f, 1.0f);   // Night sky blue-black
			Theme.AccentColor        = FLinearColor(1.0f, 0.55f, 0.10f, 1.0f);    // Campfire orange
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/CampfireCircle/T_BG_CampfireCircle");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/CampfireCircle/S_Ambient_Crackling");
			Theme.AmbientVolume      = 0.55f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 11. Cloud Nine
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("cloud_nine");
			Theme.DisplayName        = TEXT("Cloud Nine");
			Theme.BackgroundColor    = FLinearColor(0.85f, 0.90f, 1.0f, 1.0f);    // Soft white-blue
			Theme.AccentColor        = FLinearColor(1.0f, 0.75f, 0.85f, 1.0f);    // Cotton candy pink
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/CloudNine/T_BG_CloudNine");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/CloudNine/S_Ambient_WindChimes");
			Theme.AmbientVolume      = 0.3f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 12. Crystal Palace
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("crystal_palace");
			Theme.DisplayName        = TEXT("Crystal Palace");
			Theme.BackgroundColor    = FLinearColor(0.12f, 0.08f, 0.22f, 1.0f);   // Royal purple
			Theme.AccentColor        = FLinearColor(0.70f, 0.85f, 1.0f, 1.0f);    // Ice crystal blue
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/CrystalPalace/T_BG_CrystalPalace");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/CrystalPalace/S_Ambient_Shimmer");
			Theme.AmbientVolume      = 0.35f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 13. Pirate Ship
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("pirate_ship");
			Theme.DisplayName        = TEXT("Pirate Ship");
			Theme.BackgroundColor    = FLinearColor(0.12f, 0.08f, 0.04f, 1.0f);   // Weathered wood brown
			Theme.AccentColor        = FLinearColor(0.90f, 0.15f, 0.10f, 1.0f);   // Pirate red
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/PirateShip/T_BG_PirateShip");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/PirateShip/S_Ambient_Ocean");
			Theme.AmbientVolume      = 0.5f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 14. Zen Temple
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("zen_temple");
			Theme.DisplayName        = TEXT("Zen Temple");
			Theme.BackgroundColor    = FLinearColor(0.90f, 0.88f, 0.80f, 1.0f);   // Warm parchment
			Theme.AccentColor        = FLinearColor(0.40f, 0.60f, 0.35f, 1.0f);   // Bamboo green
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/ZenTemple/T_BG_ZenTemple");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/ZenTemple/S_Ambient_Fountain");
			Theme.AmbientVolume      = 0.3f;
			Themes.Add(Theme);
		}

		// ----------------------------------------------------------
		// 15. Haunted Mansion
		// ----------------------------------------------------------
		{
			FPHCRoomTheme Theme;
			Theme.ThemeId            = TEXT("haunted_mansion");
			Theme.DisplayName        = TEXT("Haunted Mansion");
			Theme.BackgroundColor    = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);   // Near-black
			Theme.AccentColor        = FLinearColor(0.45f, 0.0f, 0.65f, 1.0f);    // Ghostly purple
			Theme.BackgroundImagePath = TEXT("/Game/Rooms/Themes/HauntedMansion/T_BG_HauntedMansion");
			Theme.AmbientSoundPath   = TEXT("/Game/Rooms/Themes/HauntedMansion/S_Ambient_Creaking");
			Theme.AmbientVolume      = 0.45f;
			Themes.Add(Theme);
		}

		return Themes;
	}

	/**
	 * Look up a single room theme by its ThemeId.
	 *
	 * @param ThemeId  The machine-readable identifier to search for.
	 * @param OutTheme Populated with the theme data if found.
	 * @return True if the theme was found, false otherwise.
	 */
	inline bool FindThemeById(const FString& ThemeId, FPHCRoomTheme& OutTheme)
	{
		const TArray<FPHCRoomTheme> AllThemes = GetAllRoomThemes();
		for (const FPHCRoomTheme& Theme : AllThemes)
		{
			if (Theme.ThemeId == ThemeId)
			{
				OutTheme = Theme;
				return true;
			}
		}
		return false;
	}

	/**
	 * Get all available theme IDs as a simple string array for UI dropdowns.
	 */
	inline TArray<FString> GetAllThemeIds()
	{
		const TArray<FPHCRoomTheme> AllThemes = GetAllRoomThemes();
		TArray<FString> Ids;
		Ids.Reserve(AllThemes.Num());
		for (const FPHCRoomTheme& Theme : AllThemes)
		{
			Ids.Add(Theme.ThemeId);
		}
		return Ids;
	}
}
