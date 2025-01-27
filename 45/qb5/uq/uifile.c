/***
*uifile.c - Support File menu items.
*
*       Copyright <C> 1985-1988 Microsoft Corporation
*
*Purpose:
*       Support code for file menu items.
*
*******************************************************************************/

/* First, include version definition header */
#include <version.h>

/* Next, include COW's interface headers */
#include <cw/version.h>
#include <cw/windows.h>
#include <cw/edityp.h>
#include <uiext.h>
#include <uinhelp.h>

#include <bios.h>                            // [QH1]

/* And the dialog information */
#include <cw/dlg.h>
#include "uifile.hs"
#include "uifile.sdm"

/* Next, include QBI's headers */
#ifndef CONTEXT_H
#include <context.h>
#endif

#ifndef HEAP_H
#include <heap.h>
#endif

#ifndef NAMES_H                                 /* [2]  */
#include <names.h>                             /* [2]  */
#endif                                          /* [2]  */

#ifndef PARSER_H
#include <parser.h>
#endif

#ifndef QBIMSGS_H
#include <qbimsgs.h>
#endif

#ifndef RTINTERP_H
#include <rtinterp.h>
#endif

#ifndef RTPS_H
#include <rtps.h>
#endif

#ifndef TXTMGR_H
#include <txtmgr.h>
#endif

#ifndef UI_H
#include <ui.h>
#endif

#ifndef UIINT_H
#include <uiint.h>
#endif

#ifndef UTIL_H
#include <util.h>
#endif

extern word NEAR RetryError (WORD);             //[22]
void FAR MakeRelativeFileSpec(char *, char *);
void NEAR LoadMerge(void);
BOOL near DoSaveFile(void);
STATICF(boolean) AskNotSavedInc(void);
WORD DoDlgLoadFile (void far *, WORD, WORD);
WORD DoDlgSaveFile (void far *, WORD, WORD);
extern BOOL NEAR ValidateFile (void);                   //[25]
extern BOOL NEAR ChangeDir (char *, WORD, char *);      //[25]
extern BOOL FAR fogNamUntitled(WORD);           //[49]

boolean far ClrModifiedInc(void);               //[24]
boolean far ClrModified(void);                  //[24][30]
void near ClrAllModified(bool);                 //[30]
boolean far SetAnyMrsMain(void);                //[31]

char *GetPrintDest (void);                               // [QH1]
DWORD FAR EditSubProc (PWND pwnd, WORD message,          // [QH1]
                       WORD wParam, DWORD lParam);       // [QH1]



WORD FAR WriteFileErr(WORD, char *, WORD );
BOOL IsDeviceReady (WORD iPort);

/* set by LoadFile() whenever a DATA statement is moved from a procedure
 * to module level
 */
extern bool fDataMoved;

bool fExiting = 0;              //[40] true if we are exitting QB.
char szPathSeparator[] = "\\";
char szNull[] = "";
char szUntitled[] = "Untitled"; //[49]
extern char szWildDefault[];    //[25] "*.bas" (defined in FILENAME.ASM)

extern ushort iHelpId;
bool fDirChanged = 0;           //[39] TRUE if b$Buf1 contains current
                                //[39] directory during a dialog.

bd bdFileLoad;
bd bdFileSave;
WORD oRsFileSave;
boolean fIsProgram = TRUE;      //[44] TRUE if program, FALSE if document
sd sdUntitled;                  //[45]


//
// [QH1] Start
//
extern bool fOptionsChanged;           // Has the printer setup been changed?
unsigned    iPrintPort;                // Printer port (LPT1, LPT2, COM1, etc)
bool        fPrintToFile;              // TRUE:Use file, FALSE:Use printer
char        szPrintDest[MAX_PATH];     // Print file
char       *aszDevices[] =             // Indexed via iPrintPort
{
    "LPT1",
    "LPT2",
    "LPT3",
    "COM1",
    "COM2"
};

extern char  szPrintSetup[];           // Declared in uirsrcc.c
extern char *szDialogTitle;            // Declared in uictl.c
extern WORD  cbDialogTitle;            // Declared in uictl.c

PLFN_WNDPROC pfnOldWndProc;            // Holds old edit-control proc

// Setting pwndPrintEdit != NULL enables special VK_TAB behavior in CW lib.
PWND pwndPrintEdit = NULL;

#define TAB  9
//
// [QH1] End
//



/* Terminology:
 * FileSpec = fully specified filename including optional drive, path, ext
 * FileName = directory relative filename, i.e. 8 letter base + extension
 * Path = drive and directory
 * Path + FileName = FileSpec
 * FILNAML - used in this file is imported from the runtime.
 *           it is the maximum legal file name. (see rtps.h)
 */


/***
*char *(FAR FileSpec (szFileSpec))
*Purpose:
*       Find the file name part of szFileSpec, and normalizes it.
*
*Entry:
*       szFileSpec      The File Spec to find file name of.
*
*Exit:
*       Return a pointer to the file name in szFileSpec.
*
*Exceptions:
*       None.
*******************************************************************************/

char *(FAR FileSpec (szFileSpec))
char *szFileSpec;
{
    char szFileName[FILNAML];

    strcpy (szFileName, szFileSpec);
    NormFileNameFar (szFileName, CbSzUi (szFileName), szFileSpec, szNull);
    return (b$PN_NAME);
}

