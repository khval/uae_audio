

#define PLAYBUFFERSIZE 4*1024

#define MAX_SOUNDS 10
#define MAX_CHANNELS 5


struct SoundEntry
{
	ULONG type;
	APTR address;
	ULONG length;	
};

struct UAEAudioChannel
{
	BOOL active;
	BOOL reverse;
	ULONG offset; /* offset can count backwards if length is negative */
	ULONG length; /* show the direction of playback */
	struct SoundEntry sound;
};

struct DriverData
{
	ULONG flags;
	struct SoundEntry sounds[MAX_SOUNDS];
	struct UAEAudioChannel channels[MAX_CHANNELS];
	struct UAEAHIDriverBase *driver;

	struct Task *mainTask;
	struct Process *audioTask;

	LONG mainSignal;
	LONG playSignal;
	LONG mixSignal;

	LONG mode;
	APTR SampBuffer[2];
};


typedef BOOL PreTimer_proto( struct AHIAudioCtrlDrv * );
typedef void PostTimer_proto( struct AHIAudioCtrlDrv * );

BOOL StartPlaying(struct AHIAudioCtrlDrv *AudioCtrl, struct Process *me, APTR buffer);

static BOOL CopyChannelToPlayback(struct UAEAudioChannel *chan, UBYTE *playBuf, ULONG bufSize);

void UNum2HostPutStr( const char *what,ULONG num )
{
	char buff[100];
	char *p;
	int n,cnt = 0;
	ULONG tmp = num;

	if (num)
	{
		while (tmp)
		{
			cnt++;
			tmp/=10;
		}
	}
	else cnt=1;

	n = cnt;
	p = buff+cnt-1;
	tmp = num;

	while (n--)
	{
		*p-- = (tmp%10) + '0';
		tmp/=10;
	}

	buff[cnt]=0;

	SafeLog( what );
	SafeLog( buff ); 
}

void SNum2HostPutStr( const char *what,LONG num )
{
	char buff[100];
	char *p;
	int n,cnt = 0;
	LONG npos = num < 0 ? -num : num;
	BOOL sign = num < 0 ? 1 : 0;

	if (num)
	{
		num  = npos;
		while (num)
		{
			cnt++;
			num/=10;
		}
	}
	else cnt=1;

	if (sign) buff[0]='-';

	n = cnt;
	p = buff+cnt-1+sign;

	num = npos;

	while (n--)
	{
		*p-- = (num%10) + '0';
		num/=10;
	}

	buff[sign+cnt]=0;

	SafeLog( what );
	SafeLog( buff ); 
}

void Hex2HostPutStr( const char *what, ULONG num )
{
	char buff[100];
	char H[]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	char *p;
	int n,cnt = 8;
	ULONG tmp = num;

	buff[0]='$';
	n = cnt;
	p = buff+cnt;
	tmp = num;

	while (n--)
	{
		*p-- = H[ (tmp%16) ];
		tmp/=16;
	}

	buff[cnt+1]=0;

	SafeLog( what );
	SafeLog( buff ); 
}

void Bin2HostPutStr( const char *what, ULONG num )
{
	char *p;
	char buff[100];
	int n,cnt = 31;

	for (n=0;n<32;n++) buff[n+1]='*';

	buff[0]='#';

	p = buff+1;
	do
	{
		*p++ = (1L << cnt) & num ? '1' : '0' ;
	} while (cnt--);


	/* 1 + 32 + 1 */
	buff[33] = 0;

	SafeLog( what );
	SafeLog( buff ); 
}


