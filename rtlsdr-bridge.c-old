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
 *    rtlsdrBridge is available under GPL-V2 
 */
#include	<stdio.h>
#include	<semaphore.h>
#include	<stdlib.h>
#include	<stdint.h>
#include	<stdbool.h>
#ifdef	__MINGW32__
#include	<windows.h>
#include	<time.h>
#include	<pthread.h>
#include	<commctrl.h>
#include	"resource.h"
#else
#include	<unistd.h>
#endif
#include	<string.h>
#include	<rtl-sdr.h>
#include	"mirsdrapi-rsp.h"
#include	"signal-queue.h"
#include	"gains.h"

//	uncomment __DEBUG__ for lots of output
//#define	__DEBUG__	1
//	uncomment __SHORT__ for the simplest conversion N -> 8 bits
//#define	__SHORT__	0

#define	KHz(x)	(1000 * x)
#define	MHz(x)	(1000 * KHz (x))

#define	MAX_GRdB	59
#define	MIN_GRdB	20

//	defined later on in this file
static
char    *sdrplay_errorCodes (mir_sdr_ErrT err);
bool	installDevice ();
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
	int	lna_startState;
	int	old_lnaState;
	int	old_GRdB;
	bool	gainMode;
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
	bool	running;
	bool	testMode;
#ifdef	__MINGW32__
	bool	open;
	HWND	widgetHandle;
#endif
} devDescriptor;

static
mir_sdr_DeviceT devDesc [4];
static
int	numofDevs	= -1;
static
theSignal	*signalQueue	= NULL;

#ifdef	__MINGW32__
static
pthread_t	thread_id;
static
int started	= 0;
//
//	These functions will be called in the W32
//	version, directly from handling the widget
//
bool	set_lnaState (int state) {
int *lnaTable;

	switch (devDescriptor. hwVersion) {
	   case 1:	// "old" RSP1
	      lnaTable = getTable_RSP1 (devDescriptor. frequency);
	      break;

	   case 2:	// RSP2
	      lnaTable = getTable_RSP2 (devDescriptor. frequency);
	      break;

	   case 3:	// RSP1A end duo
	      lnaTable = getTable_RSP1a (devDescriptor. frequency);
	      break;

	}

	if ((0 < state) && (state <= lnaTable [0])) {
	   devDescriptor. lnaState = state;
	   mir_sdr_RSP_SetGr (devDescriptor. GRdB,
	                      devDescriptor. lnaState - 1, 1, 0);
	   return true;
	}
	return false;
}
//
//	The caller will check the boundaries
void	set_GRdB (int GRdB) {
	devDescriptor. GRdB = GRdB;
	mir_sdr_RSP_SetGr (devDescriptor. GRdB,
	                   devDescriptor. lnaState - 1, 1, 0);
}

void	set_agc	(bool onOff) {
mir_sdr_ErrT err;

	devDescriptor. agcOn	= onOff;
	err = mir_sdr_AgcControl (onOff ?
	                          mir_sdr_AGC_100HZ :
	                          mir_sdr_AGC_DISABLE,
	                          -devDescriptor. GRdB,
	                          0, 0, 0, 0, devDescriptor. lnaState - 1);
	if (err != mir_sdr_Success) {
	   fprintf (stderr,
	            "Error %s on mir_sdr_AgcControl\n", sdrplay_errorCodes (err));
	   return;
	}

	if (!onOff) {
	   err	=  mir_sdr_RSP_SetGr (devDescriptor. GRdB,
	                              devDescriptor. lnaState - 1, 1, 0);
	   if (err != mir_sdr_Success) {
	      fprintf (stderr,
	            "Error %s on mir_sdr_RSP_SetGr\n",
	                                sdrplay_errorCodes (err));
	      return;
	   }
	}
}

static
HMODULE hInstance 	= 0;

