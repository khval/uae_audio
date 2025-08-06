#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/devices.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <exec/tasks.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <utility/tagitem.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <devices/newstyle.h>
#include <libraries/ahi_sub.h>
#include <graphics/gfxbase.h>

#define bool BOOL
#define true TRUE
#define false FALSE

/*
#include <devices/ahi.h>
*/
#if __GCC__
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#define REG(_reg_, _type_) register _type_ asm(#_reg_)
#endif

#if __VBCC__
#include <dos/dos.h>
#include <inline/exec_protos.h>
#include <inline/dos_protos.h>
#include <inline/uae_accelerator.h>

#define NO_INLINE_STDARG

#include <inline/utility.h>
#define REG(_reg_,_type_) __reg(#_reg_) _type_
#endif

#define VERSION     4
#define REVISION    0
#define IDSTRING    "uae.audio 4.0"

#define PALFREQ	3546895
#define NTSCFREQ	3579545

const char *devname = "uae.audio";

extern struct ExecBase *SysBase;
extern struct DOSBase *DOSBase;

struct Library *AcceleratorBase = NULL;
struct Library *UtilityBase = NULL;

void UNum2HostPutStr( const char *what,ULONG num );
void SNum2HostPutStr( const char *what,LONG num );
void Hex2HostPutStr( const char *what, ULONG num );
void Bin2HostPutStr( const char *what, ULONG num );

struct DriverData;

#define TF_ISPLAYING 1

struct UAEAHIDriverBase
{
	struct Library lib;

	/* Standard paula.audio-compatible private fields */
	UBYTE  pb_Flags;
	UBYTE  pb_Pad1;
	UWORD  pb_Pad2;

	struct ExecBase	*pb_SysLib;
	ULONG			pb_SegList;

	struct GfxBase		*pb_GfxLib;
	struct Library		*pb_UtilLib;
	struct Library		*pb_DosLib;
	struct Library		*pb_IntuiLib;
	struct Library		*pb_TimerLib;
	struct Library		*pb_MiscResource;
	struct Library		*pb_CardResource;

	ULONG  			pb_AudioFreq;   /* PAL/NTSC detection result */
	APTR			pb_Lock;

	ULONG			asked_for_version;
	APTR			mixbuffer;
};

#define dd (*(struct DriverData **) &AudioCtrl->ahiac_DriverData)

extern APTR func_table[];

/* Prototypes */

struct UAEAHIDriverBase * uaeahi_init(REG(d0,struct UAEAHIDriverBase *driver), REG(a0,BPTR seglist), REG(a6, APTR sysbase));
ULONG uaeahi_open(REG(a6,struct UAEAHIDriverBase *driver));
ULONG uaeahi_close(REG(a6, struct UAEAHIDriverBase *driver));
ULONG uaeahi_expunge( REG(a6,struct UAEAHIDriverBase *driver) );

ULONG uaeahi_null(void);
void uaeahi_beginio(REG(a1, struct IOStdReq *ior));
void uaeahi_abortio(REG(a1, struct IOStdReq *ior));

/* Init table */
struct InitTable
{
    ULONG size;
    APTR *funcTable;
    APTR dataInit;
    APTR initFunc;
} init_tab =
{
    sizeof(struct UAEAHIDriverBase),
    func_table,
    NULL,
    (APTR)uaeahi_init
};

#include <inc/subFuncs.h>

/* Function table */

APTR func_table[] =
{
	(APTR)uaeahi_open,			/* -6 */
	(APTR)uaeahi_close,			/* -12 */
	(APTR)uaeahi_expunge,		/* -18 */
	(APTR)uaeahi_null,			/* -24 */
	(APTR)uaeahi_AllocAudio,
	(APTR)uaeahi_FreeAudio,
	(APTR)uaeahi_Disable,
	(APTR)uaeahi_Enable,
	(APTR)uaeahi_Start,
	(APTR)uaeahi_Update,
	(APTR)uaeahi_Stop,
	(APTR)uaeahi_SetVol,
	(APTR)uaeahi_SetFreq,
	(APTR)uaeahi_SetSound,
	(APTR)uaeahi_SetEffect,
	(APTR)uaeahi_LoadSound,
	(APTR)uaeahi_UnloadSound,
	(APTR)uaeahi_GetAttr,
	(APTR)uaeahi_HardwareControl,
	(APTR)-1
};