ULONG uaeahi_AllocAudio(
		REG(a1, struct TagItem *tagList),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl),
		REG(a6, struct UAEAHIDriverBase *driver) )
{
	ULONG n,flags = 0;
	struct TagItem *tag;
	struct DriverData *DriverData;

	SafeLog("uaeahi_AllocAudio\n"); 

	SafeLog("  TagList:\n"); 

	for (tag = tagList; tag -> ti_Tag != TAG_END; tag++)
	{
		Hex2HostPutStr ("    TAG: ", (ULONG) tag -> ti_Tag );
		Hex2HostPutStr (", DATA: ", (ULONG) tag -> ti_Data );
		SafeLog("\n"); 
	}

	flags = AHISF_KNOWSTEREO | AHISF_KNOWHIFI;

	UNum2HostPutStr(" sizeof(struct DriverData): ", sizeof(struct DriverData) );
		SafeLog("\n");

	DriverData = (struct DriverData *) AllocVec( sizeof(struct DriverData), MEMF_FAST | MEMF_CLEAR );

	if ( DriverData )
	{
		DriverData -> driver = driver;

		DriverData -> mainTask = FindTask(NULL);
		DriverData -> mainSignal = AllocSignal(-1);
		DriverData -> playSignal = -1;
		DriverData -> mixSignal = -1;

		AudioCtrl -> ahiac_DriverData = (APTR) DriverData ;
		
		for (n=0;n<MAX_CHANNELS;n++)
		{
			DriverData -> channels[n].active = FALSE;
			DriverData -> channels[n].offset = 0;
		}
	}

	AudioCtrl -> ahiac_AudioCtrl.ahiac_UserData = AllocVec( AudioCtrl -> ahiac_BuffSize, AudioCtrl -> ahiac_BuffType);

	return flags;
}

void uaeahi_FreeAudio(
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl) )
{
	struct DriverData *DriverData;

	SafeLog("uaeahi_FreeAudio\n"); 

	DriverData = AudioCtrl -> ahiac_AudioCtrl.ahiac_UserData;
	if (DriverData)
	{
		DriverData -> mainTask = NULL;
		if (DriverData -> mainSignal != -1) FreeSignal(DriverData -> mainSignal);

		FreeVec(AudioCtrl -> ahiac_AudioCtrl.ahiac_UserData);
		AudioCtrl -> ahiac_AudioCtrl.ahiac_UserData = NULL;
	}
}

void uaeahi_Disable(
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	SafeLog("uaeahi_Disable\n"); 

	Forbid();
}

void uaeahi_Enable( 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	SafeLog("uaeahi_Enable\n"); 

	Permit();
}

bool running = true;
struct Process *proc_mixer = NULL;

/* my_CallHookPkt(hook,obj,msg) */
LONG __CallHookPkt(__reg("a6") struct Library *,
              __reg("a0") APTR,
              __reg("a2") APTR,
              __reg("a1") APTR) = "\tjsr\t-102(a6)";
#define my_CallHookPkt(hook,obj,msg) __CallHookPkt(UtilityBase,hook,obj,msg)

ULONG my_CallHookPkt2( struct Hook *hook, APTR obj, APTR msg )
{
	ULONG res;

	__reg("a0") struct Hook *a0  = hook;
	__reg("a1") APTR a1  = msg;
	__reg("a2") APTR a2  = obj;
	 __reg("a3") ULONG (*entry) ( __reg("a0") struct Hook *hook,__reg("a2") APTR  msg, __reg("a1") APTR obj )  = (void *) hook->h_Entry;

	res = entry(hook,obj,msg);

	return res;
}

ULONG  h_Entry_test(__reg("a0") struct Hook *_hook , __reg("a2") APTR _object , __reg("a1") APTR _message)
{
	SafeLog("h_Entry_test\n");
		Hex2HostPutStr(" hook: ", (ULONG) _hook );
		Hex2HostPutStr(" object: ", (ULONG) _object );
		Hex2HostPutStr(" message: ", (ULONG) _message );
		SafeLog("\n");

	Hex2HostPutStr(" hook -> h_Data: ", (ULONG)  _hook -> h_Data );
		SafeLog("\n");

	return 0;
}


void free_resources( struct AHIAudioCtrlDrv *AudioCtrl )
{
	int n;

	if ( dd->playSignal == -1 ) FreeSignal(dd->playSignal);
	if ( dd->mixSignal == -1 ) FreeSignal(dd->mixSignal);

	for (n=0;n<2;n++)
	{
		if ( dd->SampBuffer[n] ) FreeVec(dd->SampBuffer[n]);
		dd->SampBuffer[n] = NULL;
	}

	dd->playSignal = -1;
	dd->mixSignal = -1;
}

static void u32zero( char *addr, ULONG size )
{
	ULONG *ptr = (ULONG *) addr;

	size >>=2;	/* DIV 4 */
	while (size--) *ptr++= 0;
}

