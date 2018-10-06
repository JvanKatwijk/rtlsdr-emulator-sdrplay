#
/*
 *    Copyright (C) 2018
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is the sdrplayBridge. 
 *    It allows the operation of rtlsdr based software with
 *    an SDRplay device emulating the rtlsdr device
 *
 *    rtlsdrBridge is available under GPL-V2 (not V3)
 */

#include	<pthread.h>
#include	<semaphore.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<stdint.h>
#include	<stdbool.h>
#ifdef	__MINGW32__
#include	<windows.h>
#include	<time.h>
#else
#include	<unistd.h>
#endif
#include	<string.h>
#include	<rtl-sdr.h>
#include	"mirsdrapi-rsp.h"

//	uncomment __DEBUG__ for lots of output
#define	__DEBUG__	0
//	uncomment __SHORT__ for the simplest conversion N -> 8 bits
#define	__SHORT__	0

//	the "signals" for handling commands asynchronously.
#define	CANCEL_ASYNC	0100
#define	SET_FREQUENCY	0101
#define	SET_BW		0102
#define	SET_RATE	0103

#define	KHz(x)	(1000 * x)
#define	MHz(x)	(1000 * KHz (x))

typedef	struct __theSignal {
	int	signal;
	void	*device;
	int	value;
	struct __theSignal	*next;
} theSignal;

static	pthread_mutex_t locker;
static	sem_t	signalElements;

static	theSignal	*signalQueue	= NULL;

static
void	signalInit	(void) {
	pthread_mutex_init (&locker, NULL);
	sem_init (&signalElements, 0, 0);
	signalQueue	= NULL;
}

static
void	signalReset (void) {
	pthread_mutex_trylock (&locker);
	pthread_mutex_unlock (&locker);
	while (signalQueue != NULL) {
	   theSignal *x = signalQueue -> next;
	   free (signalQueue);
	   signalQueue = x;
	}
}

static
void	putSignalonQueue (int signal, void *device, int value) {
theSignal	*n;
	pthread_mutex_lock (&locker);
	n	= malloc (sizeof (theSignal));
	n	-> signal	= signal;
	n	-> device	= device;
	n	-> value	= value;
	n	-> next		= signalQueue;
	signalQueue	= n;
	sem_post (&signalElements);
	pthread_mutex_unlock (&locker);
}

static
void	getSignalfromQueue (int *signal, void **device, int *value) {
theSignal *tmp;
	sem_wait (&signalElements);
	pthread_mutex_lock (&locker);
	tmp	= signalQueue;
	*signal	= signalQueue	-> signal;
	*device	= signalQueue	-> device;
	*value	= signalQueue	-> value;
	signalQueue	= signalQueue	-> next;
	free (tmp);
	pthread_mutex_unlock (&locker);
}
//
/************************ end of part I **********************************/
//
/************************* Part II ***************************************/
//	defined later on in this file
static
char    *sdrplay_errorCodes (mir_sdr_ErrT err);

//
//      lna gain reduction tables, per band.
//      The first number in each row tells the number of valid elements
static
int     RSP1_Table [3] [5] = {{4, 0, 24, 19, 43},
                              {4, 0,  7, 19, 26},
                              {4, 0,  5, 19, 24}
};

static
int     RSP2_Table [3] [10] = {
        {9, 0, 10, 15, 21, 24, 34, 39, 45, 64},
        {6, 0,  7, 10, 17, 22, 41, -1, -1, -1},
        {6, 0,  5, 21, 15, 15, 32, -1, -1, -1}
};

static
int     RSP1A_Table [4] [11] = {
        {7,  0, 6, 12, 18, 37, 42, 61, -1, -1, -1},
        {10, 0, 6, 12, 18, 20, 26, 32, 38, 57, 62},
        {10, 0, 7, 13, 19, 20, 27, 33, 39, 45, 64},
        { 9, 0, 6, 12, 20, 26, 32, 38, 43, 62, -1}
};

//
//	A little tricky, the rtlsdr device shows the gains available,
//	here we depend on both the hwdevice and the frequency 
//	for the list, so we have lots of functions to determine
//	what the "gain" actually means.
//	We dispatch, based on the hwVersion.
struct {
	int	Gain;
} gainTable [] = {
0, 10,	20, 25,	30, 35,	40, 45, 50, 55, 60, 65, 70, 75, 80, -1};

