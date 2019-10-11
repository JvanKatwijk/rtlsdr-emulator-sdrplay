
#ifndef	__SIGNAL_QUEUE__
#define	__SIGNAL_QUEUE__

#include        <stdio.h>
#include        <semaphore.h>
#include        <pthread.h>
#include        <stdlib.h>
#include        <stdint.h>

//	the "signals" for handling commands asynchronously.
#define	CANCEL_ASYNC	0100
#define	SET_FREQUENCY	0101
#define	SET_BW		0102
#define	SET_RATE	0103

typedef	struct __theSignal {
	int	signal;
	void	*device;
	int	value;
	struct __theSignal	*next;
	pthread_mutex_t locker;
	sem_t	signalElements;
} theSignal;

void	signalInit		(theSignal **);
void	signalReset		(theSignal **);
void	putSignalonQueue	(theSignal **queue, int signal, int value);
void	getSignalfromQueue	(theSignal **queue, int *signal, int *value);

#endif
