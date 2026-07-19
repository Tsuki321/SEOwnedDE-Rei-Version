#include "../../SDK/SDK.h"

#include "../Features/CFG.h"
#include "../Features/NetworkFix/NetworkFix.h"
#include "../Features/SeedPred/SeedPred.h"

MAKE_SIGNATURE(CL_Move, "engine.dll", "40 55 53 48 8D AC 24 ? ? ? ? B8 ? ? ? ? E8 ? ? ? ? 48 2B E0 83 3D", 0x0);

MAKE_HOOK(CL_Move, Signatures::CL_Move.Get(), void, __fastcall,
	float accumulated_extra_samples, bool bFinalTick)
{
	if (CFG::Misc_Ping_Reducer)
		F::NetworkFix->FixInputDelay(bFinalTick);
	else
		F::NetworkFix->Reset();

	auto callOriginal = [&](bool bFinal)
	{
		F::SeedPred->AskForPlayerPerf();

		if (Shifting::nAvailableTicks < MAX_COMMANDS)
		{
			if (H::Entities->GetWeapon())
			{
				if (!Shifting::bRecharging && !Shifting::bShifting && !Shifting::bShiftingWarp)
				{
					if (H::Input->IsDown(CFG::Exploits_Shifting_Recharge_Key))
					{
						Shifting::bRecharging = !I::MatSystemSurface->IsCursorVisible() && !I::EngineVGui->IsGameUIVisible();
					}
				}
			}

			if (Shifting::bRecharging)
			{
				Shifting::nAvailableTicks++;
				return;
			}
		}
		else
		{
			Shifting::bRecharging = false;
		}

		CALL_ORIGINAL(accumulated_extra_samples, bFinal);
	};

	auto getShiftCommandCapacity = []()
	{
		return CNetworkFix::GetShiftCommandCapacity(
			I::ClientState ? I::ClientState->chokedcommands : MAX_COMMANDS
		);
	};

	if (Shifting::bRapidFireWantShift)
	{
		Shifting::bRapidFireWantShift = false;
		Shifting::bShifting = true;

		const int nTicks = std::min({
			CFG::Exploits_RapidFire_Ticks,
			Shifting::nAvailableTicks,
			getShiftCommandCapacity()
		});

		if (nTicks <= 0)
		{
			Shifting::bShifting = false;
			callOriginal(bFinalTick);
			return;
		}

		for (int n = 0; n < nTicks; n++)
		{
			callOriginal(n == nTicks - 1);
			Shifting::nAvailableTicks--;
		}

		Shifting::bShifting = false;

		return;
	}

	if (const auto pLocal = H::Entities->GetLocal())
	{
		if (!pLocal->deadflag() && !Shifting::bRecharging && !Shifting::bShifting && !Shifting::bShiftingWarp && !Shifting::bRapidFireWantShift)
		{
			if (!I::MatSystemSurface->IsCursorVisible() && !I::EngineVGui->IsGameUIVisible() && (H::Input->IsDown(CFG::Exploits_Warp_Key)))
			{
				if (Shifting::nAvailableTicks)
				{
					Shifting::bShifting = true;
					Shifting::bShiftingWarp = true;
					const int nCommandCapacity = getShiftCommandCapacity();

					if (CFG::Exploits_Warp_Mode == 0)
					{
						if (nCommandCapacity < 2)
						{
							Shifting::bShifting = false;
							Shifting::bShiftingWarp = false;
							callOriginal(bFinalTick);
							return;
						}

						for (int n = 0; n < 2; n++)
						{
							callOriginal(n == 1);
						}

						Shifting::nAvailableTicks--;
					}

					if (CFG::Exploits_Warp_Mode == 1)
					{
						const int nTicks = std::min(Shifting::nAvailableTicks, nCommandCapacity);
						if (nTicks <= 0)
						{
							Shifting::bShifting = false;
							Shifting::bShiftingWarp = false;
							callOriginal(bFinalTick);
							return;
						}

						for (int n = 0; n < nTicks; n++)
						{
							callOriginal(n == nTicks - 1);
							Shifting::nAvailableTicks--;
						}
					}

					Shifting::bShifting = false;
					Shifting::bShiftingWarp = false;

					return;
				}
			}
		}
	}

	callOriginal(bFinalTick);
}