void fn_AudioTask(void)
{
	struct Process *me;
	struct AHIAudioCtrlDrv *AudioCtrl = NULL;
	PreTimer_proto *pretimer;
	PostTimer_proto *posttimer;
	ULONG signalset;
	BOOL pretimer_rc;
	ULONG sigmask;

	ULONG bytesToCopy;
	struct UAEAudioChannel *chan;
	struct DriverData *DriverData;

	APTR currentBuffer;
	int cb = 0;

	me = (struct Process *) FindTask(NULL);

	if ( me -> pr_Task.tc_UserData == NULL )
	{
		SafeLog("Process AudioTask failed...\n");
		Signal( dd -> mainTask,  1L << dd -> mainSignal );
		return;
	}

	AudioCtrl = (struct AHIAudioCtrlDrv *)  me -> pr_Task.tc_UserData;

	dd -> playSignal = AllocSignal(-1);
	dd -> mixSignal = AllocSignal(-1);
	dd -> SampBuffer[0] = AllocVec( PLAYBUFFERSIZE, MEMF_FAST );
	dd -> SampBuffer[1] = AllocVec( PLAYBUFFERSIZE, MEMF_FAST );

	if ( (dd -> playSignal==-1) || (dd -> mixSignal==-1) || (dd -> SampBuffer[0]==NULL) || (dd -> SampBuffer[1]==NULL) )
	{
		free_resources( AudioCtrl );
		Signal( dd -> mainTask,  1L << dd -> mainSignal );
		return;
	}

	Signal( dd -> mainTask,  1L << dd -> mainSignal );

	Delay(500);	/* just try not write into the same console at the same time... */

	sigmask = SIGBREAKF_CTRL_C | (1L << dd -> playSignal) | (1L << dd -> mixSignal);

	SafeLog("Process audioTask: started, waiting on signals...\n");
	Hex2HostPutStr(" wating on sigmask: ", (ULONG) sigmask );
	SafeLog("\n");

	for (;;)
	{
		signalset = Wait( sigmask  );

		SafeLog("Process audioTask loop...\n");

		/* Call the hooks AHI gave us */

		if(signalset & SIGBREAKF_CTRL_C)
		{
			SafeLog("Process audioTask: Break singal recived...\n");
			/* Quit */
			break;
		}

		if (signalset & (1L<<dd -> playSignal))
		{
			SafeLog("Process audioTask: PlaySingal bit set...");
				UNum2HostPutStr("buffer idx: ", cb );
				SafeLog("\n");

			DriverData = (struct DriverData *) AudioCtrl->ahiac_DriverData;
			if (DriverData) 
			{
				bytesToCopy = ((chan -> length) > PLAYBUFFERSIZE) ? PLAYBUFFERSIZE : (chan -> length);
				currentBuffer = dd -> SampBuffer[cb];

				if ( CopyChannelToPlayback( &DriverData->channels[0], currentBuffer , bytesToCopy ))
				{
					if (bytesToCopy<PLAYBUFFERSIZE)
					{
						u32zero( (char *) currentBuffer + bytesToCopy, PLAYBUFFERSIZE - bytesToCopy );
					}
				}
				else
				{
					u32zero( currentBuffer, PLAYBUFFERSIZE );
				}

				StartPlaying( AudioCtrl,  (struct Process *) me, currentBuffer );
			}

			cb ^=1;	/* toggel bit 0 */
		}

		if (signalset & (1L<<dd -> mixSignal))
		{
			pretimer = (PreTimer_proto *) AudioCtrl->ahiac_PreTimer;
			posttimer = (PostTimer_proto *) AudioCtrl->ahiac_PostTimer;
			
			if (pretimer) 
			{
				pretimer_rc = pretimer(AudioCtrl);
			}
			else
			{
				pretimer_rc = TRUE;
			}

			if (pretimer_rc)
			{
				if (AudioCtrl->ahiac_PlayerFunc)
				{
					SafeLog("my_CallHookPkt( AudioCtrl->ahiac_PlayerFunc, AudioCtrl, NULL );\n");
					my_CallHookPkt( AudioCtrl->ahiac_PlayerFunc, AudioCtrl, currentBuffer );
				}

				if (AudioCtrl->ahiac_MixerFunc)
				{
					SafeLog("my_CallHookPkt( AudioCtrl->ahiac_MixerFunc, AudioCtrl, NULL );\n");
					my_CallHookPkt( AudioCtrl->ahiac_MixerFunc, AudioCtrl, currentBuffer );  
				}
	
				if (posttimer) posttimer(AudioCtrl);
			}
		}
	}

	SafeLog("Process audioTask stoped.\n");

	/* Task exits here */
	dd -> audioTask = NULL;

	free_resources( AudioCtrl );

	Signal( (struct Task *) dd->audioTask,1L << dd->mainSignal);
}