//
//	Note that the RSPX_tables have as first element in the row
//	the number of valid elements
static
void	handleGain_rsp1 (int frequency, int gain, int *lnaState, int *GRdB) {
int	gainreduction	= 102 - gain;
int	i;
	*lnaState	= 3;
	*GRdB		= 30;
	if (frequency < MHz (420)) {
	   for (i = 1; i <= RSP1_Table [0][0]; i ++) {
	      if (RSP1_Table [0][i] >= gainreduction / 2) {
	         *lnaState	= i - 1;
	         *GRdB		= gainreduction - RSP1_Table [0] [i];
	         return;
	      }
	   }
	}
	else
	if (frequency < MHz (1000)) {
	   for (i = 1; i <= RSP1_Table [1][0]; i ++) {
	      if (RSP1_Table [1][i] >= gainreduction / 2) {
	         *lnaState	= i - 1;
	         *GRdB		= gainreduction - RSP1_Table [1] [i];
	         return;
	      }
	   }
	}
	else {
	   for (i = 0; i < RSP1_Table [2][0]; i ++) {
	      if (RSP1_Table [2][i] >= gainreduction / 2) {
	         *lnaState	= i - 1;
	         *GRdB		= gainreduction - RSP1_Table [2] [i];
	         return;
	      }
	   }
	}
//	we should not be here
}

void	handleGain_rsp2 (int frequency, int gain, int *lnaState, int *GRdB) {
int	gainreduction	= 102 - gain / 10;
int	i;
	*lnaState	= 3;
	*GRdB		= 30;
	if (frequency < MHz (420)) {
	   for (i = 1; i <= RSP2_Table [0][0]; i ++) {
	      if (RSP2_Table [0][i] >= gainreduction / 2) {
	         fprintf (stderr,
	                  "lna (%d) reduction found %d (gainred %d)\n",
	                   i - 1, RSP2_Table [0][i], gainreduction);
	         *lnaState	= i - 1;
	         *GRdB		= gainreduction - RSP2_Table [0] [i];
	         return;
	      }
	   }
	}
	else
	if (frequency < MHz (1000)) {
	   for (i = 1; i <= RSP2_Table [1][0]; i ++) {
	      if (RSP2_Table [1][i] >= gainreduction / 2) {
	         *lnaState	= i - 1;
	         *GRdB		= gainreduction - RSP2_Table [1] [i];
	         return;
	      }
	   }
	}
	else {
	   for (i = 0; i <= RSP2_Table [2][0]; i ++) {
	      if (RSP1_Table [2][i] >= gainreduction / 2) {
	         *lnaState	= i - 1;
	         *GRdB		= gainreduction - RSP1_Table [2] [i];
	         return;
	      }
	   }
	}
//	we should not be here
}

void	handleGain_rsp1a (int frequency, int gain, int *lnaState, int *GRdB) {
int	gainreduction	= 102 - gain / 10;
int	i;
	*lnaState	= 3;
	*GRdB		= 30;
	if (frequency < MHz (60)) {
	   for (i = 1; i <= RSP1A_Table [0][0]; i ++) {
	      if (RSP1A_Table [0][i] >= gainreduction / 2) {
	         *lnaState	= i;
	         *GRdB		= gainreduction - RSP1A_Table [0] [i];
	         return;
	      }
	   }
	}
	else
	if (frequency < MHz (420)) {
	   for (i = 1; i < RSP2_Table [0][0]; i ++) {
	      if (RSP1A_Table [1][i] >= gainreduction / 2) {
	         *lnaState	= i;
	         *GRdB		= gainreduction - RSP1A_Table [1] [i];
	         return;
	      }
	   }
	}
	else
	if (frequency < MHz (1000)) {
	   for (i = 1; i <= RSP1A_Table [2][0]; i ++) {
	      if (RSP1A_Table [2][i] >= gainreduction / 2) {
	         *lnaState	= i;
	         *GRdB		= gainreduction - RSP1A_Table [2] [i];
	         return;
	      }
	   }
	}
	else {
	   for (i = 0; i <= RSP2_Table [3][0]; i ++) {
	      if (RSP1_Table [3][i] >= gainreduction / 2) {
	         *lnaState	= i;
	         *GRdB		= gainreduction - RSP1_Table [3] [i];
	         return;
	      }
	   }
	}
//	we should not be here
}

static
void	gainHandler (int hwVersion, int frequency, int gain,
	                            int* lnaState, int* GRdB) {
	switch (hwVersion) {
	   case 1:		// RSP1
	      handleGain_rsp1 (frequency, gain , lnaState, GRdB);
	      return;

	   case 2:
	      handleGain_rsp2 (frequency, gain , lnaState, GRdB);
	      return;

	   default:
	      handleGain_rsp1a (frequency, gain , lnaState, GRdB);
	      return;
	}
}
//
//	The user's callback type:
typedef void (*rtlsdr_read_async_cb_t)(uint8_t *buf, uint32_t len, void *ctx);

