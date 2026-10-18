#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TrickOrTreat.generated.h"

class UPuzzleManager;
class AGridManager;

// Where a treat is paid for. The Windows build has only the test store below; an iPhone build will put StoreKit
// behind the same call (that needs the Mac and an Apple developer account).
class IPuzzleStore
{
public:
	virtual ~IPuzzleStore() = default;
	virtual void Purchase(const FString& ProductId, TFunction<void(bool bSuccess)> OnDone) = 0;
	// True for a store that takes no money (the card says so).
	virtual bool IsTestStore() const = 0;
};

// Every purchase succeeds at once and costs nothing.
class FTestPuzzleStore : public IPuzzleStore
{
public:
	virtual void Purchase(const FString& ProductId, TFunction<void(bool bSuccess)> OnDone) override
	{
		UE_LOG(LogTemp, Display, TEXT("TrickOrTreat: test purchase of %s"), *ProductId);
		OnDone(true);
	}
	virtual bool IsTestStore() const override { return true; }
};

// One thing the Treat side of the packet sells: a single treat or a Halloween bundle of several.
struct FTreatOffer
{
	FString ProductId;
	FString Title;
	FString Detail;
	FString Price;
	bool bBundle = false;
	int32 Luck = 0;
	int32 HolyLights = 0;
	int32 Rerolls = 0;
	int32 Tiles = 0;      // an extra single tile in HOLD
	int32 Pumpkins = 0;   // pumpkin bonus tiles dropped on the board
};

// The Halloween packet: it turns up every PacketEvery points (with the Relics option on, since it deals in luck
// and relics) and asks "Trick or treat?". A treat is bought (luck, an extra tile, a Holy Light); a trick spins a
// wheel of eight fortunes, half good and half bad. The rules live here, the cards in UPuzzleHUDWidget, the flow
// in APuzzleGameMode.
UCLASS()
class PUZZLEGAME5X5_API UTrickOrTreat : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 PacketEvery = 300;
	static constexpr int32 SegmentCount = 8;
	static constexpr int32 LuckyCandleLuck = 25;
	static constexpr int32 TrickLuck = 15;

	void Init(UPuzzleManager* InRules, AGridManager* InBoard);
	void StartRound();

	// After every scoring move: true when the score has just reached the next packet.
	bool CheckArrival();
	// A packet arrives now, whatever the score (-treatpacket, for the trailer).
	void ForceArrival() { bAvailable = true; }
	bool IsAvailable() const { return bAvailable; }
	// The packet is opened (treat or trick); the next one comes PacketEvery points later.
	void Consume() { bAvailable = false; }

	static const TArray<FTreatOffer>& Offers();
	bool IsTestStore() const { return Store && Store->IsTestStore(); }
	// Pays for offer Index, then applies it. OnDone(true) once it is in effect.
	void BuyTreat(int32 Index, TFunction<void(bool bSuccess)> OnDone);

	// Picks where the wheel stops (-tricksegment=N forces it). The effect waits for ApplyTrick, when the wheel has stopped.
	int32 SpinTrick();
	// Applies the fortune of Segment and returns the line to show for it.
	FString ApplyTrick(int32 Segment);

	static FString SegmentLabel(int32 Segment);
	static bool IsGoodSegment(int32 Segment) { return Segment % 2 == 0; }

private:
	UPROPERTY()
	TObjectPtr<UPuzzleManager> Rules;

	UPROPERTY()
	TObjectPtr<AGridManager> Board;

	TSharedPtr<IPuzzleStore> Store;
	int32 NextPacketScore = PacketEvery;
	bool bAvailable = false;
	int32 ForcedSegment = -1;
};
