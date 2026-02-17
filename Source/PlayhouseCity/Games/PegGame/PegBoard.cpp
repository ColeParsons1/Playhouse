// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/PegGame/PegBoard.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"

// ---------------------------------------------------------------------------
// Static constants
// ---------------------------------------------------------------------------

const FLinearColor UPegBoard::PegColor       = FLinearColor(0.65f, 0.35f, 0.15f, 1.f); // Brown.
const FLinearColor UPegBoard::EmptyColor     = FLinearColor(0.2f, 0.2f, 0.2f, 0.4f);   // Dark gray.
const FLinearColor UPegBoard::HighlightColor = FLinearColor(0.2f, 0.8f, 0.3f, 1.f);    // Green.
const FLinearColor UPegBoard::SelectedColor  = FLinearColor(1.f, 0.85f, 0.f, 1.f);     // Gold.

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void UPegBoard::NativeConstruct()
{
	Super::NativeConstruct();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void UPegBoard::InitializeBoard()
{
	ComputePositions();

	PegButtons.Empty();
	PegButtons.SetNum(NUM_PEGS);
	PegImages.Empty();
	PegImages.SetNum(NUM_PEGS);

	// Function pointers for the 15 peg handlers.
	typedef void (UPegBoard::*FPegHandler)();
	FPegHandler Handlers[NUM_PEGS] = {
		&UPegBoard::HandlePeg0,  &UPegBoard::HandlePeg1,
		&UPegBoard::HandlePeg2,  &UPegBoard::HandlePeg3,
		&UPegBoard::HandlePeg4,  &UPegBoard::HandlePeg5,
		&UPegBoard::HandlePeg6,  &UPegBoard::HandlePeg7,
		&UPegBoard::HandlePeg8,  &UPegBoard::HandlePeg9,
		&UPegBoard::HandlePeg10, &UPegBoard::HandlePeg11,
		&UPegBoard::HandlePeg12, &UPegBoard::HandlePeg13,
		&UPegBoard::HandlePeg14
	};

	for (int32 i = 0; i < NUM_PEGS; ++i)
	{
		// Create a button for each peg position.
		UButton* Btn = NewObject<UButton>(this);
		UImage* Img = NewObject<UImage>(this);

		if (Btn && Img && BoardCanvas)
		{
			Img->SetColorAndOpacity(PegColor);
			Img->SetDesiredSizeOverride(FVector2D(PegSize, PegSize));

			// Add the image as child of the button.
			Btn->AddChild(Img);

			UCanvasPanelSlot* Slot = BoardCanvas->AddChildToCanvas(Btn);
			if (Slot)
			{
				Slot->SetPosition(PegPositions[i] - FVector2D(PegSize * 0.5f, PegSize * 0.5f));
				Slot->SetSize(FVector2D(PegSize, PegSize));
			}

			// Bind the click handler.
			Btn->OnClicked.AddDynamic(this, Handlers[i]);

			PegButtons[i] = Btn;
			PegImages[i] = Img;
		}
	}
}

// ---------------------------------------------------------------------------
// Updates
// ---------------------------------------------------------------------------

void UPegBoard::UpdatePegs(const TArray<bool>& PegStates)
{
	for (int32 i = 0; i < NUM_PEGS && i < PegStates.Num(); ++i)
	{
		if (!PegImages.IsValidIndex(i) || !PegImages[i])
		{
			continue;
		}

		if (PegStates[i])
		{
			PegImages[i]->SetColorAndOpacity(PegColor);
			PegImages[i]->SetRenderOpacity(1.0f);
		}
		else
		{
			PegImages[i]->SetColorAndOpacity(EmptyColor);
			PegImages[i]->SetRenderOpacity(0.5f);
		}
	}
}

void UPegBoard::HighlightSelected(int32 SelectedIndex)
{
	if (PegImages.IsValidIndex(SelectedIndex) && PegImages[SelectedIndex])
	{
		PegImages[SelectedIndex]->SetColorAndOpacity(SelectedColor);
	}
}

void UPegBoard::HighlightValidJumps(const TArray<int32>& Destinations)
{
	for (int32 Dest : Destinations)
	{
		if (PegImages.IsValidIndex(Dest) && PegImages[Dest])
		{
			PegImages[Dest]->SetColorAndOpacity(HighlightColor);
			PegImages[Dest]->SetRenderOpacity(1.0f);
		}
	}
}

void UPegBoard::AnimateJump(int32 From, int32 Over, int32 To)
{
	// In a full implementation, this would use a widget animation or timeline.
	// For now, we update visuals immediately; the HUD calls UpdatePegs after.

	if (PegImages.IsValidIndex(Over) && PegImages[Over])
	{
		// Flash the jumped-over peg before it disappears.
		PegImages[Over]->SetColorAndOpacity(FLinearColor::Red);
		PegImages[Over]->SetRenderOpacity(0.6f);
	}
}

FVector2D UPegBoard::GetPegScreenPosition(int32 Index) const
{
	if (PegPositions.IsValidIndex(Index))
	{
		return PegPositions[Index];
	}
	return FVector2D::ZeroVector;
}

void UPegBoard::ClearHighlights()
{
	// Reset all pegs to default colors -- caller should follow with UpdatePegs.
	for (int32 i = 0; i < NUM_PEGS; ++i)
	{
		if (PegImages.IsValidIndex(i) && PegImages[i])
		{
			PegImages[i]->SetColorAndOpacity(PegColor);
			PegImages[i]->SetRenderOpacity(1.0f);
		}
	}
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void UPegBoard::ComputePositions()
{
	PegPositions.Empty();
	PegPositions.SetNum(NUM_PEGS);

	/*
	 * Triangle layout:
	 *     Row 0: 1 peg  (index 0)
	 *     Row 1: 2 pegs (indices 1-2)
	 *     Row 2: 3 pegs (indices 3-5)
	 *     Row 3: 4 pegs (indices 6-9)
	 *     Row 4: 5 pegs (indices 10-14)
	 */

	int32 Idx = 0;
	for (int32 Row = 0; Row < 5; ++Row)
	{
		int32 PegsInRow = Row + 1;
		float RowWidth = (PegsInRow - 1) * HorizontalSpacing;
		float StartX = BoardOrigin.X - RowWidth * 0.5f;
		float Y = BoardOrigin.Y + Row * VerticalSpacing;

		for (int32 Col = 0; Col < PegsInRow; ++Col)
		{
			PegPositions[Idx] = FVector2D(StartX + Col * HorizontalSpacing, Y);
			Idx++;
		}
	}
}

// ---------------------------------------------------------------------------
// Peg click handlers (one per peg, routes to shared handler)
// ---------------------------------------------------------------------------

void UPegBoard::HandlePeg0()  { HandlePegAtIndex(0); }
void UPegBoard::HandlePeg1()  { HandlePegAtIndex(1); }
void UPegBoard::HandlePeg2()  { HandlePegAtIndex(2); }
void UPegBoard::HandlePeg3()  { HandlePegAtIndex(3); }
void UPegBoard::HandlePeg4()  { HandlePegAtIndex(4); }
void UPegBoard::HandlePeg5()  { HandlePegAtIndex(5); }
void UPegBoard::HandlePeg6()  { HandlePegAtIndex(6); }
void UPegBoard::HandlePeg7()  { HandlePegAtIndex(7); }
void UPegBoard::HandlePeg8()  { HandlePegAtIndex(8); }
void UPegBoard::HandlePeg9()  { HandlePegAtIndex(9); }
void UPegBoard::HandlePeg10() { HandlePegAtIndex(10); }
void UPegBoard::HandlePeg11() { HandlePegAtIndex(11); }
void UPegBoard::HandlePeg12() { HandlePegAtIndex(12); }
void UPegBoard::HandlePeg13() { HandlePegAtIndex(13); }
void UPegBoard::HandlePeg14() { HandlePegAtIndex(14); }

void UPegBoard::HandlePegAtIndex(int32 Index)
{
	OnPegClicked.Broadcast(Index);
}
