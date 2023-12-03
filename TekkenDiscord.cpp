#include "TekkenDiscord.h"
#include "Utilities/MinHook.h"
#include "discord.h"
#include <ObjectProxy.h>
#include <TekkenEntities.h>
#include <chrono>

discord::Core* core{};
tekkendiscord::TekkenStatus status{};
constexpr static int64_t applicationID = 1178519907953021148;
static std::map<int, std::string> tekkenStages;
static std::map<int, std::string> tekkenGameModes;
static std::map<int, std::string> tekkenGameStates;
static std::map<int, std::string> tekkenGameMenus;
static std::string const tekkenCharacters[] = {
	"Paul",
	"Law",
	"King",
	"Yoshimitsu",
	"Hwoarang",
	"Xiaoyu",
	"Jin",
	"Bryan",
	"Heihachi",
	"Kazyua",
	"Steve",
	"Jack-7",
	"Asuka",
	"Devil Jin",
	"Feng",
	"Lili",
	"Dragunov",
	"Leo",
	"Lars",
	"Alisa",
	"Claudio",
	"Katarina",
	"Lucky Chloe",
	"Shaheen",
	"Josie",
	"Gigas",
	"Kazumi",
	"Devil Kazumi",
	"Nina",
	"Master Raven",
	"Lee",
	"Bob",
	"Akuma",
	"Kuma",
	"Panda",
	"Eddy",
	"Eliza",
	"Miguel",
	"Tekken Force Grunt",
	"Young Kazuya",
	"Jack-4",
	"Young Heihachi",
	"Dummy",
	"Geese",
	"Noctis",
	"Anna",
	"Lei",
	"Armor King",
	"Marduk",
	"Julia",
	"Negan",
	"Zafina",
	"Ganryu",
	"Leroy",
	"Fahkumram",
	"Kunimitsu",
	"Lidia",
};

bool runDiscordPlugin;

BPFUNCTION(WriteToFile)
{
	std::cout << "WriteToFile" << std::endl;
	struct InputParams
	{
		UE4::FString NameTest;
	};
	auto Inputs = stack->GetInputParams<InputParams>();
	stack->SetOutput<UE4::FString>("OutPutString", L"KboyGang");
	stack->SetOutput<bool>("ReturnValue", true);
}

// Only Called Once, if you need to hook shit, declare some global non changing values
void TekkenDiscord::InitializeMod()
{
	UE4::InitSDK();
	SetupHooks();

	REGISTER_FUNCTION(WriteToFile);

	tekkenStages = tekkendiscord::InitStageMap();
	tekkenGameStates = tekkendiscord::InitGameStates();
	tekkenGameModes = tekkendiscord::InitGameModes();
	tekkenGameMenus = tekkendiscord::InitGameMenus();

	auto result = discord::Core::Create(applicationID, DiscordCreateFlags_Default, &core);
	if (result == discord::Result::Ok)
	{
		runDiscordPlugin = true;
	}

	//MinHook::Init(); //Uncomment if you plan to do hooks

	UseMenuButton = true; // Allows Mod Loader To Show Button
}

void TekkenDiscord::InitGameState()
{
}

void TekkenDiscord::BeginPlay(UE4::AActor* Actor)
{
}

void TekkenDiscord::PostBeginPlay(std::wstring ModActorName, UE4::AActor* Actor)
{
	// Filters Out All Mod Actors Not Related To Your Mod
	std::wstring TmpModName(ModName.begin(), ModName.end());
	if (ModActorName == TmpModName)
	{
		//Sets ModActor Ref
		ModActor = Actor;
	}
}

void TekkenDiscord::DX11Present(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, ID3D11RenderTargetView* pRenderTargetView)
{
	if (runDiscordPlugin)
	{
		if (core)
		{
			FetchAndUpdateDiscordStatus();
			discord::Activity activity{};

			activity.SetState(status.state);
			activity.SetDetails(status.details);

			if (status.character > -1)
			{
				std::string s;
				s.append("chr_");
				s.append(std::to_string(status.character));

				activity.GetAssets().SetSmallImage(s.c_str());
				activity.GetAssets().SetSmallText(tekkenCharacters[status.character].c_str());
			}

			if (status.stage > -1)
			{
				std::string s2;
				s2.append("stg_");
				s2.append(std::to_string(status.stage));

				activity.GetAssets().SetLargeImage(s2.c_str());
				activity.GetAssets().SetLargeText(tekkenStages[status.stage].c_str());
			}
			else
			{
				activity.GetAssets().SetLargeImage("fallback");
				activity.GetAssets().SetLargeText("All Fights are Personal");
			}
			
			activity.GetTimestamps().SetStart(status.startTime);
			activity.SetType(discord::ActivityType::Playing);

			core->ActivityManager().UpdateActivity(activity, [](discord::Result res)
				{
					if (res != discord::Result::Ok)
					{
						std::cout << "There was an issue updating Discord!\n";
					}
				});

			core->RunCallbacks();
		}
	}
}

