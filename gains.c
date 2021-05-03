
#include	"gains.h"
#include	<stdio.h>

#define	KHz(x)	(x * 1000)
#define	MHz(x)	(x * 1000000)

static
int     RSP1_Table [3] [5] = {{4, 0, 24, 19, 43},
	                      {4, 0,  7, 19, 26},
	                      {4, 0,  5, 19, 24}
};

#ifdef	__MINGW32__
int	*getTable_RSP1 (int freq) {
	if (freq < MHz (420))
	   return RSP1_Table [0];
	if (freq < MHz (1000))
	   return RSP1_Table [1];
	return RSP1_Table [2];
}
#else
void    gainMapper_RSP1 (int frequency, int gainReduction,
	                           int *lnaState, int *GRdB) {
int     i;
int	*lnaTable;
	*lnaState       = 3;
	*GRdB           = 30;
	if (frequency < MHz (420))
	   lnaTable	= RSP1_Table [0];
	else
	if (frequency < MHz (1000)) 
	   lnaTable	= RSP1_Table [1];
	else
	   lnaTable	= RSP1_Table [2];

	for (i = 1; i <= lnaTable [0]; i ++) {
	   if (lnaTable [i] >= gainReduction / 3) {
	      *lnaState      = i - 1;
	      *GRdB          = gainReduction - lnaTable [i];
	      if (*GRdB < 20)
	         *GRdB = 20;
	      if (*GRdB > 59)
	         *GRdB = 59;
	      return;
	   }
	}
//      we should not be here
}
#endif

static
int     RSP2_Table [3] [10] = {
	{9, 0, 10, 15, 21, 24, 34, 39, 45, 64},
	{6, 0,  7, 10, 17, 22, 41, -1, -1, -1},
	{6, 0,  5, 21, 15, 15, 32, -1, -1, -1}
};

#ifdef	__MINGW32__
int	*getTable_RSP2 (int freq) {
	if (freq < MHz (420))
	   return RSP2_Table [0];
	if (freq < MHz (1000))
	   return RSP2_Table [1];
	return RSP1_Table [2];
}

#else
void    gainMapper_RSP2 (int freq, int gainReduction,
	                          int *lnaState, int *GRdB) {
int	*lnaTable;
int	i;
	if (freq < MHz (420))
	   lnaTable	= RSP2_Table [0];
	else
	if (freq < MHz (1000))
	   lnaTable	= RSP2_Table [1];
	else
	   lnaTable	= RSP2_Table [2];

	for (i = 1; i <= lnaTable [0]; i ++) {
	   if (lnaTable [i] >= gainReduction / 3) {
	      *lnaState      = i - 1;
	      *GRdB          = gainReduction - lnaTable [i];
	      if (*GRdB < 20)
	         *GRdB = 20;
	      if (*GRdB > 59)
	         *GRdB = 59;
	      return;
	   }
	}
//      we should not be here
}
#endif

static
int     RSP1A_Table [4] [11] = {
	{7,  0, 6, 12, 18, 37, 42, 61, -1, -1, -1},
	{10, 0, 6, 12, 18, 20, 26, 32, 38, 57, 62},
	{10, 0, 7, 13, 19, 20, 27, 33, 39, 45, 64},
	{ 9, 0, 6, 12, 20, 26, 32, 38, 43, 62, -1}
};

#ifdef	__MINGW32__
int	*getTable_RSP1a (int freq) {
	if (freq < MHz (60))
	   return RSP1A_Table [0];
	if (freq < MHz (420))
	   return RSP1A_Table [1];
	if (freq < MHz (1000))
	   return RSP1A_Table [2];
	return RSP1A_Table [3];
}
#else
void    gainMapper_RSP1a (int freq, int gainReduction,
	                     int *lnaState, int *GRdB) {
int	*lnaTable;
int	i;
	if (freq < MHz (60))
	   lnaTable	= RSP1A_Table [0];
	else
	if (freq < MHz (420))
	   lnaTable	= RSP1A_Table [1];
	else
	if (freq < MHz (1000))
	   lnaTable	= RSP1A_Table [2];
	else
	   lnaTable	= RSP1A_Table [3];

	for (i = 1; i <= lnaTable [0]; i ++) {
	   if (lnaTable [i] >= gainReduction / 3) {
	      *lnaState      = i - 1;
	      *GRdB          = gainReduction - lnaTable [i];
	      if (*GRdB < 20)
	         *GRdB = 20;
	      if (*GRdB > 59)
	         *GRdB = 59;
	      return;
	   }
	}
//      we should not be here
}

#endif

#ifndef	__MINGW32__
void    gainMapper (int hw, int f, int g, int *lna, int *grdb) {
int	gainReduction = 102 - g / 10;

	fprintf (stderr, "hw version is %d\n", hw);
	if (hw == 1)
	   gainMapper_RSP1 (f, gainReduction, lna, grdb);
	else
	if (hw == 2)
	   gainMapper_RSP2 (f, gainReduction, lna, grdb);
	else
	   gainMapper_RSP1a (f, gainReduction, lna, grdb);
}
#endif