/* Resident tag */
struct Resident romtag =
{
    RTC_MATCHWORD,
    &romtag,
    &romtag + 1,
    RTF_AUTOINIT,
    VERSION,
    NT_LIBRARY,
    0,
    (UBYTE *)"uae.audio",
    (UBYTE *)IDSTRING,
    (APTR)&init_tab
};


void ALERT(ULONG err)
{
}

void __couse_error(__reg("d0") ULONG val, __reg("a0") void *addr)=
				"\tmove.l\td0,a0\n"
				"\tillegal\t";

/* Init */

void SafeLog(const char *msg)
{
	if (AcceleratorBase)
	{
		HostPutStr(msg);
	}
}


#if 0

char *dupStr(char *str)
{
	int l = 1;
	char *c,*d, *nstr;

	for (c=str;*c;c++) l++;
	nstr = (char *) AllocVec( l, MEMF_FAST );

	d=nstr;
	for (c=str;*c;c++) { *d = *c; d++; }
	*d = 0;

	return nstr;
}

#endif

/* The init function: a replacement for `initRoutine` */

struct UAEAHIDriverBase * uaeahi_init(REG(d0,struct UAEAHIDriverBase *driver), REG(a0,BPTR seglist), REG(a6, APTR sysbase))
{
	struct GfxBase *GfxBase;
	struct Library *lib;

	SysBase = *( (struct ExecBase **) 4UL);

	if (SysBase != NULL)
	{
		DOSBase = (struct DOSBase *) FindName(&SysBase -> LibList, "dos.library");		
		if (DOSBase)
		{
			SafeLog("uaeahi_init\n");
			SafeLog(devname);
			SafeLog("\n");
			SafeLog(IDSTRING);
			SafeLog("\n");
		}
	}

	AcceleratorBase = OpenLibrary("accelerator.library",0L);
	if (AcceleratorBase)
	{
		SafeLog("accelerator.library is open\n");
	}

	UtilityBase = OpenLibrary("utility.library",0L);
	if (UtilityBase)
	{
		SafeLog("utility.library is open\n");
	}

	UNum2HostPutStr("lib_NegSize: ", driver -> lib.lib_NegSize );
	SafeLog("\n");

	UNum2HostPutStr("lib_NegSize: ", driver -> lib.lib_PosSize );
	SafeLog("\n");

	UNum2HostPutStr("sizeof(struct UAEAHIDriverBase): ", sizeof(struct UAEAHIDriverBase) );
	SafeLog("\n");

	driver -> pb_SysLib  = sysbase;
	driver -> pb_SegList = seglist;
	driver -> mixbuffer = NULL;

    /* Open graphics.library */
    lib = OpenLibrary("graphics.library", 37);
    if (!lib) { ALERT(0x100000 | 0x0001); return NULL; }
    driver->pb_GfxLib = (struct GfxBase *)lib;


    /* Open utility.library */
    lib = OpenLibrary("utility.library", 37);
    if (!lib) { ALERT(0x100000 | 0x0002); return NULL; }
    driver->pb_UtilLib = lib;


    /* Open dos.library */
    lib = OpenLibrary("dos.library", 37);
    if (!lib) { ALERT(0x100000 | 0x0003); return NULL; }
    driver->pb_DosLib = lib;


    /* Open intuition.library */
    lib = OpenLibrary("intuition.library", 37);
    if (!lib) { ALERT(0x100000 | 0x0004); return NULL; }
    driver->pb_IntuiLib = lib;


    /* Open misc.resource */
    lib = OpenResource("misc.resource");
    if (!lib) { ALERT(0x200000 | 0x0001); return NULL; }
    driver->pb_MiscResource = lib;