//	our version of the device descriptor
struct rtlsdr_dev {
	int	deviceIndex;
	int	hwVersion;
	uint8_t	ppm;
#ifdef	__SHORT__
	int	shiftFactor;
#else
	float	downScale;
#endif
	int	GRdB;
	int	tunerGain;
	int	lnaState;
	uint8_t	gainMode;
	bool	agcOn;
//
//	In this implementation inputRate and outputRate may differ
//	if the "client" asks for a frequency lower than 2M, we
//	read in samples at a double speed and convert
	int	inputRate;
	int	outputRate;
	int	frequency;
	int	bandWidth;
	rtlsdr_read_async_cb_t callback;
	void	*ctx;
	int	buf_num;
	int	buf_len;
	int	fbP;
} devDescriptor;

static	mir_sdr_DeviceT devDesc [4];
static	int	numofDevs	= -1;
static	bool	running		= 0;

static
int	getBandwidth (int input) {
	if (input <= KHz (200))
	   return mir_sdr_BW_0_200;
	if (input <= KHz (300))
	   return mir_sdr_BW_0_300;
	if (input <= KHz (600))
	   return mir_sdr_BW_0_600;
	if (input <= KHz (1536))
	   return mir_sdr_BW_1_536;
	return mir_sdr_BW_5_000;
}
//
//	Here we should think on what to do with rates < 1Mhz,
//	after all a dabstick will work fine on 960K
static
int	getRate	(int rate) {
	return rate >= MHz (2) ? rate : 
	       rate >= MHz (1) ? 2 * rate : MHz (2);
}

static
int16_t bankFor_sdr (int32_t freq) {
        if (freq < 12 * MHz (1))
           return mir_sdr_BAND_AM_LO;
        if (freq < 30 * MHz (1))
           return mir_sdr_BAND_AM_MID;
        if (freq < 60 * MHz (1))
           return mir_sdr_BAND_AM_HI;
        if (freq < 120 * MHz (1))
           return mir_sdr_BAND_VHF;
        if (freq < 250 * MHz (1))
           return mir_sdr_BAND_3;
        if (freq < 420 * MHz (1))
           return mir_sdr_BAND_X;
        if (freq < 1000 * MHz (1))
           return mir_sdr_BAND_4_5;
        if (freq < 2000 * MHz (1))
           return mir_sdr_BAND_L;
        return -1;
}

static int enquireDevs (void) {
float	ver;
mir_sdr_ErrT err	= mir_sdr_ApiVersion (&ver);

	if (ver < 2.13) {
	   fprintf (stderr, "please upgrade to sdrplay library to >= 2.13\n");
	   return -1;
	}

	mir_sdr_GetDevices (devDesc, &numofDevs, (uint32_t)4);
	if (numofDevs == 0) {
	   fprintf (stderr, "Sorry, no device found\n");
	   return -1;
	}
	fprintf (stderr, "we found %d devices\n", numofDevs);
	return 0;
}

RTLSDR_API uint32_t rtlsdr_get_device_count (void) {
	if (numofDevs < 0) {
	   if (enquireDevs () < 0)
	      return -1;
	}
	return numofDevs;
}

RTLSDR_API const char* rtlsdr_get_device_name (uint32_t devIndex) {
	if (numofDevs < 0) {
	   if (enquireDevs () < 0)
	      return " ";
	}

	if (devIndex < numofDevs)
	   return devDesc [devIndex]. DevNm;
	return " ";
}

RTLSDR_API int rtlsdr_get_device_usb_strings (uint32_t devIndex,
					      char *manufacturer,
					      char *product,
					      char *serial) {
	if (numofDevs < 0) {
	   if (enquireDevs () < 0)
	      return -1;
	}

	if (devIndex >= numofDevs)
	   return -1;

	(void)strcpy (manufacturer, "sdrplay.com");
	(void)strcpy (product,       devDesc [devIndex]. DevNm);
	(void)strcpy (serial,        devDesc [devIndex]. SerNo);
}

RTLSDR_API int rtlsdr_get_index_by_serial (const char *serial) {
int	i;

	if (numofDevs < 0) {
	   if (enquireDevs () < 0)
	      return -1;
	}

	for (i = 0; i < numofDevs; i ++)
	   if (strcmp (serial, devDesc [i]. SerNo) == 0)
	      return i;
	return -1;
}

