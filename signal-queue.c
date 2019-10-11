
#include	"signal-queue.h"

static  pthread_mutex_t locker;
static  sem_t   signalElements;

void	signalInit	(theSignal **q) {
	pthread_mutex_init (&locker, NULL);
	sem_init (&signalElements, 0, 0);
	*q	= NULL;
}

void	signalReset (theSignal **q) {
	pthread_mutex_trylock (&locker);
	pthread_mutex_unlock (&locker);
	while (*q != NULL) {
	   theSignal *x = (*q) -> next;
	   free (*q);
	   *q = x;
	}
}

void	putSignalonQueue (theSignal **q, int signal, int value) {
theSignal	*n;
	pthread_mutex_lock (&locker);
	n	= (theSignal *)malloc (sizeof (theSignal));
	n	-> signal	= signal;
	n	-> value	= value;
	n	-> next		= (*q);
	(*q)			= n;
	sem_post (&signalElements);
	pthread_mutex_unlock (&locker);
}

void	getSignalfromQueue (theSignal **q, int *signal, int *value) {
theSignal *tmp;
	sem_wait (&signalElements);
	pthread_mutex_lock (&locker);
	tmp	= (*q);
	*signal	= (*q)	-> signal;
	*value	= (*q)	-> value;
	(*q)	= (*q)	-> next;
	free (tmp);
	pthread_mutex_unlock (&locker);
}
