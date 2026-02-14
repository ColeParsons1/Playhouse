// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Ludo/LudoBoard.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void ULudoBoard::NativeConstruct()
{
	Super::NativeConstruct();

	if (DiceButton)
	{
		DiceButton->OnClicked.AddDynamic(this, &ULudoBoard::HandleDiceClicked);
	}
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void ULudoBoard::InitializeBoard()
{
	BuildPositionLUT();

	// Create 16 piece images (4 per player color).
	PieceImages.Empty();
	PieceImages.SetNum(16);

	const FLinearColor Colors[] = {
		FLinearColor::Red,
		FLinearColor::Blue,
		FLinearColor::Green,
		FLinearColor::Yellow
	};

	for (int32 i = 0; i < 16; ++i)
	{
		UImage* PieceImg = NewObject<UImage>(this);
		if (PieceImg && BoardCanvas)
		{
			PieceImg->SetColorAndOpacity(Colors[i / 4]);
			PieceImg->SetDesiredSizeOverride(FVector2D(24.f, 24.f));

			UCanvasPanelSlot* Slot = BoardCanvas->AddChildToCanvas(PieceImg);
			if (Slot)
			{
				Slot->SetSize(FVector2D(24.f, 24.f));
				Slot->SetPosition(FVector2D(-100.f, -100.f)); // Off-screen initially.
			}

			PieceImages[i] = PieceImg;
		}
	}
}

// ---------------------------------------------------------------------------
// Updates
// ---------------------------------------------------------------------------

void ULudoBoard::UpdatePieces(const TArray<FPHCLudoPiece>& AllPieces)
{
	for (int32 i = 0; i < AllPieces.Num() && i < PieceImages.Num(); ++i)
	{
		UImage* Img = PieceImages[i];
		if (!Img)
		{
			continue;
		}

		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Img->Slot);
		if (!Slot)
		{
			continue;
		}

		const FPHCLudoPiece& Piece = AllPieces[i];

		if (Piece.bFinished)
		{
			// Move to the center of the board.
			Slot->SetPosition(BoardOffset + FVector2D(BoardSize * 0.5f - 12.f, BoardSize * 0.5f - 12.f));
			Img->SetVisibility(ESlateVisibility::HitTestInvisible);
			continue;
		}

		if (Piece.bAtHome)
		{
			// Place in the home yard area.
			int32 PieceSubIndex = i % 4;
			FVector2D HomePos = GetHomeYardPosition(Piece.Color, PieceSubIndex);
			Slot->SetPosition(HomePos);
			Img->SetVisibility(ESlateVisibility::Visible);
			continue;
		}

		// On the board: use the position LUT.
		FVector2D ScreenPos = GetBoardPosition(Piece.Position, Piece.Color);
		Slot->SetPosition(ScreenPos);
		Img->SetVisibility(ESlateVisibility::Visible);
	}
}

void ULudoBoard::ShowDiceResult(int32 Value)
{
	// Update dice image tint based on value for visual feedback.
	if (DiceImage)
	{
		// Simple visual: scale brightness with dice value.
		float Brightness = 0.4f + (Value / 6.f) * 0.6f;
		DiceImage->SetColorAndOpacity(FLinearColor(Brightness, Brightness, Brightness, 1.f));
	}

	// Show the value next to dice or in turn text temporarily.
	if (TurnText)
	{
		TurnText->SetText(FText::FromString(FString::Printf(TEXT("Rolled: %d"), Value)));
	}
}

void ULudoBoard::HighlightValidMoves(const TArray<int32>& PieceIndices)
{
	// Reset all piece opacities.
	for (int32 i = 0; i < PieceImages.Num(); ++i)
	{
		if (PieceImages[i])
		{
			PieceImages[i]->SetRenderOpacity(0.5f);
		}
	}

	// Highlight valid pieces with full opacity.
	for (int32 Idx : PieceIndices)
	{
		if (PieceImages.IsValidIndex(Idx))
		{
			if (PieceImages[Idx])
			{
				PieceImages[Idx]->SetRenderOpacity(1.0f);
			}
		}
	}
}

void ULudoBoard::AnimateMovement(int32 PieceIndex, int32 FromPos, int32 ToPos)
{
	// For a full implementation, this would use a timeline or animation.
	// For now, we snap to the final position (the widget update handles visuals).
	if (!PieceImages.IsValidIndex(PieceIndex) || !PieceImages[PieceIndex])
	{
		return;
	}

	// Determine the color from the piece index (4 pieces per color).
	EPHCLudoColor Color = static_cast<EPHCLudoColor>(PieceIndex / 4);
	FVector2D TargetPos = GetBoardPosition(ToPos, Color);

	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PieceImages[PieceIndex]->Slot);
	if (Slot)
	{
		Slot->SetPosition(TargetPos);
	}
}

void ULudoBoard::ShowCapture(int32 CapturedPieceIndex)
{
	// Flash the captured piece before it returns home.
	if (PieceImages.IsValidIndex(CapturedPieceIndex) && PieceImages[CapturedPieceIndex])
	{
		PieceImages[CapturedPieceIndex]->SetRenderOpacity(0.3f);
	}
}