ULONG uaeahi_Start(
		REG(d0, ULONG Flags),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	SafeLog("uaeahi_Start( ");
		Bin2HostPutStr(" Flags in D0: ",Flags );
		SafeLog(", AudioCtrl in A2)\n"); 

	uaeahi_Stop(Flags, AudioCtrl);

	if(Flags & AHISF_PLAY)
	{
		struct TagItem procTags[] = {
		 	{ NP_Entry,     (ULONG) fn_AudioTask },
			{ NP_Name,      (ULONG) "uae.audio.process" },
			{ NP_StackSize, 4096 },
			{ NP_Priority,  5 },
			{ TAG_DONE,     0 }};

		if (Flags & AHISF_PLAY)
		{
			Forbid();
			dd->audioTask = CreateNewProcTagList(procTags);
			if (dd->audioTask)
			{
				dd->audioTask -> pr_Task.tc_UserData = (APTR) AudioCtrl;
			}
			Permit();

			if (dd->audioTask)
			{
				Wait( 1L << dd -> mainSignal );

				Signal( (struct Task *) dd->audioTask, 1L<<dd->playSignal);
			}
		}
	}

	if(Flags & AHISF_RECORD)
	{
	}

	return AHIE_OK;
}

void uaeahi_Stop( 
		REG(d0, ULONG Flags), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	SafeLog("_Stop_( Flags: "); 
		Bin2HostPutStr(" Flags: ",Flags );
	SafeLog(", AudioCtrl )\n"); 

	if ( Flags & AHISF_PLAY )
	{
		if(dd->playSignal != -1)
		{
			Signal((struct Task *)dd->audioTask, SIGBREAKF_CTRL_C );
			Wait(1L<<dd->mainSignal);
		}
	}
}

void uaeahi_Update(
		REG(d0, ULONG Flags),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	SafeLog("_Update_( Flags: "); 
		Bin2HostPutStr(" Flags: ",Flags );
	SafeLog(", AudioCtrl )\n"); 
}

ULONG uaeahi_SetVol( 
		REG(d0, UWORD Channel), 
		REG(d1, UWORD Volume),
		REG(d2, UWORD Pan), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl), 
		REG(d3, ULONG Flags))
{
	SafeLog("_SetVol_("); 
		UNum2HostPutStr(" Channel: ", Channel );
		UNum2HostPutStr(", Volume: ",Volume );
		SNum2HostPutStr(", Pan: ", Pan );
		Bin2HostPutStr(", Flags: ", Flags );
	SafeLog(" )\n");

	return 0;
}

#define next(n) (n^1)

#ifdef PushAudioSample
#error 1
#endif


ULONG uaeahi_SetFreq(
		REG(d0, UWORD Channel), 
		REG(d1, ULONG Freq), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl), 
		REG(d2, ULONG Flags))
{
	struct DriverData *DriverData ;

	SafeLog("_SetFreq_("); 
		UNum2HostPutStr(" Channel: ", Channel );
		UNum2HostPutStr(", Freq: ", Freq );
		Bin2HostPutStr(", Flags: ", Flags );
	SafeLog(" )\n");

	DriverData = (struct DriverData *) AudioCtrl->ahiac_DriverData;
	if (DriverData == NULL)	return AHIE_UNKNOWN;

	return 0;
}