RTLSDR_API int rtlsdr_open (rtlsdr_dev_t **dev,
	                    uint32_t deviceIndex) {
mir_sdr_ErrT err;

	if (numofDevs < 0) {
	   if (enquireDevs () < 0)
	      return -1;
	}
	
	err = mir_sdr_SetDeviceIdx (deviceIndex);
        if (err != mir_sdr_Success) {
           fprintf (stderr, "error at SetDeviceIdx %s \n",
                                         sdrplay_errorCodes (err));
	   return -1;
	}

	devDescriptor. hwVersion = devDesc [deviceIndex]. hwVer == 1 ? 1 :
	                           devDesc [deviceIndex]. hwVer == 2 ? 2 : 3;
	switch (devDescriptor. hwVersion) {
	   case 1:
#ifdef	__SHORT__
	      devDescriptor. shiftFactor	= 4;
#else
	      devDescriptor. downScale		= 2048.0;
#endif
	      break;
	   case 2:
#ifdef	__SHORT__
	      devDescriptor. shiftFactor	= 4;
#else
	      devDescriptor. downScale		= 2048.0;
#endif
	      break;
	   default:		// RSP1A and RSP_DUO
#ifdef	__SHORT__
	      devDescriptor. shiftFactor	= 6;
#else
	      devDescriptor. downScale		= 8192.0;
#endif
	      break;
	}
	*dev	= &devDescriptor;
	(*dev) -> deviceIndex	= deviceIndex;

//	default values
	running				= false;
	devDescriptor. GRdB		= 45;
        devDescriptor. lnaState		= 3;
	devDescriptor. tunerGain	= 40;
	devDescriptor. gainMode		= 0;
	devDescriptor. agcOn		= false;
        devDescriptor. inputRate	= 2048000;
        devDescriptor. outputRate	= 2048000;
        devDescriptor. frequency	= MHz (220);
        devDescriptor. ppm		= 0;
        devDescriptor. deviceIndex	= 0;
	devDescriptor. bandWidth	= mir_sdr_BW_1_536;
	signalInit	();	// create the queue
}

RTLSDR_API int rtlsdr_close (rtlsdr_dev_t *dev) {
	if (dev == NULL)
	   return -1;

	if (running)
	   rtlsdr_cancel_async (dev);

#ifdef __DEBUG__
	fprintf (stderr, "going to release the device\n");
#endif
	mir_sdr_ReleaseDeviceIdx ();
#ifdef	__DEBUG
	fprintf (stderr, "close completed\n");
#endif
}

RTLSDR_API int rtlsdr_set_center_freq (rtlsdr_dev_t *dev,
	                               uint32_t freq) {
	if (dev == NULL)
	   return -1;

        if (!running) 	// record for later use
           dev -> frequency = freq;
	else
	   putSignalonQueue (SET_FREQUENCY, dev, freq);
	return 0;
}

RTLSDR_API int rtlsdr_set_freq_correction (rtlsdr_dev_t *dev,
	                                   int		ppm) {
	if (dev == NULL)
	   return -1;

	dev -> ppm	= ppm;

	if (!running)		// will be handled later on
	   return 0;
	mir_sdr_SetPpm    ((float)ppm);
	return 0;
}

RTLSDR_API enum rtlsdr_tuner rtlsdr_get_tuner_type (rtlsdr_dev_t *dev) {
	return RTLSDR_TUNER_UNKNOWN;
}
//

RTLSDR_API int rtlsdr_get_tuner_gains (rtlsdr_dev_t *dev, int *gains) {
int	i;
int	m	= 0;
	if (dev == NULL)
	   return -1;

	for (i = 0; gainTable [i]. Gain != -1; i ++) m++;
	if (gains == NULL) 
	   return m;

	for (i = 0; gainTable [i]. Gain != -1; i ++) {
	   gains [i] = 10 * gainTable [i]. Gain;
	   fprintf (stderr, " %d", gains [i]);
	}

	return m;
}

RTLSDR_API int rtlsdr_set_tuner_gain (rtlsdr_dev_t *dev, int gain) {
mir_sdr_ErrT    err;
int	i;

#ifdef	__DEBUG__
	fprintf (stderr, "request for gain setting to %d\n", gain);
#endif

	for (i = 0; gainTable [i]. Gain != -1; i ++) {
	   if ((gain >= 10 * gainTable [i]. Gain) &&
	       ((gainTable [i + 1]. Gain ==  -1) ||
	                (gain < 10 * gainTable [i + 1]. Gain))) {
	      gainHandler (dev -> hwVersion,
	                   dev -> frequency,
	                   gain,
	                   &dev -> lnaState,
		           &dev -> GRdB);

	      if (dev -> GRdB < 20)
	         dev -> GRdB = 20;
#ifdef	__DEBUG__
	      fprintf (stderr, "for gain %d, we set state %d GRdB %d\n",
	                                 gain, dev -> lnaState, dev -> GRdB);
#endif
	      break;
	   }
	}
	dev	-> tunerGain	= gain;
	if ((dev -> agcOn) || !running)
	   return 0;

	err     =  mir_sdr_RSP_SetGr (dev -> GRdB,  dev -> lnaState, 1, 0);
	if (err != mir_sdr_Success)
	   fprintf (stderr, "Error at set_ifgain %s (%d %d)\n",
                                        sdrplay_errorCodes (err),
	                                gain, dev -> lnaState);
	return 0;
}