// ---------------------------------------------------------------------------
// Coordinate mapping
// ---------------------------------------------------------------------------

FVector2D ULudoBoard::GetBoardPosition(int32 TrackPosition, EPHCLudoColor Color) const
{
	if (TrackPosition < 0)
	{
		return FVector2D(-100.f, -100.f);
	}

	// Shared track positions (0-51).
	if (TrackPosition < ALudoGameMode::TRACK_LENGTH)
	{
		int32 AbsPos = (TrackPosition + ALudoGameMode::GetStartOffset(Color)) % ALudoGameMode::TRACK_LENGTH;
		if (TrackPositionLUT.IsValidIndex(AbsPos))
		{
			return TrackPositionLUT[AbsPos];
		}
	}

	// Home column positions (52-57).
	if (TrackPosition >= ALudoGameMode::TRACK_LENGTH)
	{
		int32 HomeIdx = TrackPosition - ALudoGameMode::TRACK_LENGTH;
		float CenterX = BoardOffset.X + BoardSize * 0.5f;
		float CenterY = BoardOffset.Y + BoardSize * 0.5f;

		// Each color's home column approaches the center from their side.
		constexpr int32 HomeColumnLen = ALudoGameMode::HOME_COLUMN_LENGTH;
		switch (Color)
		{
		case EPHCLudoColor::Red:
			return FVector2D(CenterX - (HomeColumnLen - HomeIdx) * CellSize, CenterY);
		case EPHCLudoColor::Blue:
			return FVector2D(CenterX, CenterY - (HomeColumnLen - HomeIdx) * CellSize);
		case EPHCLudoColor::Green:
			return FVector2D(CenterX + (HomeColumnLen - HomeIdx) * CellSize, CenterY);
		case EPHCLudoColor::Yellow:
			return FVector2D(CenterX, CenterY + (HomeColumnLen - HomeIdx) * CellSize);
		}
	}

	return BoardOffset;
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void ULudoBoard::BuildPositionLUT()
{
	TrackPositionLUT.Empty();
	TrackPositionLUT.SetNum(ALudoGameMode::TRACK_LENGTH);

	HomeYardPositions.Empty();
	HomeYardPositions.SetNum(16);

	// The Ludo board is a cross. We define the 52 shared track positions
	// going clockwise starting from Red's side (bottom-left).
	// Board is logically a 15x15 grid.

	const float G = CellSize; // Grid unit.
	const float OX = BoardOffset.X;
	const float OY = BoardOffset.Y;

	// Define the 52 positions of the outer track clockwise.
	// Bottom arm (Red side): positions 0-4 go right along bottom.
	// Right arm going up: positions 5-12.
	// Top arm: positions 13-17 go left along top.
	// Left arm going down: positions 18-25.
	// And so on for the four sides.

	// Simplified layout: 13 positions per side of the cross.
	// Side 0 (Red, bottom-left): goes up the left side.
	// Side 1 (Blue, top-left): goes right across top.
	// Side 2 (Green, top-right): goes down the right side.
	// Side 3 (Yellow, bottom-right): goes left across bottom.

	struct FTrackPoint { float X; float Y; };

	// We lay out 52 positions around the cross track.
	// The board grid: 0,0 is top-left. 14,14 is bottom-right.
	// Center is at 7,7. Arms extend 6 cells in each direction.

	auto GridToPixel = [&](float GX, float GY) -> FVector2D
	{
		return FVector2D(OX + GX * G, OY + GY * G);
	};

	// Bottom to top on left arm (Red start area), then across top, down right, across bottom.
	// Positions carefully mapped to a standard Ludo board layout.

	// Segment 1: Red side - bottom left, going up (positions 0-5)
	TrackPositionLUT[0]  = GridToPixel(1.f, 13.f);
	TrackPositionLUT[1]  = GridToPixel(1.f, 12.f);
	TrackPositionLUT[2]  = GridToPixel(1.f, 11.f);
	TrackPositionLUT[3]  = GridToPixel(1.f, 10.f);
	TrackPositionLUT[4]  = GridToPixel(1.f, 9.f);

	// Turn right at top of left column (positions 5-6)
	TrackPositionLUT[5]  = GridToPixel(0.f, 8.f);
	TrackPositionLUT[6]  = GridToPixel(0.f, 7.f);

	// Across top row left to right (positions 7-12)
	TrackPositionLUT[7]  = GridToPixel(1.f, 6.f);
	TrackPositionLUT[8]  = GridToPixel(2.f, 6.f);
	TrackPositionLUT[9]  = GridToPixel(3.f, 6.f);
	TrackPositionLUT[10] = GridToPixel(4.f, 6.f);
	TrackPositionLUT[11] = GridToPixel(5.f, 6.f);
	TrackPositionLUT[12] = GridToPixel(6.f, 6.f);

	// Blue side - top left, going right then down (positions 13-18)
	TrackPositionLUT[13] = GridToPixel(6.f, 5.f);
	TrackPositionLUT[14] = GridToPixel(6.f, 4.f);
	TrackPositionLUT[15] = GridToPixel(6.f, 3.f);
	TrackPositionLUT[16] = GridToPixel(6.f, 2.f);
	TrackPositionLUT[17] = GridToPixel(6.f, 1.f);

	// Turn down (positions 18-19)
	TrackPositionLUT[18] = GridToPixel(7.f, 0.f);
	TrackPositionLUT[19] = GridToPixel(8.f, 0.f);

	// Down right column (positions 20-25)
	TrackPositionLUT[20] = GridToPixel(8.f, 1.f);
	TrackPositionLUT[21] = GridToPixel(8.f, 2.f);
	TrackPositionLUT[22] = GridToPixel(8.f, 3.f);
	TrackPositionLUT[23] = GridToPixel(8.f, 4.f);
	TrackPositionLUT[24] = GridToPixel(8.f, 5.f);
	TrackPositionLUT[25] = GridToPixel(8.f, 6.f);

	// Green side - top right, going down then left (positions 26-31)
	TrackPositionLUT[26] = GridToPixel(9.f,  6.f);
	TrackPositionLUT[27] = GridToPixel(10.f, 6.f);
	TrackPositionLUT[28] = GridToPixel(11.f, 6.f);
	TrackPositionLUT[29] = GridToPixel(12.f, 6.f);
	TrackPositionLUT[30] = GridToPixel(13.f, 6.f);

	// Turn left (positions 31-32)
	TrackPositionLUT[31] = GridToPixel(14.f, 7.f);
	TrackPositionLUT[32] = GridToPixel(14.f, 8.f);

	// Left across bottom of right side (positions 33-38)
	TrackPositionLUT[33] = GridToPixel(13.f, 8.f);
	TrackPositionLUT[34] = GridToPixel(12.f, 8.f);
	TrackPositionLUT[35] = GridToPixel(11.f, 8.f);
	TrackPositionLUT[36] = GridToPixel(10.f, 8.f);
	TrackPositionLUT[37] = GridToPixel(9.f,  8.f);
	TrackPositionLUT[38] = GridToPixel(8.f,  8.f);

	// Yellow side - bottom right, going left then up (positions 39-44)
	TrackPositionLUT[39] = GridToPixel(8.f,  9.f);
	TrackPositionLUT[40] = GridToPixel(8.f, 10.f);
	TrackPositionLUT[41] = GridToPixel(8.f, 11.f);
	TrackPositionLUT[42] = GridToPixel(8.f, 12.f);
	TrackPositionLUT[43] = GridToPixel(8.f, 13.f);

	// Turn up (positions 44-45)
	TrackPositionLUT[44] = GridToPixel(7.f, 14.f);
	TrackPositionLUT[45] = GridToPixel(6.f, 14.f);

	// Up left column back to Red (positions 46-51)
	TrackPositionLUT[46] = GridToPixel(6.f, 13.f);
	TrackPositionLUT[47] = GridToPixel(6.f, 12.f);
	TrackPositionLUT[48] = GridToPixel(6.f, 11.f);
	TrackPositionLUT[49] = GridToPixel(6.f, 10.f);
	TrackPositionLUT[50] = GridToPixel(6.f, 9.f);
	TrackPositionLUT[51] = GridToPixel(6.f, 8.f);

	// Home yard positions (4 positions in each corner quadrant).
	// Red (bottom-left).
	HomeYardPositions[0]  = GridToPixel(2.f, 11.f);
	HomeYardPositions[1]  = GridToPixel(3.f, 11.f);
	HomeYardPositions[2]  = GridToPixel(2.f, 12.f);
	HomeYardPositions[3]  = GridToPixel(3.f, 12.f);

	// Blue (top-left).
	HomeYardPositions[4]  = GridToPixel(2.f, 2.f);
	HomeYardPositions[5]  = GridToPixel(3.f, 2.f);
	HomeYardPositions[6]  = GridToPixel(2.f, 3.f);
	HomeYardPositions[7]  = GridToPixel(3.f, 3.f);

	// Green (top-right).
	HomeYardPositions[8]  = GridToPixel(11.f, 2.f);
	HomeYardPositions[9]  = GridToPixel(12.f, 2.f);
	HomeYardPositions[10] = GridToPixel(11.f, 3.f);
	HomeYardPositions[11] = GridToPixel(12.f, 3.f);

	// Yellow (bottom-right).
	HomeYardPositions[12] = GridToPixel(11.f, 11.f);
	HomeYardPositions[13] = GridToPixel(12.f, 11.f);
	HomeYardPositions[14] = GridToPixel(11.f, 12.f);
	HomeYardPositions[15] = GridToPixel(12.f, 12.f);
}

void ULudoBoard::HandleDiceClicked()
{
	OnDiceClicked.Broadcast();
}

FVector2D ULudoBoard::GetHomeYardPosition(EPHCLudoColor Color, int32 PieceSubIndex) const
{
	int32 ColorIdx = static_cast<int32>(Color);
	int32 LookupIdx = ColorIdx * 4 + FMath::Clamp(PieceSubIndex, 0, 3);

	if (HomeYardPositions.IsValidIndex(LookupIdx))
	{
		return HomeYardPositions[LookupIdx];
	}

	return FVector2D(-100.f, -100.f);
}
