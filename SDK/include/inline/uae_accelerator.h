#ifndef uae_accelerator_H
#define uae_accelerator_H

#include <exec/types.h>

#define PAT_LoopbackVolume 1

enum
{
	TT_PlayTask = 1000,
	TT_PlaySignal,
	TT_RawInt,
	TT_Mode,
	TT_Frequency,
	TT_RawBuffer,
	TT_BufferSize,
	TT_RawIrqSize
};

/* You must define this in your code and open the library */

extern struct Library *AcceleratorBase;

/* CopyMem(source,dest,size) */
LONG __HostCopyMem(__reg("a6") struct Library *,
              __reg("a0") APTR,
              __reg("a1") APTR,
              __reg("d0") LONG) = "\tjsr\t-30(a6)";
#define HostCopyMem(source,dest,size) __HostCopyMem(AcceleratorBase, source, dest, size)


/* CopyMemQuick(source,dest,size) */
LONG __HostCopyMemQuick(__reg("a6") struct Library *,
              __reg("a0") APTR,
              __reg("a1") APTR,
              __reg("d0") LONG) = "\tjsr\t-36(a6)";
#define HostCopyMemQuick(source,dest,size) __HostCopyMem(AcceleratorBase,source,dest,size)

/* HostPutStr(txt) */
LONG __HostPutStr(__reg("a6") struct Library *,
              __reg("a0") APTR) = "\tjsr\t-42(a6)";
#define HostPutStr(txt) __HostPutStr(AcceleratorBase,txt)

/****************************** AUDIO **********************************/

/* SetPartTagList( taglist ) */
LONG __SetPartTagList(__reg("a6") struct Library *,
              __reg("a0") APTR) = "\tjsr\t-48(a6)";
#define SetPartTagList(taglist) __SetPartTagList(AcceleratorBase,taglist)

/* RawPlaybackTagList( taglist ) */
LONG __RawPlaybackTagList(__reg("a6") struct Library *,
              __reg("a0") APTR) = "\tjsr\t-54(a6)";
#define RawPlaybackTagList(taglist) __RawPlaybackTagList(AcceleratorBase,taglist)


#endif /* uae_accelerator_H */