RTLSDR_API int rtlsdr_set_tuner_bandwidth (rtlsdr_dev_t *dev, uint32_t bw) {
	if (dev == NULL)
	   return -1;

	if (dev -> outputRate > bw)
	   bw = dev -> outputRate;

	if (!running)
	   dev -> bandWidth = bw;	// handling will be later on
	else
	   putSignalonQueue (SET_BW, dev, bw);
	return 0;
}

RTLSDR_API int rtlsdr_set_tuner_if_gain (rtlsdr_dev_t *dev,
	                                 int stage,
	                                 int gain) {
	return -1;
}


RTLSDR_API int rtlsdr_set_tuner_gain_mode (rtlsdr_dev_t *dev, int manual) {
	if (dev == NULL)
	   return -1;
	dev	-> gainMode	= manual != 0;
	return 0;
}


RTLSDR_API int rtlsdr_set_sample_rate (rtlsdr_dev_t *dev,
	                               uint32_t rate) {
	if (dev == NULL)
	   return -1;
	if (!running) {
	   dev	-> inputRate	= getRate (rate);
	   dev	-> outputRate	= rate;
	}
	else
	   putSignalonQueue (SET_RATE, dev, rate);
	return 0;
}


RTLSDR_API int rtlsdr_set_agc_mode(rtlsdr_dev_t *dev, int on) {
mir_sdr_ErrT    err;

	if (dev == NULL)
	   return -1;

	if (!running) {		// save for later
	   dev -> agcOn = on != 0;
	   return 0;
	}

	err = mir_sdr_AgcControl (on != 0 ?
	                    mir_sdr_AGC_100HZ :
                            mir_sdr_AGC_DISABLE,
                            -dev -> GRdB,
                            0, 0, 0, 0, dev -> lnaState);
        if (err != mir_sdr_Success) {
           fprintf (stderr,
	            "Error %s on mir_sdr_AgcControl\n", sdrplay_errorCodes (err));
	   return -1;
	}
	if (on == 0) {
	   err	=  mir_sdr_RSP_SetGr (dev -> GRdB,  dev -> lnaState, 1, 0);
           if (err != mir_sdr_Success) {
              fprintf (stderr,
	            "Error %s on mir_sdr_RSP_SetGr\n",
	                                sdrplay_errorCodes (err));
	      return -1;
	   }
	}
	dev -> agcOn	= on != 0;
	return 0;
}

/* streaming functions */

RTLSDR_API int rtlsdr_reset_buffer (rtlsdr_dev_t *dev) {
	return 0;
}

RTLSDR_API int rtlsdr_read_sync (rtlsdr_dev_t *dev,
	                         void *buf,
	                         int len, int *n_read) {
	return -1;
}


RTLSDR_API int rtlsdr_wait_async (rtlsdr_dev_t *dev,
	                          rtlsdr_read_async_cb_t cb,
	                          void *ctx) {
	return -1;
}
//
//	the size of the request for data is done in the async function,
//	so that that is the place to allocate. The SDRplay
//	callback will fill the buffer and -once full- will call the
//	callback for the rtlsdr user.
//
//	Note that (rtlsdr-)samplerates between 1Mhz and 2MHz are handled
//	by reading in samples at a rate twice as high and decimating
//	by averaging subsequent samples.

static	int8_t	*finalBuffer	= NULL;
static
void myStreamCallback (int16_t		*xi,
	               int16_t		*xq,
	               uint32_t		firstSampleNum, 
	               int32_t		grChanged,
	               int32_t		rfChanged,
	               int32_t		fsChanged,
	               uint32_t		numSamples,
	               uint32_t		reset,
	               uint32_t		hwRemoved,
	               void		*cbContext) {
int	i;
struct rtlsdr_dev *ctx = (struct rtlsdr_dev *)cbContext;
static	int16_t	old_xi	= 0;
static	int16_t	old_xq	= 0;
#ifdef	__SHORT__
int	shiftFactor	= ctx -> shiftFactor;
#else
int	downScale	= ctx	-> downScale;
#endif
static	int	decimator	= 0;

	for (i = 0; i < numSamples; i ++) {
	   if (ctx -> inputRate > ctx -> outputRate) {
	      if (decimator != 0) {
	         decimator = 0;
	         old_xi	= xi [i];
	         old_xq	= xq [i];
	         continue;
	      }
	      decimator ++;
	      xi [i] = (xi [i] + old_xi) / 2;
	      xq [i] = (xq [i] + old_xq) / 2;
	   }
#ifdef	__SHORT__
	   finalBuffer [ctx -> fbP++] = (int8_t)(xi [i] >> shiftFactor) + 128;
	   finalBuffer [ctx -> fbP++] = (int8_t)(xq [i] >> shiftFactor) + 128;
#else
	   finalBuffer [ctx -> fbP++] =
	                      (int8_t)((float)(xi [i]) * 128.0 / downScale) + 128;
	   finalBuffer [ctx -> fbP++] =
	                      (int8_t)((float)(xq [i]) * 128.0 / downScale) + 128;
#endif
	   if (ctx -> fbP >= ctx -> buf_len) {
	      ctx -> callback (finalBuffer,
	                       ctx -> buf_len,
	                       ctx -> ctx);
	      ctx -> fbP = 0;
	   }
	}
}