ULONG uaeahi_SetSound(
		REG(d0, UWORD Channel), 
		REG(d1, UWORD Sound), 
		REG(d2, UWORD Offset), 
		REG(d3, WORD Length), 
		REG(a2, struct AHIAudioCtrlDrv * AudioCtrl), 
		REG(d4, ULONG Flags))
{
	struct DriverData *DriverData ;
	struct SoundEntry tmp;
	struct UAEAudioChannel *chan;
	struct SoundEntry *snd;
	struct SoundEntry *current;

	SafeLog("_SetSound_("); 
		UNum2HostPutStr(" Channel: ",Channel );
		UNum2HostPutStr(", Sound: ",Sound );
		UNum2HostPutStr(", Offset: ",Offset );
		UNum2HostPutStr(", Length: ",Length );
		Bin2HostPutStr(", Flags: ",Flags );
	SafeLog(" )\n");

	DriverData = (struct DriverData *)AudioCtrl->ahiac_DriverData;

	if (DriverData == NULL) return AHIE_UNKNOWN;

	if (Channel >= MAX_CHANNELS) return AHIE_UNKNOWN;

	chan = &DriverData->channels[Channel];

	if (Sound == AHI_NOSOUND)
	{
		Forbid();
		chan->offset = 0;
		chan->sound.type = AHIST_NOTYPE;
		chan->sound.length = 0;
		Permit();

		 SafeLog(" Sound == AHI_NOSOUND (mutes a channel)\n ");
	}
	else
	{
		if (Sound >= MAX_SOUNDS) return AHIE_BADSOUNDTYPE;

		snd = &DriverData->sounds[Sound];

		if (!snd->address || snd->length == 0)  return AHIE_BADSOUNDTYPE;

		if (Length == 0 ||  Length > snd->length)
		{
			 SafeLog(" Length == 0, using sound length insted\n");
			Length = snd->length ;
		}

		tmp.type = snd -> type;
		tmp.address = snd->address ;

		/* limit Length so safe ranges. */
		if (Length > snd->length) Length = snd->length;

		Forbid();
		chan->reverse = ((LONG) Length < 0) ? TRUE : FALSE;
		chan->offset = Offset;
		chan->length = Length;
		chan->sound = tmp;
		Permit();
	}

	if (Flags & (1L<<AHISB_IMM))
	{
		struct AHISoundMessage msg;

		/* Immediate play (simulate Enable) */
		SafeLog(" Immediate play (simulate Enable)\n ");

		chan->active = TRUE;

		Forbid();
		chan -> sound.type = snd->type;
		chan -> offset = Offset;
		Permit();

		msg.ahism_Channel = Channel;

		/* Send channel to playback routine for scheduling */
		my_CallHookPkt( AudioCtrl->ahiac_SoundFunc, AudioCtrl, &msg);
	}

	return AHIE_OK;
}

ULONG uaeahi_SetEffect(
		REG(a0, APTR Effect),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	SafeLog("_SetEffect_\n"); 
	return 0;
}

ULONG uaeahi_LoadSound(
		REG(d0, UWORD Sound), 
		REG(d1, ULONG Type), 
		REG(a0, struct AHISampleInfo *Info), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	struct DriverData *DriverData = (struct DriverData *)AudioCtrl->ahiac_DriverData;

	SafeLog("_LoadSound_("); 
		UNum2HostPutStr(" Sound: ", Sound );
		UNum2HostPutStr(", Type: ", Type );
	SafeLog(", info, AudioCtrl )\n");

	/* Accept only SAMPLE or DYNAMICSAMPLE */

	if (Type != AHIST_SAMPLE && Type != AHIST_DYNAMICSAMPLE)
		return AHIE_BADSAMPLETYPE;

	/* Only accept specific sample formats */

	switch (Info->ahisi_Type)
	{
		case AHIST_M8S:
		case AHIST_M16S:
		case AHIST_S8S:
		case AHIST_S16S:
			break;

		default:
			return AHIE_BADSOUNDTYPE;
	}

	if (Sound >= MAX_SOUNDS)
		return AHIE_ABORTED;

	/* Save sample data to internal table */

	DriverData->sounds[Sound].type = Info->ahisi_Type;
	DriverData->sounds[Sound].address = Info->ahisi_Address;
	DriverData->sounds[Sound].length = Info->ahisi_Length;

	return AHIE_OK;
}

ULONG uaeahi_UnloadSound(
		REG(d0, UWORD Sound),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	struct DriverData *data;

	SafeLog("_UnloadSound_("); 
		UNum2HostPutStr(" Sound: ", Sound );
	SafeLog(", AudioCtrl )\n");

	data = (struct DriverData *) AudioCtrl->ahiac_DriverData;
	if (data == NULL)	return AHIE_ABORTED;

	if (Sound<MAX_SOUNDS)
	{
		/* Save sample data to internal table */

		data->sounds[Sound].type = 0;
   	 	data->sounds[Sound].address = NULL;
   	 	data->sounds[Sound].length = 0;

		return AHIE_OK;
	}

	return AHIE_ABORTED;
}