/***
*void far MakeRelativeFileSpec (szFile, szWorkingDir)
*Purpose:
*       The given file spec in szFile is converted from a fully qualified
*       spec to a spec relative to the current directory.
*
*Entry:
*       szFile          The file spec.
*       szWorkingDir    The current working drive and directory.
*
*Exit:
*       Return relative path name in szFile.
*
*Exceptions:
*       None.
*******************************************************************************/
void FAR MakeRelativeFileSpec( szFile, szWorkingDir)
REG1 char *szFile;
char *szWorkingDir;
{
    REG2 WORD cb;
    char chSave;
    WORD result;
    register char *p;

    cb = CbSzUi (szWorkingDir);

    chSave = szFile[cb];
    szFile[cb] = 0;
    result = strcmp (szFile, szWorkingDir);
    szFile[cb] = chSave;
    if (result == 0) {
        /* szFile is a decendant of the working directory */
        if (cb == 3) {
            /* name is "<drive>:\".  Don't include '\' in count */
            cb = 2;
        }
        strcpy (szFile, szFile + cb + 1);
    }
    else {
        /* See if it is in a sibling */
        p = szWorkingDir + cb - 1;
        while (*p != '\\')
            p--;
        if (p > &szWorkingDir[3]) {
            *p = 0;
            cb = CbSzUi (szWorkingDir);
            chSave = szFile[cb];
            szFile[cb] = 0;
            result = strcmp (szFile, szWorkingDir);
            szFile[cb] = chSave;
            *p = '\\';

            if (result == 0) {
                strcpy (szFile, "..\\");
                strcat (szFile, szFile + cb + 1);
            }
        }
    }
}

/***
*BOOL FAR FDlgLoadFile (dlm, tmc, wNew, wOld, wParam)
*Purpose:
*       Dialog procedure for dlgFileLoad and dlgFileOpen.
*       Set the file listbox columns number on initialization, and
*       validates the filename.
*
*Entry:
*       dlm             dialog message type.
*       tmc             Ignored.
*       wNew            Ignored.
*       wOld            Ignored.
*       wParam          Ignored.
*
*Exit:
*       Return TRUE.
*
*Exceptions:
*       None.
*******************************************************************************/
BOOL FAR
FDlgLoadFile (dlm, tmc, wNew, wOld, wParam)
WORD dlm;
TMC tmc;
WORD wNew, wOld, wParam;
{
    Unreferenced (wNew);
    Unreferenced (wOld);
    Unreferenced (wParam);

    if (dlm == dlmInit) {
        SetTmcListWidth (tmcListFiles, 3);      // [10]
    } else {
        if ((dlm == dlmClick && tmc == tmcOK) ||
            (dlm == dlmDblClk && tmc == tmcListFiles)) {
            return (ValidateFile());
        }
    }
    return (TRUE);
}

/***
*BOOL FAR FDlgSaveCreateFile (dlm, tmc, wNew, wOld, wParam)
*Purpose:
*       Added with revision [25].
*
*       Dialog procedure for dlgFileSave and dlgFileCreate.
*       Validates the filename.
*
*Entry:
*       dlm             dialog message type.
*       tmc             Ignored.
*       wNew            Ignored.
*       wOld            Ignored.
*       wParam          Ignored.
*
*Exit:
*       Return TRUE.
*
*Exceptions:
*       None.
*******************************************************************************/
BOOL FAR
FDlgSaveCreateFile (dlm, tmc, wNew, wOld, wParam)
WORD dlm;
TMC tmc;
WORD wNew, wOld, wParam;
{
    Unreferenced (wNew);
    Unreferenced (wOld);
    Unreferenced (wParam);

    if (dlm == dlmClick && tmc == tmcOK) {
        return (ValidateFile());
    }

    return (TRUE);
}


/***
*WORD DoDlgLoadFile (pDlg, cbDlg, cabi)
*Purpose:
*       Do a file loading dialog until we get back a valid file name.
*
*       The path is put into bdFileLoad.
*
*Entry:
*       pDlg            File loading dialog.
*       cbDlg           Size of the dialog.
*       cabi            Cab index to initialize cab structure.
*
*Exit:
*       Returns file type returned for loaded file if dialog returns tmcOk,
*       or returns isNothing if tmcCancel, and leaves path in bdFileLoad.
*
*Exceptions:
*       None.
*******************************************************************************/
WORD DoDlgLoadFile (pDlg, cbDlg, cabi)
void far *pDlg;
WORD cbDlg, cabi;
{
    HCABFileOpen hcabFile;              //[22]
    sd  sdBuf2;                         //[25]
    WORD gr = FALSE;

    DbAssert (uierr == FALSE);                  //[22]
    hcabFile = (HCABFileOpen) HcabAlloc (cabi); //[22]
    if (uierr)  // If HcabAlloc failed, return cancelled
        return (isNothing);

    DbChkHoldBuf1();                    //[19] b$Buf1 used for life of routine
    DbChkHoldBuf2();                    //[19] b$Buf2 used for life of routine

    GetCurDriveDir (&b$Buf1);           //[19] save current dir in b$Buf1
    fDirChanged++;                      //[39] b$Buf1 contains directory

    if (bdFileLoad.pb != NULL)          //[25] if there was previous file loaded
        ChangeDir (bdFileLoad.pb, bdFileLoad.cbLogical, szNull);
                                        //[25] attempt to set current directory
                                        //[25] to that of last file loaded.
                                        //[25] Ignore errors.

    SzToCab (hcabFile, (cmdSwitches & CMD_SW_ED) ? "*.TXT" : szWildDefault, Iag (CABFileOpen, szFileName));     //[42]

    // [23] if SzToCab memory allocation failed, free cab and exit.
    if (uierr) {
        goto EndLoadFile;
    }

    HookInt24();        //[22] reset int 24 state so errors can be checked
    if (!(gr = (TmcDoDlgFar (pDlg, cbDlg, hcabFile) != tmcCancel)))
        goto EndLoadFile;

    DbAssert (b$fInt24Err == UNDEFINED);        //[25] no unhandled int24 errors

//[25] Realloc bdFileLoad to length of new pathname (in b$Buf2) and copy
//[25] b$Buf2 into it.
    sdBuf2.pb = &b$Buf2;                        //[25]
    sdBuf2.cb = CbSzUi(&b$Buf2);                //[25]
    if (!BdChgContents(&bdFileLoad,&sdBuf2)) {  //[25]
        SetUiErrOm ();                          //[25]
        goto EndLoadFile;                       //[25]
    }                                           //[25]

EndLoadFile:
    SetCurDir2 (&b$Buf1);               //[19] restore current directory
    fDirChanged--;                      //[39] b$Buf1 doesn't contain directory

    DbChkFreeBuf1();                    //[19] done with b$Buf1
    DbChkFreeBuf2();                    //[19] done with b$Buf2

    FreeCab (hcabFile);
    return gr;
}

