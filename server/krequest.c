/***[ThuNderSoft]*************************************************************
								KUANG2: request
								   ver: 0.10
								     WEIRD
*****************************************************************************/

/* HISTORY */
// ver 0.10 (28-apr-1999): born code

#include <windows.h>
#include <winsock.h>
#include "kuang2.h"

// Kod velikih servera Request lista bi trebalo da se dobija dinamički, tj.
// da se memorija odvaja pri svakom dodavanju. Pošto ovo nije veliki server
// već mali, Request lista se može i ovako napraviti.
REQUEST request[MAXCONN];

/*
	ClearRequests
	-------------
  + Briše celu request listu. */

void ClearRequests(void)
{
	int i;

	for (i=0; i<MAXCONN; i++) request[i].socket=-1;

	return;
}


/*
	NewRequest
	----------
  + Pretrazuje Request listu za slobodnim mestom i vraća njegov index.
  + Vraća -2 ako nema slobonih mesta. */

int NewRequest(void)
{
	unsigned int i;

	for (i=0; i<MAXCONN; i++)
		if (request[i].socket==-1) return i;

	return NOMOREREQUEST;
}

/*
	GetRequest
	----------
  + Pretrazuje Request listu i vraća index kada se poklope socketi.
  + Vra†a -3 ako nema slobonih mesta. */

int GetRequest(SOCKET s)
{
	unsigned int i;

	for (i=0; i<MAXCONN; i++)
		if (request[i].socket==s) return i;

	return BADREQUEST;
}
