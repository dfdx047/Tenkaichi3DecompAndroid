/* The automatic update (gs/net_update.c): check for a newer release, download and verify it, then install it safely
   from the render thread (gs/ui.cpp). gs/net.c uses the online gate while an update is ready. */
#ifndef PORT_NET_UPDATE_H
#define PORT_NET_UPDATE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

enum {
    UPD_IDLE,        /* not checked yet */
    UPD_CHECKING,    /* asking GitHub */
    UPD_NONE,        /* this is the newest (or no answer, or a "dev" build) */
    UPD_AVAILABLE,   /* a newer signed release can be installed */
    UPD_DOWNLOADING, /* archive and signature are downloading in the background */
    UPD_VERIFYING,   /* its size and SHA-256 (and signature) */
    UPD_READY,       /* verified: waiting for a safe main-menu install */
    UPD_INSTALLING,  /* files are being swapped */
    UPD_INSTALLED,   /* swapped: the new version is started and this one closes (Update_Relaunch) */
    UPD_FAILED       /* the download, the check or the install failed: `why`; an install put everything back */
};

typedef struct {
    int state;
    const char *current;  /* this build's number ("0.1.39", "dev") */
    char latest[32];      /* the newest release's ("0.1.39.2"), "" not known */
    char page[256];       /* its page on GitHub */
    char why[512];        /* UPD_FAILED: what went wrong */
    int canInstall;       /* the update can be installed by the game (else: only the page) */
    char cannot[256];     /* ... and if not, why not */
    long long got, total; /* UPD_DOWNLOADING: bytes so far, of */
} UpdateStatus;

const char *Update_Version(void);          /* this build's full number */
void Update_Cleanup(void);                 /* at start: the files the last update left (*.old) go */
void Update_Check(void);                   /* asks GitHub once (in a thread of its own) */
void Update_Recheck(void);                 /* manual retry; leaves an active download/install alone */
void Update_Status(UpdateStatus *out);
const char *Update_Notes(void);            /* the release's notes (its body on GitHub), "" if none */
void Update_NotesFor(const char *locale,char *out,size_t size); /* bounded localized release summary */
void Update_Download(void);                /* downloads the verified release in a thread */
void Update_Install(void);                 /* swaps files in a thread -> UPD_INSTALLED or UPD_FAILED */
int Update_StartTextureMaintenance(int locale); /* caller must be in an offline menu; 1: close this game */
int Update_Relaunch(void);                 /* UPD_INSTALLED: starts the new version (the caller then exits); 0 failed */
int Update_FetchOnlineRoute(char *out,size_t capacity); /* background caller, fixed signed-authority URL */
void Update_SetOnlineRequired(int required);
int Update_OnlineRequired(void);
int Update_BlocksOnline(void);             /* an update is under way: no new online session until it is installed */
const char *Update_OnlineNote(void);       /* ... the line that says so (English) */
/* What happened to the update installed before this start (read once): 0 none, 1 this is it (`version`). */
int Update_LastResult(const char **version);

#ifdef __cplusplus
}
#endif
#endif
