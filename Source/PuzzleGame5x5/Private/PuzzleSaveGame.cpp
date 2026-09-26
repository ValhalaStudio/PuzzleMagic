#include "PuzzleSaveGame.h"
#include "Kismet/GameplayStatics.h"

UPuzzleSaveGame* UPuzzleSaveGame::LoadOrCreate()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		if (UPuzzleSaveGame* Loaded = Cast<UPuzzleSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
		{
			return Loaded;
		}
	}
	return Cast<UPuzzleSaveGame>(UGameplayStatics::CreateSaveGameObject(UPuzzleSaveGame::StaticClass()));
}

void UPuzzleSaveGame::Save()
{
	UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
}
