#include <exec/types.h>
#include <exec/io.h>
#include <exec/devices.h>
#include <devices/ahi.h>

#if __GCC__
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#endif

#if __VBCC__
#include <dos/dos.h>
#include <inline/exec_protos.h>
#include <inline/dos_protos.h>
extern struct SysBase *SysBase;
extern struct DOSBase *DOSBase;
#endif

struct Library *uae_audio_base;

int main(void)
{
	PutStr("trying to open uae.audio library\n");

	uae_audio_base = OpenLibrary("devs:ahi/uae.audio", 4);

	if (uae_audio_base)
	{
		PutStr("Successfully opened uae.audio\n");

		PutStr("wait 100...\n");
		Delay(100);

		PutStr("Successfully closed uae.audio\n");
		CloseLibrary(uae_audio_base);
	}
	else
	{
		PutStr("Failed to open uae.audio\n");
	}

	return 0;
}
