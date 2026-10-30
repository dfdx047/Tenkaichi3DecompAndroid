/* The automatic update (gs/net_update.c): is there a newer release, download it, check it, install it over the
   game's folder and start the new version. Called from the settings window's thread (gs/ui.cpp) and, for the online
   gate, from gs/net.c. */
#ifndef PORT_NET_UPDATE_H
#define PORT_NET_UPDATE_H
#ifdef __cplusplus
extern "C" {
#endif

enum {
    UPD_IDLE,        /* not asked (yet), or "Check for updates" is off */
    UPD_CHECKING,    /* asking GitHub */
    UPD_NONE,        /* this is the newest (or no answer, or a "dev" build) */
    UPD_AVAILABLE,   /* a newer release: Update now / Later / What's new */
    UPD_DOWNLOADING, /* "Update now" was pressed: in the background */
    UPD_VERIFYING,   /* its size and SHA-256 (and signature) */
    UPD_READY,       /* verified: "Restart now" */
    UPD_INSTALLING,  /* "Restart now" was pressed: the files are being swapped */
    UPD_INSTALLED,   /* swapped: the new version is started and this one closes (Update_Relaunch) */
    UPD_FAILED       /* the download, the check or the install failed: `why`; an install put everything back */
};

typedef struct {
    int state;
    const char *current;  /* this build's number ("0.1.38.9", "dev") */
    char latest[32];      /* the newest release's ("0.1.38.10"), "" not known */
    char page[256];       /* its page on GitHub */
    char why[512];        /* UPD_FAILED: what went wrong */
    int canInstall;       /* the update can be installed by the game (else: only the page) */
    char cannot[256];     /* ... and if not, why not */
    long long got, total; /* UPD_DOWNLOADING: bytes so far, of */
} UpdateStatus;

const char *Update_Version(void);          /* this build's full number */
void Update_Cleanup(void);                 /* at start: the files the last update left (*.old) go */
void Update_Check(void);                   /* asks GitHub once (in a thread of its own) */
void Update_Status(UpdateStatus *out);
const char *Update_Notes(void);            /* the release's notes (its body on GitHub), "" if none */
void Update_Download(void);                /* "Update now" */
void Update_Install(void);                 /* "Restart now": swaps the files (in a thread) -> UPD_INSTALLED or UPD_FAILED */
int Update_Relaunch(void);                 /* UPD_INSTALLED: starts the new version (the caller then exits); 0 failed */
int Update_BlocksOnline(void);             /* an update is under way: no new online session until it is installed */
const char *Update_OnlineNote(void);       /* ... the line that says so (English) */
/* What happened to the update installed before this start (read once): 0 none, 1 this is it (`version`). */
int Update_LastResult(const char **version);

#ifdef __cplusplus
}
#endif
#endif
