// Copyright PlayhouseCity. All Rights Reserved.

#include "Theme/PHCThemeManager.h"
#include "PlayhouseCity.h"

// ===========================================================================
// Construction
// ===========================================================================

UPHCThemeManager::UPHCThemeManager()
{
}

// ===========================================================================
// Lifecycle
// ===========================================================================

void UPHCThemeManager::Initialize()
{
	ThemeRegistry.Empty();

	// Each call registers one theme.  Colour order:
	//   Primary, Secondary, Accent,
	//   Background, Surface,
	//   Text, TextSecondary,
	//   Success, Warning, Error, Border

	// -----------------------------------------------------------------
	// 1. Ocean Blue
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("ocean_blue"), TEXT("Ocean Blue"),
		/*Primary*/      FLinearColor(0.10f, 0.46f, 0.82f, 1.0f),
		/*Secondary*/    FLinearColor(0.18f, 0.56f, 0.90f, 1.0f),
		/*Accent*/       FLinearColor(0.00f, 0.80f, 0.78f, 1.0f),
		/*Background*/   FLinearColor(0.04f, 0.08f, 0.16f, 1.0f),
		/*Surface*/      FLinearColor(0.08f, 0.14f, 0.24f, 1.0f),
		/*Text*/         FLinearColor(0.93f, 0.96f, 1.00f, 1.0f),
		/*TextSecondary*/FLinearColor(0.60f, 0.72f, 0.84f, 1.0f),
		/*Success*/      FLinearColor(0.20f, 0.78f, 0.35f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.76f, 0.03f, 1.0f),
		/*Error*/        FLinearColor(0.90f, 0.22f, 0.21f, 1.0f),
		/*Border*/       FLinearColor(0.16f, 0.28f, 0.44f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 2. Sunset Orange
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("sunset_orange"), TEXT("Sunset Orange"),
		/*Primary*/      FLinearColor(0.96f, 0.49f, 0.13f, 1.0f),
		/*Secondary*/    FLinearColor(0.88f, 0.34f, 0.15f, 1.0f),
		/*Accent*/       FLinearColor(1.00f, 0.78f, 0.18f, 1.0f),
		/*Background*/   FLinearColor(0.12f, 0.06f, 0.04f, 1.0f),
		/*Surface*/      FLinearColor(0.20f, 0.10f, 0.07f, 1.0f),
		/*Text*/         FLinearColor(1.00f, 0.96f, 0.90f, 1.0f),
		/*TextSecondary*/FLinearColor(0.82f, 0.70f, 0.58f, 1.0f),
		/*Success*/      FLinearColor(0.30f, 0.82f, 0.40f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.85f, 0.10f, 1.0f),
		/*Error*/        FLinearColor(0.85f, 0.18f, 0.18f, 1.0f),
		/*Border*/       FLinearColor(0.40f, 0.22f, 0.12f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 3. Forest Green
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("forest_green"), TEXT("Forest Green"),
		/*Primary*/      FLinearColor(0.18f, 0.62f, 0.30f, 1.0f),
		/*Secondary*/    FLinearColor(0.26f, 0.52f, 0.24f, 1.0f),
		/*Accent*/       FLinearColor(0.65f, 0.85f, 0.20f, 1.0f),
		/*Background*/   FLinearColor(0.04f, 0.10f, 0.05f, 1.0f),
		/*Surface*/      FLinearColor(0.08f, 0.16f, 0.08f, 1.0f),
		/*Text*/         FLinearColor(0.92f, 0.97f, 0.92f, 1.0f),
		/*TextSecondary*/FLinearColor(0.62f, 0.76f, 0.62f, 1.0f),
		/*Success*/      FLinearColor(0.25f, 0.82f, 0.45f, 1.0f),
		/*Warning*/      FLinearColor(0.95f, 0.80f, 0.15f, 1.0f),
		/*Error*/        FLinearColor(0.88f, 0.25f, 0.20f, 1.0f),
		/*Border*/       FLinearColor(0.20f, 0.36f, 0.18f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 4. Royal Purple
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("royal_purple"), TEXT("Royal Purple"),
		/*Primary*/      FLinearColor(0.50f, 0.18f, 0.78f, 1.0f),
		/*Secondary*/    FLinearColor(0.38f, 0.14f, 0.62f, 1.0f),
		/*Accent*/       FLinearColor(0.85f, 0.55f, 1.00f, 1.0f),
		/*Background*/   FLinearColor(0.08f, 0.04f, 0.14f, 1.0f),
		/*Surface*/      FLinearColor(0.14f, 0.08f, 0.22f, 1.0f),
		/*Text*/         FLinearColor(0.95f, 0.92f, 1.00f, 1.0f),
		/*TextSecondary*/FLinearColor(0.72f, 0.62f, 0.85f, 1.0f),
		/*Success*/      FLinearColor(0.30f, 0.82f, 0.50f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.80f, 0.10f, 1.0f),
		/*Error*/        FLinearColor(0.92f, 0.20f, 0.30f, 1.0f),
		/*Border*/       FLinearColor(0.30f, 0.18f, 0.48f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 5. Cherry Blossom
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("cherry_blossom"), TEXT("Cherry Blossom"),
		/*Primary*/      FLinearColor(0.92f, 0.50f, 0.65f, 1.0f),
		/*Secondary*/    FLinearColor(0.82f, 0.40f, 0.55f, 1.0f),
		/*Accent*/       FLinearColor(1.00f, 0.72f, 0.82f, 1.0f),
		/*Background*/   FLinearColor(0.14f, 0.06f, 0.08f, 1.0f),
		/*Surface*/      FLinearColor(0.22f, 0.10f, 0.14f, 1.0f),
		/*Text*/         FLinearColor(1.00f, 0.95f, 0.96f, 1.0f),
		/*TextSecondary*/FLinearColor(0.85f, 0.68f, 0.74f, 1.0f),
		/*Success*/      FLinearColor(0.30f, 0.80f, 0.50f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.82f, 0.20f, 1.0f),
		/*Error*/        FLinearColor(0.90f, 0.22f, 0.22f, 1.0f),
		/*Border*/       FLinearColor(0.44f, 0.22f, 0.30f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 6. Midnight Dark
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("midnight_dark"), TEXT("Midnight Dark"),
		/*Primary*/      FLinearColor(0.22f, 0.22f, 0.30f, 1.0f),
		/*Secondary*/    FLinearColor(0.16f, 0.16f, 0.22f, 1.0f),
		/*Accent*/       FLinearColor(0.45f, 0.55f, 1.00f, 1.0f),
		/*Background*/   FLinearColor(0.02f, 0.02f, 0.04f, 1.0f),
		/*Surface*/      FLinearColor(0.06f, 0.06f, 0.10f, 1.0f),
		/*Text*/         FLinearColor(0.88f, 0.88f, 0.92f, 1.0f),
		/*TextSecondary*/FLinearColor(0.55f, 0.55f, 0.62f, 1.0f),
		/*Success*/      FLinearColor(0.20f, 0.75f, 0.35f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.75f, 0.05f, 1.0f),
		/*Error*/        FLinearColor(0.88f, 0.22f, 0.22f, 1.0f),
		/*Border*/       FLinearColor(0.18f, 0.18f, 0.25f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 7. Arctic White
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("arctic_white"), TEXT("Arctic White"),
		/*Primary*/      FLinearColor(0.25f, 0.52f, 0.96f, 1.0f),
		/*Secondary*/    FLinearColor(0.40f, 0.62f, 0.92f, 1.0f),
		/*Accent*/       FLinearColor(0.10f, 0.74f, 0.62f, 1.0f),
		/*Background*/   FLinearColor(0.96f, 0.97f, 0.98f, 1.0f),
		/*Surface*/      FLinearColor(1.00f, 1.00f, 1.00f, 1.0f),
		/*Text*/         FLinearColor(0.10f, 0.12f, 0.16f, 1.0f),
		/*TextSecondary*/FLinearColor(0.40f, 0.44f, 0.52f, 1.0f),
		/*Success*/      FLinearColor(0.15f, 0.68f, 0.38f, 1.0f),
		/*Warning*/      FLinearColor(0.90f, 0.70f, 0.05f, 1.0f),
		/*Error*/        FLinearColor(0.85f, 0.20f, 0.18f, 1.0f),
		/*Border*/       FLinearColor(0.82f, 0.84f, 0.88f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 8. Golden Luxury
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("golden_luxury"), TEXT("Golden Luxury"),
		/*Primary*/      FLinearColor(0.85f, 0.68f, 0.20f, 1.0f),
		/*Secondary*/    FLinearColor(0.72f, 0.56f, 0.15f, 1.0f),
		/*Accent*/       FLinearColor(1.00f, 0.88f, 0.42f, 1.0f),
		/*Background*/   FLinearColor(0.06f, 0.05f, 0.02f, 1.0f),
		/*Surface*/      FLinearColor(0.12f, 0.10f, 0.05f, 1.0f),
		/*Text*/         FLinearColor(1.00f, 0.97f, 0.88f, 1.0f),
		/*TextSecondary*/FLinearColor(0.78f, 0.72f, 0.55f, 1.0f),
		/*Success*/      FLinearColor(0.30f, 0.78f, 0.40f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.82f, 0.20f, 1.0f),
		/*Error*/        FLinearColor(0.88f, 0.22f, 0.18f, 1.0f),
		/*Border*/       FLinearColor(0.45f, 0.36f, 0.14f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 9. Neon Cyber
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("neon_cyber"), TEXT("Neon Cyber"),
		/*Primary*/      FLinearColor(0.00f, 1.00f, 0.88f, 1.0f),
		/*Secondary*/    FLinearColor(0.90f, 0.00f, 1.00f, 1.0f),
		/*Accent*/       FLinearColor(1.00f, 1.00f, 0.00f, 1.0f),
		/*Background*/   FLinearColor(0.02f, 0.02f, 0.06f, 1.0f),
		/*Surface*/      FLinearColor(0.05f, 0.05f, 0.12f, 1.0f),
		/*Text*/         FLinearColor(0.92f, 0.98f, 1.00f, 1.0f),
		/*TextSecondary*/FLinearColor(0.50f, 0.65f, 0.72f, 1.0f),
		/*Success*/      FLinearColor(0.00f, 1.00f, 0.50f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.90f, 0.00f, 1.0f),
		/*Error*/        FLinearColor(1.00f, 0.10f, 0.30f, 1.0f),
		/*Border*/       FLinearColor(0.10f, 0.18f, 0.28f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 10. Autumn Harvest
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("autumn_harvest"), TEXT("Autumn Harvest"),
		/*Primary*/      FLinearColor(0.80f, 0.40f, 0.10f, 1.0f),
		/*Secondary*/    FLinearColor(0.65f, 0.30f, 0.08f, 1.0f),
		/*Accent*/       FLinearColor(0.90f, 0.72f, 0.15f, 1.0f),
		/*Background*/   FLinearColor(0.10f, 0.07f, 0.04f, 1.0f),
		/*Surface*/      FLinearColor(0.16f, 0.12f, 0.07f, 1.0f),
		/*Text*/         FLinearColor(1.00f, 0.95f, 0.88f, 1.0f),
		/*TextSecondary*/FLinearColor(0.78f, 0.68f, 0.55f, 1.0f),
		/*Success*/      FLinearColor(0.35f, 0.75f, 0.30f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.80f, 0.15f, 1.0f),
		/*Error*/        FLinearColor(0.85f, 0.20f, 0.15f, 1.0f),
		/*Border*/       FLinearColor(0.38f, 0.24f, 0.12f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 11. Tropical Paradise
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("tropical_paradise"), TEXT("Tropical Paradise"),
		/*Primary*/      FLinearColor(0.00f, 0.75f, 0.65f, 1.0f),
		/*Secondary*/    FLinearColor(0.12f, 0.60f, 0.85f, 1.0f),
		/*Accent*/       FLinearColor(1.00f, 0.65f, 0.00f, 1.0f),
		/*Background*/   FLinearColor(0.03f, 0.10f, 0.10f, 1.0f),
		/*Surface*/      FLinearColor(0.06f, 0.16f, 0.16f, 1.0f),
		/*Text*/         FLinearColor(0.95f, 1.00f, 0.98f, 1.0f),
		/*TextSecondary*/FLinearColor(0.60f, 0.78f, 0.75f, 1.0f),
		/*Success*/      FLinearColor(0.20f, 0.85f, 0.45f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.82f, 0.10f, 1.0f),
		/*Error*/        FLinearColor(0.92f, 0.25f, 0.25f, 1.0f),
		/*Border*/       FLinearColor(0.12f, 0.32f, 0.30f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 12. Candy Pop
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("candy_pop"), TEXT("Candy Pop"),
		/*Primary*/      FLinearColor(0.95f, 0.30f, 0.60f, 1.0f),
		/*Secondary*/    FLinearColor(0.55f, 0.20f, 0.90f, 1.0f),
		/*Accent*/       FLinearColor(0.20f, 0.90f, 0.90f, 1.0f),
		/*Background*/   FLinearColor(0.12f, 0.05f, 0.10f, 1.0f),
		/*Surface*/      FLinearColor(0.18f, 0.08f, 0.16f, 1.0f),
		/*Text*/         FLinearColor(1.00f, 0.96f, 0.98f, 1.0f),
		/*TextSecondary*/FLinearColor(0.80f, 0.62f, 0.75f, 1.0f),
		/*Success*/      FLinearColor(0.30f, 0.88f, 0.50f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.85f, 0.15f, 1.0f),
		/*Error*/        FLinearColor(0.95f, 0.18f, 0.18f, 1.0f),
		/*Border*/       FLinearColor(0.40f, 0.18f, 0.35f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 13. Steampunk Bronze
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("steampunk_bronze"), TEXT("Steampunk Bronze"),
		/*Primary*/      FLinearColor(0.72f, 0.52f, 0.28f, 1.0f),
		/*Secondary*/    FLinearColor(0.55f, 0.40f, 0.22f, 1.0f),
		/*Accent*/       FLinearColor(0.82f, 0.70f, 0.40f, 1.0f),
		/*Background*/   FLinearColor(0.08f, 0.06f, 0.04f, 1.0f),
		/*Surface*/      FLinearColor(0.14f, 0.11f, 0.07f, 1.0f),
		/*Text*/         FLinearColor(0.95f, 0.90f, 0.82f, 1.0f),
		/*TextSecondary*/FLinearColor(0.70f, 0.62f, 0.50f, 1.0f),
		/*Success*/      FLinearColor(0.35f, 0.72f, 0.35f, 1.0f),
		/*Warning*/      FLinearColor(0.92f, 0.78f, 0.20f, 1.0f),
		/*Error*/        FLinearColor(0.82f, 0.25f, 0.18f, 1.0f),
		/*Border*/       FLinearColor(0.38f, 0.28f, 0.16f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 14. Galaxy Nebula
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("galaxy_nebula"), TEXT("Galaxy Nebula"),
		/*Primary*/      FLinearColor(0.40f, 0.20f, 0.72f, 1.0f),
		/*Secondary*/    FLinearColor(0.22f, 0.10f, 0.52f, 1.0f),
		/*Accent*/       FLinearColor(0.90f, 0.40f, 0.80f, 1.0f),
		/*Background*/   FLinearColor(0.03f, 0.02f, 0.08f, 1.0f),
		/*Surface*/      FLinearColor(0.06f, 0.04f, 0.14f, 1.0f),
		/*Text*/         FLinearColor(0.92f, 0.90f, 1.00f, 1.0f),
		/*TextSecondary*/FLinearColor(0.62f, 0.56f, 0.78f, 1.0f),
		/*Success*/      FLinearColor(0.25f, 0.82f, 0.55f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.78f, 0.10f, 1.0f),
		/*Error*/        FLinearColor(0.92f, 0.20f, 0.30f, 1.0f),
		/*Border*/       FLinearColor(0.22f, 0.14f, 0.38f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 15. Zen Garden
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("zen_garden"), TEXT("Zen Garden"),
		/*Primary*/      FLinearColor(0.45f, 0.62f, 0.42f, 1.0f),
		/*Secondary*/    FLinearColor(0.60f, 0.55f, 0.40f, 1.0f),
		/*Accent*/       FLinearColor(0.80f, 0.72f, 0.55f, 1.0f),
		/*Background*/   FLinearColor(0.94f, 0.92f, 0.88f, 1.0f),
		/*Surface*/      FLinearColor(0.98f, 0.96f, 0.92f, 1.0f),
		/*Text*/         FLinearColor(0.18f, 0.20f, 0.16f, 1.0f),
		/*TextSecondary*/FLinearColor(0.42f, 0.44f, 0.38f, 1.0f),
		/*Success*/      FLinearColor(0.25f, 0.70f, 0.38f, 1.0f),
		/*Warning*/      FLinearColor(0.85f, 0.72f, 0.10f, 1.0f),
		/*Error*/        FLinearColor(0.78f, 0.22f, 0.20f, 1.0f),
		/*Border*/       FLinearColor(0.78f, 0.75f, 0.68f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 16. Retro Arcade
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("retro_arcade"), TEXT("Retro Arcade"),
		/*Primary*/      FLinearColor(0.92f, 0.10f, 0.14f, 1.0f),
		/*Secondary*/    FLinearColor(0.10f, 0.10f, 0.85f, 1.0f),
		/*Accent*/       FLinearColor(1.00f, 0.92f, 0.00f, 1.0f),
		/*Background*/   FLinearColor(0.04f, 0.04f, 0.04f, 1.0f),
		/*Surface*/      FLinearColor(0.10f, 0.10f, 0.10f, 1.0f),
		/*Text*/         FLinearColor(1.00f, 1.00f, 1.00f, 1.0f),
		/*TextSecondary*/FLinearColor(0.65f, 0.65f, 0.65f, 1.0f),
		/*Success*/      FLinearColor(0.00f, 0.85f, 0.20f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.80f, 0.00f, 1.0f),
		/*Error*/        FLinearColor(1.00f, 0.00f, 0.00f, 1.0f),
		/*Border*/       FLinearColor(0.28f, 0.28f, 0.28f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 17. Winter Frost
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("winter_frost"), TEXT("Winter Frost"),
		/*Primary*/      FLinearColor(0.45f, 0.68f, 0.88f, 1.0f),
		/*Secondary*/    FLinearColor(0.55f, 0.75f, 0.92f, 1.0f),
		/*Accent*/       FLinearColor(0.70f, 0.88f, 1.00f, 1.0f),
		/*Background*/   FLinearColor(0.90f, 0.94f, 0.98f, 1.0f),
		/*Surface*/      FLinearColor(0.96f, 0.98f, 1.00f, 1.0f),
		/*Text*/         FLinearColor(0.12f, 0.18f, 0.28f, 1.0f),
		/*TextSecondary*/FLinearColor(0.38f, 0.48f, 0.60f, 1.0f),
		/*Success*/      FLinearColor(0.20f, 0.72f, 0.42f, 1.0f),
		/*Warning*/      FLinearColor(0.88f, 0.72f, 0.10f, 1.0f),
		/*Error*/        FLinearColor(0.82f, 0.22f, 0.22f, 1.0f),
		/*Border*/       FLinearColor(0.75f, 0.82f, 0.90f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 18. Safari Savanna
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("safari_savanna"), TEXT("Safari Savanna"),
		/*Primary*/      FLinearColor(0.72f, 0.58f, 0.30f, 1.0f),
		/*Secondary*/    FLinearColor(0.58f, 0.45f, 0.25f, 1.0f),
		/*Accent*/       FLinearColor(0.85f, 0.50f, 0.15f, 1.0f),
		/*Background*/   FLinearColor(0.12f, 0.10f, 0.06f, 1.0f),
		/*Surface*/      FLinearColor(0.18f, 0.15f, 0.10f, 1.0f),
		/*Text*/         FLinearColor(0.96f, 0.92f, 0.84f, 1.0f),
		/*TextSecondary*/FLinearColor(0.72f, 0.66f, 0.55f, 1.0f),
		/*Success*/      FLinearColor(0.35f, 0.72f, 0.30f, 1.0f),
		/*Warning*/      FLinearColor(0.95f, 0.78f, 0.12f, 1.0f),
		/*Error*/        FLinearColor(0.85f, 0.22f, 0.15f, 1.0f),
		/*Border*/       FLinearColor(0.40f, 0.32f, 0.18f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 19. Deep Sea
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("deep_sea"), TEXT("Deep Sea"),
		/*Primary*/      FLinearColor(0.05f, 0.30f, 0.50f, 1.0f),
		/*Secondary*/    FLinearColor(0.08f, 0.22f, 0.40f, 1.0f),
		/*Accent*/       FLinearColor(0.00f, 0.72f, 0.65f, 1.0f),
		/*Background*/   FLinearColor(0.01f, 0.04f, 0.08f, 1.0f),
		/*Surface*/      FLinearColor(0.03f, 0.08f, 0.14f, 1.0f),
		/*Text*/         FLinearColor(0.82f, 0.92f, 0.98f, 1.0f),
		/*TextSecondary*/FLinearColor(0.50f, 0.64f, 0.74f, 1.0f),
		/*Success*/      FLinearColor(0.18f, 0.78f, 0.55f, 1.0f),
		/*Warning*/      FLinearColor(0.92f, 0.78f, 0.15f, 1.0f),
		/*Error*/        FLinearColor(0.88f, 0.25f, 0.25f, 1.0f),
		/*Border*/       FLinearColor(0.10f, 0.22f, 0.34f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 20. Cotton Candy
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("cotton_candy"), TEXT("Cotton Candy"),
		/*Primary*/      FLinearColor(0.72f, 0.55f, 0.90f, 1.0f),
		/*Secondary*/    FLinearColor(0.90f, 0.60f, 0.78f, 1.0f),
		/*Accent*/       FLinearColor(0.55f, 0.85f, 0.92f, 1.0f),
		/*Background*/   FLinearColor(0.96f, 0.94f, 0.98f, 1.0f),
		/*Surface*/      FLinearColor(1.00f, 0.98f, 1.00f, 1.0f),
		/*Text*/         FLinearColor(0.22f, 0.16f, 0.30f, 1.0f),
		/*TextSecondary*/FLinearColor(0.48f, 0.40f, 0.55f, 1.0f),
		/*Success*/      FLinearColor(0.30f, 0.78f, 0.50f, 1.0f),
		/*Warning*/      FLinearColor(0.90f, 0.75f, 0.15f, 1.0f),
		/*Error*/        FLinearColor(0.85f, 0.25f, 0.30f, 1.0f),
		/*Border*/       FLinearColor(0.80f, 0.74f, 0.86f, 1.0f)
	);

	// -----------------------------------------------------------------
	// 21. Volcanic
	// -----------------------------------------------------------------
	RegisterTheme(
		TEXT("volcanic"), TEXT("Volcanic"),
		/*Primary*/      FLinearColor(0.85f, 0.18f, 0.08f, 1.0f),
		/*Secondary*/    FLinearColor(0.65f, 0.12f, 0.05f, 1.0f),
		/*Accent*/       FLinearColor(1.00f, 0.55f, 0.00f, 1.0f),
		/*Background*/   FLinearColor(0.06f, 0.03f, 0.02f, 1.0f),
		/*Surface*/      FLinearColor(0.12f, 0.06f, 0.04f, 1.0f),
		/*Text*/         FLinearColor(1.00f, 0.94f, 0.88f, 1.0f),
		/*TextSecondary*/FLinearColor(0.78f, 0.60f, 0.50f, 1.0f),
		/*Success*/      FLinearColor(0.30f, 0.75f, 0.35f, 1.0f),
		/*Warning*/      FLinearColor(1.00f, 0.82f, 0.10f, 1.0f),
		/*Error*/        FLinearColor(1.00f, 0.15f, 0.10f, 1.0f),
		/*Border*/       FLinearColor(0.38f, 0.16f, 0.10f, 1.0f)
	);

	// -----------------------------------------------------------------
	// Set the default theme.
	// -----------------------------------------------------------------
	if (ThemeRegistry.Num() > 0)
	{
		SetTheme(TEXT("ocean_blue"));
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCThemeManager::Initialize -- Registered %d themes."), ThemeRegistry.Num());
}

// ===========================================================================
// Theme selection
// ===========================================================================

void UPHCThemeManager::SetTheme(const FString& ThemeId)
{
	const FPHCThemeData* Found = ThemeRegistry.Find(ThemeId);
	if (!Found)
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("PHCThemeManager::SetTheme -- Unknown theme '%s'."), *ThemeId);
		return;
	}

	if (CurrentThemeId == ThemeId)
	{
		// Already active.
		return;
	}

	CurrentThemeId = ThemeId;
	CurrentTheme = *Found;

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCThemeManager::SetTheme -- Active theme is now '%s'."), *CurrentTheme.ThemeName);

	OnThemeChanged.Broadcast(CurrentTheme);
}

// ===========================================================================
// Accessors
// ===========================================================================

FPHCThemeData UPHCThemeManager::GetCurrentTheme() const
{
	return CurrentTheme;
}

FString UPHCThemeManager::GetCurrentThemeId() const
{
	return CurrentThemeId;
}

FPHCThemeData UPHCThemeManager::GetTheme(const FString& ThemeId) const
{
	const FPHCThemeData* Found = ThemeRegistry.Find(ThemeId);
	if (Found)
	{
		return *Found;
	}

	return FPHCThemeData();
}

TArray<FString> UPHCThemeManager::GetAllThemeIds() const
{
	TArray<FString> Ids;
	ThemeRegistry.GetKeys(Ids);
	return Ids;
}

TArray<FPHCThemeData> UPHCThemeManager::GetAllThemes() const
{
	TArray<FPHCThemeData> Result;
	Result.Reserve(ThemeRegistry.Num());
	for (const auto& Pair : ThemeRegistry)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

// ===========================================================================
// Internal helpers
// ===========================================================================

void UPHCThemeManager::RegisterTheme(
	const FString& InId,
	const FString& InName,
	FLinearColor InPrimary,
	FLinearColor InSecondary,
	FLinearColor InAccent,
	FLinearColor InBackground,
	FLinearColor InSurface,
	FLinearColor InText,
	FLinearColor InTextSecondary,
	FLinearColor InSuccess,
	FLinearColor InWarning,
	FLinearColor InError,
	FLinearColor InBorder)
{
	FPHCThemeData Theme;
	Theme.ThemeId            = InId;
	Theme.ThemeName          = InName;
	Theme.PrimaryColor       = InPrimary;
	Theme.SecondaryColor     = InSecondary;
	Theme.AccentColor        = InAccent;
	Theme.BackgroundColor    = InBackground;
	Theme.SurfaceColor       = InSurface;
	Theme.TextColor          = InText;
	Theme.TextSecondaryColor = InTextSecondary;
	Theme.SuccessColor       = InSuccess;
	Theme.WarningColor       = InWarning;
	Theme.ErrorColor         = InError;
	Theme.BorderColor        = InBorder;
	Theme.CardColor          = InSurface; // Legacy compatibility alias.

	ThemeRegistry.Add(InId, Theme);
}