/***
*WORD DoDlgSaveFile (pDlg, cbDlg, cabi)
*Purpose:
*       Do a file save dialog until we get back a valid file name.
*
*       The path is put into bdFileSave.
*
*Entry:
*       pDlg            File saving dialog.
*       cbDlg           Size of the dialog.
*       cabi            Cab index to initialize cab structure.
*
*Exit:
*       Returns file type returned for saved file if dialog returns tmcOk,
*       or returns isNothing if tmcCancel, and leaves path in bdFileSave.
*
*Exceptions:
*       None.
*******************************************************************************/
WORD DoDlgSaveFile (pDlg, cbDlg, cabi)
void far *pDlg;
WORD cbDlg, cabi;
{
    HCABFileSave hcabFileSave;
    WORD oRsFound;
    WORD id;
    char *pName;                        //[25]
    WORD gr;

    DbAssert (uierr == FALSE);                          //[22]
    hcabFileSave = (HCABFileSave) HcabAlloc (cabi);     //[22]
    if (uierr)  // If HcabAlloc failed, return cancelled
        return FALSE;

    DbChkHoldBuf1();                    //[19] b$Buf1 used for life of routine
    DbChkHoldBuf2();                    //[19] b$Buf2 used for life of routine

    GetCurDriveDir (&b$Buf1);           //[19] save current dir in b$Buf1
    fDirChanged++;                      //[39] b$Buf1 contains directory

    while (TRUE) {                      //[25]

        gr = FALSE;                     //[37] assume error

        if (ChangeDir (bdFileSave.pb, bdFileSave.cbLogical, szNull)) //[25]
                                        //[25] attempt to set current dir to
                                        //[25] that of last file file saved.
            pName = bdFileSave.pb;      //[25] if error, use full pathname
        else
            pName = b$PN_NAME;          //[25] if successful, use only name
                                        //[25] portion
        strcpy (&b$Buf2, pName);        //[35] ensure static before SzToCab
        SzToCab (hcabFileSave, &b$Buf2, Iag (CABFileSave, szFileName)); //[35]
        // [23] if SzToCab memory allocation failed, free cab and exit.
        if (uierr)                      //[37]
            break;                      // return isNothing

        (*hcabFileSave)->u.sab = sabSaveAll;    /* [8] */

        HookInt24();    //[22] reset int 24 state so errors can be checked
        if (TmcDoDlgFar (pDlg, cbDlg, hcabFileSave) == tmcCancel)
            break;                              // return isNothing

        DbAssert (b$fInt24Err == UNDEFINED);    //[25] no unhandled int24 errors

        if (!BdRealloc(&bdFileSave, CbSzUi(&b$Buf2)+1)) { //[51] Grow bd if needed
            SetUiErrOm ();                      //[51] OM if no space
            break;
        }

        strcpy (bdFileSave.pb, &b$Buf2);        //[25] copy pathname into bd
        bdFileSave.cbLogical--;                 //[51] Don't include 0 term.

        if (!(oRsFound = OgNamOfPsd((sd *)&bdFileSave))) { //[27]
            SetUiErrOm ();                      //[27] out of memory
            break;                              //[27] return isNothing
        }                                       //[27]

        gr = TRUE;
        oRsFound = MrsFind(oRsFound);           //[27]
        DbAssert (oRsFound != 0);  /* out-of-memory not possible */
        if (oRsFound != oRsFileSave) {
            /* user renamed mrs during dialog - remember to make sure
             * file doesn't already exist on disk */
            mrsCur.flags2 |= FM2_NewFile;
        }
        if ((oRsFound != UNDEFINED) &&
            (oRsFound != oRsFileSave)) {
            /* tried to save module with name of another already loaded
             * module - we can't 2 mrs's in memory with same name */
            MsgBoxStd( MB_OK, MSG_ModuleExists );
            continue;           // prompt for filename again
        }
        if (mrsCur.flags2 & FM2_NewFile) {
            mrsCur.flags2 &= ~FM2_NewFile;
            if (FileExists(bdFileSave.pb)) {
                /* file already exists */
                id = MsgBoxStd (MB_YESNOCANCEL, MSG_Overwrite);
                /* File already exists.  Over write? */
                if (id == IDNO)
                    continue;   /* prompt for FileName again */
                UiRsActivate (oRsFileSave);
                /* MsgBoxStd could have altered grs.oRsCur */
                if (id == IDCANCEL) {
                    gr = FALSE;
                    mrsCur.flags2 |= FM2_NewFile;
                }
            }
        }
        break;
    }   // while (TRUE)

    SetCurDir2 (&b$Buf1);               //[19] restore current directory
    fDirChanged--;                      //[39] b$Buf1 doesn't contain directory

    DbChkFreeBuf1();                    //[19] done with b$Buf1
    DbChkFreeBuf2();                    //[19] done with b$Buf2

    FreeCab (hcabFileSave);
    return gr;
}

/***
*void near CmdFileNew ()
*Purpose:
*       Perform File Menu, New Program command.  Eliminates all current
*       code, and initializes new program.
*
*Entry:
*       None.
*
*Exit:
*       None.
*
*Exceptions:
*       None.
*******************************************************************************/
VOID NEAR CmdFileNew ()
{
    // [26] If NewStmt succeeds, it calls MrsDiscard which calls WndReAssign
    // [26] to change all visible window's oRs's
    uierr = CallRtTrap (NewStmt);
    if (!uierr && (cmdSwitches & CMD_SW_ED)) {  //[45][47]
        sdUntitled.pb = szUntitled;             //[49]
        sdUntitled.cb = CbSzUi(szUntitled);     //[49]
        RsMake (&sdUntitled, RS_document);      //[45]
    }                                           //[45]
    fIsProgram = !(mrsCur.flags2 & FM2_NoPcode);
    DrawDebugScr ();
}