void TekkenDiscord::FetchAndUpdateDiscordStatus()
{
	uintptr_t baseAddress = (uintptr_t)GetModuleHandleA(nullptr);
	TekkenOverlayCommon::DataAccess::ObjectProxy<int> move_pointer_p1{ baseAddress, 0x34DF630, 0x218, 0x0 };
	if (!CanReadGameMode())
	{
		TekkenOverlayCommon::DataAccess::ObjectProxy<int> character_select_p1{ baseAddress,  0x034D65C0, 0x508 };
		TekkenOverlayCommon::DataAccess::ObjectProxy<int> character_select_p2{ baseAddress,  0x034D65C0, 0x948 };
		if (character_select_p1.IsValid())
		{
			TekkenOverlayCommon::DataAccess::ObjectProxy<int> menu_selected{ baseAddress,  0x34D5B50};
			if (menu_selected != 5)
			{
				if (character_select_p1 == 255 && character_select_p2 == 255)
				{
					status.state = "Side Select";
					status.side_saved = -1;
				}
				else
				{
					
					// We save the player side the first time the player backs into the character select.
					// This is done to fix my P2 glitch in this code.
					if (status.side_saved < 0)
					{
						status.side_saved = character_select_p1 != 255 ? 0 : 1;
					}
					
					status.state = "Character Select";
					status.character = status.side_saved == 0 ? character_select_p1 : character_select_p2;
				}
			}
			else
			{
				status.side_saved = -1;
				status.state = "Character Select";
			}

			status.details = tekkenGameMenus[menu_selected].c_str();
		}
		else
		{
			TekkenOverlayCommon::DataAccess::ObjectProxy<int> stage_select{ baseAddress,  0x034D6918, 0x188 };
			if (stage_select.IsValid())
			{
				status.state = "Stage Select";
				status.stage = stage_select;
			}
			else
			{
				status.state = "Waiting";
				status.details = "";
				status.character = -1;
				status.stage = -1;
			}
		}

		status.gameMode = -1;
		status.startTime = 0;
	}
	else
	{
		TekkenOverlayCommon::DataAccess::ObjectProxy<int> game_mode{ baseAddress, 0x379B158, 0x8, 0x8, 0x0 , 0x470 , 0x10 };
		if (!game_mode.IsValid())
		{
			status.state = "Dead Game Mode";
			return;
		}

		if (!tekkenGameModes.count(game_mode))
		{
			status.state = "Unknown Game Mode";
			return;
		}

		// If the player is watching a replay display the replay text instead of the game mode.
		TekkenOverlayCommon::DataAccess::ObjectProxy<int> menu_selected{ baseAddress,  0x34D5B50 };
		if (menu_selected == 8)
		{
			status.state = "Watching Replay";
		}
		else
		{
			status.state = tekkenGameModes[game_mode].c_str();
		}

		TekkenOverlayCommon::DataAccess::ObjectProxy<int> game_state{ baseAddress + 0x34CD4F4 };
		if (!tekkenGameStates.count(game_state))
		{
			status.state = "Waiting";
			status.startTime = 0;
			return;
		}

		TekkenOverlayCommon::DataAccess::ObjectProxy<int> round_count{ baseAddress + 0x34CD6E0 };
		std::string stateText;

		if (game_mode != 0 && game_mode != 5 && round_count > 0)
		{
			stateText.append("Round ");
			stateText.append(std::to_string(round_count));
			stateText.append(": ");
		}

		stateText.append(tekkenGameStates[game_state].c_str());
		status.details = stateText.c_str();

		// Versus mode we don't need any of the specific character information.
		if (IsMultiplePlayerGameMode())
		{
			status.character = -1;
		}
		else
		{
			// Change Character Icon based on player side information.
			TekkenOverlayCommon::DataAccess::ObjectProxy<bool> are_sides_reversed_online{ baseAddress + 0x34DF554 };
			TekkenOverlayCommon::DataAccess::ObjectProxy<bool> is_player_right_side{ baseAddress + 0x344788C };
			TekkenOverlayCommon::DataAccess::ObjectProxy<int> char_p1{ baseAddress , 0x34DF630 , 0xD8 };
			TekkenOverlayCommon::DataAccess::ObjectProxy<int> char_p2{ baseAddress , 0x34DF628 , 0xD8 };
			TekkenOverlayCommon::DataAccess::ObjectProxy<int> player_char;

			// If we're playing online we need to check if sides have been reversed.
			if (game_mode != 4)
			{
				player_char = !is_player_right_side ? char_p1 : char_p2;
			}
			else
			{
				if (are_sides_reversed_online)
				{
					player_char = !is_player_right_side ? char_p2 : char_p1;
				}
				else
				{
					player_char = !is_player_right_side ? char_p1 : char_p2;
				}
				
			}

			status.character = player_char;
		}

		// Change Stage Icon based on stage information.
		TekkenOverlayCommon::DataAccess::ObjectProxy<int> stage_id{ baseAddress + 0x34DF550 };
		status.stage = stage_id;

		// Activate timer in correct match conditions.
		if (round_count < 2)
		{
			if (game_state == 4 || game_state == 21 || game_state == 22)
			{
				if (game_state != status.currentGameState)
				{
					status.startTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				}
			}
		}

		status.currentGameState = game_state;
	}
}

