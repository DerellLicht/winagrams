//**********************************************************************
//  Copyright (c) 2009-2014  Daniel D Miller
//  winagrams.exe - A Win32 anagram program
//  
//  Written by:   Daniel D. Miller
//**********************************************************************
//  version		changes
//	 =======		======================================
// 	1.00		Initial release
//    1.01     > Add counter to show status of string construction.
//               Otherwise, "password manager" takes several minutes to 
//               complete, and the program appears hung while this runs.
//             > Add separate thread to handle search, so messages are
//               still processed during search.  We also want to add an
//               ABORT button to the program, for long searches.
//    1.02     Add field to modify minimum word length
//    1.03     Reverse results list, so largest words are first.
//             This seems more likely to provide most-interesting results.
//    1.04     > Convert to using new classes
//             > Create new string-list class
//    1.05     Switch to generic functions for creating status bar and
//             up/down control
//    1.06     Add exclusion list, after ! at end of word list.
//             Example:
//                 led zeppelin kiss ! zip zen
//****************************************************************************

static char const * const VerNum = "V1.06" ;
static char szClassName[] = "WinaGrams" ;

#include <windows.h>
#ifdef _lint
#include <stdlib.h>  //  atoi() 
#endif
#include <tchar.h>

#include "resource.h"
#include "common.h"
#include "commonw.h"
#include "winagrams.h"
#include "statbar.h"
#include "cterminal.h"

//lint -esym(715, hwnd, private_data, message, wParam, lParam)

//  anagram.cpp
extern bool read_word_list(char *wordlist_filename);
extern char *get_dict_filename(void);

extern uint min_word_len ;

//*************************************************************************
#define  KEY_ACTIVE     1  

HINSTANCE g_hinst = 0;

// INITCOMMONCONTROLSEX icex;    // Structure for control initialization.

// CStrList *StrList = NULL;
// static CVListView *VListView = NULL ;
CTerminal *myTerminal = NULL;

// static HWND hwndStatusBar ;
static CStatusBar *MainStatusBar = NULL;

static HWND hwndMaxChars ;
// static HWND hwndMaxDevsSpin ;

static uint cxClient = 0;
static uint cyClient = 0;   //  subtrace height of status bar

//*******************************************************************
void status_message(char *msgstr)
{
   MainStatusBar->show_message(msgstr);
}

//*******************************************************************
void status_message(char *msgstr, uint idx)
{
   MainStatusBar->show_message(idx, msgstr);
}

//****************************************************************************
void update_listview(void)
{
   myTerminal->listview_update(myTerminal->get_element_count()-1);
}

//****************************************************************************
void clear_message_area(void)
{
   myTerminal->clear_message_area() ;
}

//****************************************************************************
//  This function handles the WM_NOTIFY:LVN_GETDISPINFO message
//****************************************************************************
static void vlview_get_terminal_entry(LPARAM lParam)
{
   LV_DISPINFO *lpdi = (LV_DISPINFO *) lParam;

   if (lpdi->item.mask & LVIF_TEXT) {
      static char szString[MAX_PATH];
      term_lview_item_p lvptr = myTerminal->find_element(lpdi->item.iItem) ;
      if (lvptr == NULL) {
         wsprintf(szString, _T("listview element %d not found [%u total]"), 
            lpdi->item.iItem, 
            myTerminal->get_element_count());
         strcpy(lpdi->item.pszText, szString);
      } else {
         char *str = lvptr->msg ;
         if (str == NULL) {
            wsprintf(szString, _T("listview element %d string not found [%u total]"), 
               lpdi->item.iItem, 
               myTerminal->get_element_count());
            lpdi->item.pszText = szString ;
            // this_term->term_fgnd = this_term->err_fgnd ;
            // this_term->term_bgnd = this_term->err_bgnd ;
         } else {
            // syslog("LVN_GETDISPINFO: element %u/%u: [%s]\n", lvptr->idx, lpdi->item.iItem, str) ;
            lpdi->item.pszText = str ;
            // this_term->term_fgnd = lvptr->fgnd ;
            // this_term->term_bgnd = lvptr->bgnd ;
         }
      }
   }
   
}

//****************************************************************************
static void copy_selected_rows(void)
{
   char msgstr[81] ;
   // uint selcount = ListView_GetSelectedCount(this_term->hwndRxData) ;
   // uint selcount = SendMessage(this_term->hwndRxData, LVM_GETSELECTEDCOUNT,0,0) ;
   uint selcount = myTerminal->get_selected_count() ;
   int nCurItem = -1 ;
   uint elements_found = 0 ;
   while (1) {
      // nCurItem = ListView_GetNextItem(this_term->hwndRxData, nCurItem, LVNI_SELECTED);
      // nCurItem = SendMessage(this_term->hwndRxData, LVM_GETNEXTITEM, nCurItem, MAKELPARAM((LVNI_SELECTED), 0)) ;
      nCurItem = myTerminal->get_next_listview_index(nCurItem) ;
      if (nCurItem < 0)
         break;
      // syslog("mark %d\n", nCurItem) ;
      myTerminal->mark_element(nCurItem) ;
      elements_found++ ;
   }
   if (elements_found == selcount) {
      myTerminal->copy_elements_to_clipboard() ;
      wsprintf(msgstr, "%u rows copied", selcount) ;
   } else {
      wsprintf(msgstr, "found %u of %u elements", elements_found, selcount) ;
   }
   status_message(msgstr) ;
   myTerminal->clear_marked_elements() ;
}

