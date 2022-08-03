/***[ThuNderSoft]*************************************************************
                           KUANG2: anti-virus thread
                                   ver: 0.12
                                     WEIRD
*****************************************************************************/

/* HISTORY */
// ver 0.12 (15-may-1999): maxdeep
// ver 0.10 (10-may-1999): born code

#include <windows.h>
#include <strmem.h>
#include <win95e.h>
#include <ctypew.h>
#include <tools.h>
#include "k2c.h"

// maximalna dozvoljena dubina rekurzije
#define     MAX_DEEP        12

HWND hsmall, hsmallist;
extern HWND hDlg;
extern BOOL connected;
char t[BUFFER_SIZE];
extern char buffer[BUFFER_SIZE];
extern HANDLE hThread;
BOOL scanning;
unsigned int deep;
unsigned int totalfiles, totalinfected, totalcleaned;

extern int IsFileInfect(char *, char* );

/*
    Enabler
    -------
   + pomoćna funkcija za enable/disable. */

void Enabler(BOOL cond)
{
    EnableWindow(hDlg, cond);
    EnableWindow(GetDlgItem(hDlg, ID_BUTTON_CLEAN), cond);
    EnableWindow(GetDlgItem(hDlg, ID_BUTTON), cond);
    return;
}


/*
    SmallSay
    --------
  + ispisuje poruku u pomoćnom prozoru. */

void SmallSay(char *poruka)
{
    unsigned int i;

    strcopyF(t, poruka); i=0;
    while ( (t[i]!='\r') && t[i]) i++;      // pronađi kraj poruke ili prvi /r/n (CRLF)
    t[i]=0;                                 // u ovom drugom slučaju iseci CRLF jer se to ne prikazuje

    SendMessage(hsmallist, WM_SETREDRAW, FALSE, 0);                     // isključi ažuriranje list boxa da bi smanjli flicker
    SendDlgItemMessage(hsmall, ID_LISTBOX, LB_ADDSTRING, 0, (LPARAM) t);
    i=SendDlgItemMessage(hsmall, ID_LISTBOX, LB_GETTOPINDEX, 0, 0);     // preuzmi index prve prikazane linije
    if (i==32101) {                                                     // više od 32100 nevidljivih linija linija na gore?
        SendDlgItemMessage(hsmall, ID_LISTBOX, LB_DELETESTRING, 0, 0);  // da, obriši prvu
        i=32100;
    }
    SendDlgItemMessage(hsmall, ID_LISTBOX, LB_SETTOPINDEX, i+1, 0);     // postavi da se sadržaj listboxa promeni tako da se dodata linija vidi

    i=SendDlgItemMessage(hsmall, ID_LISTBOX, LB_GETCOUNT, 0, 0);            // nađi index+1 poslednjeg
    SendMessage(hsmallist, WM_SETREDRAW, TRUE, 0);                      // uključi ažuriranje list boxa (trebalo bi posle sledeće pa onda InvalidateRect, ali neću da komplikujem)
    SendDlgItemMessage(hsmall, ID_LISTBOX, LB_SETCARETINDEX, i-1, MAKELPARAM(TRUE, 0)); // fokusiraj poslednji i osveži ceo list box
    return;
}


/*
    SmallSayReplace
    ---------------
   + ispisuje poruku u pomoćnom prozoru ali tako što zameni poslednju. */

void SmallSayReplace(char *poruka)
{
    unsigned int i;
    strcopyF(t, poruka); i=0;
    while ( (t[i]!='\r') && t[i]) i++;      // pronađi kraj poruke ili prvi /r/n (CRLF)
    t[i]=0;                                 // u ovom drugom slučaju iseci CRLF jer se to ne prikazuje

    SendMessage(hsmallist, WM_SETREDRAW, FALSE, 0);                     // isključi a‚uriranje
    i=SendDlgItemMessage(hsmall, ID_LISTBOX, LB_GETCOUNT, 0, 0);        // nađi index+1 poslednjeg
    SendDlgItemMessage(hsmall, ID_LISTBOX, LB_DELETESTRING, i-1, 0);    // obriši poslednji
    SendMessage(hsmallist, WM_SETREDRAW, TRUE, 0);                      // uključi ažuriranje
    SendDlgItemMessage(hsmall, ID_LISTBOX, LB_ADDSTRING, 0, (LPARAM) t);// dodaj novi umesto obrisanog i osveži list box
    return;
}


