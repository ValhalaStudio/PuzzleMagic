#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// A second language for the instructions and the tutorial. Only those two are translated: the title, the headings, the buttons and the
// game's own terms (ROUTE, TRICK, TREAT, HOLD, RELIC, LUCK, ...) stay English, as the sentences around them are Greek.
//
// It follows the language of the system (the Windows display language, the phone's language): Greek when that is Greek, English for
// everything else. There is no setting for it in the game. For testing: the PuzzleLang el|en|auto console command, -lang=el|en at
// launch, and -oslanguage=el to pretend the system is Greek (Development builds).
// English is the source text in the code; the Greek is data (Content/Localization/el.json for the how-to pages,
// Content/Campaigns/tutorial_el.json for the tutorial), so a missing string or file falls back to English.
// Greek letters need a font that has them: the body font falls back to Content/UI/Fonts/MPLUSRounded1c-ExtraBold.ttf (see UPuzzleHUDWidget).
namespace PuzzleLocale
{
	inline bool GGreek = false;
	inline bool IsGreek() { return GGreek; }

	// Whether the language of the system (the Windows display language, the phone's language) is Greek. This reads the OS's own language,
	// not the engine's current one: the engine only accepts a language the game has localization data for and quietly falls back to
	// English otherwise, so on a Greek system its current language would say "en".
	inline bool FromSystem()
	{
#if !UE_BUILD_SHIPPING
		FString Simulated; // test only: -oslanguage=el pretends the system is Greek
		if (FParse::Value(FCommandLine::Get(), TEXT("oslanguage="), Simulated))
		{
			return Simulated.Equals(TEXT("el"), ESearchCase::IgnoreCase);
		}
#endif
		const FCulturePtr Language = FInternationalization::Get().GetDefaultLanguage();
		UE_LOG(LogTemp, Display, TEXT("System language: %s"), Language.IsValid() ? *Language->GetName() : TEXT("unknown"));
		return Language.IsValid() && Language->GetTwoLetterISOLanguageName() == TEXT("el");
	}

	// Called once at startup.
	inline void Detect()
	{
		FString Forced;
		if (FParse::Value(FCommandLine::Get(), TEXT("lang="), Forced))
		{
			GGreek = Forced.Equals(TEXT("el"), ESearchCase::IgnoreCase);
			return;
		}
		GGreek = FromSystem();
	}

	// The dev command PuzzleLang: "el" forces Greek, "en" forces English, anything else (auto) goes back to the system language.
	inline void SetOverride(const FString& Language)
	{
		GGreek = Language.Equals(TEXT("el"), ESearchCase::IgnoreCase) ? true : (Language.Equals(TEXT("en"), ESearchCase::IgnoreCase) ? false : FromSystem());
	}

	// The Greek strings by key, read on first use.
	inline const TMap<FString, FString>& GreekStrings()
	{
		static TMap<FString, FString> Strings;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const FString Candidates[] = { FPaths::ProjectDir() / TEXT("Localization/el.json"), FPaths::ProjectContentDir() / TEXT("Localization/el.json") };
			for (const FString& Path : Candidates)
			{
				FString Text;
				TSharedPtr<FJsonObject> Root;
				if (FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (Root->TryGetObjectField(TEXT("strings"), Object))
					{
						for (auto It = (*Object)->Values.CreateConstIterator(); It; ++It)
						{
							Strings.Add(FString(It.Key()), It.Value()->AsString()); // the JSON object's key type is not always FString
						}
					}
					break;
				}
			}
		}
		return Strings;
	}

	// The text for a key: the Greek one when Greek is on and the file has it, otherwise the English one given here.
	inline FString Text(const TCHAR* Key, const TCHAR* English)
	{
		if (GGreek)
		{
			if (const FString* Greek = GreekStrings().Find(Key))
			{
				return *Greek;
			}
		}
		return English;
	}
}
