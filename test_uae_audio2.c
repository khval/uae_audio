/* Test harness for UAE AHI Audio Driver (v2 Spec)
 * Purpose: Validate core driver functions for conformance and safety
 * Target: uae.audio compiled as AHI-compatible device
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <devices/ahi.h>
#include <libraries/ahi_sub.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <inline/ahi_sub.h>

struct Library *AHIsubBase = NULL;

extern int printf(const char *, ...);

void test_OpenCloseLibrary()
{
	struct Library *lib = OpenLibrary("devs:ahi/uae.audio", 2);
	if (lib)
	{
		printf("PASS: OpenLibrary() succeeded (lib: %ld)\n", (ULONG) lib);
		CloseLibrary(lib);
	}
	else
	{
		printf("FAIL: OpenLibrary() failed\n");
	}
}

void test_LoadSound_Invalid()
{
	struct AHIAudioCtrlDrv dummy_ctrl;
	ULONG errCode;

	dummy_ctrl.ahiac_DriverData = NULL;

	errCode = AHIsub_LoadSound(9999,AHIST_SAMPLE,NULL,&dummy_ctrl);

	if (errCode == AHIE_BADSOUNDTYPE || errCode == AHIE_UNKNOWN || errCode == AHIE_ABORTED)
		printf("PASS: LoadSound rejected bad input with err=%lu\n", errCode);
	else
		printf("FAIL: LoadSound unexpected response err=%lu\n", errCode);
}

void test_GetAttr_Unknown()
{

	struct AHIAudioCtrlDrv dummy_ctrl;
	ULONG result;

	dummy_ctrl.ahiac_DriverData = NULL;
	result = AHIsub_GetAttr(0xFFFF0000UL, 123, 1234, NULL, &dummy_ctrl);
	printf("GetAttr unknown tag returned: %lu (expected 1234 fallback)\n", result);

}

void test_SetFreq_Invalid()
{
	struct AHIAudioCtrlDrv dummy_ctrl;
	ULONG errCode;

	dummy_ctrl.ahiac_DriverData = NULL;
	errCode = AHIsub_SetFreq(99, 30, &dummy_ctrl, 0); 
	printf("SetFreq on invalid channel returned: %lu\n", errCode);
}

int main()
{
	AHIsubBase = OpenLibrary("devs:ahi/uae.audio", 2);
	if (!AHIsubBase)
	{
		printf("Unable to open uae.audio\n");
		return 1;
	}

	printf("Testing uae.audio conformance...\n");

	test_OpenCloseLibrary();

/*
	test_LoadSound_Invalid();
	test_GetAttr_Unknown();
	test_SetFreq_Invalid();
*/

	CloseLibrary(AHIsubBase);
	return 0;
}

