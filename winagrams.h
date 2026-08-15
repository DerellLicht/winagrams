extern HINSTANCE g_hinst ;

#define  WM_DO_COMM_TASK    (WM_USER + 101)
#define  WM_COMM_TASK_DONE  (WM_USER + 102)

//  winagrams.cpp
extern uint cyClient ;

void status_message(char *msgstr);
void status_message(char *msgstr, uint idx);
void clear_message_area(void);
void update_listview(void);

//  about.cpp
BOOL CmdAbout(HWND hwnd);

//  config.cpp
extern uint dbg_flags ;
extern uint window_top ;
extern uint window_left ;
extern uint client_height ;

LRESULT save_cfg_file(void);
LRESULT init_config(void);

