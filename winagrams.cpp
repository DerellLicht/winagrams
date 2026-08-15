//**********************************************************************
//  Copyright (c) 2009-2026  Daniel D Miller
//  winagrams.exe - A Windows anagram program
//  
//  Written by:   Daniel D. Miller
//**********************************************************************

// char const * const VerNum = "V1.07" ;
static char szClassName[] = "WinaGrams" ;

#include <windows.h>
#include <memory>
#include <vector>
#include <tchar.h>

#include "version.h"
#include "resource.h"
#include "common.h"
#include "commonw.h"
#include "winagrams.h"
#include "statbar.h"
#include "cterminal.h"
#include "winmsgs.h"

//lint -esym(715, hwnd, private_data, message, wParam, lParam)

//  anagram.cpp
extern bool read_word_list(char *wordlist_filename);
extern char *get_dict_filename(void);

extern uint min_word_len ;

//*******************************************************************
// #define WM_CHANGEUISTATE 0x0127

//  thread.cpp
extern HWND hwndCommTask ;
extern void start_anagram_thread(LPVOID iValue);

//*******************************************************************
#define  KEY_ACTIVE     1  

HINSTANCE g_hinst = 0;

HWND hwndMainDialog = NULL ;
// static HWND hwndStatusBar ;
// static CStatusBar *MainStatusBar = NULL;
static std::unique_ptr<CStatusBar> MainStatusBar {};

// CTerminal *myTerminal = NULL;
std::unique_ptr<CTerminal> myTerminal {};

static HWND hwndMaxChars ;
// static HWND hwndMaxDevsSpin ;

static uint cxClient = 0;
uint cyClient = 0;

//*******************************************************************
// Claude 08/14/26 - smallest listview height (pixels) we'll allow the
// live-resize floor to shrink down to, so a few rows stay visible/usable
// no matter how far the user drags the bottom edge up.
#define  MIN_LISTVIEW_VISIBLE_DY   80

// Claude 08/14/26
// MINMAXINFO's ptMinTrackSize/ptMaxTrackSize are WINDOW (outer) dimensions,
// not client-area dimensions -- but cxClient/cyClient come from GetClientRect(),
// which excludes the caption/border. Pinning ptMinTrackSize.x==ptMaxTrackSize.x
// directly to cxClient tells Windows "the whole window, borders included, is
// only as wide as the client area" -- i.e. a few pixels *too narrow* by exactly
// the border width. That's the "shrinks by a few pixels on first width-drag"
// symptom. Fix: measure the real window-minus-client delta once at init, and
// add it back in whenever a track size is derived from a client dimension.
static int dx_frame = 0;   //  window width  - client width
static int dy_frame = 0;   //  window height - client height

// Claude 08/14/26 - term_window_height tracks the LISTVIEW's current height
// and gets recalculated on every resize (see resize_font_dialog). It is NOT
// a safe floor for WM_GETMINMAXINFO, because by the time a live drag is
// underway its value has already moved. min_application_window_height is
// the true floor: computed once in do_init_dialog from the fixed pieces
// (top controls + a minimum usable listview height + status bar + frame)
// and never modified afterward.
static uint min_application_window_height = 0;

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
//  small font-dependent layout fudge factor; shared by do_init_dialog's
//  min-height calculation and resize_font_dialog's live layout so the two
//  stay consistent with each other.
//****************************************************************************
static int get_dy_offset(void)
{
   if (!are_normal_fonts_active()) {
      return 2 ;
   }
   return 4 ;
}

//****************************************************************************
static uint get_terminal_top(void)
{
   static uint local_ctrl_top = 0 ;
   if (local_ctrl_top == 0) {
      local_ctrl_top = get_bottom_line(hwndMainDialog, IDC_WORDS) ;
      local_ctrl_top += 3 ;
      // syslog("CommPort: ctrl_top = %u, or %u\n", local_ctrl_top, win_ctrl_top+3) ;
   }
   return local_ctrl_top ;
}  //lint !e715

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

//****************************************************************************
static uint read_max_chars(void)
{
   char tempbfr[11] ;
   GetWindowText(hwndMaxChars, tempbfr, 10);
   tempbfr[10] = 0 ;
   char *tptr = strip_leading_spaces(tempbfr) ;
   return (uint) atoi(tptr) ; // NOLINT(bugprone-unchecked-string-to-number-conversion)
}