INT_PTR CALLBACK Dialog1Proc (HWND hwndDlg, UINT uMsg,
	                          WPARAM wParam, LPARAM lParam) {
int		nCode;
int		iPosIndicated;
LPNMUPDOWN	lpnmud;
BOOL		success;
int		newVal;

	switch (uMsg) {
	   case WM_INITDIALOG:
	      SetDlgItemInt (hwndDlg, IDC_LNA_STATE, 
	                          devDescriptor. lna_startState, FALSE);
	      SetDlgItemInt (hwndDlg, IDC_GRdB, MIN_GRdB, FALSE);
	      return true;

	   case WM_COMMAND:
	      switch (LOWORD (wParam)) {
	         case IDOK:
	         case IDCANCEL:
	            fprintf (stderr, "about to delete through id/cancel\n");
	            if (devDescriptor. widgetHandle != NULL)
	               DestroyWindow (devDescriptor. widgetHandle);
	            devDescriptor. widgetHandle = NULL;
	            return true;;

	         case IDC_LNA_STATE:
	            newVal = GetDlgItemInt (hwndDlg, IDC_LNA_STATE,
	                                         &success, FALSE);
	            if (set_lnaState (newVal)) 
	               devDescriptor. old_lnaState = newVal;
	            else
	               SetDlgItemInt (hwndDlg,
	                              IDC_LNA_STATE,
	                              devDescriptor. old_lnaState, FALSE);
	             break;

	         case IDC_GRdB:
	            newVal = GetDlgItemInt (hwndDlg, IDC_GRdB,
	                                         &success, FALSE);
	            if ((newVal < MIN_GRdB) || (newVal > MAX_GRdB))
	               SetDlgItemInt (hwndDlg, IDC_GRdB,
	                              devDescriptor. old_GRdB, FALSE);
	            else {
	               set_GRdB (newVal);
	               devDescriptor. old_GRdB = newVal;
	            }
	            break;

	         case IDC_AGC:
	            if (!SendDlgItemMessage (hwndDlg,
	                                    IDC_AGC, BM_GETCHECK, 0, 0)) {
	               SendDlgItemMessage (hwndDlg, IDC_AGC,
	                                   BM_SETCHECK, BST_CHECKED, 0);
	                set_agc (true);
	            }
	            else {
	               SendDlgItemMessage (hwndDlg, IDC_AGC,
	                                   BM_SETCHECK, BST_UNCHECKED, 0);
	               set_agc (false);
	            }
	            break;

	         case IDC_RESET:
	            devDescriptor. lnaState	= devDescriptor.
	                                                   lna_startState;
	            devDescriptor. GRdB		= MIN_GRdB;
	            devDescriptor. old_GRdB	= MIN_GRdB;
	            SetDlgItemInt (hwndDlg, IDC_LNA_STATE,
	                           devDescriptor. lna_startState, FALSE);
	            SetDlgItemInt (hwndDlg, IDC_GRdB, MIN_GRdB, FALSE);
	            set_GRdB (MIN_GRdB);
	            SendDlgItemMessage (hwndDlg, IDC_AGC,
	                                   BM_SETCHECK, BST_UNCHECKED, 0);
	            set_agc (false);
	            break;

	         default:
//	            fprintf (stderr, "unknown %d (%d)\n",
//	                               LOWORD (wParam), HIWORD (wParam));
	            break;
	      }
	      return true;

	   default:
	      return false;
	}
	return true;
}

void	*StartDialog (void * varg) {
int err;
	devDescriptor. old_lnaState	= devDescriptor. lna_startState;
	devDescriptor. old_GRdB		= MIN_GRdB;
	devDescriptor. lnaState		= devDescriptor. lna_startState;
	devDescriptor. GRdB		= MIN_GRdB;

	devDescriptor. widgetHandle =
	                  CreateDialog (hInstance,
                                        MAKEINTRESOURCE (IDD_DIALOG1),
	                                NULL, Dialog1Proc);
	err	= GetLastError ();
	fprintf (stderr, "Last Error = %d\n", err);
	if (err == 0) {
	   ShowWindow (devDescriptor. widgetHandle, SW_SHOW);
	   MSG msg;
	   devDescriptor. open	= true;
	   while (devDescriptor. open && GetMessage (&msg, NULL, 0, 0)) {
	      if (!IsWindow (devDescriptor. widgetHandle))
	         break;
	      if (IsDialogMessage (devDescriptor. widgetHandle, &msg)) 
	         continue;
	      TranslateMessage (&msg);
	      DispatchMessage  (&msg);
	   }
	}
}

BOOL WINAPI DllMain (HMODULE hModule, DWORD dwReason, LPVOID lpvReserver) {
INT_PTR ptr;
	switch (dwReason) {
	   case DLL_PROCESS_ATTACH:
	      if (started != 0)
	         return TRUE;
	      if (!installDevice ())
	         return FALSE;
	      started	= 1;
	      devDescriptor. widgetHandle	= NULL;
	      hInstance	= hModule;
	      numofDevs	= -1;
	      return TRUE;

	   case DLL_PROCESS_DETACH:
	      if (devDescriptor. widgetHandle != NULL)
	         DestroyWindow (devDescriptor. widgetHandle);
	       devDescriptor. widgetHandle = NULL;
	      break;

	   case DLL_THREAD_ATTACH:
//	      fprintf (stderr, "thread attach called with %d\n", dwReason);
	      break;

	   case DLL_THREAD_DETACH:
//	      fprintf (stderr, "thread detach called with %d\n", dwReason);
	      break;
	}
	return TRUE;
}