    /* Open card.resource (optional) */
    lib = OpenResource("card.resource");
    driver->pb_CardResource = lib;  /* may be NULL, non-fatal */

    /* Set AudioFreq depending on PAL/NTSC */

	GfxBase = driver->pb_GfxLib;

	if (GfxBase->DisplayFlags & REALLY_PAL)
	{
		driver->pb_AudioFreq = PALFREQ;
	}
	else
	{
		driver->pb_AudioFreq = NTSCFREQ;
	}

	return driver;
}


/* Open */
ULONG uaeahi_open(REG(a6,struct UAEAHIDriverBase *driver))
{
	REG(d0, ULONG ver);

	driver -> asked_for_version = ver;

	SafeLog("uae.audio: OpenLibrary( "); 
		Hex2HostPutStr( " ver: ", (ULONG)  ver );
		Hex2HostPutStr( " ,LibBase: ",  (ULONG) driver );
	SafeLog(" )\n"); 

	if (UtilityBase)
	{
	 	driver->lib.lib_OpenCnt++;

		UNum2HostPutStr( "LibCount: ", driver-> lib.lib_OpenCnt );
		SafeLog(" )\n"); 

	 	driver-> lib.lib_Flags &= ~LIBF_DELEXP; /* Clear LIBF_DELEXP bit */

		return (ULONG) driver;
	}
	else
	{
		SafeLog("uae.audio: OpenLibrary failed\n"); 
		return 0L;
	}
}

/* Close */
ULONG uaeahi_close( REG(a6,struct UAEAHIDriverBase *driver) )
{
	SafeLog("uae.audio: CloseLibrary( "); 
		Hex2HostPutStr( " LibBase: ", (ULONG) driver );
	SafeLog(" )\n"); 

	if (driver->lib.lib_OpenCnt>0)
	{
		driver->lib.lib_OpenCnt--;
		UNum2HostPutStr( "LibCount: ", driver->lib.lib_OpenCnt );
		SafeLog(" )\n"); 
	}
	else
	{
		SafeLog("more CloseLibrary's then OpenLibrarie's\n"); 
	}

	return 0;
}

/* resources are only opened, never closed, so its just a dummy */
#define CloseResource(x)

#define sf(fn,type,ptr) if (ptr) { fn( (type) ptr); ptr = NULL; }

/* Expunge */
ULONG uaeahi_expunge( REG(a6,struct UAEAHIDriverBase *driver) )
{
	BPTR seglist;
	
	SafeLog("ahi.audio: expunge\n"); 

	if ( driver -> lib.lib_OpenCnt > 0 )
	{
		SafeLog("uae.audio: wtf expunge failed, called before its zero\n"); 
		 return 0;
	}

	SafeLog("ahi.audio: expunge: close all open libraies\n"); 

	/* Close libraries */
	sf( CloseLibrary, struct Library *, driver -> pb_GfxLib );
	sf( CloseLibrary, struct Library *, driver -> pb_UtilLib );
	sf( CloseLibrary, struct Library *, driver -> pb_DosLib );
	sf( CloseLibrary, struct Library *, driver -> pb_IntuiLib );

	/* Close resources */
	sf( CloseResource, void *, driver->pb_MiscResource );
	sf( CloseResource, void *, driver->pb_CardResource );

	sf( CloseLibrary, struct Library *, UtilityBase );
	sf( CloseLibrary, struct Library *, AcceleratorBase );

	SafeLog("ahi.audio: expunge remove from list and free memory\n"); 

	seglist = driver -> pb_SegList;
	Remove( (struct Node *) driver );
	FreeMem( (APTR) ((UBYTE *) driver - driver -> lib.lib_NegSize), driver -> lib.lib_NegSize +driver -> lib.lib_PosSize);

	return seglist;
}

/* Null */
ULONG uaeahi_null(void)
{
	SafeLog("uae.audio: uaeahi_null(void)\n"); 
	return 0;
}

#include <subFuncs.c>

int main()
{
	PutStr("you cant run this file\n");
	return 0;
}