ULONG Frequencies[]={44100};

LONG uaeahi_GetAttr(
		REG(d0, ULONG Attribute), 
		REG(d1, LONG Argument), 
		REG(d2, LONG DefValue), 
		REG(a1, struct TagItem *tagList), 
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	ULONG value = DefValue;	

	SafeLog("_GetAttr_("); 
		Hex2HostPutStr(" Attribute: ", Attribute );
		UNum2HostPutStr(" Argument: ", Argument );
		UNum2HostPutStr(" DefValue: ", DefValue );
		Hex2HostPutStr(" tagList: ", (ULONG) tagList );
		Hex2HostPutStr(" AudioCtrl: ", (ULONG) AudioCtrl );
	SafeLog(" )\n");

	switch(Attribute)
	 {
		case AHIDB_Name:
				value = (ULONG) "UAE Audio Output";
				break;

		case AHIDB_Author:
				value = (ULONG) "Your Name";
				break;

		case AHIDB_Copyright:
				value = (ULONG) "(c) 2025 Your Name";
				break;

		 case AHIDB_Version:
				value = (ULONG) "4.0";
				break;

		 case AHIDB_MaxChannels:
				value = 2;  /* stereo */
				break;

		case AHIDB_Outputs:
				value = 1;
				break;

		case AHIDB_Output:
				value = (ULONG) "host audio output";
				break;

		case AHIDB_Frequencies:
				value = 1;
				break;

		case AHIDB_Frequency:
				value = (LONG) 44100;
				break;

		case AHIDB_MinMixFreq:
		case AHIDB_MaxMixFreq:
				value = 0;
				break;

		  default:
				return value;  /* unknown attribute */
	}

	return value;
}

ULONG uaeahi_HardwareControl(
		REG(d0, ULONG Attribute), 
		REG(d1, LONG Argument),
		REG(a2, struct AHIAudioCtrlDrv *AudioCtrl))
{
	SafeLog("_HardwareControl_\n"); 
	return 0;
}

#define set_tag( tagv, value ) *tag++=tagv; *tag++= (ULONG) value; 

BOOL StartPlaying(struct AHIAudioCtrlDrv *AudioCtrl, struct Process *me, APTR buffer)
{
	BOOL playing;
	ULONG *tag;
	ULONG set_part_tags[20];
	ULONG raw_playback_tags[20];

	tag = set_part_tags;

	set_tag( PAT_LoopbackVolume, -64 );
	set_tag( TAG_DONE, 0 );

  	/* Disable Loopback */

	SetPartTagList( set_part_tags );

	tag = raw_playback_tags;

	set_tag( TT_PlayTask,   me );
	set_tag( TT_PlaySignal,   dd->playSignal );
	set_tag( TT_Mode, dd->mode );
	set_tag( TT_Frequency, AudioCtrl->ahiac_MixFreq );
	set_tag( TT_RawBuffer, buffer );
	set_tag( TT_BufferSize, PLAYBUFFERSIZE );
	set_tag( TAG_DONE, 0 );

	playing = RawPlaybackTagList( raw_playback_tags );
  
	if(playing)
	{
		dd->flags |= TF_ISPLAYING;
	}

	return playing;
}

static BOOL CopyChannelToPlayback(struct UAEAudioChannel *chan, UBYTE *playBuf, ULONG bytesToCopy)
{
	if (!chan->active || chan->sound.address == NULL || chan->length == 0)
		return FALSE;

	if (chan -> reverse)
	{
		short *src, *dst, *dstEnd;

		src = ((short *) chan -> sound.address) + (chan -> sound.length - chan -> offset) / sizeof(short); 

		/* assume 16bit, make sure we don't swap the bytes */

		if (dst<dstEnd)
		{
			*dst++ = *(--src);
		}

		chan -> offset += bytesToCopy;
	}
	else
	{
		const UBYTE *src = ((UBYTE *)chan->sound.address) + chan->offset;
		CopyMemQuick( src, playBuf, bytesToCopy);
		chan->offset += bytesToCopy;
	}

	return TRUE;
}