//*******************************************************************
//  handlers for message window
//*******************************************************************

//*******************************************************************
// static void process_comm_task_done(HWND hwnd)
// {
// }  //lint !e715

//****************************************************************************
static uint read_max_chars(void)
{
   char tempbfr[11] ;
   GetWindowText(hwndMaxChars, tempbfr, 10);
   tempbfr[10] = 0 ;
   char *tptr = strip_leading_spaces(tempbfr) ;
   return (uint) atoi(tptr) ;
}

//*******************************************************************
// #define WM_CHANGEUISTATE 0x0127
HWND hwndMainDialog ;

//  thread.cpp
extern HWND hwndCommTask ;
extern void start_anagram_thread(LPVOID iValue);

//*******************************************************************
static bool do_init_dialog(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LPVOID private_data)
{
   char msgstr[81] ;
   RECT myRect ;
   wsprintf(msgstr, "%s %s", szClassName, VerNum) ;
   SetWindowText(hwnd, msgstr) ;
   //  the following should enable the busy cursor,
   //  but it doesn't actually work!!
   SetClassLong(hwnd,GCL_HCURSOR,(long) 0);  //  disable class cursor 

   hwndMainDialog = hwnd ;
   //  read configuration *before* creating edit fields
   // read_config_file() ;
   // GetWindowRect(hwnd, &myRect) ;
   GetClientRect(hwnd, &myRect) ;
   cxClient = (uint) (myRect.right - myRect.left) ;
   cyClient = (uint) (myRect.bottom - myRect.top) ;

   //**********************************************************
   //  do other config tasks *after* creating fields,
   //  so we can display status messages.
   //**********************************************************
   SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM) LoadIcon(g_hinst, MAKEINTRESOURCE(IDI_APPICON)));
   SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) LoadIcon(g_hinst, MAKEINTRESOURCE(IDI_APPICON)));

   //****************************************************************
   //  open max_devs fields
   //****************************************************************
   hwndMaxChars = GetDlgItem(hwnd, IDC_MAX_CHARS) ;

   //  now, create the spin control for this field
   // hwndMaxDevsSpin = CreateUpDownControl(
   MyCreateUpDownControl(
      // WS_CHILD|WS_VISIBLE|UDS_SETBUDDYINT|UDS_ALIGNRIGHT|WS_BORDER,
      // 0, 0, 0, 0,
      hwnd,           //parent handle
      IDC_MAXDEV_SPIN,    //updown ID
      g_hinst,        //instance handle
      hwndMaxChars,           //buddy, if stand alone set to NULL
      10,                        //max value
      1,                         //min value
      min_word_len); //start of value

   //****************************************************************
   //  create empty string list
   //****************************************************************
   // myTerminal = new CStrList() ;

   //****************************************************************
   //  create/configure status bar first
   //****************************************************************
   // hwndStatusBar = InitStatusBar (hwnd);
   MainStatusBar = new CStatusBar(hwnd) ;
   MainStatusBar->MoveToBottom(cxClient, cyClient) ;

   //  re-position status-bar parts
   {
   int sbparts[3];
   sbparts[0] = (int) (5 * cxClient / 10) ;
   // sbparts[1] = (int) (8 * cxClient / 10) ;
   sbparts[1] = -1;
   MainStatusBar->SetParts(2, &sbparts[0]);
   }
   
   //****************************************************************
   //  create listview class second, needs status-bar height
   //****************************************************************
   {
   uint ctrl_bottom = get_bottom_line(hwnd, IDC_WORDS) + 5 ;
   uint lvdy = cyClient - ctrl_bottom - MainStatusBar->height() ;

   myTerminal = new CTerminal(hwnd, IDC_TERMINAL, g_hinst, 
      0, ctrl_bottom, cxClient-1, lvdy,
      LVL_STY_VIRTUAL | LVL_STY_EX_GRIDLINES | LVL_STY_NO_HEADER );
   myTerminal->set_terminal_font("Courier New", 100, EZ_ATTR_BOLD) ;
   myTerminal->lview_assign_column_headers() ;
   }

   //  update other screen data
   SetDlgItemText(hwnd, IDC_WORDS, get_dict_filename()) ;

   //  start up the separate thread which will handle 
   start_anagram_thread(NULL) ;
   // main_timer_id = SetTimer(hwnd, IDT_TIMER_MAIN, 100, (TIMERPROC) NULL) ;
   return true ;
}

//*******************************************************************
static bool do_command(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LPVOID private_data)
{
   DWORD cmd = HIWORD (wParam) ;
   DWORD target = LOWORD(wParam) ;
   // putf(&this_term, "WM_COMMAND: cmd=%u, target=%u", cmd, target) ;
   // If a button is clicked...
   if (cmd == BN_CLICKED) {
      switch (target) {
      case IDM_DO_ANAGRAMS:
         min_word_len = read_max_chars() ;
         PostMessage(hwndCommTask, WM_DO_COMM_TASK, (WPARAM) 0, 0) ;
         return true;

      case IDM_READ_DICT:
         return true;
      }  //lint !e744
   } 
   return false ;
}