/*
    small_MsgLoop
    -------------
   + odgovara na poruke za mali pomoćni dijalog. */

LRESULT CALLBACK small_MsgLoop(HWND hd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

        case WM_CREATE:
            hsmallist=GetDlgItem(hd, ID_LISTBOX);
            return TRUE;


        case WM_GETMINMAXINFO: {        // zahteva se informacija o min/max dimenzijama dijaloga
            MINMAXINFO *lpMMI = (MINMAXINFO *)lParam;
            lpMMI->ptMinTrackSize.x=200;        // minimalna širina
            lpMMI->ptMinTrackSize.y=96;         // minimalna visina
            return 0;       // vraća se 0 ako je ova poruka obrađena
            }

        case WM_SIZING: {   // pomeranje u toku...
            unsigned int w,h;
            RECT *lpr=(LPRECT) lParam;
            w = (lpr->right - lpr->left)-8;     // oslobodi se bordera
            h = (lpr->bottom - lpr->top)-8;     // oslobodi se bordera
            h=(h/16)*16;                        // visina linije listboxa je 16 pixela
            lpr->bottom=lpr->top+h+15;
            MoveWindow(GetDlgItem(hd, ID_LISTBOX), 0,0, w, h+4-16, TRUE);   // po 2 pixela je razmak gornje i donje ivice listboxa
            return TRUE;                                                    // od prve i poslednje vidljive linije
            }

        case WM_SIZE:       // pomeranje je zavr„eno
            InvalidateRect(hd, NULL, FALSE);        // osveži ceo prozor
            return TRUE;

        case WM_SYSCOMMAND:
            if (wParam==SC_CLOSE) {                 // pritisnuto 'x' dugme za kraj
                if (scanning) {                     // skeniranje u toku
                    SuspendThread(hThread);         // privremeno stopiraj thread
                    if (MessageBox(hd, "Stop scanning?", "Kuang2 anti-virus",MB_ICONQUESTION | MB_YESNO | MB_APPLMODAL) == IDYES) {
                        scanning=FALSE;             // označi da je gotovo sa sekniranjem
                    }
                    ResumeThread(hThread);
                } else {                            // skeniranje završeno
                    Enabler(TRUE);
                    DestroyWindow(hd);              // ubij dijalog
                }
                return TRUE;
            }
        }
    return FALSE;
}

// Potpis Kuang2 virusa. Od pravog potpisa se razlikuje samo u prvom
// karakteru da AntiVirus ne bi našao sam sebe kao zaražen!

char signature[]={
 'W', 0x46, 0x53, 0x4F, 0x46, 0x4D, 0x34, 0x33, 0x2F, 0x45,
0x4D, 0x4D, 0x01, 0x48, 0x66, 0x75, 0x58, 0x6A, 0x6F, 0x65,
0x70, 0x78, 0x74, 0x45, 0x6A, 0x73, 0x66, 0x64, 0x75, 0x70,
0x73, 0x7A, 0x42, 0x01, 0x48, 0x66, 0x75, 0x44, 0x70, 0x6E,
0x71, 0x76, 0x75, 0x66, 0x73, 0x4F, 0x62, 0x6E, 0x66, 0x42};

/*
    DesinfectFile
    -------------
   + čisti fajl od virusa
   + virus je user-friendly tako da omogućava ultra-lako čišćenje!
   + vraća 0 ako je sve u redu. */