//*******************************************************************
static bool do_init_dialog(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   char msgstr[81] ;
   RECT myRect ;
   wsprintf(msgstr, "%s %s", szClassName, VerNum) ;
   SetWindowText(hwnd, msgstr) ;

   get_monitor_dimens(hwnd);
   hwndMainDialog = hwnd ;

   init_config();
   // syslog("config: window pos: %ux%u, dy: %u\n", window_left, window_top, client_height) ;
   // config: window pos: 1408x224, dy: 511
   
   //  read configuration *before* creating edit fields
   // read_config_file() ;
   // GetWindowRect(hwnd, &myRect) ;
   GetClientRect(hwnd, &myRect) ;
   cxClient = (uint) (myRect.right - myRect.left) ;
   cyClient = (uint) (myRect.bottom - myRect.top) ;

   // Claude 08/14/26 - measure actual border/caption size once, from live
   // window+client rects, rather than guessing at SM_CXFRAME/SM_CYCAPTION
   // (which can be wrong under theming/DPI). Used to convert client-size
   // values into the window-size values WM_GETMINMAXINFO actually wants.
   {
   RECT winRect ;
   GetWindowRect(hwnd, &winRect) ;
   dx_frame = (winRect.right - winRect.left) - (int) cxClient ;
   dy_frame = (winRect.bottom - winRect.top) - (int) cyClient ;
   // syslog("frame delta: dx_frame=%d, dy_frame=%d\n", dx_frame, dy_frame) ;
   }

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
   MyCreateUpDownControl(
      // WS_CHILD|WS_VISIBLE|UDS_SETBUDDYINT|UDS_ALIGNRIGHT|WS_BORDER,
      // 0, 0, 0, 0,
      hwnd,             // parent handle
      IDC_MAXDEV_SPIN,  // updown ID
      g_hinst,          // instance handle
      hwndMaxChars,     // buddy, if stand alone set to NULL
      10,               // max value
      1,                // min value
      min_word_len);    // start of value

   //****************************************************************
   //  create/configure status bar first
   //****************************************************************
   MainStatusBar = std::make_unique<CStatusBar>(hwnd);
   MainStatusBar->MoveToBottom(cxClient, cyClient) ;
   //  define status-bar parts
   {
   int sbparts[3];
   sbparts[0] = (int) (5 * cxClient / 10) ;
   // sbparts[1] = (int) (8 * cxClient / 10) ;
   sbparts[1] = -1;
   MainStatusBar->SetParts(2, &sbparts[0]);
   }
   
   // Claude 08/14/26 - the real, permanent floor for WM_GETMINMAXINFO.
   // Same shape as resize_font_dialog's live layout math, just solved for
   // the smallest acceptable listview height (MIN_LISTVIEW_VISIBLE_DY)
   // instead of the current one. Computed once, here, and never touched
   // again -- see the comment on the variable itself.
   min_application_window_height = get_terminal_top() + MIN_LISTVIEW_VISIBLE_DY
      + MainStatusBar->height() + (uint) get_dy_offset() + (uint) dy_frame ;

   //****************************************************************
   //  create listview class second, needs status-bar height
   //****************************************************************
   {
   uint ctrl_bottom = get_bottom_line(hwnd, IDC_WORDS) + 5 ;
   uint lvdy = cyClient - ctrl_bottom - MainStatusBar->height() ;

   myTerminal = std::make_unique<CTerminal>(hwnd, IDC_TERMINAL, g_hinst, 
      0, ctrl_bottom, cxClient-1, lvdy,
      LVL_STY_VIRTUAL | LVL_STY_EX_GRIDLINES | LVL_STY_NO_HEADER );
   myTerminal->set_terminal_font("Courier New", 100, EZ_ATTR_BOLD) ;
   myTerminal->lview_assign_column_headers() ;
   }
   
   //  update other screen data
   SetDlgItemText(hwnd, IDC_WORDS, get_dict_filename()) ;

   //  start up the separate thread which will handle anagram calcs
   start_anagram_thread(NULL) ;
   // main_timer_id = SetTimer(hwnd, IDT_TIMER_MAIN, 100, (TIMERPROC) NULL) ;
   return true ;
}