void TekkenDiscord::OnModMenuButtonPressed()
{
	Log::Print("Printing Base Address");
	uintptr_t baseAddress = (uintptr_t)GetModuleHandleA(nullptr);
	Log::Print("%04x", baseAddress);

	Log::Print("Move Pointer Base Address");
	TekkenOverlayCommon::DataAccess::ObjectProxy<int> move_pointer_p1{ baseAddress, 0x34DF630, 0x218, 0x0 };
	if (move_pointer_p1.IsValid())
	{
		Log::Print("%04x", move_pointer_p1);
		Log::Print(std::to_string(move_pointer_p1));
	}
	else
	{
		Log::Print("Pointer is not valid, will crash your game!");
	}

	Log::Print("Printing Epoch Time");
	Log::Print(std::to_string(status.startTime));
	
	
	Log::Print("Printing Current Stage ID");
	TekkenOverlayCommon::DataAccess::ObjectProxy<int> stage_id{ baseAddress + 0x34DF550 };
	Log::Print(std::to_string(stage_id));

	TekkenOverlayCommon::DataAccess::ObjectProxy<int> local_player_side{ baseAddress + 0x344788C };
	Log::Print("Local Player Side");
	Log::Print(std::to_string(local_player_side));

	TekkenOverlayCommon::DataAccess::ObjectProxy<int> player_char;
	if (local_player_side == 0)
	{
		player_char = { baseAddress , 0x34DF630 , 0xD8 };
	}
	else
	{
		player_char = { baseAddress , 0x34DF628 , 0xD8 };
	}
	
	Log::Print("Player Character With Offset");
	Log::Print(std::to_string(player_char));

	std::string s;
	s.append("chr_");
	s.append(std::to_string(status.character));
	Log::Print(s);

	std::string s2;
	s2.append("stg_");
	s2.append(std::to_string(status.stage));
	Log::Print(s2);



}

bool TekkenDiscord::CanReadGameMode()
{
	uintptr_t baseAddress = (uintptr_t)GetModuleHandleA(nullptr);
	int playerOffset[2] = { 0x34DF630, 0x34DF628};
	for (int i = 0; i < 2; i++) {

		TekkenOverlayCommon::DataAccess::ObjectProxy<int> move_pointer{ baseAddress,  playerOffset[i], 0x218, 0x0 };
		if (!move_pointer.IsValid() || move_pointer == 0 || move_pointer == -1)
		{
			return false;
		}

		TekkenOverlayCommon::DataAccess::ObjectProxy<int> move_anim_pointer{ baseAddress, playerOffset[i], 0x218, 0x10, 0x0 };
		if (!move_anim_pointer.IsValid() || move_anim_pointer == 0 || move_anim_pointer == -1)
		{
			return false;
		}
	}

	return true;
}

bool TekkenDiscord::IsMultiplePlayerGameMode()
{
	uintptr_t baseAddress = (uintptr_t)GetModuleHandleA(nullptr);
	TekkenOverlayCommon::DataAccess::ObjectProxy<int> game_mode{ baseAddress, 0x379B158, 0x8, 0x8, 0x0 , 0x470 , 0x10 };
	return game_mode == 6 || game_mode == 2;
}


void TekkenDiscord::DrawImGui()
{
	
}