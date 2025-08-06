
extern ULONG uaeahi_AllocAudio(
		REG(a1, struct TagItem *tagList),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl),
		REG(a6, struct UAEAHIDriverBase *driver) );

extern void uaeahi_FreeAudio(
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl) );

extern void uaeahi_Disable(
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

extern void uaeahi_Enable( 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

extern ULONG uaeahi_Start(
		REG(d0, ULONG Flags),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

extern void uaeahi_Update(
		REG(d0, ULONG Flags),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

extern void uaeahi_Stop( 
		REG(d0, ULONG Flags), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

ULONG uaeahi_SetVol( 
		REG(d0, UWORD Channel), 
		REG(d1, UWORD Volume),
		REG(d2, UWORD Pan), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl), 
		REG(d3, ULONG Flags));

ULONG uaeahi_SetFreq(
		REG(d0, UWORD Channel), 
		REG(a0, ULONG Freq), 
		REG(a0, struct AHIAudioCtrlDrv *AudioCtrl), 
		REG(a0, ULONG Flags));

ULONG uaeahi_SetSound(
		REG(d0, UWORD Channel), 
		REG(d1, UWORD Sound), 
		REG(d2, UWORD Offset), 
		REG(d3, UWORD Length), 
		REG(a2, struct AHIAudioCtrlDrv * AudioCtrl), 
		REG(d4, ULONG Flags));

ULONG uaeahi_SetEffect(
		REG(a0, APTR Effect),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

ULONG uaeahi_LoadSound(
		REG(d0, UWORD Sound), 
		REG(d1, ULONG Type), 
		REG(a0, struct AHISampleInfo *Info), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

ULONG uaeahi_UnloadSound(
		REG(d0, UWORD Sound),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

LONG uaeahi_GetAttr(
		REG(d0, ULONG Attribute), 
		REG(d1, LONG Argument), 
		REG(d2, LONG DefValue), 
		REG(a1, struct TagItem *tagList), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

ULONG uaeahi_HardwareControl(
		REG(d0, ULONG Attribute), 
		REG(d1, LONG Argument),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl));

