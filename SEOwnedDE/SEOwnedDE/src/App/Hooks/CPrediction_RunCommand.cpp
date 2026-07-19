#include "../../SDK/SDK.h"

MAKE_HOOK(CPrediction_RunCommand, Memory::GetVFunc(I::Prediction, 17), void, __fastcall,
	CPrediction* ecx, C_BasePlayer* player, CUserCmd* pCmd, IMoveHelper* moveHelper)
{
	if (Shifting::bRecharging)
	{
		if (const auto pLocal = H::Entities->GetLocal())
		{
			if (player == pLocal)
				return;
		}
	}

	CALL_ORIGINAL(ecx, player, pCmd, moveHelper);
}