static
void	myGainChangeCallback (uint32_t	GRdB,
	                      uint32_t	lnaGRdB,
	                      void	*cbContext) {
	(void)GRdB;
	(void)lnaGRdB;
	(void)cbContext;
}

//
//	rtlsdr_read_async is executed in a thread, created by our "client",
//	Communication to change the status is by simple signaling
//
RTLSDR_API int rtlsdr_read_async(rtlsdr_dev_t *dev,
				 rtlsdr_read_async_cb_t cb,
				 void *ctx,
				 uint32_t buf_num,
				 uint32_t buf_len) {
int     gRdBSystem;
int     samplesPerPacket;
mir_sdr_ErrT    err;
int     localGRed;

	if (dev == NULL)
	   return -1;

        if (running)
           return -1;

	dev	-> callback	= cb;
	dev	-> ctx		= ctx;
	dev	-> buf_num	= buf_num;
	dev	-> buf_len	= buf_len;
	finalBuffer		= malloc (buf_len * sizeof (int8_t));
	localGRed		= dev -> GRdB;
#ifdef	__DEBUG__
	fprintf (stderr, "StreamInit %d %f %f %d %d %d\n",
	                  dev -> GRdB,
	                  (double)(dev -> inputRate) / 1000000.0,
	                  (double)(dev -> frequency) / 1000000.0,
	                  dev -> bandWidth,
	                  dev -> lnaState
	        );
#endif
        err	= mir_sdr_StreamInit (&localGRed,
                                      ((double)(dev -> inputRate)) / 1000000.0,
                                      ((double)(dev -> frequency)) / 1000000.0,
	                              dev -> bandWidth,
                                      mir_sdr_IF_Zero,
                                      dev -> lnaState,
                                      &gRdBSystem,
                                      mir_sdr_USE_RSP_SET_GR,
                                      &samplesPerPacket,
                                      (mir_sdr_StreamCallback_t)myStreamCallback,
	                              (mir_sdr_GainChangeCallback_t)myGainChangeCallback,
                                      dev);
        if (err != mir_sdr_Success) {
           fprintf (stderr,
	            "Error %s on streamInit\n", sdrplay_errorCodes (err));
           return -1;
        }
#ifdef	__DEBUG__
	fprintf (stderr, "rtlsdr_read_async is started\n");
#endif
        err             = mir_sdr_SetDcMode (4, 1);
        err             = mir_sdr_SetDcTrackTime (63);
	err		= mir_sdr_SetPpm    ((float)dev -> ppm);
        running	= true;
//
//	we make this into a simple event loop with semaphores
	while (running) {
	   int	signal;
	   struct rtlsdr_dev *device;
	   int	value;
	   mir_sdr_ErrT err;

	   getSignalfromQueue (&signal, (void **)(&device) , &value);

	   switch (signal) {
	      case CANCEL_ASYNC:
#ifdef	__DEBUG__
	         fprintf (stderr, "cancel request\n");
#endif
	         running = false;
	         err = mir_sdr_StreamUninit ();
	         signalReset ();
	         if (err != mir_sdr_Success)
                    fprintf (stderr, "Error at StreamUnInit %s\n",
                                        sdrplay_errorCodes (err));
#ifdef	__DEBUG
	         fprintf (stderr, "StreamUnInit has called\n");
#endif
	         free (finalBuffer);
	         goto L_end;

	      case SET_FREQUENCY:
#ifdef	__DEBUG__
	         fprintf (stderr, "frequency request for %d\n", value);
#endif
	         if (bankFor_sdr (value) == bankFor_sdr (device -> frequency))
	            err = mir_sdr_SetRf ((double)value, 1, 0);
	         else {
	            int localGred	= device -> GRdB;
	            int gRdBSystem;
	            int	samplesPerPacket;

	            err = mir_sdr_Reinit (&localGred,
                                   ((double) (dev -> inputRate)) / MHz (1),
                                   ((double) value) / MHz (1),
                                   device -> bandWidth,
                                   mir_sdr_IF_Zero,
                                   mir_sdr_LO_Undefined,      // LOMode
                                   dev-> lnaState, 
                                   &gRdBSystem,
	                           mir_sdr_USE_RSP_SET_GR,
                                   &samplesPerPacket,
                                   mir_sdr_CHANGE_RF_FREQ);
	         }
	         if (err != mir_sdr_Success)
	            fprintf (stderr, "Error at frequency setting %s\n",
	                                sdrplay_errorCodes (err));

	         err = mir_sdr_AgcControl (dev -> agcOn ?
	                                   mir_sdr_AGC_100HZ :
	                                   mir_sdr_AGC_DISABLE,
	                                   -dev -> GRdB,
                                            0, 0, 0, 0, dev -> lnaState);
	         if (err != mir_sdr_Success) 
	            fprintf (stderr,
	                     "Error %s on mir_sdr_AgcControl\n",
	                                         sdrplay_errorCodes (err));
	           
	         if (!dev -> agcOn) {
	            err  =  mir_sdr_RSP_SetGr (dev -> GRdB,
	                                       dev -> lnaState, 1, 0);
	            if (err != mir_sdr_Success)
	               fprintf (stderr, "Error at set_ifgain %s (%d %d)\n",
                                         sdrplay_errorCodes (err),
	                                 dev -> GRdB, dev -> lnaState);
	         }
	         dev -> frequency = value;
	         break;

	      case SET_BW: 
	         { int new		= getBandwidth (value);
	           int localGRed	= dev -> GRdB;
	           int gRdBSystem;
	           int	samplesPerPacket;
#ifdef	__DEBUG__
	           fprintf (stderr, "Bandwith request for %d\n", new);
#endif
	           if (device -> bandWidth == new)
	              break;
	           device -> bandWidth = new;
	           err = mir_sdr_Reinit (&localGRed,
                                   ((double) (dev -> inputRate)) / MHz (1),
                                   ((double) (dev -> frequency)) / MHz (1),
                                   device -> bandWidth,
                                   mir_sdr_IF_Zero,
                                   mir_sdr_LO_Undefined,      // LOMode
                                   device -> lnaState, 
                                   &gRdBSystem,
	                           mir_sdr_USE_RSP_SET_GR,
                                   &samplesPerPacket,
                                   mir_sdr_CHANGE_BW_TYPE);
	            if (err != mir_sdr_Success) {
	               fprintf (stderr, "ReInit failed %s\n",
	                                      sdrplay_errorCodes (err));
	            }
	            err = mir_sdr_AgcControl (dev -> agcOn ?
	                                      mir_sdr_AGC_100HZ :
                                              mir_sdr_AGC_DISABLE,
                                              -dev -> GRdB,
                                              0, 0, 0, 0, dev -> lnaState);
                    if (err != mir_sdr_Success) 
                       fprintf (stderr,
	                       "Error %s on mir_sdr_AgcControl\n",
	                                         sdrplay_errorCodes (err));
	            if (!dev -> agcOn) {
	               err  =  mir_sdr_RSP_SetGr (dev -> GRdB,
	                                          dev -> lnaState, 1, 0);
	              if (err != mir_sdr_Success)
	              fprintf (stderr, "Error at set_ifgain %s (%d %d)\n",
                                        sdrplay_errorCodes (err),
	                                dev -> GRdB, dev -> lnaState);
	            }
	            break;
	         }

	      case SET_RATE:
	         { int localGRed	= dev -> GRdB;
	           int	gRdBSystem;
	           int	samplesPerPacket;
	           device	-> inputRate	= getRate (value);
	           device	-> outputRate	= value;
#ifdef	__DEBUG__
	           fprintf (stderr, "request for rate change %d\n",
	                                           device -> inputRate);
#endif
	           err = mir_sdr_Reinit (&localGRed,
                                   ((double) (dev -> inputRate)) / MHz (1),
                                   ((double) (dev -> frequency)) / MHz (1),
                                   device -> bandWidth,
                                   mir_sdr_IF_Zero,
                                   mir_sdr_LO_Undefined,      // LOMode
                                   device -> lnaState, 
                                   &gRdBSystem,
	                           mir_sdr_USE_RSP_SET_GR,
                                   &samplesPerPacket,
                                   mir_sdr_CHANGE_FS_FREQ);
	           if (err != mir_sdr_Success) {
	              fprintf (stderr, "ReInit failed %s\n",
	                                      sdrplay_errorCodes (err));
	           }
	           err = mir_sdr_AgcControl (dev -> agcOn ?
	                                     mir_sdr_AGC_100HZ :
                                             mir_sdr_AGC_DISABLE,
                                             -dev -> GRdB,
                                             0, 0, 0, 0, dev -> lnaState);
                   if (err != mir_sdr_Success) 
                      fprintf (stderr,
	                       "Error %s on mir_sdr_AgcControl\n",
	                                         sdrplay_errorCodes (err));
	           if (!dev -> agcOn) {
	              err  =  mir_sdr_RSP_SetGr (dev -> GRdB,
	                                          dev -> lnaState, 1, 0);
	              if (err != mir_sdr_Success)
	              fprintf (stderr, "Error at set_ifgain %s (%d %d)\n",
                                        sdrplay_errorCodes (err),
	                                dev -> GRdB, dev -> lnaState);
	           }
	           break;
	         }

	      default:
	         break;
	   }
	}
L_end:
#ifdef	__DEBUG__
	fprintf (stderr, "async is stopped\n");
#else
	;
#endif
	return 0;
}