#endif

bool	installDevice () {
float	ver;
mir_sdr_ErrT err;

	err	= mir_sdr_ApiVersion (&ver);
	if (ver < 2.13) {
	   fprintf (stderr, "please upgrade to sdrplay library to >= 2.13\n");
	   return false;
	}
	err = mir_sdr_GetDevices (devDesc, &numofDevs, (uint32_t)4);
	if ((err != mir_sdr_Success) || (numofDevs == 0)) {
	   fprintf (stderr, "Sorry, no device found\n");
	   return false;
	}
	return true;
}

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
//	Here we should think on what to do with rates < 2Mhz,
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

RTLSDR_API uint32_t rtlsdr_get_device_count (void) {
	if ((numofDevs < 0) && !installDevice ()) {
	   return -1;
	}
	return numofDevs;
}

RTLSDR_API const char* rtlsdr_get_device_name (uint32_t devIndex) {
	if ((numofDevs < 0) && !installDevice ()) {
	   return " ";
	}

	if (devIndex < numofDevs) {
	   fprintf (stderr, "name for %d-th device = %s\n",
	                       devIndex, devDesc [devIndex]. DevNm);
	   return devDesc [devIndex]. DevNm;
	}
	return " ";
}
//
//	Not sure how to handle this, need some thinking
RTLSDR_API int rtlsdr_get_usb_strings (rtlsdr_dev_t *dev,
	                               char *manufacturer,
	                               char *product,
	                               char *serial) {
	rtlsdr_get_device_usb_strings (0, manufacturer, product, serial);
}
	                    
RTLSDR_API int rtlsdr_get_device_usb_strings (uint32_t devIndex,
					      char *manufacturer,
					      char *product,
					      char *serial) {
	if ((numofDevs < 0) && !installDevice ()) {
	   return -1;
	}

	if (devIndex >= numofDevs)
	   return -1;

	(void)strcpy (manufacturer, "sdrplay.com");
	(void)strcpy (product,       devDesc [devIndex]. DevNm);
	(void)strcpy (serial,        devDesc [devIndex]. SerNo);
}

