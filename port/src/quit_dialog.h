#ifndef PORT_QUIT_DIALOG_H
#define PORT_QUIT_DIALOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Port-only exit question drawn by the game's own memory-card Dialog. */
void PortQuit_Request(void);
void PortQuit_Draw(void);
int PortQuit_Active(void);
int PortQuit_Failed(void);
int PortQuit_Ready(void);
int PortQuit_Cursor(void);
void PortQuit_Action(int action);
void PortQuit_Dismiss(void);

#define PORT_QUIT_MOVE 1
#define PORT_QUIT_CONFIRM 2
#define PORT_QUIT_CANCEL 3

#ifdef __cplusplus
}
#endif

#endif