//
RTLSDR_API int rtlsdr_cancel_async (rtlsdr_dev_t *dev) {
	if (dev == NULL)
	   return -1;
	if (!running)
	   return 0;
	putSignalonQueue (CANCEL_ASYNC, dev, 0);
	while (running)
#ifdef	__MINGW32__
	   Sleep (1);
#else
	   usleep (1000);
#endif
	return 0;
}


RTLSDR_API int rtlsdr_set_bias_tee (rtlsdr_dev_t *dev, int on) {
	return -1;
}

/* configuration functions */

RTLSDR_API int rtlsdr_set_xtal_freq (rtlsdr_dev_t *dev,
	                             uint32_t rtl_freq,
				     uint32_t tuner_freq) {
	return -1;
}


RTLSDR_API int rtlsdr_get_xtal_freq (rtlsdr_dev_t *dev,
	                             uint32_t *rtl_freq,
	                             uint32_t *tuner_freq) {
	return -1;
}

//
//	we do not know how to handle the eeprom
RTLSDR_API int rtlsdr_write_eeprom (rtlsdr_dev_t *dev,
	                            uint8_t *data,
	                            uint8_t offset,
	                            uint16_t len) {
	return -3;
}

RTLSDR_API int rtlsdr_read_eeprom (rtlsdr_dev_t *dev,
	                           uint8_t *data,
	                           uint8_t offset,
	                           uint16_t len) {
	return -3;
}