boolean far ClrModified() {
    mrsCur.flags2 &= ~FM2_Modified;
    return(TRUE);
}

boolean far ClrModifiedInc() {
    if (mrsCur.flags2 & FM2_Include) {
        mrsCur.flags2 &= ~FM2_Modified;
    }
    return(TRUE);
}

void near ClrAllModified(bool fAll) {
    ForEach(FE_PcodeMrs+FE_CallMrs+FE_TextMrs+FE_SaveRs+FE_FarCall,
            fAll ? ClrModified : ClrModifiedInc);
}

/***
*void near DoLoadFile (otx, pbdFileName)
*Purpose:
*       Load a file, handling INT24 errors.
*
*Entry:
*       otx             = LF_NewProg if pcode-module is to be added after
*                         doing a NewStmt.
*                       = LF_NewModule if pcode-module is to be added.
*                       = LF_NewDoc if a document-module is to be added.
*                       = otherwise otx is the text offset of a file to be
*                         merged into the current text table.
*
*Exit:
*       uierr is set to standard error code if any error occurs.
*
*Exceptions:
*       None.
*******************************************************************************/
void near DoLoadFile (otx, pbdFileName)
WORD otx;
bd * pbdFileName;
{
    WORD oRsSave;
    struct ef *pef;
    PWND pwnd;
    short result;                                       //[24]

    pwnd = (pwndAct == &wndCmd || pwndAct == &wndHelp) ? pwndTop : pwndAct;
    pef = (struct ef *) pwnd->pefExtra;                 // [36]
    oRsSave = pef->hBuffer;

    if (otx == LF_NewProg) {                            //[24]
        result = NotSavedIncSav();                      //[24]
        if (result > 0)                                 //[24]
            return;                                     //[24]
        else if (result < 0)                            //[24]
            ClrAllModified(FALSE);                      //[24][30]
        TxtReInclude();                                 //[24]
    }                                                   //[24]

LoadRetry:
    if (LoadFile ((sd *) pbdFileName, otx) != 0) {
        DoDrawDebugScr ();
        if ((result = RetryError(txtErr.errCode)) == IDRETRY)   //[38] if
                                                //[38] retryable error,
                                                //[38] put up retry/cancel box
            goto LoadRetry;                     //[38] user said to retry

        if (result != IDCANCEL)                 //[38] if CANCEL not hit, put
            uierr = txtErr.errCode;             //[38] up a message box later
        /* If the buffer is still around, reactivate it and make it visible */
        if ((otx != LF_NewProg) && (otx != LF_NewDoc)) {        //[49]
            UiRsActivate (oRsSave);
            WndAssign ();
        }
    }
}

/***
*void NEAR LoadMerge ()
*Purpose:
*       Display the dialog for FILE/OPEN, FILE/MERGE, or FILE/LOAD.
*
*Entry:
*       otx             = LF_NewProg if pcode-module is to be added after
*                         doing a NewStmt.
*                       = LF_NewModule if pcode-module is to be added.
*                       = LF_NewDoc if a document-module is to be added.
*                       = otherwise otx is the text offset of a file to be
*                         merged into the current text table.
*
*Exit:
*       None.
*
*Exceptions:
*       None.
*******************************************************************************/
void NEAR LoadMerge ()
{
    /* Don't let user load such a long program that he can't even execute a
     * CLEAR statement.
     */
    UiGrabSpace();

    /* DoDlgLoadFile will set bdFileLoad */
    if (DoDlgLoadFile (&dlgFileOpen, sizeof (dlgFileOpen),      //[43][47]
                       cabiCABFileOpen)) {                      //[43][47]
        fDataMoved = FALSE;
        DoLoadFile ((cmdSwitches & CMD_SW_ED) ? LF_NewDoc : LF_NewProg, &bdFileLoad);   //[47]
        /* causes rsNew to get set, which causes WnReset to display
         * new register set in a list window */
        if (fDataMoved && uierr == 0)
            MsgBoxStd (MB_OK, MSG_DataMoved);
    }

    UiReleaseSpace ();   /* free temp space reserved by UiGrabSpace */

}

VOID NEAR CmdFileOpen ()
{
    LoadMerge ();
    fIsProgram = !(mrsCur.flags2 & FM2_NoPcode);        //[44]
    DrawDebugScr ();    // [41]
}

/***
*BOOL near DoSaveFile ()
*Purpose:
*       Actually save a module to disk.
*
*Entry:
*       mrsCur identifies the module to be saved.
*       mrsCur.ogNam identifies the filename to save it as.
*
*Exit:
*       returns FALSE if file saved successfully
*       uierr = error code if any.
*
*Exceptions:
*       None.
*******************************************************************************/
BOOL near DoSaveFile ()
{
    WORD errCode;
    WORD result;                                //[38]

    UiGrabSpace ();
    /* SaveFile() can generate synthetic DECLAREs that could steal
     * memory from our ability to execute a SYSTEM, CLEAR, or SETMEM stmt.
     */

    if (mrsCur.flags2 & (FM2_NoPcode | FM2_Include)) {
        /* DOCUMENT and $INCLUDE files should only be saved in ASCII format */
        mrsCur.flags2 |= FM2_AsciiLoaded;
    }
SaveRetry:
    if ((errCode = SaveFile()) != 0) {
        if ((result = RetryError(errCode)) == IDRETRY)  //[38] if retryable err
                                                //[38] put up retry/cancel box
            goto SaveRetry;   /* user wants to retry */

        if (result != IDCANCEL)                 //[38] if CANCEL not hit, put
            SetUiErr (errCode);                 //[38] up a message box later
        rsNew = grs.oMrsCur;    /* show offending mrs in list window */
    }
    UiReleaseSpace ();
    return (errCode);
}

