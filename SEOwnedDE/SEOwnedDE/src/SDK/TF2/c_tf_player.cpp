#include "c_tf_player.h"

#include "../../App/Features/Players/Players.h"

bool C_TFPlayer::IsPlayerOnSteamFriendsList()
{
	const auto pLocal = reinterpret_cast<void*>(I::ClientEntityList->GetClientEntity(I::EngineClient->GetLocalPlayer()));
	auto result{ reinterpret_cast<bool(__fastcall *)(void *, void *)>(Signatures::CTFPlayer_IsPlayerOnSteamFriendsList.Get())(this, pLocal) };

	if (!result)
	{
		PlayerPriority info{};

		return F::Players->GetInfo(entindex(), info) && info.Ignored;
	}

	return result;
}