RTLSDR_API int rtlsdr_get_index_by_serial (const char *serial) {
int	i	= 0;

	if ((numofDevs < 0) && !installDevice ()){
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

#ifdef	MINGW32__
	devDescriptor. open = false;
#endif
	if ((numofDevs < 0) && !installDevice ()) {
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
	      devDescriptor. lna_startState	= 3;
	      break;

	   case 2:
#ifdef	__SHORT__
	      devDescriptor. shiftFactor	= 4;
#else
	      devDescriptor. downScale		= 2048.0;
#endif
	      devDescriptor. lna_startState	= 4;
	      break;

	   default:		// RSP1A and RSP_DUO
#ifdef	__SHORT__
	      devDescriptor. shiftFactor	= 6;
#else
	      devDescriptor. downScale		= 8192.0;
#endif
	      devDescriptor. lna_startState	= 4;
	      break;
	}
	*dev	= &devDescriptor;
	(*dev) -> deviceIndex	= deviceIndex;

//	default values
	devDescriptor. running		= false;
	devDescriptor. GRdB		= 45;
	devDescriptor. lnaState		= 3;
	devDescriptor. tunerGain	= 40;
	devDescriptor. gainMode		= false;
	devDescriptor. agcOn		= false;
	devDescriptor. testMode		= false;
	devDescriptor. inputRate	= 2048000;
	devDescriptor. outputRate	= 2048000;
	devDescriptor. frequency	= MHz (220);
	devDescriptor. ppm		= 0;
	devDescriptor. deviceIndex	= 0;
	devDescriptor. bandWidth	= mir_sdr_BW_1_536;
	signalInit	(&signalQueue);	// create the queue
#ifdef	__MINGW32__
	pthread_create (&thread_id, NULL, StartDialog, NULL);
#endif
}

RTLSDR_API int rtlsdr_close (rtlsdr_dev_t *dev) {
#ifdef	__MINGW32__
int	nResult	= -1;
#endif
	if (dev == NULL)
	   return -1;
	
	if (dev -> running)
	   rtlsdr_cancel_async (dev);
	dev -> running	= false;
#ifdef __DEBUG__
	fprintf (stderr, "going to release the device\n");
#endif
	mir_sdr_ReleaseDeviceIdx ();
#ifdef	__MINGW32__
	dev	-> open = false;
#ifdef	__DEBUG__
	fprintf (stderr, "close completed\n");
#endif
	fprintf (stderr, "about to delete through close\n");
	if (devDescriptor. widgetHandle != NULL)
	   DestroyWindow (devDescriptor. widgetHandle);
	devDescriptor. widgetHandle = NULL;
#endif
}

RTLSDR_API int rtlsdr_set_center_freq (rtlsdr_dev_t *dev,
	                               uint32_t freq) {
mir_sdr_ErrT    err;
	if (dev == NULL)
	   return -1;

	if (!dev -> running) 	// record for later use
	   dev -> frequency = freq;
	else
	if (bankFor_sdr (dev -> frequency) == bankFor_sdr (freq)) {
	   err = mir_sdr_SetRf (freq, 1, 0);
	   if (err == mir_sdr_Success)
	      dev -> frequency = freq;
	   return err == mir_sdr_Success ? 0 : -1;
	}
	putSignalonQueue (&signalQueue, SET_FREQUENCY, freq);
	return 0;
}

RTLSDR_API int rtlsdr_set_freq_correction (rtlsdr_dev_t *dev,
	                                   int		ppm) {
	if (dev == NULL)
	   return -1;

	dev -> ppm	= ppm;

	if (!dev -> running)	// will be handled later on
	   return 0;
	mir_sdr_SetPpm    ((float)ppm);
	return 0;
}

RTLSDR_API enum rtlsdr_tuner rtlsdr_get_tuner_type (rtlsdr_dev_t *dev) {
	return RTLSDR_TUNER_UNKNOWN;
}
//

struct {
	int     Gain;
} gainTable [] = {
0, 10,  20, 25, 30, 35, 40, 45, 50, 55, 60, -1};

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
#ifdef	__MINGW32__
	return 0;
#else
#ifdef	__DEBUG__
	fprintf (stderr, "request for gain setting to %d\n", gain);
#endif

	for (i = 0; gainTable [i]. Gain != -1; i ++) {
	   if ((gain >= 10 * gainTable [i]. Gain) &&
	       ((gainTable [i + 1]. Gain ==  -1) ||
	                (gain < 10 * gainTable [i + 1]. Gain))) {
	      gainMapper (dev -> hwVersion,
	                  dev -> frequency,
	                  gain,
	                  &dev -> lnaState,
		          &dev -> GRdB);

	      if (dev -> GRdB < 20)
	         dev -> GRdB = 20;
	      if (dev -> GRdB > 59)
	         dev -> GRdB = 59;
#ifdef	__DEBUG__
	      fprintf (stderr, "for gain %d, we set state %d GRdB %d\n",
	                                 gain, dev -> lnaState, dev -> GRdB);
#endif
	      break;
	   }
	}
	dev	-> tunerGain	= gain;
	if ((dev -> agcOn) || !dev -> running)
	   return 0;

	err     =  mir_sdr_RSP_SetGr (dev -> GRdB,  dev -> lnaState, 1, 0);
	if (err != mir_sdr_Success)
	   fprintf (stderr, "Error at set_ifgain %s (%d %d)\n",
	                                sdrplay_errorCodes (err),
	                                dev -> GRdB, dev -> lnaState);
	return 0;
#endif
}

RTLSDR_API int rtlsdr_set_tuner_bandwidth (rtlsdr_dev_t *dev, uint32_t bw) {
	if (dev == NULL)
	   return -1;

	if (dev -> outputRate > bw)
	   bw = dev -> outputRate;

	bw	=  getBandwidth (bw);
	if (bw == dev -> bandWidth)
	   return 0;
	dev -> bandWidth = bw;	// handling will be later on
	if (dev -> running)
	   putSignalonQueue (&signalQueue, SET_BW, bw);
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
	dev	-> inputRate	= getRate (rate);
	dev	-> outputRate	= rate;
	if (dev -> inputRate < MHz (2)) {
	   fprintf (stderr, "oops, %d too low to get support\n", 
	                                                 dev -> inputRate);
	   return -1;
	}
	if (dev -> running) 
	   putSignalonQueue (&signalQueue, SET_RATE, rate);
	return 0;
}