//*******************************************************************
static bool do_command(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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
         
      case IDC_ABOUT:
         CmdAbout(hwnd);
         return true;

      }  //lint !e744
   } 
   return false ;
}

//*******************************************************************
static bool do_notify(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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
static bool do_close(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   DestroyWindow(hwnd);
   return true ;
}

//*******************************************************************
static bool do_destroy(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   PostQuitMessage(0);
   return true ;
}

//********************************************************************************************
//  okay, this function originally gave inaccurate results,
//  because the rectangle passed by WM_SIZING was from GetWindowRect(),
//  which included the unwanted border area, rather than from
//  GetClientRect(), which works with get_bottom_line().
//********************************************************************************************
static void resize_font_dialog()
{
   RECT myRect ;
   // syslog("resize terminal, drag=%s\n", (resize_on_drag) ? "true" : "false") ;

   //  if resizing on drag-and-drop, re-read main-dialog size
   // BOOL gcr_ok = 
   GetClientRect(hwndMainDialog, &myRect) ;
   // new_window_width  = (uint) (myRect.right - myRect.left) ;
   uint new_window_height = (uint) (myRect.bottom - myRect.top) ;
   // syslog("resize: cyClient: %u, new_window_height: %u, rect=(%ld,%ld,%ld,%ld), gcr_ok=%d, err=%lu\n",
   //    cyClient, new_window_height,
   //    (long) myRect.left, (long) myRect.top, (long) myRect.right, (long) myRect.bottom,
   //    (int) gcr_ok, gcr_ok ? 0ul : (unsigned long) GetLastError());

   if (cyClient == new_window_height  ||  new_window_height == 0) {
       return ;
   }

   cyClient = new_window_height ;

   int dy_offset = get_dy_offset() ;

   MainStatusBar->MoveToBottom(cxClient, cyClient-1) ;
   //  resize the terminal (cols)
   int dyi = (int) cyClient - dy_offset - (int) get_terminal_top() - MainStatusBar->height() ;
   myTerminal->resize(cxClient-1, dyi); //  dialog is actually drawn a few pixels too small for text
   
   save_cfg_file();
}

//*************************************************************************************
// Claude: WM_SIZE — this is the only place you actually move/resize child controls. 
// Dialogs don't auto-relayout children on resize; you compute the height delta 
// and grow the listview by exactly that much, leaving the top controls alone.
//*************************************************************************************
// static const char *size_type_name(WPARAM wParam)
// {
//    switch (wParam) {
//    case SIZE_RESTORED:  return "SIZE_RESTORED" ;
//    case SIZE_MINIMIZED: return "SIZE_MINIMIZED" ;
//    case SIZE_MAXIMIZED: return "SIZE_MAXIMIZED" ;
//    case SIZE_MAXSHOW:   return "SIZE_MAXSHOW" ;
//    case SIZE_MAXHIDE:   return "SIZE_MAXHIDE" ;
//    default:             return "SIZE_??" ;
//    }
// }

static bool do_size(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   // syslog("do_size: type=%s, lParam dx=%d dy=%d, IsIconic=%d\n",
   //    size_type_name(wParam), (int) LOWORD(lParam), (int) HIWORD(lParam),
   //    (int) IsIconic(hwnd));
   resize_font_dialog();
   return true ;
}

//*************************************************************************************
// Claude: WM_SIZING itself isn't needed for this shape of problem — 
// it's for constraining to an aspect ratio or snapping to a grid during the drag. 
// Locking width via WM_GETMINMAXINFO is simpler and sufficient here.
//*************************************************************************************
// static 
bool do_sizing(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   //  handle main-dialog resizing
   switch (message) {
   case WM_SIZING:
      switch (wParam) {
      case WMSZ_BOTTOMLEFT:
      case WMSZ_BOTTOMRIGHT:
      case WMSZ_TOPLEFT:
      case WMSZ_TOPRIGHT:
      case WMSZ_LEFT:
      case WMSZ_RIGHT:
      case WMSZ_TOP:
      case WMSZ_BOTTOM:
         resize_font_dialog();
         return true;

      default:
         break;
      }
      break;
   }  //lint !e744
   return false ;
}

//*************************************************************************************
//  DDM 01/29/17 - These minima are not actually working;
//  Perhaps this is due to Windowblinds ??
//  Yes; this works fine on standard Windows 7
//*************************************************************************************
//  Claude 08/12/26
//  WM_GETMINMAXINFO — this is where you lock the width and bound the height.
//  Setting ptMinTrackSize.x == ptMaxTrackSize.x (both equal to the dialog's current
//  width) is enough to make the left/right borders un-draggable — you don't need
//  WM_SIZING for that. Height min comes from your own "smallest useful layout"
//  calculation; height max comes from SystemParametersInfo(SPI_GETWORKAREA, ...) 
//  so the dialog can't be dragged off the bottom of the screen.
//*************************************************************************************
static bool do_getminmaxinfo(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   LPMINMAXINFO lpTemp = (LPMINMAXINFO) lParam;
   POINT        ptTemp;
   // syslog("set minimum to %ux%u\n", cxClient, cyClient);
   
   //  Claude 08/14/26 - cxClient is a CLIENT-area size; ptMinTrackSize/
   //  ptMaxTrackSize must be WINDOW sizes (border+caption included), so add
   //  the frame delta captured at init. Width is pinned min==max to lock
   //  horizontal resize; that pin must land on the real current window
   //  width or Windows will fight the live window size every time this
   //  fires and can degenerate the rect mid-drag.
   //
   //  Height floor comes from min_application_window_height.
   //  min_application_window_height is computed once in do_init_dialog 
   //  and never changes, which is what a track-size floor needs to be.
   
   //  set minimum dimensions
   ptTemp.x = (LONG) cxClient + dx_frame ;
   ptTemp.y = (LONG) min_application_window_height ;
   lpTemp->ptMinTrackSize = ptTemp;
   // uint dxmin = ptTemp.x ;
   // uint dymin = ptTemp.y ;
   //  set maximum dimensions
   ptTemp.x = (LONG) cxClient + dx_frame ;
   ptTemp.y = get_screen_height() ;
   lpTemp->ptMaxTrackSize = ptTemp;
   // lpTemp->ptMaxSize = ptTemp;
   // syslog("gmmi: dxmin: %u, dxmax: %u, dymin: %u, dymax: %ld\n", dxmin, ptTemp.x, dymin, (long) ptTemp.y);
   return true ;
}

//*******************************************************************
static INT_PTR CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   if (dbg_flags != 0) {
      switch (message) {
      //  list messages to be ignored
      // case WM_MOUSEMOVE:
      // case WM_NCHITTEST:
      // case WM_NOTIFY:
      case WM_NCMOUSEMOVE:
      case WM_SETCURSOR:
      case WM_COMMAND:  //  prints its own msgs below
         break;
      default:
         syslog("TOP [%s]\n", lookup_winmsg_name(message)) ;
         break;
      }
   }
   
   switch (message) {
   case WM_INITDIALOG:
      do_init_dialog(hwnd, message, wParam, lParam) ;
      return TRUE;

   case WM_GETMINMAXINFO:
      do_getminmaxinfo(hwnd, message, wParam, lParam) ;
      return FALSE;

   case WM_EXITSIZEMOVE:
      {
      RECT rect ;
      GetWindowRect(hwnd, &rect);
      window_top = rect.top ;
      window_left = rect.left ;
      save_cfg_file();
      }
      break ;
   
   case WM_WINDOWPOSCHANGING:
      {
      WINDOWPOS* pos = (WINDOWPOS*)lParam;
      if (!(pos->flags & SWP_NOSIZE))
         pos->cx = cxClient-1;   // hardcoded, no private_data needed
      break;
      }      
      return TRUE ;

   case WM_SIZE:
      do_size(hwnd, message, wParam, lParam) ;
      return TRUE ;

   case WM_NOTIFY:
      do_notify(hwnd, message, wParam, lParam) ;
      return TRUE ;

   case WM_COMMAND:
      do_command(hwnd, message, wParam, lParam) ;
      return TRUE ;

   case WM_CLOSE:
      do_close(hwnd, message, wParam, lParam) ;
      return TRUE;
      
   case WM_DESTROY:
      do_destroy(hwnd, message, wParam, lParam) ;
      return TRUE;

   default:         
      return FALSE;
   }
   return FALSE;
}

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