int DesinfectFile(char *fname) {
    unsigned int retvalue;          // povratna vrednost
    char *filemap;                  // pointer na MMF
    char *filestart;                // pointer na početak MMF fajla
    unsigned int fsize;             // veličina fajla
    unsigned int fattr;             // atributi fajla
    unsigned int nasao;             // rezultat pretrage
    unsigned int *_val;
    FILETIME creation_time, lastacc_time, lastwr_time;
    HANDLE hfile, hfilemap;

    retvalue=0;

    // uzmi atribute fajla i vidi da li je read only!
    fattr=GetFileAttributes(fname);         // uzmi atribute fajla
    if (fattr & FILE_ATTRIBUTE_READONLY)    // resetuj readonly ako ga ima
        SetFileAttributes(fname, fattr ^ FILE_ATTRIBUTE_READONLY);

    // otvori fajl
    hfile=CreateFile(fname, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, fattr, NULL);
    if (hfile==INVALID_HANDLE_VALUE) {retvalue=0x10; goto end1;}

    // uzmi veličinu fajla
    fsize=GetFileSize(hfile, NULL);
    if (fsize==0xFFFFFFFF) {retvalue=0x11; goto end2;}

    // sačuvaj original vreme fajla
    GetFileTime(hfile, &creation_time, &lastacc_time, &lastwr_time);

    // kreiraj MMF
    hfilemap=CreateFileMapping (hfile, NULL, PAGE_READWRITE, 0, fsize, NULL);
    if (hfilemap==NULL) {retvalue=0x12; goto end2;}
    // kreiraj MMF view na ceo fajl
    filemap=(void *) MapViewOfFile (hfilemap, FILE_MAP_ALL_ACCESS, 0,0,0);
    if (filemap==NULL) {retvalue=0x13; goto end3;}
    filestart=filemap;


    // proveri da li je već zaražen (čak 50 znakova!)
    nasao=memfind(filestart, fsize, signature, 50);
    if (nasao==-1) goto end4;

    filemap=filestart+nasao-53;     // vrati se 53 bajtova iza nađenog stringa
    _val=(unsigned int*) filemap;   // preuzmi pointer na početak bloka

    // zapis #1 - EntryPoint
    filemap=filestart + *(_val+1);
    *(unsigned int *) filemap=*_val;
    // zapis #2 - FileSize
    _val+=2;
    fsize=*_val;

    // zapis #old1 - #old5
    nasao=0; _val++;
    while (nasao<5) {
        if (*_val) {
            filemap=filestart + *_val;              // ofset
            *(unsigned int *) filemap=*(_val+1);    // data
        }
        _val+=2;
        nasao++;
    }

/*** REGULARAN KRAJ ***/
    retvalue=0;

    // upiši sve promene nazad u fajl
    FlushViewOfFile(filestart,0);
end4:
    UnmapViewOfFile(filemap);   // zatvaramo MMF view
end3:
    CloseHandle(hfilemap);      // zatvaramo MMF
    if (!retvalue) {            // ako nije bilo gre„ke
        // namesti regularnu veličinu fajla
        SetFilePointer(hfile, fsize, NULL, FILE_BEGIN);
        SetEndOfFile(hfile);
        // vrati staro vreme
        SetFileTime(hfile, &creation_time, &lastacc_time, &lastwr_time);
        // a i atribute (ako je bio read-only)
        if (fattr & FILE_ATTRIBUTE_READONLY) SetFileAttributes(fname, fattr);
    }
end2:
    CloseHandle(hfile);         // zatvarmo fajl
end1:
    return retvalue;
}

char *fn;
char _bf[]="..";


/*
    DesinfectFolder
    ---------------
   + čisti folder.
   + rekurzija, ali do određene dubine. */

void DesinfectFolder(char *folder) {
    WIN32_FIND_DATA FileData;
    HANDLE hSearch;
    char path[MAX_PATH];
    unsigned int ix;

    // dubina rekurzije
    deep++;
    if (deep>MAX_DEEP) {deep--; return;}

    // započni pretragu foldera
    strcopyF(path, folder);
    setfilename(path, "*.*");
    hSearch=FindFirstFile(path, &FileData);         // traži foldere
    if (hSearch==INVALID_HANDLE_VALUE) {
        wsprintf(buffer, "[Warning]: %s", path);
        SmallSayReplace(buffer);
        SmallSay("searching...");
        return;
    }

    // vrti sve fajlove
    do {
        // proveri da li je došao zahtev za završavanjem thread-a?
        if (!scanning) break;
        setfilename(path, FileData.cFileName);      // uzmi ime trenutnog fajla

        if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            fn=getfilename(path);
            if (!strcompF(fn, &_bf[1]) || !strcompF(fn, _bf)) continue;
            ix=strlengthF(path);
            path[ix]='\\'; path[ix+1]=0;            // dodaj '/'
            DesinfectFolder(path);                  // rekurzija!
            path[ix]=0;                             // obriši '/'
        } else {
            fn=getfileext(path);                    // uzmi extenziju
            if (fn==NULL) continue;                 // ako ne postoji izađi
            ix=0; while (fn[ix]) {                  // ako postoji
                fn[ix]=_to_lower(fn[ix]);           // napravi mala slova
                ix++;                               // sva slova
            }
            if (!strcompF(fn, "exe")) {             // da li je .exe?
                SmallSayReplace(path);              // da, posao :)
                totalfiles++;
                switch (IsFileInfect(path, signature)) {
                    case -1:
                        wsprintf(buffer, "[Error] %s", path);
                        SmallSayReplace(buffer);
                        SmallSay("seraching...");
                        break;
                    case 0:
                        break;
                    case 1:
                        totalinfected++;
                        if (!DesinfectFile(path)) {
                            ix=0; while(path[ix]) {path[ix]=_to_lower(path[ix]); ix++;}
                            wsprintf(buffer, "[CLEANED] %s", path);
                            totalcleaned++;
                        } else {
                            MessageBeep(MB_ICONEXCLAMATION);
                            wsprintf(buffer, "[INFECTED] %s", path);
                        }
                        SmallSayReplace(buffer);
                        SmallSay("searching...");
                        break;
                }
            }
        }
    } while (FindNextFile(hSearch, &FileData));

    FindClose(hSearch);                 // završi sa pretragom
    deep--;
    return;
}


