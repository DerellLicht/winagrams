extern HINSTANCE g_hinst ;

#define  WM_DO_COMM_TASK    (WM_USER + 101)
#define  WM_COMM_TASK_DONE  (WM_USER + 102)

//  winagrams.cpp
void status_message(char *msgstr);
void status_message(char *msgstr, uint idx);
void clear_message_area(void);
void update_listview(void);