/***
*STATICF(boolean) AskNotSavedInc ()
*Purpose:
*       Ask user if he wants to save modifed include files before saving
*       the current file.  This is so synthetic DECLAREs won't be generated
*       if they were already entered into a modifed $INCLUDE file.
*
*Entry:
*       None.
*
*Exit:
*       If NO, or if all files were saved OK, uierr = 0, returns TRUE.
*       If CANCEL, uierr = MSG_GoDirect, returns FALSE.
*       If I/O error, uierr = error code, returns FALSE.
*
*Exceptions:
*       None.
*******************************************************************************/
STATICF(boolean)
AskNotSavedInc ()
{
    short result;

    /* don't prompt if user is currently saving an INCLUDE file,
     * they never get synthetic DECLAREs anyway
     */
    if ((mrsCur.flags2 & FM2_Include) == 0) {
        /* prompt user with "Save modified $INCLUDE files first?" */
        if ((result = NotSavedIncSav ()) > 0) {
            /* some error occurred, or user pressed CANCEL - abort
             * uierr already set */
            return FALSE;
        }
        if (result == 0) {
            /* One or more files needed to be saved.
             * No error occurred saving INCLUDE files.
             * Re-parse all $INCLUDE lines to pick up changes
             * Ignore errors while re-parsing $INCLUDE lines, like file-not-found
             * We're just trying to prevent redundant synthetic declares
             */
            TxtReInclude ();
        } /* else user didn't need to, or chose not to save include files */
    }
    return TRUE;
}

/***
*boolean NEAR CmdFileSaveAs ()
*Purpose:
*       Prompt the user for a filename, then save current module.
*
*Entry:
*       mrsCur identifies the module to be saved.
*       mrsCur.ogName identifies the filename to save it as.
*
*Exit:
*       Returns TRUE if user didn't press CANCEL in Save dialog box, and
*       there was no I/O error.  If I/O error, uierr = error code.
*
*Exceptions:
*       None.
*******************************************************************************/
boolean NEAR CmdFileSaveAs ()
{
    WORD gr;

    oRsFileSave = grs.oMrsCur;  /* save oRs of module being saved */

    /* Ask user if he wants to save modified include files first.
     * This is so synthetic DECLAREs won't be generated if they
     * were already entered into a modified $INCLUDE file.
     * If CANCEL, MSG_GoDirect; if user said NO, FFFF;
     * if I/O error, error code; if files saved ok, ax=0
     */
    if (!AskNotSavedInc ())
        return FALSE;

    UiGrabSpace ();
    if (!BdAlloc (&bdFileSave, FILNAML, IT_NO_OWNERS)) {
        SetUiErrOm ();
        goto CmdFileSaveAsEnd;
    }

    if (!fogNamUntitled(mrsCur.ogNam)) {                                 //[49]
        bdFileSave.cbLogical = CopyOgNamPb(bdFileSave.pb, mrsCur.ogNam); //[25]
        DbAssert (bdFileSave.cbLogical < FILNAML);
    } else
        *(bdFileSave.pb) = 0;   //[25] ensure first byte is a NULL, so ChangeDir
                                //[25] will exit without doing anything drastic

    if (gr = DoDlgSaveFile (&dlgFileSave, sizeof (dlgFileSave), cabiCABFileSave)) {
        /* activate active module's register set */
        /* DialogBox() activates other oRs's */
        UiRsActivate (oRsFileSave);

        /* User didn't press CANCEL button */
        /* Assume ASCII save is desired */
        mrsCur.flags2 |= FM2_AsciiLoaded;
        if ((mrsCur.ogNam = OgNamOfPsd ((sd *) &bdFileSave)) == 0)  /* [2] */
             SetUiErrOm ();
        else {
            DrawDebugScr ();
            DbAssert (oRsFileSave == grs.oRsCur);
            gr = !DoSaveFile ();
        }
    }

    BdFree (&bdFileSave);
    /* caller's depend on this */
    DbAssert (!gr || oRsFileSave == grs.oRsCur);

CmdFileSaveAsEnd:
    UiReleaseSpace ();
    return (uierr == 0 && gr);
}

/***
*boolean NEAR CmdFileSave ()
*Purpose:
*       Save current file.
*
*Entry:
*       mrsCur identifies module to be saved.
*       mrsCur.ogName identifies file name to save it as.
*
*Exit:
*       Returns TRUE if user didn't press CANCEL in Save dialog box, and
*       there was no I/O error.  If I/O error, uierr = error code.
*
*Exceptions:
*       None.
*******************************************************************************/
boolean NEAR CmdFileSave ()
   {
   /* Ask user if he wants to save modified include files first.
    * This is so synthetic DECLAREs won't be generated if they
    * were already entered into a modified $INCLUDE file.
    * If CANCEL, MSG_GoDirect; if user said NO, FFFF;
    * if I/O error, error code; if files saved ok, ax=0
    */
   if (!AskNotSavedInc ())
      return FALSE;

   if (fogNamUntitled(mrsCur.ogNam)) { /* [49] */
      /* Prompt user for name of <Untitled> module. */
      return (CmdFileSaveAs ());
   }
   return (!DoSaveFile() && uierr == 0);
}

/***
*boolean NEAR CmdFileSaveAll (fInclude)
*Purpose:
*       Save all modified files.
*
*Entry:
*       fInclude        If true, only INCLUDE mrs's are saved.
*
*Exit:
*       Returns TRUE if user didn't press CANCEL in Save dialog box, and
*       there was no I/O error.  If I/O error, uierr = error code.
*
*Exceptions:
*       None.
*******************************************************************************/
boolean NEAR CmdFileSaveAll(fInclude)
boolean fInclude;
{
   BYTE pass;

  /* This function alters ps.bdpSrc, which edit mgr also
   * uses to keep its current line in.  EditMgrFlush1 tells edit
   * mgr to forget that ps.bdpSrc contains the current cached line.
   * This is done in DoMenu() for CmdFileSave[As], but since this
   * function is called from places other than DoMenu(), we need to
   * do it here. */
   EditMgrFlush1 ();

   /* First save all modified INCLUDE files, so synthetic DECLARE
    * statements won't be generated when other modules which include
    * the include files are saved.
    */
   for (pass = 0; ++pass < 3; ) {
      UiRsActivate (UNDEFINED);
      while (uierr == 0 && NextMrsFile_All () != UNDEFINED) {   //[39]
         if (mrsCur.flags2 & FM2_Modified) {
            /* during pass 1, just save $INCLUDE files */
            if (pass == 2 || (mrsCur.flags2 & FM2_Include)) {
               if (fogNamUntitled(mrsCur.ogNam)) {      //[49]
                  /* Make Untitled mrs visible in active window so user
                   * see's which mrs he's being prompted for.
                   */
                  WndAssignList ();
                  DoDrawDebugScr ();
               }
               if (!CmdFileSave ())
                  return FALSE;
            }
         }
      } /* while */
      if (fInclude)
         break;   /* caller just wanted to save INCLUDE files */
      if ((uierr = TxtReInclude ()) != 0)
         return FALSE;   /* errors occurred re-parsing $INCLUDE lines */
   }
   return TRUE;
}