RTLSDR_API int rtlsdr_set_agc_mode (rtlsdr_dev_t *dev, int on) {
mir_sdr_ErrT    err;

#ifdef	__MINGW32__
	return 0;
#else
	fprintf (stderr, "switching agc mode to %s\n",
	                               on == 1 ? "on" : "off");
	if (dev == NULL)
	   return -1;

	if (dev -> agcOn == on != 0)
	   return 0;

	if (!dev -> running) {		// save for later
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
	   fprintf (stderr, "gain setting to %d %d\n", dev -> lnaState, 
	                                               dev -> GRdB);
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
#endif
}

/* streaming functions */

RTLSDR_API int rtlsdr_reset_buffer (rtlsdr_dev_t *dev) {
	if (dev != NULL)
	   dev -> fbP	= 0;
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

static  int8_t  *finalBuffer    = NULL;

static	int16_t testmode_Counter	= 0;
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
	   if (ctx -> testMode) {
	      finalBuffer [ctx -> fbP ++] = testmode_Counter;
	      testmode_Counter = (testmode_Counter + 1) & 0xFF;
	      finalBuffer [ctx -> fbP ++] = testmode_Counter;
	      testmode_Counter = (testmode_Counter + 1) & 0xFF;
	   }
	   else {	// the normal case
	
#ifdef  __SHORT__
	      finalBuffer [ctx -> fbP++] = (int8_t)((xi [i] >> shiftFactor) & 0xFF) + 128;
	      finalBuffer [ctx -> fbP++] = (int8_t)((xq [i] >> shiftFactor) & 0xFF) + 128;
#else
	      finalBuffer [ctx -> fbP++] =
	                      (int8_t)((float)(xi [i]) * 128.0 / downScale) + 128;
	      finalBuffer [ctx -> fbP++] =
	                      (int8_t)((float)(xq [i]) * 128.0 / downScale) + 128;
#endif
	   }
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

static
mir_sdr_ErrT	re_initialize (rtlsdr_dev_t *dev, int reason) {
mir_sdr_ErrT    err;
int	localGred	= dev -> GRdB;
int	gRdBSystem;
int	samplesPerPacket;

	err = mir_sdr_Reinit (&localGred,
	                      ((double) (dev -> inputRate)) / MHz (1),
	                      ((double) (dev -> frequency)) / MHz (1),
	                      dev	-> bandWidth,
	                      mir_sdr_IF_Zero,
	                      mir_sdr_LO_Undefined,      // LOMode
	                      dev -> lnaState, 
	                      &gRdBSystem,
	                      mir_sdr_USE_RSP_SET_GR,
	                      &samplesPerPacket,
	                      reason);
	return err;
}

static
mir_sdr_ErrT	handle_gainSetting (rtlsdr_dev_t *dev) {
mir_sdr_ErrT err = mir_sdr_AgcControl (dev -> agcOn ?
	                               mir_sdr_AGC_100HZ :
	                               mir_sdr_AGC_DISABLE,
	                               -dev -> GRdB,
	                               0, 0, 0, 0, dev -> lnaState);
	if (err != mir_sdr_Success) {
	   fprintf (stderr,
	                 "Error %s on mir_sdr_AgcControl\n",
	                                         sdrplay_errorCodes (err));
	   return err;
	}
	if (!dev -> agcOn) {
	   err  =  mir_sdr_RSP_SetGr (dev -> GRdB, dev -> lnaState, 1, 0);
	   if (err != mir_sdr_Success) {
	      fprintf (stderr,
	                   "Error at set_ifgain %s (%d %d)\n",
	                                 sdrplay_errorCodes (err),
	                                 dev -> GRdB, dev -> lnaState);
	   }
	}
	return mir_sdr_Success;
}

//	rtlsdr_read_async is executed in a thread, created by our "client",
//	Communication to change the status is by simple signaling.
//	Note that reinit and Uninit better be done in the same thread
//	therefore, we choose to have all Reinits done in the 
//	thread executing the read_async
//
RTLSDR_API int rtlsdr_read_async(rtlsdr_dev_t *dev,
				 rtlsdr_read_async_cb_t cb,
				 void	*ctx,
				 uint32_t buf_num,
				 uint32_t buf_len) {
int     gRdBSystem;
int     samplesPerPacket;
mir_sdr_ErrT    err;
int     localGRed;

	if (dev == NULL)
	   return -1;

	if (dev -> running)
	   return -1;

//	just to prevent errors from streamInit
	if (dev -> inputRate < 2000000)
	   return -1;
	if (dev -> GRdB < 20)
	   dev -> GRdB = 20;
	if (dev -> GRdB > 59)
	   dev -> GRdB = 59;
	dev	-> callback	= cb;
	dev	-> ctx		= ctx;
	dev	-> buf_num	= buf_num;
	dev	-> buf_len	= buf_len;
	finalBuffer             = malloc (buf_len * sizeof (int8_t));
	localGRed		= dev -> GRdB;
#ifdef	__DEBUG__
	fprintf (stderr, "StreamInit %d %f %f %d %d %d\n",
	                  dev -> GRdB,
	                  (double)(dev -> inputRate) / 1000000.0,
	                  (double)(dev -> frequency) / 1000000.0,
	                  dev -> bandWidth,
	                  dev -> lnaState,
	                  dev -> GRdB
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
	   dev -> running = false;
	   return -1;
	}
#ifdef	__DEBUG__
	fprintf (stderr, "rtlsdr_read_async is started\n");
#endif
//	err = handle_gainSetting (dev);
//	if (err != mir_sdr_Success) 
//	   return -1;

	dev -> running	= true;
	err		= mir_sdr_SetPpm    ((float)dev -> ppm);
//
//	we make this into a simple event loop with semaphores
	while (dev -> running) {
	   int	signal;
	   int	value;
	   mir_sdr_ErrT err;
	      
	   getSignalfromQueue (&signalQueue,  &signal, &value);

#ifdef	__DEBUG__
	   fprintf (stderr, "we got a signal %d\n", signal);
#endif

	   switch (signal) {
	      case CANCEL_ASYNC:
//#ifdef	__DEBUG__
	         fprintf (stderr, "cancel request\n");
//#endif
	         dev -> running = false;
	         goto L_end;

	      case SET_FREQUENCY:
#ifdef	__DEBUG__
	         fprintf (stderr, "frequency request for %d\n", value);
#endif
	         dev -> frequency = value;
	         err = re_initialize (dev, mir_sdr_CHANGE_RF_FREQ);
	         if (err != mir_sdr_Success) {
	            fprintf (stderr, "Error at frequency setting %s\n",
	                                sdrplay_errorCodes (err));
	            break;
	         }

	         err = handle_gainSetting (dev);
	         if (err != mir_sdr_Success) {
	            fprintf (stderr, "Error at frequency setting %s\n",
	                                sdrplay_errorCodes (err));
	            break;
	         }
	         break;

	      case SET_BW: 
	         err = re_initialize (dev, mir_sdr_CHANGE_BW_TYPE);
	         if (err != mir_sdr_Success) {
	            fprintf (stderr, "ReInit failed %s\n",
	                                      sdrplay_errorCodes (err));
	            break;
	         }
	         err = handle_gainSetting (dev);
	         if (err != mir_sdr_Success) {
	            fprintf (stderr, "ReInit failed %s\n",
	                                      sdrplay_errorCodes (err));
	            break;
	         }
	         break;

	      case SET_RATE:
	         err = re_initialize (dev, mir_sdr_CHANGE_FS_FREQ);
	         if (err != mir_sdr_Success) {
	            fprintf (stderr, "ReInit failed %s\n",
	                                      sdrplay_errorCodes (err));
	            break;
	         }
	         err = handle_gainSetting (dev);
	         if (err != mir_sdr_Success) {
	            fprintf (stderr, "ReInit failed %s\n",
	                                      sdrplay_errorCodes (err));
	            break;
	         }
	         break;

	      default:
	         break;
	   }
	}
L_end:
//	when here, we finish
	signalReset (&signalQueue);
	err = mir_sdr_StreamUninit ();
	if (err != mir_sdr_Success)
	   fprintf (stderr, "Error at StreamUnInit %s\n",
	                                sdrplay_errorCodes (err));
#ifdef	__DEBUG
	fprintf (stderr, "async is stopped\n");
#else
	;
#endif
	free (finalBuffer);
	return 0;
}

//
RTLSDR_API int rtlsdr_cancel_async (rtlsdr_dev_t *dev) {
#ifdef	__DEBUG__
	fprintf (stderr, "cancel is called, running = %d\n", dev -> running);
#endif
	if (dev == NULL)
	   return -1;

	if (!dev -> running)
	   return 0;

	putSignalonQueue (&signalQueue, CANCEL_ASYNC, 0);
	while (dev -> running)
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
	dev -> testMode = on;
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