RTLSDR_API uint32_t rtlsdr_get_center_freq (rtlsdr_dev_t *dev) {
	if (dev	== NULL)
	   return -1;
	return dev -> frequency;
}

RTLSDR_API int rtlsdr_get_freq_correction (rtlsdr_dev_t *dev) {
	if (dev == NULL)
	   return -1;

	return dev -> ppm;
}

RTLSDR_API int rtlsdr_get_tuner_gain (rtlsdr_dev_t *dev) {
	return dev -> tunerGain;
}

RTLSDR_API uint32_t rtlsdr_get_sample_rate (rtlsdr_dev_t *dev) {
	return dev	-> outputRate;
}

RTLSDR_API int rtlsdr_set_testmode (rtlsdr_dev_t *dev, int on) {
	return -1;
}


RTLSDR_API int rtlsdr_set_direct_sampling (rtlsdr_dev_t *dev, int on) {
	return 0;
}


RTLSDR_API int rtlsdr_get_direct_sampling (rtlsdr_dev_t *dev) {
	return 0;
}


RTLSDR_API int rtlsdr_set_offset_tuning (rtlsdr_dev_t *dev, int on) {
	return -1;
}


RTLSDR_API int rtlsdr_get_offset_tuning (rtlsdr_dev_t *dev) {
	return -1;
}

static
char	*sdrplay_errorCodes (mir_sdr_ErrT err) {
	switch (err) {
	   case mir_sdr_Success:
	      return "success";
	   case mir_sdr_Fail:
	      return "Fail";
	   case mir_sdr_InvalidParam:
	      return "invalidParam";
	   case mir_sdr_OutOfRange:
	      return "OutOfRange";
	   case mir_sdr_GainUpdateError:
	      return "GainUpdateError";
	   case mir_sdr_RfUpdateError:
	      return "RfUpdateError";
	   case mir_sdr_FsUpdateError:
	      return "FsUpdateError";
	   case mir_sdr_HwError:
	      return "HwError";
	   case mir_sdr_AliasingError:
	      return "AliasingError";
	   case mir_sdr_AlreadyInitialised:
	      return "AlreadyInitialised";
	   case mir_sdr_NotInitialised:
	      return "NotInitialised";
	   case mir_sdr_NotEnabled:
	      return "NotEnabled";
	   case mir_sdr_HwVerError:
	      return "HwVerError";
	   case mir_sdr_OutOfMemError:
	      return "OutOfMemError";
	   case mir_sdr_HwRemoved:
	      return "HwRemoved";
	   default:
	      return "???";
	}
}

