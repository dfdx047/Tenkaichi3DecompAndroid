// The setup's own installer for a release folder (native.cpp). It works on a thread and reports in the event lines
// of install.py.
#ifndef PORT_SETUP_NATIVE_H
#define PORT_SETUP_NATIVE_H
#include <string>

// root: the folder with bt3 and bt3.dat, ends in '/'. iso: the USA disc image (empty: the game is set up already);
// eu: the European one for Spanish, German, French and Italian (empty: none). Either may be left out, not both.
void Native_Start(const std::string &root, const std::string &iso, const std::string &eu,int mask=1,
                  const std::string &texturePack="", const std::string &ptbrPack="");
// Texture-only maintenance: never reads an ISO, rewrites language settings or runs a test fight.
void Native_StartTextures(const std::string &root);
bool Native_Poll(std::string &line);                               // the next event line, if there is one
bool Native_Running(void);
void Native_Cancel(void);
void Native_Join(void);                                            // at exit: cancel and wait
bool Native_Installed(const std::string &root);                    // the game data is unpacked already
bool Native_EuInstalled(const std::string &root);                  // the European disc's entries are there
bool Native_BundledEnglish(const std::string &root);
bool Native_EnglishInstalled(const std::string &root);
bool Native_SpanishInstalled(const std::string &root);

#endif
