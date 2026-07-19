#include "../../SDK/SDK.h"

MAKE_HOOK(CTFPlayer_UpdateClientSideAnimation, Signatures::CTFPlayer_UpdateClientSideAnimation.Get(), void, __fastcall,
	C_TFPlayer* ecx)
{
	CALL_ORIGINAL(ecx);
}