/***
*VOID NEAR CmdFilePrint ()
*Purpose:
*       Display the print dialog box and print the requested portion of text.
*
*Entry:
*       None.
*
*Exit:
*       Trashes ldEMScratch buffer.
*
*Exceptions:
*       None.
*******************************************************************************/
VOID NEAR CmdFilePrint ()
{
    REG1                printScope;
    REG2                cLnCur;
    WORD                fhPrinter, cbRead, rgLns[2], singleLn = 0;
    TMC                 tmc;
    struct ef          *pef = (struct ef *) pwndAct->pefExtra;  // [36]
    WORD                FilePrintDefault;                       //[17]


//
// [QH1] Start
//
    if (cmdSwitches & CMD_SW_QHELP)
    {
        HCABFilePrintQH       hcabFilePrintQH;

        DbAssert (uierr == FALSE);
        hcabFilePrintQH = (HCABFilePrintQH) HcabAlloc (cabiCABFilePrintQH);
        /* [4] If HcabAlloc failed, return */
        if (uierr)
            return;

        if (fPrintToFile)
            (*hcabFilePrintQH)->oFilePrintQHDest = isFilePrintQHFile;
        else
            (*hcabFilePrintQH)->oFilePrintQHDest = isFilePrintQHPrinter;
        SzToCab (hcabFilePrintQH, szPrintDest, Iag(CABFilePrintQH,szPrintFile));

        tmc = TmcDoDlgFar (&dlgFilePrintQH, sizeof (dlgFilePrintQH), hcabFilePrintQH);

        if (tmc == tmcOK)
        {
            fOptionsChanged = TRUE;
            fPrintToFile = (bool)
                ((*hcabFilePrintQH)->oFilePrintQHDest == isFilePrintQHFile);
            if (fPrintToFile)
                SzFromCab (hcabFilePrintQH, szPrintDest, MAX_PATH-1,
                           Iag(CABFilePrintQH,szPrintFile));
        }
        FreeCab (hcabFilePrintQH);
        printScope = isFilePrintCurrentWindow;
    }
//
// [QH1] End
//
    else
    {
        HCABFilePrint       hcabFilePrint;

        if (pef->fSelection)                                        //[16]
            FilePrintDefault = isFilePrintSelectedText;             //[16]
        else if (pwndAct == &wndHelp || cmdSwitches & CMD_SW_ED)    //[50]
            FilePrintDefault = isFilePrintCurrentWindow;            //[50]
        else                                                        //[16]
            FilePrintDefault = isFilePrintCurrentModule;            //[16]

        DbAssert (uierr == FALSE);                                  //[22]
        hcabFilePrint = (HCABFilePrint) HcabAlloc (cabiCABFilePrint);
        /* [4] If HcabAlloc failed, return */
        if (uierr)
            return;

        (*hcabFilePrint)->oFilePrintWhat = FilePrintDefault;
        /* [5] Do Easy dialog if Easy Menu option is TRUE */

        if (pwndAct == &wndHelp)                               //[15]
            (*hcabFilePrint)->u.sab = sabPrintHelp;                 //[15]
        else                                                        //[15]
            (*hcabFilePrint)->u.sab = (cmdSwitches & CMD_SW_ED) ?           //[47]
                                       sabPrintQedit : sabPrintQbas;        //[42][47]
        tmc = TmcDoDlgFar (&dlgFilePrint, sizeof (dlgFilePrint), hcabFilePrint);
        printScope = (*hcabFilePrint)->oFilePrintWhat;
        FreeCab (hcabFilePrint);
    }


    rgLns[0] = 0;

    /* activate the proper rs and modify scope variables if necessary if cancel
     * was requested the case will exit the procedure via goto */
    UiRsActivateWnd ();

    /* If printer not available, exit */
    if (uierr || tmc == tmcCancel) goto justExit;

    switch (printScope) {
        case isFilePrintCurrentModule:
            UiRsActivate (grs.oMrsCur);
            break;
        case isFilePrintSelectedText :
            singleLn = GetSelText (ldEMScratch.prgch, ldEMScratch.cbMax);
            /* if there is no text selected exit routine */
            if (!SendMessage (pwndAct, EM_GETLINESEL, (WORD) rgLns, 0L) &&
                !singleLn)
                goto justExit;
            break;
    }

    /***
     *  - print scope globals should be set up as follows:
     *      begLn, endLn    line range, 0 and ??? unless a multiple line
     *                      selection is being printed.
     *                      begLn is rgLns [ 0 ], endLn is rgLns [ 1 ]
     *      singleLn        != 0 => single line selection, == 0 => not sls
     ***/

    /* set up the alphabetical procedure listing */
    if (UiAlphaORsBuild () == 0) {
        SetUiErrOm();
        goto justExit;
    }

    /* open up the printer for write access, and abort if we get an error */

    if ((fhPrinter = CreateFile (GetPrintDest())) == UNDEFINED)     // [QH1]
    {                                                               // [QH1]
        if (cmdSwitches & CMD_SW_QHELP)                             // [QH1]
        {                                                           // [QH1]
            if (fPrintToFile)                                       // [QH1]
                SetUiErr (ER_IOE);                                  // [QH1]
            else                                                    // [QH1]
                SetUiErr (ER_DF);                                   // [QH1]
        }                                                           // [QH1]
        else                                                        // [QH1]
            SetUiErr(ER_IOE);
        goto justExit;
    }

    // If printing to printer, check that it's connected ok.        // [QH1]
    if (cmdSwitches & CMD_SW_QHELP)                                 // [QH1]
    {                                                               // [QH1]
        if ((!fPrintToFile) && (!IsDeviceReady(iPrintPort)))        // [QH1]
        {                                                           // [QH1]
            SetUiErr (ER_DF);                                       // [QH1]
            goto justExit;                                          // [QH1]
        }                                                           // [QH1]
    }                                                               // [QH1]

    if (!fPrintToFile)                                         // [QH1]
        StatusLineMsg(-MSG_Prt1);   /* "Waiting for printer" */

    /* loop through each line in each prs in each mrs in print scope */
    do {
        do {
            DbAssert (grs.oRsCur != UNDEFINED);
            /* set up endLin if printScope isn't selected text so that cbGetLine
             * gets valid line offsets */
            if (printScope != isFilePrintSelectedText) {
                rgLns[1] = (pwndAct == &wndHelp) ?              //[50]
                             (WORD)SendHelpMsg(WM_HELPFILESIZE,0) ://[50]
                             LinesInBuf(grs.oRsCur);            //[50]
                if (rgLns[1]) rgLns[1]--;                       //[50] make 0 relative
            }
            for (cLnCur = rgLns[0]; cLnCur <= rgLns[1]; cLnCur++) {
                /* grab line from prs */
                if (singleLn)
                    cbRead = GetSelText (ldEMScratch.prgch, ldEMScratch.cbMax);
                else
                if (pwndAct == &wndHelp)                //[16]
                     cbRead = (ushort) SendMessage(pwndAct,WM_HELPLINE,cLnCur,  //[34]
                              MAKELONG(ldEMScratch.cbMax,ldEMScratch.prgch));   //[34]
                else                                    //[16]
                    cbRead = cbGetLineBuf (grs.oRsCur, cLnCur,
                                           ldEMScratch.cbMax, ldEMScratch.prgch);
                /* print line, if an error occurs set uierr to i/o error */
                if ((uierr = WriteFileErr (fhPrinter, ldEMScratch.prgch, cbRead))
                    == 0)
                   uierr = WriteFileErr (fhPrinter, "\015\012", 2);
                if (uierr != 0) {
                    SetUiErr (uierr);
                    if ((cmdSwitches & CMD_SW_QHELP) && !fPrintToFile) // [QH1]
                        SetUiErr (ER_DF);                              // [QH1]
                    goto closeAndExit;
                }

                StatusLineMsg (-MSG_Prt2);      /* "Printing" */

                fPollKeyboard = TRUE;
                PollKeyboard ();                // [33]
                if (fAbort)                     // [33]
                    goto closeAndExit;
            }
            WriteFileErr (fhPrinter, "\015\012", 2);
        }
        while (printScope != isFilePrintSelectedText &&
               printScope != isFilePrintCurrentWindow &&        //[50]
               NextAlphaPrs () != 0) ;
    }
    while (FALSE) ;

    if ((cmdSwitches & CMD_SW_QHELP) && !fPrintToFile)               // [QH1]
    {   //                                                           // [QH1]
        // Tell printer to flush/print the page                      // [QH1]
        //                                                           // [QH1]
        WriteFileErr (fhPrinter, "\014", 1);                         // [QH1]
    }                                                                // [QH1]

closeAndExit:
    CloseFileNear (fhPrinter);          //[32]
justExit:
    DoStatusMsg(pwndAct);   //[9] restore normal status line message
    return;
}                   /* end of CmdFilePrint */



