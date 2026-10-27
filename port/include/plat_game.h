#ifndef PORT_GAME_H
#define PORT_GAME_H
#ifdef __cplusplus
extern "C" {
#endif
/* Disc profiles keep native code independent of each disc's resource numbering. */
int PortGame_IsBT4(void);
int PortGame_Region(void); /* 0 USA/legacy, 1 Europe, 2 Japan */
const char *PortGame_DefaultLanguage(void);
int PortGame_VolumeCount(int volume);
int PortGame_CharacterCount(void);
int PortGame_IsCharacter(int chara);
const char *PortGame_CharacterName(int chara);
int PortGame_StageCount(void);
int PortGame_MusicFile(int track);
int PortGame_StreamFile(int original);
int PortGame_Costumes(int chara);
int PortGame_Model(int chara, int costume, int damaged);
int PortGame_ModelCharacter(int id);
void PortGame_RepairModelBody(void *body, int size, int model);
int PortGame_Animation(int chara);
int PortGame_Combat(int chara);
int PortGame_Voice(int chara, int japanese);
int PortGame_VoiceLine(int chara, int line, int japanese);
int PortGame_Member(int chara, int side);
int PortGame_Partition(int id);
int PortGame_FileIndex(int id);
void *PortGame_Metadata(int chara, int kind);
void *PortGame_CustomSlot(void *original, int slot);
int PortGame_SaveCustom(void);
void PortGame_ResetAnimations(void);
void *PortGame_AnimationBuffer(void *original);
int PortGame_AnimationCapacity(void);
int PortGame_CheckAnimation(void *data, int size);
#ifdef __cplusplus
}
#endif
#endif