/*
    CleanSystem
    -----------
   + čisti ceo sistem od Kuang2 virusa. */

DWORD WINAPI CleanSystem(LPVOID _d)
{
    DWORD drives;
    char root[]="a:\\";
    char explorer[MAX_PATH];
    char temp[MAX_PATH];
    char wininit[MAX_PATH];

    scanning=TRUE;
    Enabler(FALSE);

    signature[0]=0x4C;
    totalfiles=totalinfected=totalcleaned=0;

    // FAZA #1
    SmallSay("Phase #1: scanning system kernel.");
    // sledeći deo je identičan kao i kod inficiranja, osim što se
    // koristi funkcija 'DesinfectFile'
    GetWindowsDirectory(explorer, MAX_PATH);
    strcopyF(wininit, explorer);
    straddF(explorer, "\\Explorer.exe");        // napravi string sa Explorer.exe
    straddF(wininit, "\\wininit.ini");          // napravi string sa Winit.exe

    if (IsFileInfect(explorer, signature)==1) { // ako je zaražen...
        SmallSay("System kernel is infected!");
        strcopyF(temp, explorer);
        setfileext(temp, "wk2");                // napravi string za kopiju Exlorer.exe
        CopyFile(explorer, temp, FALSE);        // kopiraj Explorer.exe
        if (!DesinfectFile(temp)) {             // ako je uspešno dezinfikovan
            WritePrivateProfileString("Rename", explorer, temp, wininit);   // napravi 'wininit.ini' :)
            SmallSay("[CLEANED] system. Reboot and Run anti-virus again!");
            scanning=FALSE;
            return 0;
        } else {
            SmallSay("[Error] can't desinfect!");
            DeleteFile(temp);               // a ako nije obriši kopirani fajl
        }
    } else SmallSay("System kernel is clean.");


    // FAZA #2
    SmallSay("------------------------------------------------------------");
    SmallSay("Phase #2: scanning all fixed drives.");

    // Prvo nađi samo fixne drajvove
    drives=GetDrives(DRIVE_FIXED);
    // priprema drajvova...
    root[0]='a';

    SmallSay("scanning...");
    /* Pretraga */
    while (drives) {
        if (!scanning) break;
        deep=0;
        if (drives & 1) DesinfectFolder(root);
        drives=drives>>1;
        root[0]++;
    }

    // Kraj, statistika
    SmallSayReplace("------------------------------------------------------------");
    wsprintf(temp, "Total scanned: %u files.", totalfiles);
    SmallSay(temp);
    wsprintf(temp, "Total infected: %u files.", totalinfected);
    SmallSay(temp);
    // ako ima zaraženih
    if (totalinfected) {
        wsprintf(temp, "Total cleaned: %u files.", totalcleaned);
        SmallSay(temp);
    }
    // ako ima grešaka
    deep=totalinfected - totalcleaned;
    if (deep) {
        wsprintf(temp, "No. of errors: %u files.", deep);
        SmallSay(temp);
    }

    SmallSay("Done.");
    scanning=FALSE;
    return 0;
}