// [QH1]
/****************************************************************************
*  IsDeviceReady() - Does the port have a printer hooked to it?
*
*  Entry
*     iPort - DEV_LPT1, DEV_LPT2, DEV_LPT3, DEV_COM1, DEV_COM2
*  Entry
*     TRUE if port is usable, FALSE if not.
*  NOTES
*     (Info taken from MSD 2.00a)
****************************************************************************/
BOOL IsDeviceReady (WORD iPort)
{
    WORD status;
    BOOL bReady = TRUE;

//
// BUGBUG a-emoryh 10/31/92 - May want to implement COM-port checking too
//

    if (iPort >= DEV_LPT1 && iPort <= DEV_LPT3)
    {
        iPort -= DEV_LPT1;
        status = _bios_printer (_PRINTER_STATUS, iPort, 0);

        bReady &= (status & 0x06) ? FALSE : TRUE;        // Port detected?
        bReady &= (status & 0x10) ? TRUE  : FALSE;       // Online?
        bReady &= (status & 0x20) ? FALSE : TRUE;        // Paper out?
        bReady &= (status & 0x08) ? FALSE : TRUE;        // IO error?
    }
    else       // If COM Port
    {
       bReady = TRUE;
    }
    return (bReady);
}



// [QH1]
/*****************************************************************************
*BOOL FAR FDlgFilePrintQH (dlm, tmc, wNew, wOld, wParam)
*Purpose:
*       Dialog procedure for FilePrintQHelp dialog
*
*Entry:
*Exit:
*       Return TRUE.
*****************************************************************************/
BOOL FAR FDlgFilePrintQH (WORD dlm, TMC tmc, WORD wNew, WORD wOld, WORD wParam)
{
    Unreferenced (wNew);
    Unreferenced (wOld);
    Unreferenced (wParam);

    switch (dlm)
    {
        case dlmInit:
            // Subclass the edit control, so we can trap up/down arrow keys
            // Yah, i know it's not a listbox, but this guy works anyway.
            pwndPrintEdit = PwndOfListbox(tmcFilePrintQHEdit);
            pfnOldWndProc = pwndPrintEdit->pfnWndProc;
            SetWindowProc (pwndPrintEdit, (PLFN_WNDPROC)EditSubProc);

            // Display current printer port selection
            SetTmcText (tmcFilePrintQHPort, aszDevices[iPrintPort]);
            break;

        case dlmTerm:
            // Make sure this guy gets set to NULL before exiting the dialog!
            pwndPrintEdit = NULL;
            break;

        case dlmClick:
            if (tmc == tmcPrintSetup)
            {   //
                // Display the PrinterSetup sub-dialog, which lets user select
                // printer port.
                //
                HCABPrintSetup  hcabPrintSetup;
                char *pszOldTitle;
                WORD  cbOldTitle;

                DbAssert (uierr == FALSE);
                hcabPrintSetup = (HCABPrintSetup) HcabAlloc (cabiCABPrintSetup);
                /* If HcabAlloc failed, return */
                if (uierr)
                    return TRUE;

                // Save previous dialog title
                pszOldTitle = szDialogTitle;
                cbOldTitle  = cbDialogTitle;

                // Set the new title
                szDialogTitle = szPrintSetup;               // Globals used by
                cbDialogTitle = 0;                          //   TmcDoDlgFar().

                (*hcabPrintSetup)->oPrintSetupPort = iPrintPort;

                tmc = TmcDoDlgFar(&dlgPrintSetup, sizeof(dlgPrintSetup),
                                  hcabPrintSetup);
                if (tmc == tmcOK)
                {
                    iPrintPort = (*hcabPrintSetup)->oPrintSetupPort;
                    // Change displayed port name in FilePrint dialog.
                    SetTmcText (tmcFilePrintQHPort, aszDevices[iPrintPort]);
                    fOptionsChanged = TRUE;
                }

                // Restore previous dialog title
                szDialogTitle = pszOldTitle;
                cbDialogTitle = cbOldTitle;

                FreeCab (hcabPrintSetup);
                break;
            }
            break;

        case dlmSetFocus:
            switch (tmc)
            {
                case tmcQHFile:
                    //
                    // Enable filename edit-control, and set focus to it
                    //
                    if (!FEnabledTmc(tmcFileName))
                    {
                        EnableTmc (tmcFilePrintQHEdit, TRUE);
                        EnableTmc (tmcFileName,        TRUE);
                    }
                    SetFocusTmc(tmcFilePrintQHEdit);
                    break;

                case tmcQHPrinter:
                    //
                    // Disable file edit-control, since printer was selected
                    //
                    if (FEnabledTmc(tmcFileName))
                    {
                        EnableTmc (tmcFilePrintQHEdit, FALSE);
                        EnableTmc (tmcFileName,        FALSE);
                    }
                    break;
            }
            break;
    }

    return (TRUE);
}



