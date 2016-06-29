//**********************************************************************
//  Copyright (c) 2009-2012  Daniel D Miller
//  winagrams.exe - A Win32 anagram program
//  
//  Written by:   Daniel D. Miller
//  
//  Last Update:  01/17/10 14:33
//****************************************************************************
//  The anagram search will be executed in a separate thread.
//  This will allow the main program message handler to continue
//  functioning, so we can offer an abort function for the user.
//****************************************************************************
#include <windows.h>
#include <stdio.h>
#include <time.h>
#ifdef _lint
#include <stdlib.h>             //  atoi()
#endif

#include "resource.h"
#include "common.h"
#include "commonw.h"
#include "winagrams.h"
#include "wthread.h"

//******************************************************************
//  interesting...  If I enable USE_GETMESSAGE_HWND, and use hwnd in GetMessage(),
//  then PostQuitMessage() does not terminate the message loop!!
//  Actually, this problem only occurs if the message handler is a Window.
//  Dialogs terminate just fine in this case.
//******************************************************************
#define  USE_GETMESSAGE_HWND  1

HWND hwndCommTask = NULL ;
extern HWND hwndMainDialog ;
extern void find_anagrams(HWND hwnd);

//******************************************************************
static BOOL CALLBACK AnagramProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
   // int result ;
   // char msgstr[91] ;

   //***************************************************
   //  debug: log all windows messages
   //***************************************************
//    if (dbg_flags & DBG_WINMSGS) {
//       switch (msg) {
//       //  list messages to be ignored
//       case WM_NCHITTEST:
//       case WM_SETCURSOR:
//       case WM_MOUSEMOVE:
//       case WM_NCMOUSEMOVE:
//       case WM_COMMAND:
//          break;
//       default:
//          // syslog("CommTask [%s]\n", get_winmsg_name(result));
//          syslog("CommTask [%s]\n", lookup_winmsg_name(msg)) ;
//          break;
//       }
//    }
   //********************************************************************
   //  Windows message handler for this dialog
   //********************************************************************
   switch(msg) {
   case WM_INITDIALOG:
      hwndCommTask = hwnd ;
      return TRUE ;

   //  process "comm message pending" message
   case WM_DO_COMM_TASK:
      find_anagrams(hwndMainDialog) ;
      PostMessage(hwndMainDialog, WM_COMM_TASK_DONE, (WPARAM) 0, 0);
      return TRUE ;

   case WM_CLOSE:
      DestroyWindow(hwnd);
      return FALSE;

   case WM_DESTROY:
      hwndCommTask = 0 ;
      PostQuitMessage(0) ;
      return FALSE;

   default:
      return FALSE;
   }

   return FALSE;  //lint !e527
}  //lint !e715

//****************************************************************************
static DWORD WINAPI AnagramThreadFunc(LPVOID iValue)
{
   //  CreateDialogParam note:
   //  Use NULL for the parent hwnd, if the dialog is being created in
   //  a separate thread.  Otherwise, the two message queues will 
   //  interact with each other.  In particular, anything which stops the
   //  child message queue will stop the parent as well!!
   HWND hwnd = CreateDialogParam(g_hinst, MAKEINTRESOURCE(IDD_ANAGRAM_TASK), 
      NULL, AnagramProc, (LPARAM) iValue) ;
   if (hwnd == NULL) {
      syslog("CreateDialog (thread): [%u] %s\n", GetLastError(), get_system_message()) ;
      return 0;
   }
   MSG Msg;
#ifdef  USE_GETMESSAGE_HWND
   while(GetMessage(&Msg, hwnd,0,0)) {
#else      
   while(GetMessage(&Msg, NULL,0,0)) {
#endif
      // this_port->commtask_count++ ;
      if(!IsDialogMessage(hwnd, &Msg)) {
          TranslateMessage(&Msg);
          DispatchMessage(&Msg);
      }
   }
   syslog("CommTask thread returning\n") ;
   return 0;
}

//****************************************************************************
// static CThread const *AnagramThread = NULL ;

void start_anagram_thread(LPVOID iValue)
{
   // AnagramThread = 
   new CThread(AnagramThreadFunc, (LPVOID) iValue, NULL) ;
}