//*******************************************************************
// static bool do_comm_task_done(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LPVOID private_data)
// {
//    process_comm_task_done(hwnd) ;
//    // SetWindowPos (this_port->hwndCport, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
//    return true ;
// }

//*******************************************************************
static bool do_notify(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LPVOID private_data)
{
   int msg_code = (int) ((NMHDR FAR *) lParam)->code ;
   switch (msg_code) {

   //**********************************************************
   //  terminal listview notifications
   //**********************************************************
   case LVN_GETDISPINFO:
      // get_terminal_entry(&this_term, lParam) ;
      vlview_get_terminal_entry(lParam) ;
      return true;

   // typedef struct tagLVKEYDOWN {
   //     NMHDR hdr;
   //     WORD wVKey;   //  Virtual key code
   //     UINT flags;   //  this member must always be zero
   // } NMLVKEYDOWN, *LPNMLVKEYDOWN;
   case LVN_KEYDOWN:
     // syslog("LVN_KEYDOWN\n") ;
      {
      LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN) lParam ;
      // if (dbg_flags & DBG_WINMSGS)
      //    syslog("WM_NOTIFY: LVN_KEYDOWN: key=%u\n", pnkd->wVKey) ;

      //  Odd note: though I can detect pressing of Control (etc),
      //  I don't know how to detect RELEASE!!
      //  So Control-C and C both just report C - I previously saw
      //  "CONTROL pressed", but I don't know if it is still pressed
      //  when I see later chars.
      if (pnkd->wVKey == 'C') {
         SHORT lcontrol = GetKeyState(VK_LCONTROL) ;
         SHORT rcontrol = GetKeyState(VK_RCONTROL) ;
         if (lcontrol & KEY_ACTIVE  ||  rcontrol & KEY_ACTIVE) {
            // syslog("copy_selected_rows: not yet implemented\n") ;
            // copy_selected_rows(&this_term) ;
            copy_selected_rows() ;
         }
      }
      }
      return true;

   default:
      // if (dbg_flags & DBG_WINMSGS)
      //    syslog("Cport WM_NOTIFY: [%d] %s\n", msg_code, lookup_winmsg_name(msg_code)) ;
      return false;
   }
}

//*******************************************************************
static bool do_close(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LPVOID private_data)
{
   DestroyWindow(hwnd);
   return true ;
}

//*******************************************************************
static bool do_destroy(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LPVOID private_data)
{
   PostQuitMessage(0);
   return true ;
}

//*******************************************************************
// typedef struct winproc_table_s {
//    uint win_code ;
//    bool (*winproc_func)(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, LPVOID private_data) ;
// } winproc_table_t ;

static winproc_table_t const winproc_table[] = {
{ WM_INITDIALOG,     do_init_dialog },
{ WM_COMMAND,        do_command },
// { WM_COMM_TASK_DONE, do_comm_task_done },
{ WM_NOTIFY,         do_notify },
{ WM_CLOSE,          do_close },
{ WM_DESTROY,        do_destroy },

{ 0, NULL } } ;

//*******************************************************************
static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   uint idx ;
   for (idx=0; winproc_table[idx].win_code != 0; idx++) {
      if (winproc_table[idx].win_code == message) {
         return (*winproc_table[idx].winproc_func)(hwnd, message, wParam, lParam, NULL) ;
      }
   }
   return false;
}  //lint !e715

//***********************************************************************
static BOOL WeAreAlone(LPSTR szName)
{
   HANDLE hMutex = CreateMutex (NULL, TRUE, szName);
   if (GetLastError() == ERROR_ALREADY_EXISTS)
   {
      CloseHandle(hMutex);
      return FALSE;
   }
   return TRUE;
}

//*********************************************************************
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpszArgument, int nFunsterStil)
{
   if (!WeAreAlone (szClassName)) {
      MessageBox(NULL, "WinaGrams is already running!!", "collision", MB_OK | MB_ICONEXCLAMATION) ;
      return 0;
   }

   g_hinst = hInstance;
   load_exec_filename() ;     //  get our executable name

   //  build one-time network tables
   // load_led_images() ;        //  load our image list
   read_word_list(NULL);

   //  create the main application
   HWND hwnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, (DLGPROC) WndProc);
   if (hwnd == NULL) {
      // Notified your about the failure
      syslog("CreateDialog (main): %s [%u]\n", get_system_message(), GetLastError()) ;
      // Set the return value
      return FALSE;
   }
   ShowWindow (hwnd, SW_SHOW) ;
   UpdateWindow(hwnd);

   MSG msg ;
   while (GetMessage (&msg, NULL, 0, 0)) {
      if (!IsDialogMessage(hwnd, &msg)) {
         TranslateMessage (&msg) ;
         DispatchMessage (&msg) ;
      }
   }
   return (int) msg.wParam ;
}  //lint !e715