// [QH1]
/*****************************************************************************
*  EditSubProc() -
*     Subclasses the edit-control in QH FilePrint dialog, allowing the arrow
*     keys to move user to the other radio-buttons (since this edit-control is
*     bound to one of the radio buttons).
*  Entry:
*  Exit:
*****************************************************************************/
DWORD FAR EditSubProc (PWND pwnd, WORD message, WORD wParam, DWORD lParam)
{
    switch (message)
    {
        case WM_CHAR:
            switch (wParam)
            {
                case VK_UP:
                case VK_DOWN:
                    // Set focus to printer radio-button, and select it
                    SetFocusTmc (tmcQHPrinter);
                    SetTmcVal (tmcFilePrintQHDest, isFilePrintQHPrinter);
                    break;

                case VK_TAB:
                case TAB:
                    // SDM default tab behavior wasn't working for this edit,
                    // so we handle the tab ourselves.  Note that we have a
                    // special hack in DLGCORE.C to give us tab messages here.
                    if (HIWORD(lParam) & KK_SHIFT)
                        SetFocusTmc (tmcHelp);
                    else
                        SetFocusTmc (tmcOK);
                    return (0l);
            }
    }
    return ((*pfnOldWndProc)(pwnd,message,wParam,lParam));
}



/***
*bool far SetMrsMain
*Purpose:
*       Called by ForEachMrs.
*       Sets grs.oMrsMain to grs.oMrsCur and returns FALSE to stop ForEachMrs
*
*       New for revision [31]
*
*Entry:
*       grs.oMrsCur
*
*Exit:
*       grs.oMrsMain == grs.oMrsCur
*       return FALSE
*
*******************************************************************************/
boolean far SetAnyMrsMain() {
     grs.oMrsMain = grs.oMrsCur;
     return(FALSE);
}

/***
*VOID NEAR CmdFileExt ()
*Purpose:
*       Exit from program.
*
*Entry:
*       None.
*
*Exit:
*       Cmd buffer has "system" command to exit from program.
*
*Exceptions:
*       None.
*******************************************************************************/
VOID NEAR CmdFileExit()
{
   int result;                          //[30]

   result = NotSaved();                 //[30]
   if (result > 0)                      //[30]
      return;                           //[30]
   if (result < 0)                      //[30]
      ClrAllModified(TRUE);             //[30]

   //[31] We need a main module to execute the SYSTEM command.
   //     At this point we will absolutely not be comming back,
   //     so it does not matter which module is main.
   //     Make the main module oMrsCur so we don't get a `No main module'
   //     dialog.
   ForEachMrs(SetAnyMrsMain);           //[31]
   DoCmd("system");
   fExiting = TRUE;                     //[40]
}



// [QH1]
/***  GetPrintDest() -- Retrieves current destination for printing
 *
 *  User has a choice of printing to LPT1, LPT2, LPT3, COM1, COM2, or a file.
 *
 *  Entry
 *      None
 *
 *  Exit
 *      Returns ptr to name of print dest - ("LPT1", "COM2", "C:\FOO.TXT", etc)
 *      This ptr is READ-ONLY!!
 */
char *GetPrintDest (void)
{
    char *pszDest;

    if (cmdSwitches & CMD_SW_QHELP)
    {
        if (fPrintToFile)
            pszDest = szPrintDest;
        else
            pszDest = aszDevices[iPrintPort];
    }
    else
    {   //
        // If not in QHelp mode, revert to old LPT1 print-dest
        //
        pszDest = aszDevices[DEV_LPT1];
    }
    return (pszDest);
}





