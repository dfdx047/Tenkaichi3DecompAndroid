// The settings window of the PC build: Dear ImGui over the finished picture. F1 opens and closes it, or START + SELECT
// of a controller, or the key and button of its Controls list (gs_input.c: PortInput_Hotkey).
// Everything it shows lives elsewhere (ui.h): the renderer's settings in gs_gpu.c, the bindings in gs_input.c.
// It runs on the render thread, which owns the window and its events.
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "imgui.h"
#include "imgui_internal.h" // ImGuiTabBar: the settings window as wide as its tabs
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_opengl3.h"
#ifdef _WIN32
#include "imgui_impl_dx9.h"
static void *sD3D9;
extern "C" void *GsDx9_StripTexture(const void *, int, int);
extern "C" void GsDx9_StripTextureFree(void *);
#endif
extern "C" int GsGl_DirectXVersion(void);
#include "ui.h"
#include "presentation.h"
#include "namefont.h"
#include "../port_release.h"
#include "menu_text.h"
#include "game_profile.h"
extern "C" int GsDraw_PerformanceMode(void);
extern "C" int Port_SpecialMoveCameras(int change);
#include "menu_translation.h"
#include "online_language.h"
#include "../quit_dialog.h"
#include "net_chat.h"
#include "voice.h"
#include "net_lobby.h"
#include "net_update.h"
#include "update_notes.h"

static int sMeter = -1; // the meter (frame rate, connection): -1 not read yet
static bool meter_on(void);
static int sGL; // 1: the OpenGL back end draws the window (ImGui's OpenGL3 backend, no SDL GPU device)
static char sGpuName[256];

extern "C" {
extern volatile int gPortNetMenuRequest;
void Port_NetOptions(int rollback, int delay); // gs/net.c
void Port_NetRulesSet(int type, int dp, int time); // gs/net.c: the host's battle type, DP limit row, time limit
void Port_NetSpeedSet(int percent);                // ... and the match's speed, 100..200 (set before the rules)
int Port_NetSpeedStandard(void);                   // what a quick match runs at
void Port_NetNameSet(const char *name);         // gs/net.c: this player's name for the match
int Port_IsWide(void);                         // plat_stub.c: the picture is wider than 4:3
int Port_AspectMilli(void);
extern "C" int gPortTboTyping;                     // gs_input.c: a keypad of Tenkaichi Budokai Online takes text
extern "C" int PortInput_TboEvent(const SDL_Event *ev);
extern "C" void PortInput_TboClipboard(void);      // ... and its "Copy Code": onto the clipboard, from this thread
int Port_GameMode(void);                       // a11y_game.c: the game's progress mode (4 the main menu, 80 the port's screens)
int Port_UpdateMenuScreen(void);               // gs_marker.c: last outer menu actually drawn
extern int gPortMenuMode;
const char *Port_OnlineBlocked(void);          // gs/net.c: an update is under way, no new online session (the line), or NULL
int Lobby_Active(void);                        // gs/net_lobby.c: this copy is in a lobby
int Port_Menus43(void);                       // plat_stub.c: the menus are shown at 4:3 in a wider picture
int Port_LateInput(void);                     // plat_stub.c: "Low input lag", the blanks are waited for before the pad read
void Port_LateInputSet(int on);
void Port_Menus43Set(int on);
int Port_MenusWide(void);                     // plat_stub.c: the menu layout "Widescreen" (beta)
void Port_MenusWideSet(int on);
int Port_HudLayout(void);                      // gs_draw.c: 2 = the HUD is about the middle of the screen
int GsDraw_HudShown(void);                     // ... and 1 while the game is drawing the HUD's side panels
const char *Port_NetName(int player);          // the name of the player of side 0 or 1 ("" outside a match)
int Port_HudSwap(void);                        // the two sides of the HUD are in each other's places on this copy
int Port_NetActive(void);
int Port_NetMe(void);                          // gs/net.c: this copy's side, 0 or 1
void *BtlChar_FindByObjId(int objId);          // the game: the fighter of a side, NULL outside a fight
int Port_NetStats(int *out);                   // gs/net.c: the meter's numbers of an online match
int Port_TagNetwork(void),Port_TagPing(void),Port_TagLocalBattle(void);
int Port_LobbyOther(void);
extern unsigned gPortLiveBlanks;               // plat_stub.c: vertical blanks the game has gone through
int Port_RollCan(void);                        // gs/state.c
const char *Port_FileRoot(void);               // plat_file.c: the game's data folder
unsigned GsGl_StripTexture(const void *rgba, int w, int h); // gs_gl.c
void GsGl_StripTextureFree(unsigned id);
const char *PortStages_Dir(void);                          // plat_stages.c
const char *PortStages_Name(int index);
int PortStages_Count(void);
const char *PortSongs_Dir(void);                           // plat_songs.c
const char *PortSongs_Name(int index);
int PortSongs_Count(void);
int Port_Setting(const char *name, int def);   // plat_settings.c
void Port_SettingSave(const char *name, int value);
void Port_SettingText(const char *name, const char *def, char *out, int size);
void Port_SettingTextSave(const char *name, const char *value);
void Port_SettingsWrite(void);
int Port_ClashSquare(void);                    // gs_input.c: Square counts as a direction in clashes
void Port_ClashSquareSet(int on);
int Port_ClashStick(void);
void Port_ClashStickSet(int on);
int Port_ClashSquareX(void);                   // what a press is worth, in quarters of a point (4, 6, 7, 8)
void Port_ClashSquareXSet(int quarters);
extern int gsInterp;                           // gs_draw.c: in-between pictures on (1) / off (0); -1 before the first frame
int Port_TagBattleRenderActive(void);
void Port_SettingsWrite(void);
int Port_LobbyStart(int host, const char *address, int port); // gs/net.c
int Port_LobbyStartCode(int host, const char *code);         // the same with a room code (the matchmaking service)
int Port_LobbyStartQuick(int queue);                         // a quick match: 0 single, 1 team, 2..4 DP battle of 10 / 15 / 20
void Port_LobbyQueuesAsk(void);                              // asks the service how many wait in each queue
void Port_LobbyHelloAsk(void);                               // asks it whether this release is new enough
int Port_LobbyRelease(int *min, int *latest);                // this release's number; the oldest let in and the newest (0: not known)
int Port_LobbyQueueWaiting(int queue);                       // ... as last heard, -1 if not known
const char *Port_LobbyCode(void);                            // the host's room code, "" until it is known
const char *Port_LobbyError(void);                           // why the matchmaking failed (state -3)
int Port_LobbyRelayed(void);                                 // the other player was reached through the relay
int Port_LobbyPoll(void);
int Port_LobbyCheckingData(void);                           // a start waits for this game's gameplay fingerprint
const char *Port_LobbyQuickRefusal(void);                   // why no quick match: NULL none, "" data being checked
void Port_LobbyCancel(void);
void Port_LobbyLaunch(void);
int Port_NetSpectating(void);                                // this copy watches a match
void Port_NetSpecLeave(void);                                // ... stop watching (at the game's next blank)
void Port_NetSpecViewSet(int view);                          // ... 0 player 1's view, 1 player 2's, 2 both (split)
int Port_NetSpecView(void);
int Port_NetSpecStatus(int *out);                            // ... catching up (%), waiting, desync blank
int Port_NetWatchers(void);                                  // how many watch this match (the players)
const char *Port_NetWatcher(int i);                          // ... their names
}
static bool sReady, sOpen;
static MenuTranslationOption sLanguages[256];
static int sLanguageCount, sLanguageChoice;
static bool sLanguageRestartPending;
static int sF1Language = -1;                 // independent of the installed disc's text and voice profile
static bool sUpdPromptQueued, sUpdPromptOpen, sUpdPromptDismissed;
static volatile int sSaving;
static bool sNet;                              // the open window is the online screen (Dragon Net Battle), not the settings
// A spectator's own pause menu is the game's (src/menu/port_tbo.c, PortTbo_SpecDraw): Esc, or the test's moment, asks
// for it here; the controller's START and the key bound to it reach it through the pad.
extern "C" volatile int gPortSpecMenuAsk;
volatile int gPortSpecMenuAsk;
extern "C" void Port_UiNotice(const char *text);
static void spec_view_next(void);               // watching a match: the next view (player 1, player 2, both)
void ModsUi_Tab(void);                          // gs/mods_ui.cpp: the settings window's Mods tab
void ModsUi_Draw(void);                         // ... the script mods' shapes over the picture
extern "C" int Mods_HudActive(void);            // gs/mods.c: they drew something at the last blank
static const char *spec_view_name(int view);
static int sForceTab = -1;                     // BT3_UI_OPEN=<tab>: open at start on that tab (testing)
static int sCapKind, sCapPlayer, sCapAction;   // waiting for a key (1) or a controller button (2) to bind
static Uint64 sCapUntil;                       // ... until then: a controller has no button that is not a binding, so the wait ends by itself
static bool sQuit;                             // the question before the game closes is up
static bool sQuitNo;                           // ... and Esc answered it: no
static bool sPadHold;                          // the controller does not work the window: a binding is waited for, or its button is still down
static int sTab;                               // the settings tab that is open (the shoulder buttons go to its neighbours)
static bool sPadTab;                           // the shoulder buttons changed the tab this frame
static SDL_GPUDevice *sDevice;

static int f1_language(void) {
    char saved[16];
    if(sF1Language==-2)return Port_GameProfileLanguage();
    if (sF1Language >= 0) return sF1Language;
    Port_SettingText("f1_language", "", saved, sizeof(saved));
    if (!strcmp(saved, "es")) sF1Language = 0;
    else if (!strcmp(saved, "en")) sF1Language = 1;
    else if (!strcmp(saved, "pt")) sF1Language = 2;
    else {sF1Language=-2;return Port_GameProfileLanguage();}
    return sF1Language;
}

static void f1_language_set(int language) {
    static const char *const locales[] = {"es", "en", "pt"};
    if (language < 0 || language >= (int)(sizeof(locales) / sizeof(locales[0]))) return;
    sF1Language = language;
    Port_SettingTextSave("f1_language", locales[language]);
    Port_SettingsWrite();
}

static const char *f1_text(const char *english) {
    struct Row { const char *en, *es, *pt; };
    static const Row rows[] = {
#include "mods_locale.inc"
        {"Settings", "Ajustes", "Configurações"},
        {"Video", "Vídeo", "Vídeo"},
        {"Effects", "Efectos", "Efeitos"},
        {"Audio", "Audio", "Áudio"},
        {"UI", "Interfaz", "Interface"},
        {"Controls", "Controles", "Controles"},
        {"Cheats", "Trucos", "Trapaças"},
        {"Misc", "Varios", "Outros"},
        {"Online", "En línea", "Online"},
        {"Spectator", "Espectador", "Espectador"},
        {"Exit game", "Salir del juego", "Sair do jogo"},
        {"D-pad: move   Cross / A: choose   Circle / B: back   L1 / R1: tab", "Cruceta: mover   Cruz / A: elegir   Círculo / B: volver   L1 / R1: pestaña", "Direcional: mover   X / A: escolher   Círculo / B: voltar   L1 / R1: aba"},
        {"Menu language", "Idioma del menú", "Idioma do menu"},
        {"Game language", "Idioma del juego", "Idioma do jogo"},
        {"Spanish", "Español", "Espanhol"},
        {"English", "Inglés", "Inglês"},
        {"Portuguese", "Portugués", "Português"},
        {"Language saved for the next launch.", "Idioma guardado para el próximo inicio.", "Idioma salvo para a próxima inicialização."},
        {"Full disc profile: text and voices. Install each ISO once using the launcher. The startup selector confirms your choice before the game begins.", "Perfil completo del disco: textos y voces. Instala cada ISO una vez con el instalador. El selector inicial confirma tu elección antes de empezar.", "Perfil completo do disco: textos e vozes. Instale cada ISO uma vez pelo instalador. A seleção inicial confirma a escolha antes do jogo começar."},
        {"Menu font", "Fuente de los menús", "Fonte dos menus"},
        {"Battle font", "Fuente de combate", "Fonte de batalha"},
        {"Dialogue / UI font", "Fuente de diálogos e interfaz", "Fonte de diálogos e interface"},
        {"HUD Style", "Estilo del HUD", "Estilo do HUD"},
        {"Side bars", "Barras laterales", "Barras laterais"},
        {"Menus", "Menús", "Menus"},
        {"Stretched", "Estirados", "Esticados"},
        {"Widescreen (beta)", "Panorámico (beta)", "Widescreen (beta)"},
        {"Menu display on a picture wider than 4:3. Stretched fills the window. 4:3 keeps side bars. Widescreen (beta) anchors menus and HUD to the wider view while preserving artwork proportions.", "Menús en una imagen más ancha que 4:3. Estirado ocupa toda la ventana. 4:3 conserva las barras laterales. Panorámico (beta) adapta la posición de los menús y el HUD conservando las proporciones de sus texturas.", "Menus em uma imagem mais larga que 4:3. Esticado ocupa toda a janela. 4:3 mantém as barras laterais. Widescreen (beta) adapta a posição dos menus e do HUD preservando as proporções das texturas."},
        {"Original", "Original", "Original"},
        {"Black", "Negro", "Preto"},
        {"Border", "Borde", "Moldura"},
        {"Fighter panels", "Paneles de luchadores", "Painéis dos lutadores"},
        {"Name font", "Fuente del nombre", "Fonte do nome"},
        {"Form font", "Fuente de la transformación", "Fonte da forma"},
        {"BT4 original, en los bordes", "BT4 original, en los bordes", "BT4 original, nas bordas"},
        {"BT4 original, at the edges", "BT4 original, en los bordes", "BT4 original, nas bordas"},
        {"BT4 original, centered", "BT4 original, centrado", "BT4 original, centralizado"},
        {"Extended health bars", "Barras extendidas", "Barras de vida estendidas"},
        {"Show HUD", "Mostrar HUD", "Mostrar HUD"},
        {"Hide HUD", "Ocultar HUD", "Ocultar HUD"},
        {"Game camera", "Cámara del juego", "Câmera do jogo"},
        {"Free camera", "Cámara libre", "Câmera livre"},
        {"Follow fighter", "Seguir luchador", "Seguir lutador"},
        {"Fight overview", "Vista general del combate", "Visão geral da batalha"},
        {"Fighter", "Luchador", "Lutador"},
        {"Camera speed", "Velocidad de cámara", "Velocidade da câmera"},
        {"Camera zoom", "Zoom de cámara", "Zoom da câmera"},
        {"Reset camera", "Restablecer cámara", "Redefinir câmera"},
        {"Close F1 to control the camera. WASD: move; Q/E: descend/ascend; arrows: turn; Shift: accelerate; +/-: zoom.", "Cierra F1 para controlar la cámara. WASD: mover; Q/E: bajar/subir; flechas: girar; Shift: acelerar; +/-: zoom.", "Feche F1 para controlar a câmera. WASD: mover; Q/E: descer/subir; setas: girar; Shift: acelerar; +/-: zoom."},
        {"Controller: left stick moves, right stick turns, L1/R1 descend/ascend, L2/R2 zoom, L3 accelerates.", "Mando: palanca izquierda para mover, derecha para girar, L1/R1 para bajar/subir, L2/R2 para zoom y L3 para acelerar.", "Controle: analógico esquerdo move, direito gira, L1/R1 descem/sobem, L2/R2 ajustam o zoom e L3 acelera."},
        {"Camera controls replace your local fighter controls during battle. Start still opens the pause menu.", "Durante el combate, los controles de cámara sustituyen a los de tu luchador local. Start sigue abriendo el menú de pausa.", "Durante a batalha, os comandos da câmera substituem os do seu lutador local. Start continua abrindo o menu de pausa."},
        {"Spanish (text and voices)", "Español (textos y voces)", "Espanhol (texto e vozes)"},
        {"English (text and voices)", "Inglés (textos y voces)", "Inglês (texto e vozes)"},
        {"New update available. Restart required.", "Nueva actualización disponible, requiere reiniciar.", "Nova atualização disponível. É necessário reiniciar."},
        {"Restart now", "Reiniciar ahora", "Reiniciar agora"},
        {"Online play requires this update. Offline play remains available.", "Necesitas esta actualización para jugar online. Puedes seguir jugando sin conexión.", "Esta atualização é necessária para jogar online. Você pode continuar jogando offline."},
        {"A game update is required for online play. Return to a menu to download it and restart.", "Necesitas actualizar el juego para jugar online. Vuelve a un menú para descargarla y reiniciar.", "É necessário atualizar o jogo para jogar online. Volte a um menu para baixar e reiniciar."},
        {"Later", "Más tarde", "Mais tarde"},
        {"This update is ready. Restart now to apply it, or choose Later to install it another time.", "La actualización está lista. Reinicia ahora para aplicarla o elige Más tarde para instalarla en otro momento.", "A atualização está pronta. Reinicie agora para aplicá-la ou escolha Mais tarde para instalá-la depois."},
        {"Downloading automatically", "Descargando automáticamente", "Baixando automaticamente"},
        {"Restart required to finish installing the update.", "Debes reiniciar para terminar de instalar la actualización.", "É necessário reiniciar para concluir a instalação da atualização."},
        {"Changes the language of this settings menu immediately. It does not change BT4 text or voices.", "Cambia de inmediato el idioma de este menú de ajustes. No modifica los textos ni las voces de BT4.", "Altera imediatamente o idioma deste menu de configurações. Não muda os textos nem as vozes de BT4."},
        {"Original uses the game's sprites. Other fonts replace matching menu labels immediately.\nBattle announcements have their own setting below.", "Original usa los sprites del juego. Las demás fuentes reemplazan de inmediato las etiquetas compatibles.\nLos anuncios de combate tienen su propia opción más abajo.", "Original usa os sprites do jogo. As outras fontes substituem imediatamente os rótulos compatíveis.\nOs anúncios de batalha têm uma opção própria abaixo."},
        {"Choose original battle announcements and HUD text, or any battle font listed in fonts.tsv.", "Elige los anuncios y textos del HUD originales o una fuente de combate incluida en fonts.tsv.", "Escolha os anúncios e textos do HUD originais ou uma fonte de batalha listada em fonts.tsv."},
        {"Replace the shared font used by dialogue, subtitles and ordinary UI text.\nOriginal keeps the game's glyphs. Text placement, colors, shadows and icons are preserved.", "Reemplaza la fuente compartida de diálogos, subtítulos y textos de interfaz.\nOriginal conserva los glifos del juego. Se mantienen posiciones, colores, sombras e iconos.", "Substitui a fonte compartilhada de diálogos, legendas e textos da interface.\nOriginal mantém os caracteres do jogo. Posições, cores, sombras e ícones são preservados."},
        {"Where a fight's health bars and gauges sit in a picture wider than 4:3.\nLong health bars: the bars reach from the edges to the timer.\nIn the middle: the original 4:3 arrangement, next to the timer.", "Posición de las barras de vida y medidores en una imagen más ancha que 4:3.\nBarras extendidas: van desde los bordes hasta el temporizador.\nCentrado: disposición original 4:3, junto al temporizador.", "Posição das barras de vida e medidores em uma imagem mais larga que 4:3.\nBarras longas: vão das bordas até o cronômetro.\nCentralizado: disposição original 4:3, ao lado do cronômetro."},
        {"What fills the sides where the picture is narrower than the window:\na 4:3 picture, or the menus with \"Menus: 4:3\", in a wide window.\nFighter panels: the border, and in a fight each side's fighter with its name and form.\nYour own: a folder \"bars\" next to the game with trim.png (the strip along the\npicture, kept in proportion) and fill.png (stretched over the rest). Read at start.", "Qué aparece en los laterales cuando la imagen es más estrecha que la ventana:\nla imagen 4:3 o los menús con \"Menús: 4:3\" en una ventana panorámica.\nPaneles de luchadores: el borde y, durante el combate, el luchador de cada lado con su nombre y transformación.\nPersonalizadas: carpeta \"bars\" junto al juego con trim.png (borde proporcional) y fill.png (relleno estirado). Se leen al iniciar.", "O que aparece nas laterais quando a imagem é mais estreita que a janela:\na imagem 4:3 ou os menus com \"Menus: 4:3\" em uma janela widescreen.\nPainéis dos lutadores: a moldura e, na batalha, o lutador de cada lado com nome e forma.\nPersonalizadas: pasta \"bars\" ao lado do jogo com trim.png (moldura proporcional) e fill.png (preenchimento esticado). Lida ao iniciar."},
        {"Hide what's new", "Ocultar novedades", "Ocultar novidades"},
        {"What's new", "Novedades", "Novidades"},
        {"Release page", "Página de la versión", "Página da versão"},
        {"Cancel", "Cancelar", "Cancelar"},
        {"Game Audio", "Audio del juego", "Áudio do jogo"},
        {"Music", "Música", "Música"},
        {"Sound effects", "Efectos de sonido", "Efeitos sonoros"},
        {"Voices", "Voces", "Vozes"},
        {"Stage sounds", "Sonidos del escenario", "Sons do cenário"},
        {"Voice Chat", "Chat de voz", "Bate-papo por voz"},
        {"Enable voice chat", "Activar chat de voz", "Ativar bate-papo por voz"},
        {"Mute microphone", "Silenciar micrófono", "Silenciar microfone"},
        {"Microphone", "Micrófono", "Microfone"},
        {"Voice output", "Salida de voz", "Saída de voz"},
        {"Transmit mode", "Modo de transmisión", "Modo de transmissão"},
        {"Push to Talk", "Pulsar para hablar", "Pressione para falar"},
        {"Voice Activation", "Activación por voz", "Ativação por voz"},
        {"Activation threshold", "Umbral de activación", "Limite de ativação"},
        {"Microphone level", "Nivel del micrófono", "Nível do microfone"},
        {"Voice volume", "Volumen de voz", "Volume da voz"},
        {"Show microphone icon", "Mostrar icono del micrófono", "Mostrar ícone do microfone"},
        {"System default", "Predeterminado del sistema", "Padrão do sistema"},
        {"No microphone (listen only)", "Sin micrófono (solo escuchar)", "Sem microfone (somente ouvir)"},
        {"Push-to-Talk key", "Tecla para hablar", "Tecla para falar"},
        {"Press a key...", "Pulsa una tecla...", "Pressione uma tecla..."},
        {"Push-to-Talk controller button", "Botón del mando para hablar", "Botão do controle para falar"},
        {"Press a controller button...", "Pulsa un botón del mando...", "Pressione um botão do controle..."},
        {"Not assigned", "Sin asignar", "Não atribuído"},
        {"Transmitting", "Transmitiendo", "Transmitindo"},
        {"Proximity voice reaches both teams. Volume fades smoothly with distance; players outside its reach are silent.", "La voz por proximidad llega a ambos equipos. El volumen baja gradualmente con la distancia; los jugadores fuera de alcance no se oyen.", "A voz por proximidade alcança as duas equipes. O volume diminui gradualmente com a distância; jogadores fora do alcance não são ouvidos."},
        {"Proximity voice: all players, both teams", "Voz por proximidad: todos los jugadores, ambos equipos", "Voz por proximidade: todos os jogadores, as duas equipes"},
        {"Mute", "Silenciar", "Silenciar"},
        {"Test", "Probar", "Testar"},
        {"Reset", "Restablecer", "Redefinir"},
        {"Internal resolution", "Resolución interna", "Resolução interna"},
        {"Aspect ratio", "Relación de aspecto", "Proporção da tela"},
        {"2D filtering", "Filtro 2D", "Filtro 2D"},
        {"Sharp (console)", "Nítido (consola)", "Nítido (console)"},
        {"Smooth", "Suave", "Suave"},
        {"Full screen", "Pantalla completa", "Tela cheia"},
        {"V-sync", "Sincronización vertical", "Sincronização vertical"},
        {"Skip pictures instead of slowing down", "Omitir imágenes en vez de ralentizar", "Pular quadros em vez de reduzir a velocidade"},
        {"Low input lag", "Reducir latencia de entrada", "Reduzir latência de entrada"},
        {"Render on a second processor core (restart required)", "Renderizar en otro núcleo del procesador (requiere reiniciar)", "Renderizar em outro núcleo do processador (requer reinicialização)"},
        {"Display", "Pantalla", "Monitor"},
        {"Chosen by the desktop", "Elegida por el sistema", "Escolhida pelo sistema"},
        {"Renderer", "Renderizador", "Renderizador"},
        {"Smooth motion", "Movimiento fluido", "Movimento suave"},
        {"Menu size", "Tamaño de los menús", "Tamanho dos menus"},
        {"Show frame rate, and the connection in an online match", "Mostrar FPS y conexión durante una partida en línea", "Mostrar FPS e conexão durante uma partida online"},
        {"Texture pack (none found)", "Paquete de texturas (no se encontró ninguno)", "Pacote de texturas (nenhum encontrado)"},
        {"Weather amount", "Intensidad del clima", "Intensidade do clima"},
        {"Full", "Completo", "Completo"},
        {"Light", "Suave", "Leve"},
        {"Glow strength", "Intensidad del resplandor", "Intensidade do brilho"},
        {"Glare strength", "Intensidad del destello", "Intensidade do reflexo"},
        {"Sharp stage blur", "Desenfoque nítido del escenario", "Desfoque nítido do cenário"},
        {"Game speed", "Velocidad del juego", "Velocidade do jogo"},
        {"Unlock everything...", "Desbloquear todo...", "Desbloquear tudo..."},
        {"Unlock everything?", "¿Desbloquear todo?", "Desbloquear tudo?"},
        {"Unlock", "Desbloquear", "Desbloquear"},
        {"Done.", "Listo.", "Concluído."},
        {"Skip logos", "Omitir logotipos", "Pular logotipos"},
        {"Skip intro video", "Omitir video de introducción", "Pular vídeo de abertura"},
        {"Skip copyright page", "Omitir página de derechos de autor", "Pular página de direitos autorais"},
        {"Hide the mouse pointer during play", "Ocultar el puntero durante la partida", "Ocultar o ponteiro durante o jogo"},
        {"Quick saving", "Guardado rápido", "Salvamento rápido"},
        {"End an online match on a desync", "Terminar la partida en línea si se desincroniza", "Encerrar a partida online em caso de dessincronização"},
        {"Online chat", "Chat en línea", "Chat online"},
        {"Chat size", "Tamaño del chat", "Tamanho do chat"},
        {"Advanced", "Avanzado", "Avançado"},
        {"Open Dragon Net Battle", "Abrir Dragon Net Battle", "Abrir Dragon Net Battle"},
        {"List Dragon Net Battle in the main menu", "Mostrar Dragon Net Battle en el menú principal", "Mostrar Dragon Net Battle no menu principal"},
        {"Music follows game speed", "La música sigue la velocidad del juego", "A música acompanha a velocidade do jogo"},
        {"Player 1", "Jugador 1", "Jogador 1"},
        {"Player 2", "Jugador 2", "Jogador 2"},
        {"Controller", "Control", "Controle"},
        {"Reset to defaults", "Restablecer valores", "Restaurar padrões"},
        {"Sticks, triggers and vibration", "Palancas, gatillos y vibración", "Analógicos, gatilhos e vibração"},
        {"Action", "Acción", "Ação"},
        {"Keyboard", "Teclado", "Teclado"},
        {"Open this window", "Abrir esta ventana", "Abrir esta janela"},
        {"Open online chat", "Abrir el chat en línea", "Abrir o chat online"},
        {"Left stick", "Palanca izquierda", "Analógico esquerdo"},
        {"Right stick", "Palanca derecha", "Analógico direito"},
        {"Left trigger", "Gatillo izquierdo", "Gatilho esquerdo"},
        {"Right trigger", "Gatillo derecho", "Gatilho direito"},
        {"Vibration", "Vibración", "Vibração"},
        {"Strength", "Intensidad", "Intensidade"},
        {"No controller", "Sin mando", "Sem controle"},
        {"None", "Ninguno", "Nenhum"},
        {"Performance mode: 1x resolution, reduced post-processing and no HD texture pack. Your other renderers keep their saved quality settings.", "Modo de rendimiento: resolución 1x, posprocesado reducido y sin paquete de texturas HD. Los demás renderizadores conservan sus ajustes de calidad.", "Modo de desempenho: resolução 1x, pós-processamento reduzido e sem pacote de texturas HD. Os outros renderizadores mantêm as configurações de qualidade salvas."},
        {"4:3 (original)", "4:3 (original)", "4:3 (original)"},
        {"How finely the game is drawn. 1x is the PlayStation 2's own resolution.\nHigher is sharper and uses more video memory.", "Nivel de detalle de la imagen. 1x es la resolución original de PlayStation 2.\nLos valores mayores ofrecen más nitidez y usan más memoria de vídeo.", "Nível de detalhe da imagem. 1x é a resolução original do PlayStation 2.\nValores maiores oferecem mais nitidez e usam mais memória de vídeo."},
        {"How the display of a fight, the menus and text are drawn above 1x internal resolution.\nSharp: each of the PlayStation 2's pixels is a block. Smooth: as an emulator shows them.\nNo difference at 1x.", "Cómo se muestran los combates, los menús y los textos por encima de la resolución interna 1x.\nNítido: cada píxel de PlayStation 2 forma un bloque. Suave: como en un emulador.\nNo hay diferencia en 1x.", "Como as lutas, os menus e os textos são exibidos acima da resolução interna 1x.\nNítido: cada pixel do PlayStation 2 forma um bloco. Suave: como em um emulador.\nNão há diferença em 1x."},
        {"Each picture waits for the display's refresh. Turn it on if the picture tears.\nOff is the default: it adds a little delay.", "Cada imagen espera al refresco de la pantalla. Actívalo si la imagen se corta.\nEstá desactivado por defecto: activarlo añade un poco de retraso.", "Cada quadro espera a atualização da tela. Ative se a imagem apresentar cortes.\nFica desativado por padrão: ativá-lo acrescenta um pequeno atraso."},
        {"When this machine cannot show a picture in time, the game normally runs slower.\nWith this on it stays at full speed and leaves pictures out instead (choppier,\nnot slower). It helps where the graphics chip is the slow part. On by default.", "Si el equipo no puede mostrar una imagen a tiempo, normalmente el juego se ralentiza.\nCon esta opción sigue a velocidad normal y omite imágenes (menos fluido,\npero no más lento). Ayuda cuando la GPU es el límite. Activado por defecto.", "Se o computador não consegue exibir um quadro a tempo, o jogo normalmente fica mais lento.\nCom esta opção, mantém a velocidade e pula quadros (menos fluido,\nmas não mais lento). Ajuda quando a GPU é o limite. Ativado por padrão."},
        {"The game reads the controller, computes, and then waits before the picture is shown:\nabout 33 ms in a fight. With this on the wait comes first, so the picture follows\nyour press by the few milliseconds the machine needs. The game itself is unchanged.\nWith Smooth motion as well: its in-between picture then follows your press at once and\nthe full one a refresh later. Not used online. Off by default: on a machine that\ncannot keep up it may stutter where the original order would not.", "El juego lee el mando, calcula y espera antes de mostrar la imagen:\naproximadamente 33 ms durante una pelea. Al activarlo, la espera ocurre primero y la imagen\nsigue tu pulsación con los pocos milisegundos que necesita el equipo. El juego no cambia.\nCon Movimiento fluido, la imagen intermedia sigue la pulsación de inmediato y la completa llega un refresco después.\nNo se usa en línea. Desactivado por defecto: si el equipo no puede mantener el ritmo, puede producir tirones.", "O jogo lê o controle, calcula e espera antes de exibir o quadro:\ncerca de 33 ms durante uma luta. Ao ativar, a espera vem primeiro e a imagem\nsegue o comando com os poucos milissegundos de que o computador precisa. O jogo não muda.\nCom Movimento suave, o quadro intermediário acompanha o comando imediatamente e o completo chega uma atualização depois.\nNão é usado online. Desativado por padrão: em computadores que não acompanham o ritmo, pode causar travamentos."},
        {"The picture is prepared and shown on a core of its own while the game computes the next\nmoment: faster where the processor is the slow part (older or low-power machines).\nThe same picture and the same game. Takes effect when the game is started again.\nNot with the OpenGL renderer, and not on machines with two cores or fewer.", "La imagen se prepara y muestra en otro núcleo mientras el juego calcula el siguiente instante:\npuede ayudar cuando el procesador es el límite, sobre todo en equipos antiguos o de bajo consumo.\nNo cambia la imagen ni el juego. Se aplica al reiniciar.\nNo funciona con OpenGL ni en equipos con dos núcleos o menos.", "A imagem é preparada e exibida em outro núcleo enquanto o jogo calcula o próximo instante:\npode ajudar quando o processador é o limite, sobretudo em computadores antigos ou de baixo consumo.\nNão muda a imagem nem o jogo. É aplicado ao reiniciar.\nNão funciona com OpenGL nem em computadores com dois núcleos ou menos."},
        {"Takes effect the next time the game starts.", "Se aplica la próxima vez que inicies el juego.", "Entra em vigor na próxima vez que o jogo iniciar."},
        {"Takes effect after restarting the game. Performance depends on your GPU and drivers.\nIf the saved renderer is unavailable, the game tries Vulkan and then OpenGL.", "Se aplica al reiniciar el juego. El rendimiento depende de la GPU y sus controladores.\nSi el renderizador guardado no está disponible, el juego prueba Vulkan y luego OpenGL.", "Entra em vigor após reiniciar o jogo. O desempenho depende da GPU e dos drivers.\nSe o renderizador salvo não estiver disponível, o jogo tenta Vulkan e depois OpenGL."},
        {"Running now: %s", "En uso: %s", "Em uso: %s"},
        {"The requested renderer could not start. Using %s.", "No se pudo iniciar el renderizador solicitado. Se usa %s.", "Não foi possível iniciar o renderizador escolhido. Usando %s."},
        {"After restart: %s", "Después de reiniciar: %s", "Após reiniciar: %s"},
        {"Active GPU: %s", "GPU activa: %s", "GPU ativa: %s"},
        {"Off (30, as the console)", "Desactivado (30, como en la consola)", "Desativado (30, como no console)"},
        {"60 pictures a second", "60 imágenes por segundo", "60 quadros por segundo"},
        {"120 pictures a second", "120 imágenes por segundo", "120 quadros por segundo"},
        {"240 pictures a second", "240 imágenes por segundo", "240 quadros por segundo"},
        {"The game computes a fight 30 times a second. With this, more pictures are\ndrawn between each two. The fight itself does not change, and neither does online play.\nThe newest picture reaches the screen later: 17 ms at 60, 25 ms at 120, 29 ms at 240.\nChoose what your screen can show. F9 switches it off and on.", "El juego calcula la pelea 30 veces por segundo. Esta opción dibuja imágenes adicionales\nentre cada una. No cambia la pelea ni el juego en línea.\nLa imagen más reciente llega más tarde: 17 ms a 60, 25 ms a 120 y 29 ms a 240.\nElige lo que pueda mostrar tu pantalla. F9 activa o desactiva la opción.", "O jogo calcula a luta 30 vezes por segundo. Esta opção desenha quadros adicionais\nentre cada um. Não muda a luta nem o jogo online.\nO quadro mais recente chega depois: 17 ms a 60, 25 ms a 120 e 29 ms a 240.\nEscolha o que sua tela consegue exibir. F9 ativa ou desativa a opção."},
        {"The size of this window, the online windows, the chat and the notices. They already grow\nwith a picture taller than 1080 pixels; this makes them larger or smaller than that.", "Tamaño de esta ventana, de las salas en línea, del chat y de los avisos. Ya aumentan\ncon imágenes de más de 1080 píxeles; este ajuste los hace aún mayores o menores.", "Tamanho desta janela, das salas online, do chat e dos avisos. Eles já aumentam\nem imagens com mais de 1080 pixels; este ajuste os deixa ainda maiores ou menores."},
        {"Texture pack", "Paquete de texturas", "Pacote de texturas"},
        {"Light: half of the haze's sprites, a thinner haze; about half of what switching it off gains.", "Suave: la mitad de los sprites de neblina; el efecto de apagarla por completo es aproximadamente el doble.", "Leve: metade dos sprites da névoa; desligá-la por completo oferece aproximadamente o dobro desse ganho."},
        {"The glow around characters and things (a Super Saiyan's hair, auras).\n100%% is what the PlayStation 2 draws; the default here is 60%%.", "Resplandor alrededor de personajes y objetos (pelo de Super Saiyan, auras).\n100%% equivale a PlayStation 2; el valor predeterminado aquí es 60%%.", "Brilho ao redor de personagens e objetos (cabelo de Super Saiyajin, auras).\n100%% equivale ao PlayStation 2; o padrão aqui é 60%%."},
        {"The glare of the sky and the stage's light.\n100%% is what the PlayStation 2 draws; the default here is 60%%.", "Destello del cielo y de la iluminación del escenario.\n100%% equivale a PlayStation 2; el valor predeterminado aquí es 60%%.", "Reflexo do céu e da iluminação do cenário.\n100%% equivale ao PlayStation 2; o padrão aqui é 60%%."},
        {"On: the stage's blur keeps the detail of the internal resolution (crisper, as other\nemulators show it upscaled). Off: as soft as the PlayStation 2 draws it. No difference at 1x.", "Activado: el desenfoque conserva el detalle de la resolución interna (más nítido al ampliar).\nDesactivado: mantiene la suavidad de PlayStation 2. Sin diferencia en 1x.", "Ativado: o desfoque preserva os detalhes da resolução interna (mais nítido ao ampliar).\nDesativado: mantém a suavidade do PlayStation 2. Sem diferença em 1x."},
        {"Start without the publisher and developer logos. From the next start of the game.", "Inicia el juego sin los logotipos de la editora y del equipo desarrollador. Se aplica al próximo inicio.", "Inicia o jogo sem os logotipos da publicadora e da equipe de desenvolvimento. Entra em vigor na próxima inicialização."},
        {"Start without the opening movie. From the next start of the game.", "Inicia el juego sin el vídeo de apertura. Se aplica al próximo inicio.", "Inicia o jogo sem o vídeo de abertura. Entra em vigor na próxima inicialização."},
        {"The page of legal text shown for five seconds before the title screen, every time.", "Página legal que aparece durante cinco segundos antes del título en cada inicio.", "Página de informações legais exibida por cinco segundos antes do título, a cada início."},
        {"The pointer is hidden over the game and shown in this window, the online windows, the\nquestion before the game closes and while a chat line is typed.", "El puntero se oculta sobre el juego y aparece en esta ventana, las salas en línea,\nla confirmación antes de cerrar el juego y mientras escribes en el chat.", "O ponteiro fica oculto sobre o jogo e aparece nesta janela, nas salas online,\nna confirmação antes de fechar o jogo e enquanto uma mensagem é digitada no chat."},
        {"Save and load without the memory card pages: no question, no waiting, a small card in the\ncorner while it saves, and the save loaded at start without a page. Load in the options\nstill asks first, then loads at once. A missing card, a full card or a failed save is\nstill shown. Online the game's own pages always run. Off: the game's own pages.", "Guarda y carga sin las pantallas de la tarjeta de memoria: sin preguntas ni esperas; aparece\nuna tarjeta pequeña en la esquina mientras guarda y carga la partida al iniciar sin mostrar una pantalla.\nLa opción Cargar sigue pidiendo confirmación y luego carga de inmediato. Se muestran los errores\npor tarjeta ausente, llena o guardado fallido. En línea siempre se usan las pantallas del juego. Desactivado: pantallas originales.", "Salva e carrega sem as telas do cartão de memória: sem perguntas ou espera; aparece um cartão\npequeno no canto durante o salvamento e a partida é carregada sem tela na inicialização.\nA opção Carregar ainda pede confirmação e depois carrega imediatamente. Erros de cartão ausente,\ncheio ou falha ao salvar continuam sendo exibidos. Online, as telas originais são sempre usadas. Desativado: telas originais."},
        {"The two games compare what they compute. Off: when they no longer agree you are told, and the match\ngoes on (the two screens may then show different fights). On: the match ends there.", "Ambos juegos comparan sus cálculos. Desactivado: recibirás un aviso si dejan de coincidir, pero la partida\ncontinúa (las pantallas podrían mostrar peleas distintas). Activado: la partida termina en ese momento.", "Os dois jogos comparam os resultados calculados. Desativado: você recebe um aviso se houver divergência, mas a luta\ncontinua (as telas podem mostrar lutas diferentes). Ativado: a luta termina nesse momento."},
        {"Text with the other player of an online match. Tab opens a line (setting key_chat), Enter sends it,\nEsc puts it away; while it is typed the keyboard does not play, a controller does. Off: no lines\nare shown or sent. A player without it sees nothing.", "Mensajes con los demás jugadores en línea. Tab abre el campo de texto (configurable en Controles) y Enter lo envía.\nEsc lo cierra; al escribir, el teclado no controla el juego, pero el mando sí. Desactivado: no se muestran\nni se envían mensajes. Los jugadores que lo desactiven no verán el chat.", "Mensagens para os outros jogadores online. Tab abre o campo de texto (configurável em Controles) e Enter envia.\nEsc fecha; enquanto digita, o teclado não controla o jogo, mas o controle sim. Desativado: nenhuma mensagem\né exibida ou enviada. Jogadores que desativarem não verão o chat."},
        {"Compact", "Compacto", "Compacto"},
        {"Normal", "Normal", "Normal"},
        {"Large", "Grande", "Grande"},
        {"How wide the online chat is and how many lines it shows (while typing / otherwise).\nCompact: 4 / 2, narrower.\nNormal: 6 / 4.\nLarge: 8 / 6, wider, for large screens.", "Ancho del chat y cantidad de líneas visibles (al escribir / en reposo).\nCompacto: 4 / 2, más estrecho.\nNormal: 6 / 4.\nGrande: 8 / 6, más ancho, para pantallas grandes.", "Largura do chat e quantidade de linhas exibidas (digitando / em repouso).\nCompacto: 4 / 2, mais estreito.\nNormal: 6 / 4.\nGrande: 8 / 6, mais largo, para telas grandes."},
        {"Shows the entry beside Tenkaichi Budokai Online the next time the main menu is opened.", "Muestra esta opción junto a Tenkaichi Budokai Online la próxima vez que abras el menú principal.", "Mostra esta opção ao lado de Tenkaichi Budokai Online na próxima vez que o menu principal for aberto."},
        {"Use stick rotation in clashes", "Girar la palanca en los choques", "Girar o analógico nos confrontos"},
        {"Rotate the left stick during beam or melee clashes. Button controls remain available.", "Gira la palanca izquierda durante los choques de poderes o cuerpo a cuerpo. También puedes usar los botones.", "Gire o analógico esquerdo nos confrontos de poderes ou corpo a corpo. Os botões continuam disponíveis."},
        {"BT4 clashes use repeated button presses. Follow the on-screen prompt.", "Los choques de BT4 requieren pulsar los botones repetidamente. Sigue las indicaciones en pantalla.", "Os choques de BT4 exigem pressionar os botões repetidamente. Siga as instruções na tela."},
        {"Choose a binding, then press the new key or button. Esc keeps it, Delete clears it;", "Elige una asignación y pulsa la nueva tecla o botón. Esc la conserva y Supr la borra;", "Escolha uma atribuição e pressione a nova tecla ou botão. Esc mantém e Delete apaga;"},
        {"with a controller, press nothing for five seconds to keep it.", "con un mando, no pulses nada durante cinco segundos para conservarla.", "com um controle, não pressione nada por cinco segundos para manter."},
        {"press a key...", "pulsa una tecla...", "pressione uma tecla..."},
        {"press a button...", "pulsa un botón...", "pressione um botão..."},
        {"START + SELECT", "START + SELECT", "START + SELECT"},
        {"Controller %d (not connected)", "Mando %d (no conectado)", "Controle %d (desconectado)"},
        {"Lobby%s%s", "Sala%s%s", "Sala%s%s"},
        {"Back to the lobby", "Volver a la sala", "Voltar à sala"},
        {"Ends the match for everyone: all go back to the lobby window, where the rules can be changed.", "Finaliza la partida para todos y vuelve a la sala, donde se pueden cambiar las reglas.", "Encerra a partida para todos e volta à sala, onde as regras podem ser alteradas."},
        {"Close the lobby", "Cerrar la sala", "Fechar a sala"},
        {"Leave the lobby", "Abandonar la sala", "Sair da sala"},
        {"Ends the match and the lobby for everyone.", "Finaliza la partida y cierra la sala para todos.", "Encerra a partida e fecha a sala para todos."},
        {"Back to the main menu. A player leaving ends the match; a spectator leaving changes nothing for the others.", "Vuelve al menú principal. Si un jugador se va, la partida termina; si se va un espectador, los demás continúan.", "Volta ao menu principal. Se um jogador sair, a partida termina; se um espectador sair, nada muda para os demais."},
        {"Spectating %s vs %s", "Viendo a %s contra %s", "Assistindo %s contra %s"},
        {"Change view", "Cambiar vista", "Mudar visualização"},
        {"Player 1's view, player 2's, or both (split screen). Also V or R1 while watching.", "Vista del jugador 1, del jugador 2 o de ambos (pantalla dividida). También puedes pulsar V o R1 mientras observas.", "Visão do jogador 1, do jogador 2 ou de ambos (tela dividida). Também é possível pressionar V ou R1 enquanto assiste."},
        {"Stop watching", "Dejar de observar", "Parar de assistir"},
        {"View: %s", "Vista: %s", "Visão: %s"},
        {"up", "arriba", "cima"},
        {"down", "abajo", "baixo"},
        {"left", "izquierda", "esquerda"},
        {"right", "derecha", "direita"},
        {"stick", "palanca", "analógico"},
        {"game", "juego", "jogo"},
        {"no controller", "sin mando", "sem controle"},
        {" pressed", " pulsado", " pressionado"},
        {"D-pad up", "Cruceta arriba", "Direcional para cima"},
        {"D-pad down", "Cruceta abajo", "Direcional para baixo"},
        {"D-pad left", "Cruceta izquierda", "Direcional para a esquerda"},
        {"D-pad right", "Cruceta derecha", "Direcional para a direita"},
        {"Cross", "Cruz", "X"},
        {"Circle", "Círculo", "Círculo"},
        {"Square", "Cuadrado", "Quadrado"},
        {"Triangle", "Triángulo", "Triângulo"},
        {"Left stick up", "Palanca izquierda arriba", "Analógico esquerdo para cima"},
        {"Left stick down", "Palanca izquierda abajo", "Analógico esquerdo para baixo"},
        {"Left stick left", "Palanca izquierda a la izquierda", "Analógico esquerdo para a esquerda"},
        {"Left stick right", "Palanca izquierda a la derecha", "Analógico esquerdo para a direita"},
        {"Right stick up", "Palanca derecha arriba", "Analógico direito para cima"},
        {"Right stick down", "Palanca derecha abajo", "Analógico direito para baixo"},
        {"Right stick left", "Palanca derecha a la izquierda", "Analógico direito para a esquerda"},
        {"Right stick right", "Palanca derecha a la derecha", "Analógico direito para a direita"},
        {"Voice ready; join an online room.", "Chat de voz listo; únete a una sala en línea.", "Bate-papo por voz pronto; entre em uma sala online."},
        {"Voice chat is off.", "El chat de voz está desactivado.", "O bate-papo por voz está desativado."},
        {"Voice output unavailable; check your audio device.", "La salida de voz no está disponible; revisa el dispositivo de audio.", "A saída de voz está indisponível; verifique o dispositivo de áudio."},
        {"Listening; microphone unavailable.", "Escuchando; el micrófono no está disponible.", "Ouvindo; microfone indisponível."},
        {"Voice connected; waiting for another player.", "Chat de voz conectado; esperando a otro jugador.", "Bate-papo por voz conectado; aguardando outro jogador."},
        {"Voice connected directly.", "Chat de voz conectado directamente.", "Bate-papo por voz conectado diretamente."},
        {"Voice connected; relay available.", "Chat de voz conectado; retransmisión disponible.", "Bate-papo por voz conectado; retransmissão disponível."},
        {"Optional voice during online battles. The microphone is opened only while voice is enabled and connected to a room.", "Chat de voz opcional durante las partidas en línea. El micrófono solo se activa si el chat está habilitado y conectado a una sala.", "Bate-papo por voz opcional durante partidas online. O microfone só é ativado quando o recurso está habilitado e conectado a uma sala."},
        {"Higher values ignore quieter sounds. Use a headset to avoid sending game audio back to other players.", "Los valores altos ignoran los sonidos suaves. Usa auriculares para evitar que el audio del juego se transmita a los demás.", "Valores altos ignoram sons baixos. Use fones de ouvido para evitar que o áudio do jogo seja transmitido aos outros jogadores."},
        {"Hold this key to transmit. Escape cancels binding.", "Mantén pulsada esta tecla para hablar. Escape cancela la asignación.", "Mantenha esta tecla pressionada para falar. Escape cancela a atribuição."},
        {"Choose a button that does not conflict with your combat controls.", "Elige un botón que no interfiera con los controles de combate.", "Escolha um botão que não conflite com os comandos de combate."},
        {"A: move   B: choose   Esc: back", "A: mover   B: elegir   Esc: volver", "A: mover   B: escolher   Esc: voltar"},
        {"both (split screen)", "ambos (pantalla dividida)", "ambos (tela dividida)"},
        {"player 1", "jugador 1", "jogador 1"},
        {"player 2", "jugador 2", "jogador 2"},
        {"Outline", "Contorno", "Contorno"},
        {"The black line around the fighters.", "Línea negra alrededor de los luchadores.", "Linha preta ao redor dos lutadores."},
        {"See-through tint", "Tinte de transparencia", "Tonalidade de transparência"},
        {"The tint that shows a fighter behind scenery.", "Tinte que permite ver a un luchador detrás del escenario.", "Tonalidade que permite ver um lutador atrás do cenário."},
        {"Depth tint", "Tinte de profundidad", "Tonalidade de profundidade"},
        {"The haze that colours distant things.", "Neblina que da color a los elementos lejanos.", "Névoa que colore os elementos distantes."},
        {"Glare and glow", "Destello y resplandor", "Reflexo e brilho"},
        {"The bloom around bright things and the sky's glare.", "Resplandor alrededor de los elementos brillantes y destello del cielo.", "Brilho ao redor de elementos luminosos e reflexo do céu."},
        {"Distance blur", "Desenfoque de distancia", "Desfoque de distância"},
        {"The soft focus on distant scenery.", "Enfoque suave de los elementos lejanos del escenario.", "Foco suave nos elementos distantes do cenário."},
        {"Movement blur", "Desenfoque de movimiento", "Desfoque de movimento"},
        {"The blur of the scenery while the view turns or moves fast.", "Desenfoque del escenario cuando la cámara gira o se mueve rápido.", "Desfoque do cenário quando a câmera gira ou se move rapidamente."},
        {"Weather", "Clima", "Clima"},
        {"The haze of dust or mist that drifts over some stages (Desert and Rocky Area at noon, Rocky Area\nin the evening, Kami's Lookout; Glacier, Muscle Tower and the Time Chamber have a little of it too).\nA faint tint, made of big soft sprites over the whole picture: off, those stages run as fast as the\nothers (a fifth faster at 3x).", "Neblina de polvo o bruma presente en algunos escenarios (Desierto y Área rocosa al mediodía, Área rocosa\nal atardecer, Torre de Karin; también aparece un poco en Glaciar, Torre Muscle y Sala del Tiempo).\nEs un tinte leve creado con sprites grandes y suaves sobre toda la imagen. Al desactivarlo, esos escenarios\nfuncionan igual de rápido que los demás (un quinto más rápido en 3x).", "Névoa de poeira ou bruma presente em alguns cenários (Deserto e Área Rochosa ao meio-dia, Área Rochosa\nao entardecer, Torre de Karin; também aparece um pouco na Geleira, Torre Muscle e Sala do Tempo).\nÉ uma tonalidade suave criada por sprites grandes sobre toda a imagem. Desativada, esses cenários\nfuncionam tão rápido quanto os outros (um quinto mais rápido em 3x)."},
        {"Restart the update from an offline menu.", "Reinicia la actualización desde un menú sin conexión.", "Reinicie a atualização em um menu offline."},
        {"The update is installed, but the new version could not be started: close the game and start it again.", "La actualización se instaló, pero no se pudo iniciar la nueva versión. Cierra el juego y vuelve a abrirlo.", "A atualização foi instalada, mas não foi possível iniciar a nova versão. Feche o jogo e abra-o novamente."},
        {"Updated to %s.", "Actualizado a %s.", "Atualizado para %s."},
        {"Updates", "Actualizaciones", "Atualizações"},
        {"Installed version: %s", "Versión instalada: %s", "Versão instalada: %s"},
        {"Check for updates", "Buscar actualizaciones", "Verificar atualizações"},
        {"Checking for updates...", "Buscando actualizaciones...", "Verificando atualizações..."},
        {"No newer release is available for this build.", "No hay una versión más reciente disponible para esta compilación.", "Não há uma versão mais recente disponível para esta compilação."},
        {"Could not check for updates. Check your connection and try again.", "No se pudieron buscar actualizaciones. Revisa la conexión e inténtalo de nuevo.", "Não foi possível verificar as atualizações. Verifique a conexão e tente novamente."},
        {"Retry download", "Reintentar descarga", "Tentar baixar novamente"},
        {"Return to an offline menu to apply the update.", "Vuelve a un menú sin conexión para aplicar la actualización.", "Volte a um menu offline para aplicar a atualização."},
        {"Update %s found. Downloading automatically.", "Se encontró la actualización %s. Se descargará automáticamente.", "Atualização %s encontrada. O download será feito automaticamente."},
        {"Downloading %s: %d%% (%.1f of %.1f MB). You can go on playing.", "Descargando %s: %d%% (%.1f de %.1f MB). Puedes seguir jugando.", "Baixando %s: %d%% (%.1f de %.1f MB). Você pode continuar jogando."},
        {"Checking the download of %s (its signature)...", "Verificando la descarga de %s (firma digital)...", "Verificando o download de %s (assinatura)..."},
        {"Installing %s...", "Instalando %s...", "Instalando %s..."},
        {"The update to %s was not installed: %s", "No se instaló la actualización %s: %s", "A atualização %s não foi instalada: %s"},
        {"Automatic installation is unavailable: %s", "La instalación automática no está disponible: %s", "A instalação automática não está disponível: %s"},
        {"Opens the release page. Verified updates install automatically at the main menu.", "Abre la página de la versión. Las actualizaciones verificadas se aplican después de que confirmes el reinicio en el menú principal.", "Abre a página da versão. Atualizações verificadas são aplicadas depois que você confirma a reinicialização no menu principal."},
        {"(The release has no notes.)", "(Esta versión no tiene notas.)", "(Esta versão não tem notas.)"},
        {"Automatic updates: %s", "Actualizaciones automáticas: %s", "Atualizações automáticas: %s"},
        {"The launcher checks for signed releases, downloads them in the background, and installs them at the main menu.", "El juego busca versiones firmadas y las descarga en segundo plano. En el menú principal, te pregunta antes de reiniciar e instalarlas.", "O jogo procura versões assinadas e as baixa em segundo plano. No menu principal, pede confirmação antes de reiniciar e instalá-las."},
        {"Opens the release page. A verified update is applied only after you confirm a restart from an offline menu.", "Abre la página de la versión. La actualización verificada solo se aplica después de que confirmes el reinicio desde un menú sin conexión.", "Abre a página da versão. A atualização verificada só é aplicada depois que você confirma a reinicialização em um menu offline."},
        {"The updater checks for signed releases and downloads them in the background. In an offline menu, it asks before restarting to apply an update.", "El actualizador busca versiones firmadas y las descarga en segundo plano. En un menú sin conexión, pregunta antes de reiniciar para aplicar la actualización.", "O atualizador procura versões assinadas e as baixa em segundo plano. Em um menu offline, pede confirmação antes de reiniciar para aplicar a atualização."},
        {"Update {version} found. Downloading automatically.", "Se encontró la actualización {version}. Se descargará automáticamente.", "Atualização {version} encontrada. O download será feito automaticamente."},
        {"Downloading update {version}: {percent}%", "Descargando actualización {version}: {percent}%", "Baixando atualização {version}: {percent}%"},
        {"How fast the game runs outside online play (the sound keeps its pitch). 100%% is the game's own;\n114%% is what competitive play uses. An online match never uses this value:\nits speed is the host's or the queue's.", "Velocidad del juego fuera de las partidas en línea (el sonido conserva su tono). 100%% es el valor original;\nel modo competitivo usa 114%%. Las partidas en línea usan la velocidad del anfitrión o de la cola.", "Velocidade do jogo fora das partidas online (o som mantém o tom). 100%% é o padrão do jogo;\no modo competitivo usa 114%%. Partidas online usam a velocidade do anfitrião ou da fila."},
        {"Unlock everything: all characters, stages, music and items, and the largest amount of Zenni. This is a debug function the game's developers left in.", "Desbloquea todos los personajes, escenarios, música y objetos, además de la cantidad máxima de Zenni. Es una función de depuración que dejaron los desarrolladores.", "Desbloqueia todos os personagens, cenários, músicas e itens, além da quantidade máxima de Zenni. É uma função de depuração deixada pelos desenvolvedores."},
        {"It also empties your records list, and it becomes permanent the next time the game saves. Use it after your save has been loaded (from the main menu on).", "También borra la lista de récords y el cambio será permanente cuando el juego guarde. Úsalo después de cargar la partida, desde el menú principal.", "Também apaga a lista de recordes, e a alteração será permanente quando o jogo salvar. Use depois de carregar o jogo, a partir do menu principal."},
        {"Your records list will be emptied. This cannot be undone once the game saves.", "Se borrará tu lista de récords. No se puede deshacer después de que el juego guarde.", "Sua lista de recordes será apagada. Não é possível desfazer depois que o jogo salvar."},
        {"A controller counts as pressed from %d%% on. The game is told pressed or not, nothing between.", "El juego considera pulsado el gatillo desde el %d%%. Solo recibe el estado pulsado o suelto.", "O jogo considera o gatilho pressionado a partir de %d%%. O estado enviado é pressionado ou solto."},
        {"The game's own vibration (hits, clashes, charging), on this player's controller.\nStrength applies to both vibration motors. Test plays them for half a second.", "Vibración del juego (golpes, choques, carga) en el mando de este jugador.\nLa intensidad afecta a ambos motores. Probar los activa durante medio segundo.", "Vibração do jogo (golpes, choques, carregamento) no controle deste jogador.\nA intensidade se aplica aos dois motores. Testar ativa ambos por meio segundo."},
        {"Half a second of both motors at this strength.", "Activa ambos motores durante medio segundo con esta intensidad.", "Ativa os dois motores por meio segundo nesta intensidade."},
        {"(this controller has none)", "(este mando no tiene vibración)", "(este controle não tem vibração)"},
        {"Default", "Predeterminada", "Padrão"},
        {"Exit", "Salir", "Sair"},
        {"Close the game?\nUnsaved progress will be lost.", "¿Cerrar el juego?\nSe perderá el progreso sin guardar.", "Fechar o jogo?\nO progresso não salvo será perdido."},
        {"Deadzone", "Zona muerta", "Zona morta"},
        {"A stage's background sound (wind, water) and its own effects.", "Sonido ambiente del escenario (viento, agua) y sus efectos.", "Som ambiente do cenário (vento, água) e seus efeitos."},
        {"A trigger counts as pressed from %d%% on. The game is told pressed or not, nothing between.", "El gatillo se considera pulsado a partir del %d%%. El juego solo recibe pulsado o suelto.", "O gatilho é considerado pressionado a partir de %d%%. O jogo só recebe pressionado ou solto."},
        {"Dragon Net Battle is the first online window of this port. Tenkaichi Budokai Online (the main menu) does everything it does: lobbies by code or by address, and quick matches.", "Dragon Net Battle es la ventana de conexión original de este port. Tenkaichi Budokai Online, en el menú principal, ofrece las mismas opciones: salas por código o dirección y partidas rápidas.", "Dragon Net Battle é a janela de conexão original deste port. Tenkaichi Budokai Online, no menu principal, oferece as mesmas opções: salas por código ou endereço e partidas rápidas."},
        {"Hits, blasts, menu sounds. Takes effect with the next sound that starts.", "Golpes, ataques de energía y sonidos de los menús. Se aplica al siguiente sonido que comience.", "Golpes, ataques de energia e sons dos menus. A alteração se aplica ao próximo som que começar."},
        {"How far the stick has to be moved before the game is told anything (the shaded middle).\nOriginal: the game's own, 39%% for left / right and for up / down separately, a square.\n0%% to 90%%: a round one of that size instead; from its edge the value grows evenly\nto full at full tilt, and the stick counts as a direction half way between the two.\nThe white dot is the stick, the ring what the game is given; after the stick's name,\nthe direction the game takes it for. Choose a size that covers where the dot rests\nwhen the stick is let go.", "Distancia que debes mover la palanca antes de que el juego responda (centro sombreado).\nOriginal: zona cuadrada del juego, del 39%% en cada eje.\nDe 0%% a 90%%: zona circular de ese tamaño; la respuesta aumenta gradualmente desde el borde\nhasta la inclinación máxima. La dirección se activa a mitad de ese recorrido.\nEl punto blanco muestra la palanca; el anillo, la entrada que recibe el juego.\nElige un tamaño que cubra la posición del punto cuando sueltes la palanca.", "Distância que o analógico deve percorrer antes de o jogo responder (centro sombreado).\nOriginal: zona quadrada do jogo, de 39%% em cada eixo.\nDe 0%% a 90%%: zona circular desse tamanho; a resposta aumenta gradualmente da borda\naté a inclinação máxima. A direção é ativada na metade desse percurso.\nO ponto branco mostra o analógico; o anel, a entrada recebida pelo jogo.\nEscolha um tamanho que cubra a posição do ponto quando soltar o analógico."},
        {"Put a ZIP pack or PNG/DDS files (PCSX2 naming) into the \"textures\" folder next to the game.", "Coloca el paquete ZIP o sus archivos PNG/DDS (con nombres de PCSX2) en la carpeta \"textures\" junto al juego.", "Coloque o pacote ZIP ou os arquivos PNG/DDS (com nomes do PCSX2) na pasta \"textures\" ao lado do jogo."},
        {"Replacement textures from the folder \"textures\" next to the game.", "Texturas de reemplazo de la carpeta \"textures\" junto al juego.", "Texturas de substituição da pasta \"textures\" ao lado do jogo."},
        {"The fighters' voices in a fight and the spoken lines outside one.", "Voces de los luchadores en combate y diálogos fuera de él.", "Vozes dos lutadores em batalha e diálogos fora dela."},
        {"The game's own vibration (hits, clashes, charging), on this player's controller.\nThe game's Options > Controller > Vibration has to be on as well: it is, unless you turned it off.\nThe game gives none in a replay or for a side the computer plays.", "Vibración del juego (golpes, choques y carga) en el mando de este jugador.\nTambién debes activar Opciones > Mando > Vibración dentro del juego.\nNo hay vibración en repeticiones ni para los luchadores controlados por la CPU.", "Vibração do jogo (golpes, choques e carregamento) no controle deste jogador.\nAtive também Opções > Controle > Vibração dentro do jogo.\nNão há vibração em replays nem para os lutadores controlados pela CPU."},
        {"When the game runs at another speed than 100%% (an online match, the Cheats tab):\noff, the music plays as it is; on, it is sped up or slowed down with the game,\nat its own pitch, as an emulator does. Spoken lines always follow the game.", "Cuando el juego funciona a una velocidad distinta del 100%%:\ndesactivado, la música mantiene su velocidad; activado, sigue la velocidad del juego\nsin cambiar el tono. Los diálogos siempre siguen la velocidad del juego.", "Quando o jogo funciona em uma velocidade diferente de 100%%:\ndesativado, a música mantém a velocidade original; ativado, acompanha o jogo\nsem mudar o tom. Os diálogos sempre acompanham a velocidade do jogo."},
        {"The fighter's name down the panel. The game's fonts, as for the Menu font\n(a font of your own: add it to fonts/fonts.tsv). Default: the first of the list.", "Fuente del nombre del luchador en el panel. Usa la lista de fuentes de los menús.\nPara añadir una fuente, inclúyela en fonts/fonts.tsv. Predeterminada: primera de la lista.", "Fonte do nome do lutador no painel. Usa a lista de fontes dos menus.\nPara adicionar uma fonte, inclua-a em fonts/fonts.tsv. Padrão: primeira da lista."},
        {"The form under the portrait. The game's fonts, as for the Menu font.\nDefault: the first of the list.", "Fuente de la transformación debajo del retrato. Usa la lista de fuentes de los menús.\nPredeterminada: primera de la lista.", "Fonte da forma abaixo do retrato. Usa a lista de fontes dos menus.\nPadrão: primeira da lista."},
        {"%s (%d textures)", "%s (%d texturas)", "%s (%d texturas)"},
        {"Checking your game data... The search begins when it is done.", "Verificando los datos del juego... La búsqueda comenzará al finalizar.", "Verificando os dados do jogo... A busca começará ao terminar."},
        {"Found a player. Connecting...", "Jugador encontrado. Conectando...", "Jogador encontrado. Conectando..."},
        {"Searching for a player in this queue...", "Buscando un jugador en esta cola...", "Buscando um jogador nesta fila..."},
        {"Searching for %d:%02d more. Leave the window open.", "Tiempo de búsqueda restante: %d:%02d. Mantén abierta esta ventana.", "Tempo de busca restante: %d:%02d. Mantenha esta janela aberta."},
        {"The games could not establish a connection. Search again for another player.", "No se pudo establecer la conexión entre los juegos. Busca otro jugador.", "Não foi possível estabelecer a conexão entre os jogos. Busque outro jogador."},
        {"The other player's game is a different version. Search again.", "El otro jugador usa una versión distinta del juego. Vuelve a buscar.", "O outro jogador usa uma versão diferente do jogo. Busque novamente."},
        {"Nobody was searching this queue in the last three minutes. Search again, or try another queue.", "No se encontraron jugadores en esta cola durante los últimos tres minutos. Vuelve a buscar o prueba otra cola.", "Nenhum jogador foi encontrado nesta fila nos últimos três minutos. Busque novamente ou tente outra fila."},
        {"Checking your game data...", "Verificando los datos del juego...", "Verificando os dados do jogo..."},
        {"Search again", "Volver a buscar", "Buscar novamente"},
        {"Search for a match", "Buscar partida", "Buscar partida"},
        {"Update available. Online servers are unreachable until you update.", "Hay una actualización disponible. Actualiza el juego para conectarte al modo en línea.", "Há uma atualização disponível. Atualize o jogo para se conectar ao modo online."},
        {"You have 0.%d.%d; 0.%d.%d or newer is needed. github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases", "Versión instalada: 0.%d.%d. Se requiere 0.%d.%d o posterior. github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases", "Versão instalada: 0.%d.%d. É necessário 0.%d.%d ou posterior. github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases"},
        {"Update available: release 0.%d.%d.", "Actualización disponible: versión 0.%d.%d.", "Atualização disponível: versão 0.%d.%d."},
        {"Fight a player on another computer. One of you hosts, the other joins with the host's address.", "Juega con otra persona. Un jugador crea la sala y el otro se une con su código o dirección.", "Jogue com outra pessoa. Um jogador cria a sala e o outro entra com o código ou endereço."},
        {"Player name", "Nombre del jugador", "Nome do jogador"},
        {"Host", "Crear sala", "Criar sala"},
        {"Use a code (no port forwarding needed)", "Usar un código (sin redirección de puertos)", "Usar um código (sem redirecionamento de portas)"},
        {"Your lobby gets a short code to tell the others, and the games find each other through a\nsmall matchmaking service. Works with most home routers. Without it the lobby is on a port,\nwhich must be reachable from the others (port forwarding or a virtual network).", "La sala recibe un código para compartir. El servicio de conexión permite que los juegos se encuentren\ny funciona con la mayoría de routers domésticos. Sin código, los demás deben poder acceder\nal puerto de la sala mediante redirección de puertos o una red virtual.", "A sala recebe um código para compartilhar. O serviço de conexão permite que os jogos se encontrem\ne funciona com a maioria dos roteadores domésticos. Sem código, os outros precisam acessar\na porta da sala por redirecionamento de portas ou uma rede virtual."},
        {"Port", "Puerto", "Porta"},
        {"Rollback", "Rollback", "Rollback"},
        {"Off (wait for each other)", "Desactivado (espera sincronizada)", "Desativado (espera sincronizada)"},
        {"Up to 2 frames", "Hasta 2 fotogramas", "Até 2 quadros"},
        {"Up to 4 frames", "Hasta 4 fotogramas", "Até 4 quadros"},
        {"Up to 6 frames", "Hasta 6 fotogramas", "Até 6 quadros"},
        {"Up to 8 frames", "Hasta 8 fotogramas", "Até 8 quadros"},
        {"With rollback your own moves come out at once and the game corrects itself when the\nother player's buttons arrive. More frames cope with a slower connection and cost\nmore processor time. Off: both games wait for each other every frame.", "El rollback responde a tus acciones y corrige el estado cuando recibe la entrada del otro jugador.\nMás fotogramas permiten compensar conexiones más lentas, pero requieren más CPU.\nDesactivado: ambos juegos se esperan en cada fotograma.", "O rollback responde às suas ações e corrige o estado ao receber os comandos do outro jogador.\nMais quadros permitem compensar conexões mais lentas, mas exigem mais CPU.\nDesativado: os dois jogos esperam um pelo outro a cada quadro."},
        {"Automatic (from the connection)", "Automático (según la conexión)", "Automático (conforme a conexão)"},
        {"0 frames", "0 fotogramas", "0 quadros"},
        {"1 frame", "1 fotograma", "1 quadro"},
        {"2 frames", "2 fotogramas", "2 quadros"},
        {"3 frames", "3 fotogramas", "3 quadros"},
        {"4 frames", "4 fotogramas", "4 quadros"},
        {"5 frames", "5 fotogramas", "5 quadros"},
        {"6 frames", "6 fotogramas", "6 quadros"},
        {"Input delay", "Retardo de entrada", "Atraso de entrada"},
        {"How long after a press your own move comes out. A slow connection needs more, or the game\nhas to guess too far ahead and stutters. Automatic measures the connection when the match\nstarts: 1 frame on a good one, more on a slow one.", "Tiempo entre pulsar un botón y ejecutar la acción. Una conexión lenta puede necesitar más retardo\npara limitar las predicciones y correcciones. Automático mide la conexión al iniciar la partida:\n1 fotograma en una conexión buena y más en una lenta.", "Tempo entre pressionar um botão e executar a ação. Uma conexão lenta pode exigir mais atraso\npara limitar previsões e correções. Automático mede a conexão no início da partida:\n1 quadro em uma conexão boa e mais em uma lenta."},
        {"Single battle", "Combate individual", "Batalha individual"},
        {"Team battle", "Combate por equipos", "Batalha em equipes"},
        {"DP battle (10 DP)", "Combate PD (10 PD)", "Batalha PD (10 PD)"},
        {"DP battle (15 DP)", "Combate PD (15 PD)", "Batalha PD (15 PD)"},
        {"DP battle (20 DP)", "Combate PD (20 PD)", "Batalha PD (20 PD)"},
        {"60 seconds", "60 segundos", "60 segundos"},
        {"90 seconds", "90 segundos", "90 segundos"},
        {"180 seconds", "180 segundos", "180 segundos"},
        {"240 seconds", "240 segundos", "240 segundos"},
        {"No limit", "Sin límite", "Sem limite"},
        {"Battle", "Combate", "Batalha"},
        {"Time limit", "Tiempo límite", "Limite de tempo"},
        {"How fast the match runs, for both players; the sound keeps its pitch.\n100%% is the game's own speed. %d%% is the competitive standard used by quick matches.", "Velocidad de la partida para ambos jugadores, sin cambiar el tono del sonido.\n100%% es la velocidad original. %d%% es el estándar competitivo de las partidas rápidas.", "Velocidade da partida para ambos os jogadores, sem mudar o tom do som.\n100%% é a velocidade original. %d%% é o padrão competitivo das partidas rápidas."},
        {"The lobby's first rules; its owner changes them on its page. Everyone needs the same game data.", "Reglas iniciales de la sala. El anfitrión puede cambiarlas en la sala. Todos necesitan datos de juego compatibles.", "Regras iniciais da sala. O anfitrião pode alterá-las na sala. Todos precisam de dados de jogo compatíveis."},
        {"Quick match", "Partida rápida", "Partida rápida"},
        {"Be matched with whoever is searching the same queue. All quick matches have no time limit and run at %d%% speed, between games with the original disc's data.", "Busca jugadores en la misma cola. Las partidas rápidas no tienen límite de tiempo y funcionan al %d%%, con datos compatibles de BT4.", "Busque jogadores na mesma fila. Partidas rápidas não têm limite de tempo e funcionam a %d%%, com dados compatíveis de BT4."},
        {"%s   (%d searching)###q%d", "%s   (%d buscando)###q%d", "%s   (%d buscando)###q%d"},
        {"The two games connect to each other directly when they can, as in most online fighting", "Los juegos se conectan directamente cuando es posible. Un jugador que inspeccione su", "Os jogos se conectam diretamente quando possível. Um jogador que inspecione o próprio"},
        {"games: a player who inspects their own network traffic can see the other's address.", "tráfico de red puede ver la dirección del otro jugador.", "tráfego de rede pode ver o endereço do outro jogador."},
        {"Join", "Unirse", "Entrar"},
        {"Room code", "Código de sala", "Código da sala"},
        {"Paste", "Pegar", "Colar"},
        {"or, without a code, the host's address and port:", "O, sin código, la dirección y el puerto del anfitrión:", "Ou, sem código, o endereço e a porta do anfitrião:"},
        {"Host's address", "Dirección del anfitrión", "Endereço do anfitrião"},
        {"Checking your game data... Connecting begins when it is done.", "Verificando los datos del juego... La conexión comenzará al finalizar.", "Verificando os dados do jogo... A conexão começará ao terminar."},
        {"Found the lobby. Connecting to its owner...", "Sala encontrada. Conectando con el anfitrión...", "Sala encontrada. Conectando ao anfitrião..."},
        {"Looking for the lobby...", "Buscando la sala...", "Buscando a sala..."},
        {"The games could not establish a connection. Hosting on a port (with port forwarding or a virtual network such as Tailscale) still works.", "No se pudo establecer la conexión entre los juegos. Puedes crear una sala por puerto con redirección de puertos o una red virtual como Tailscale.", "Não foi possível estabelecer a conexão entre os jogos. Você pode criar uma sala por porta com redirecionamento de portas ou uma rede virtual como Tailscale."},
        {"Open a lobby", "Crear una sala", "Criar uma sala"},
        {"Join the lobby", "Unirse a la sala", "Entrar na sala"},
        {"Back", "Volver", "Voltar"},
        {"You are in the lobby. Its screen opens from the main menu (Tenkaichi Budokai Online).", "Te has unido a la sala. Puedes abrirla desde el menú principal, en Tenkaichi Budokai Online.", "Você entrou na sala. Abra-a pelo menu principal, em Tenkaichi Budokai Online."},
        {"The other game is a different version. Everyone needs the same release.", "El otro juego usa una versión distinta. Todos necesitan la misma versión.", "O outro jogo usa uma versão diferente. Todos precisam da mesma versão."},
        {"The search could not be started.", "No se pudo iniciar la búsqueda.", "Não foi possível iniciar a busca."},
        {"In a match both players get the same roster, with everything unlocked, and your own save is", "En línea, ambos jugadores tienen la misma plantilla, con todo desbloqueado. Tu partida guardada", "Online, ambos os jogadores têm a mesma lista de lutadores, com tudo desbloqueado. Seu arquivo salvo"},
        {"left alone. When the match ends everyone is back on the lobby's page.", "se conserva. Al finalizar el combate, todos vuelven a la sala.", "é preservado. Ao terminar a batalha, todos voltam à sala."},
        {"Button %d", "Botón %d", "Botão %d"},
        {"A / Cross", "A / Cruz", "A / X"},
        {"B / Circle", "B / Círculo", "B / Círculo"},
        {"X / Square", "X / Cuadrado", "X / Quadrado"},
        {"Y / Triangle", "Y / Triángulo", "Y / Triângulo"},
        {"Back / Select", "Atrás / Select", "Voltar / Select"},
        {"Guide", "Guía", "Guia"},
        {"Start", "Start", "Start"},
        {"Left stick click", "Clic de palanca izquierda", "Clique no analógico esquerdo"},
        {"Right stick click", "Clic de palanca derecha", "Clique no analógico direito"},
        {"Left shoulder", "Botón superior izquierdo", "Botão superior esquerdo"},
        {"Right shoulder", "Botón superior derecho", "Botão superior direito"},
        {"Misc 1", "Adicional 1", "Adicional 1"},
        {"Right paddle 1", "Palanca trasera derecha 1", "Botão traseiro direito 1"},
        {"Left paddle 1", "Palanca trasera izquierda 1", "Botão traseiro esquerdo 1"},
        {"Right paddle 2", "Palanca trasera derecha 2", "Botão traseiro direito 2"},
        {"Left paddle 2", "Palanca trasera izquierda 2", "Botão traseiro esquerdo 2"},
        {"Touchpad", "Panel táctil", "Painel de toque"},
        {"Updated to {version}.", "Actualizado a {version}.", "Atualizado para {version}."},
    };
    int language = f1_language();
    if (english == NULL || language == 1) return english;
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        if (strcmp(english, rows[i].en) == 0) return language == 0 ? rows[i].es : rows[i].pt;
    }
    return english;
}
extern "C" const char *Port_F1Text(const char *text) { return f1_text(text); }
// The online chat (net_chat.c): its key opens a line to type, Enter sends it, Esc puts it away.
static bool sChat;                             // a line is being typed
static bool sChatFocus;                        // ... and its field takes the keyboard next frame
static bool sChatKeyHeld;                      // ... and the key that opened it is still down (its letter is not typed)
static bool sChatSoon;                         // the last Enter came within a second of the line before: not sent
static char sChatBuf[CHAT_TEXT + 1];
static void chat_close(void);

/* Name strips (gamedata/stages/names.rgba and gamedata/songs/names.rgba): the names of the stages and tracks added
   from outside the disc, drawn by the port over the menu in the game's own lettering. Raw RGBA, a 12-byte header
   (w, h, count) and then `count` images of w x h stacked. */
static SDL_GPUTexture *sNameTex, *sSongTex;
static uint32_t sNameW, sNameH, sNameCount, sSongW, sSongH, sSongCount;

static uint8_t *read_strip(const char *path, size_t *bytes, uint32_t *w, uint32_t *h, uint32_t *count) {
    FILE *fp = fopen(path, "rb");
    uint32_t hdr[3];
    uint8_t *pix;

    if (fp == NULL) {
        return NULL;
    }
    if (fread(hdr, sizeof(hdr), 1, fp) != 1 || hdr[0] == 0 || hdr[1] == 0 || hdr[2] == 0) {
        fclose(fp);
        return NULL;
    }
    *w = hdr[0];
    *h = hdr[1];
    *count = hdr[2];
    *bytes = (size_t)hdr[0] * hdr[1] * hdr[2] * 4;
    pix = (uint8_t *)malloc(*bytes);
    if (pix == NULL || fread(pix, 1, *bytes, fp) != *bytes) {
        free(pix);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return pix;
}

static SDL_GPUTexture *make_strip(uint8_t *pix, size_t bytes, uint32_t w, uint32_t h, uint32_t count) {
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTransferBufferCreateInfo tbi;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *cp;
    SDL_GPUTextureTransferInfo src;
    SDL_GPUTextureRegion dst;
    SDL_GPUTexture *tex;
    void *map;

#ifdef _WIN32
    if (sD3D9) {
        void *texture = GsDx9_StripTexture(pix, w, h * count);
        free(pix); return (SDL_GPUTexture *)texture;
    }
#endif
    if (sGL) { // the OpenGL back end: a GL texture, its name carried in the pointer (ImGui's OpenGL3 backend takes it so)
        unsigned id = GsGl_StripTexture(pix, (int)w, (int)(h * count));
        free(pix);
        return (SDL_GPUTexture *)(intptr_t)id;
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.width = w;
    ci.height = h * count;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex = SDL_CreateGPUTexture(sDevice, &ci);
    if (tex == NULL) {
        free(pix);
        return NULL;
    }
    SDL_zero(tbi);
    tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbi.size = (Uint32)bytes;
    tb = SDL_CreateGPUTransferBuffer(sDevice, &tbi);
    map = SDL_MapGPUTransferBuffer(sDevice, tb, false);
    memcpy(map, pix, bytes);
    SDL_UnmapGPUTransferBuffer(sDevice, tb);
    free(pix);
    cmd = SDL_AcquireGPUCommandBuffer(sDevice);
    cp = SDL_BeginGPUCopyPass(cmd);
    SDL_zero(src);
    src.transfer_buffer = tb;
    src.pixels_per_row = w;
    src.rows_per_layer = h * count;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w = w;
    dst.h = h * count;
    dst.d = 1;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(sDevice, tb);
    return tex;
}

static SDL_GPUTexture *load_strip(const char *env, const char *rel, uint32_t *w, uint32_t *h, uint32_t *count,
                                  const char *what) {
    const char *paths[3];
    uint8_t *pix = NULL;
    size_t bytes = 0;
    SDL_GPUTexture *tex;
    uint32_t i;

    char inData[600];
    // in the stages / songs folder (plat_extras.c); the environment variable names another file
    snprintf(inData, sizeof(inData), "%s", rel);
    paths[0] = getenv(env);
    paths[1] = inData;
    paths[2] = NULL;
    for (i = 0; i < 3 && pix == NULL; i++) {
        if (paths[i] != NULL) {
            pix = read_strip(paths[i], &bytes, w, h, count);
        }
    }
    if (pix == NULL) {
        return NULL;
    }
    tex = make_strip(pix, bytes, *w, *h, *count);
    if (tex != NULL) {
        fprintf(stderr, "bt3: %s overlay: %u names, %ux%u each\n", what, *count, *w, *h);
    }
    return tex;
}

// ---- the side bars ----
// Where the picture is narrower than the window (a 4:3 picture, or "Menus in 4:3", in a wide window) the sides are
// black, or with "Side bars: Border" a panel with a border along the picture. Two pictures: the border (trim.png),
// kept in proportion at the picture's edge and as high as the picture, and the plain panel (fill.png), stretched
// over the rest, so a bar of any width looks right. Both are drawn as they are on the left and mirrored on the right.
// The program carries a gold set (port/assets/bars, art by the project's owner: 34 x 720 and 126 x 720). A player
// replaces it by putting a trim.png and / or a fill.png of any size into a folder "bars" next to the game.
#include "bars_png.h"
static SDL_GPUTexture *sBarTrim, *sBarFill;
static int sBars = -1; // 0 black, 1 the border, 2 the border with the fight panels; -1 not read yet
static int sBarsTried;
static int sChatSize = -1; // 0 = compact, 1 = normal, 2 = large; -1 = not read yet

static float sBarTrimShape = 34.0f / 720.0f; // the border's width over its height

static SDL_GPUTexture *bar_texture(const char *file, const unsigned char *png, size_t size, float *shape) {
    SDL_Surface *s = SDL_LoadPNG(file), *rgba; // the player's own picture, if there is one
    if (s != NULL) {
        fprintf(stderr, "bt3: side bars: %s, %d x %d\n", file, s->w, s->h);
    } else {
        s = SDL_LoadPNG_IO(SDL_IOFromConstMem(png, size), true);
    }
    uint8_t *pix;
    size_t bytes;
    if (s == NULL) {
        return NULL;
    }
    rgba = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (rgba == NULL) {
        return NULL;
    }
    if (shape != NULL && rgba->h > 0) {
        *shape = (float)rgba->w / (float)rgba->h;
    }
    bytes = (size_t)rgba->w * rgba->h * 4;
    pix = (uint8_t *)malloc(bytes);
    for (int y = 0; pix != NULL && y < rgba->h; y++) {
        memcpy(pix + (size_t)y * rgba->w * 4, (const uint8_t *)rgba->pixels + (size_t)y * rgba->pitch, (size_t)rgba->w * 4);
    }
    SDL_GPUTexture *tex = pix != NULL ? make_strip(pix, bytes, (uint32_t)rgba->w, (uint32_t)rgba->h, 1) : NULL;
    SDL_DestroySurface(rgba);
    return tex;
}

static bool bars_on(void) {
    if (sBars < 0) {
        sBars = Port_Setting("side_bars", 0);
        sBars = sBars < 0 ? 0 : sBars > 2 ? 2 : sBars;
    }
    return sBars != 0 && gUiPresentX > 0 && gUiPresentW > 0 && gUiPresentH > 0;
}

// For the fight panels (fight_panels_ui.cpp): textures of their own and the display faces.
static ImFont *sFontBold;
SDL_GPUTexture *Ui_Texture(uint8_t *pix, uint32_t w, uint32_t h) {
    return make_strip(pix, (size_t)w * h * 4, w, h, 1);
}
void Ui_TextureFree(SDL_GPUTexture *tex) {
    if (tex == NULL) {
        return;
    }
#ifdef _WIN32
    if (sD3D9) { GsDx9_StripTextureFree(tex); return; }
#endif
    if (sGL) {
        GsGl_StripTextureFree((unsigned)(intptr_t)tex);
    } else {
        SDL_ReleaseGPUTexture(sDevice, tex);
    }
}
ImFont *Ui_FontBold(void) { return sFontBold; }
bool FightPanels_Draw(ImDrawList *dl, float lx, float rx, float y0, float y1, float bar, float trim);

// The fight panels' two faces ("Name font", "Form font" in the settings): fonts of the game's own list (fonts.tsv,
// as the Menu font and the Dialogue / UI font), checked as those are (menu_text.c) and saved by their id; "Default"
// is the first of the list that can be used. A choice is loaded into the font atlas between two frames (fonts_apply).
struct PanelFont {
    const char *setting;
    ImFont *font;
    int choice;  // in use (0: Default)
    int pending; // chosen, loaded before the next frame (-1: nothing)
};
static PanelFont sPanelFont[2] = {{"panel_name_font_id", NULL, 0, -1}, {"panel_form_font_id", NULL, 0, -1}};
ImFont *Ui_FontName(void) { return sPanelFont[0].font; }
ImFont *Ui_FontForm(void) { return sPanelFont[1].font; }

// A choice of the list as an ImGui font (NULL: none can be used).
static ImFont *panel_font_load(int choice) {
    char path[1400];
    ImGuiIO &io = ImGui::GetIO();
    for (int i = choice > 0 ? choice : 1; i < MenuText_FontCount(); i++) {
        if (MenuText_FontFile(i, path, sizeof(path))) {
            return io.Fonts->AddFontFromFileTTF(path, 48.0f);
        }
        if (choice > 0) {
            break;
        }
    }
    return NULL;
}

static void fonts_apply(void) {
    for (PanelFont &pf : sPanelFont) {
        if (pf.pending < 0) {
            continue;
        }
        ImFont *f = panel_font_load(pf.pending);
        if (f != NULL) {
            if (pf.font != NULL) {
                ImGui::GetIO().Fonts->RemoveFont(pf.font);
            }
            pf.font = f;
            pf.choice = pf.pending;
            Port_SettingSave(pf.setting, MenuText_FontId(pf.choice));
            Port_SettingsWrite();
        }
        pf.pending = -1;
    }
}

// A combo of the game's fonts; the choice is loaded before the next frame.
static void font_picker(const char *label, PanelFont &pf, const char *tip) {
    if (ImGui::BeginCombo(f1_text(label), f1_text(pf.choice > 0 ? MenuText_FontName(pf.choice) : "Default"))) {
        if (ImGui::Selectable(f1_text("Default"), pf.choice == 0)) {
            pf.pending = 0;
        }
        for (int i = 1; i < MenuText_FontCount(); i++) {
            ImGui::PushID(i);
            if (ImGui::Selectable(f1_text(MenuText_FontName(i)), pf.choice == i)) {
                pf.pending = i;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("%s", f1_text(tip));
}

static void bars_draw(void) {
    if (!sBarsTried) {
        sBarsTried = 1;
        sBarTrim = bar_texture("bars/trim.png", kBarsTrimPng, sizeof(kBarsTrimPng), &sBarTrimShape);
        sBarFill = bar_texture("bars/fill.png", kBarsFillPng, sizeof(kBarsFillPng), NULL);
    }
    if (sBarTrim == NULL || sBarFill == NULL) {
        return;
    }
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float y0 = (float)gUiPresentY, y1 = y0 + (float)gUiPresentH;
    float bar = (float)gUiPresentX; // the width of each side
    float trim = (float)gUiPresentH * sBarTrimShape;
    float u0 = 0.0f;
    if (trim > bar) { // a bar narrower than the border: the border's inner part
        u0 = 1.0f - bar / trim;
        trim = bar;
    }
    float lx = (float)gUiPresentX, rx = lx + (float)gUiPresentW; // the picture's two edges
    ImTextureRef tt((ImTextureID)(intptr_t)sBarTrim), ft((ImTextureID)(intptr_t)sBarFill);
    if (bar > trim) {
        dl->AddImage(ft, ImVec2(lx - bar, y0), ImVec2(lx - trim, y1), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        dl->AddImage(ft, ImVec2(rx + trim, y0), ImVec2(rx + bar, y1), ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f));
        // In a fight: each side's fighter over the plain panel, with the border along the picture at half its size
        // (in proportion, twice down the height) so the fighter has more room (asked for).
        const int tiles = 2;
        float thin = trim / tiles;
        if (sBars == 2 && FightPanels_Draw(dl, lx, rx, y0, y1, bar, thin)) {
            float th = (y1 - y0) / tiles;
            for (int i = 0; i < tiles; i++) {
                float a = y0 + th * i, b = a + th;
                dl->AddImage(tt, ImVec2(lx - thin, a), ImVec2(lx, b), ImVec2(u0, 0.0f), ImVec2(1.0f, 1.0f));
                dl->AddImage(tt, ImVec2(rx, a), ImVec2(rx + thin, b), ImVec2(1.0f, 0.0f), ImVec2(u0, 1.0f));
            }
            return;
        }
    }
    dl->AddImage(tt, ImVec2(lx - trim, y0), ImVec2(lx, y1), ImVec2(u0, 0.0f), ImVec2(1.0f, 1.0f));
    dl->AddImage(tt, ImVec2(rx, y0), ImVec2(rx + trim, y1), ImVec2(1.0f, 0.0f), ImVec2(u0, 1.0f));
}

static void load_strips(void) {
    char path[600];
    // The names of added stages and songs are drawn by this overlay, over the menu: from a strip of pre-rendered
    // names if the install script made one (names.rgba in the folder), else as text (frame_build). Either way the
    // game leaves the name to the overlay, so the flags are set whenever the overlay exists.
    snprintf(path, sizeof(path), "%s/names.rgba", PortStages_Dir());
    sNameTex = PortStages_Dir()[0] != '\0' ? load_strip("BT3_STAGE_NAMES", path, &sNameW, &sNameH, &sNameCount, "stage-name") : NULL;
    snprintf(path, sizeof(path), "%s/names.rgba", PortSongs_Dir());
    sSongTex = PortSongs_Dir()[0] != '\0' ? load_strip("BT3_SONG_NAMES", path, &sSongW, &sSongH, &sSongCount, "song-name") : NULL;
    if (sNameTex == NULL) { sNameCount = 0; }
    if (sSongTex == NULL) { sSongCount = 0; }
    gUiNameReady = 1;
    gUiSongReady = 1;
}

// The size of the windows drawn here: the style below and every width written in this file are for a picture 1080
// pixels high, and a taller one makes them that much larger (2880 x 1800: 1.67), times the player's "Menu size"
// (setting ui_scale, in percent, 100 by default). U() is a width or height of this file at that size.
static float sUiScale = 1.0f;
static int sUiScalePct = -1;  // (the setting, read once)
static ImGuiStyle sBaseStyle; // (as style() makes it)
static inline float U(float v) { return v * sUiScale; }

static void ui_scale_apply(void) {
    ImGuiIO &io = ImGui::GetIO();
    float s = io.DisplaySize.y > 1080.0f ? io.DisplaySize.y / 1080.0f : 1.0f;
    if (sUiScalePct < 0) {
        sUiScalePct = Port_Setting("ui_scale", 100);
    }
    s *= (float)sUiScalePct / 100.0f;
    s = s < 0.5f ? 0.5f : s > 4.0f ? 4.0f : s;
    if (s != sUiScale) {
        ImGuiStyle &st = ImGui::GetStyle();
        float base = st.FontSizeBase;
        sUiScale = s;
        st = sBaseStyle;
        st.FontSizeBase = base;
        st.ScaleAllSizes(s);
        st.FontScaleMain = s;
    }
}

static void style() {
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 10.0f;
    st.FrameRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.PopupRounding = 6.0f;
    st.WindowPadding = ImVec2(18.0f, 16.0f);
    st.FramePadding = ImVec2(10.0f, 6.0f);
    st.ItemSpacing = ImVec2(10.0f, 9.0f);
    st.WindowBorderSize = 0.0f;
    st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    ImVec4 *c = st.Colors;
    const ImVec4 accent(0.96f, 0.55f, 0.13f, 1.0f), accentDim(0.96f, 0.55f, 0.13f, 0.55f);
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.11f, 0.96f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.12f, 0.17f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.17f, 0.23f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.32f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.29f, 0.38f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonHovered] = accentDim;
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Header] = ImVec4(0.96f, 0.55f, 0.13f, 0.30f);
    c[ImGuiCol_HeaderHovered] = accentDim;
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_Tab] = ImVec4(0.13f, 0.15f, 0.20f, 1.0f);
    c[ImGuiCol_TabHovered] = accentDim;
    c[ImGuiCol_TabSelected] = ImVec4(0.96f, 0.55f, 0.13f, 0.80f);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.14f, 0.98f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    sBaseStyle = st;
    sUiScale = 1.0f;
}

static void language_init() {
    const char *override = getenv("BT3_MENU_LANG");
    char saved[16];
    Port_SettingText("menu_lang", "", saved, sizeof(saved));
    sLanguageCount = MenuTranslation_Available(sLanguages, (int)(sizeof(sLanguages) / sizeof(sLanguages[0])));
    sLanguageChoice = 0;
    const char *preferred = override != NULL && override[0] != '\0' ? override : saved[0] != '\0' ? saved : "en";
    for (int i = 0; i < sLanguageCount; i++) if (strcmp(sLanguages[i].locale, preferred) == 0) sLanguageChoice = i;
}

extern "C" void Ui_LanguageChosen(const char *locale) {
    if (locale == NULL) return;
    for (int i = 0; i < sLanguageCount; i++) {
        if (strcmp(sLanguages[i].locale, locale) == 0) {
            sLanguageChoice = i;
            sLanguageRestartPending = false;
            break;
        }
    }
}

static void language_choose(int choice) {
    if (choice < 0 || choice >= sLanguageCount) return;
    MenuTranslation_SaveChoice(sLanguages[choice].locale);
    sLanguageChoice = choice;
    sLanguageRestartPending = true;
}

int Ui_Init(SDL_Window *window, SDL_GPUDevice *device, void *gl_context, const char *gpu_name) {
    sGL = 0;
    static const char *fonts[] = { // a proportional system font if there is one; else the library's built-in font
        "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    ImGui_ImplSDLGPU3_InitInfo info;

    snprintf(sGpuName, sizeof(sGpuName), "%s", gpu_name && gpu_name[0] ? gpu_name : "Unavailable");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL; // no window-layout file next to the game
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad; // (a controller opens it too: START + SELECT)
    if (getenv("BT4_RENDER_TEST")) { io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; }
    style();
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        FILE *fp = fopen(fonts[i], "rb");
        if (fp != NULL) {
            fclose(fp);
            io.Fonts->AddFontFromFileTTF(fonts[i], 18.0f);
            break;
        }
    }
    { // the fight panels' faces: the two chosen ones (sPanelFont), and the list's first where one cannot be had
        if (io.Fonts->Fonts.Size == 0) {
            io.Fonts->AddFontDefault(); // (the window's font stays the first)
        }
        sFontBold = panel_font_load(0);
        for (PanelFont &pf : sPanelFont) {
            pf.choice = MenuText_FontChoice(Port_Setting(pf.setting, 0));
            pf.font = panel_font_load(pf.choice);
        }
    }
#ifdef _WIN32
    if (sD3D9) {
        if (!ImGui_ImplSDL3_InitForD3D(window) || !ImGui_ImplDX9_Init((IDirect3DDevice9 *)sD3D9)) { return 0; }
    } else
#endif
    if (device == NULL) { // the OpenGL back end (gs_gl.c), with its context current
        sGL = 1;
        if (!ImGui_ImplSDL3_InitForOpenGL(window, (SDL_GLContext)gl_context) || !ImGui_ImplOpenGL3_Init(GsGl_DirectXVersion() ? "#version 300 es" : "#version 330 core")) {
            return 0;
        }
    } else {
        if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
            return 0;
        }
        info.Device = device;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        if (!ImGui_ImplSDLGPU3_Init(&info)) {
            return 0;
        }
    }
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll, NULL, 0); // any connected controller works the window
    sDevice = device;
    sReady = true;
    language_init();
    load_strips(); /* the stage- and song-name overlays; the ready flags stay 0 if there is no strip */
    if (getenv("BT3_UI_OPEN") != NULL && !MenuTranslation_BootPending()) {
        sForceTab = atoi(getenv("BT3_UI_OPEN"));
        if (sForceTab == 9) { // testing: the online screen
            sForceTab = -1;
            gPortNetMenuRequest = 1;
        }
        if (sForceTab == 10) { // testing: the Tech tab (8 is the exit question's)
            sForceTab = 10;
        } else if (sForceTab == 11) { // testing: the Mods tab
            sForceTab = 9;
        } else if (sForceTab == 8) { // testing: the exit question
            sForceTab = -1;
            sQuit = true;
            PortQuit_Request();
        }
        if (!strcmp(getenv("BT3_UI_OPEN"), "spectator")) sForceTab = 8;
        if (!strcmp(getenv("BT3_UI_OPEN"), "updates")) sForceTab = 11;
        sOpen = !sQuit;
        gPortOverlayOpen = 1;
    }
    return 1;
}

int Ui_InitD3D9(SDL_Window *window, void *device, const char *gpu_name) {
#ifdef _WIN32
    sD3D9 = device;
    int ok = Ui_Init(window, NULL, NULL, gpu_name);
    if (!ok) {
        if (ImGui::GetCurrentContext()) {
            if (ImGui::GetIO().BackendRendererUserData) { ImGui_ImplDX9_Shutdown(); }
            if (ImGui::GetIO().BackendPlatformUserData) { ImGui_ImplSDL3_Shutdown(); }
            ImGui::DestroyContext();
        }
        sD3D9 = NULL; sReady = false;
    }
    return ok;
#else
    return 0;
#endif
}
void Ui_D3D9Reset(int create) {
#ifdef _WIN32
    if (!sD3D9) { return; }
    if (create) { ImGui_ImplDX9_CreateDeviceObjects(); }
    else { ImGui_ImplDX9_InvalidateDeviceObjects(); }
#endif
}

// The game gets nothing from keyboard or controller while the window or the exit question is up; and the button
// that closed either is not the game's.
static void input_owner(void) {
    int up = sOpen || sQuit || sUpdPromptOpen;
    if (gPortOverlayOpen && !up) {
        PortInput_Swallow();
    }
    gPortOverlayOpen = up;
}

static void set_open(bool open) {
    if (open && sChat) {
        chat_close();
    }
    sOpen = open;
    sNet = false;
    sCapKind = 0;
    gPortInputCapture = 0;
    input_owner();
}

static void capture(int kind, int player, int action) {
    sCapKind = kind; sCapPlayer = player; sCapAction = action; gPortInputCapture = 1;
    sCapUntil = SDL_GetTicks() + 5000;
    sPadHold = true;
}

// A controller button pressed in the window (not while it waits for a binding, a list is open or a field is edited).
static bool pad_pressed(ImGuiKey key) {
    return sCapKind == 0 && !sPadHold && !sQuit && !ImGui::IsAnyItemActive() &&
           !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && ImGui::IsKeyPressed(key, false);
}

extern "C" void PortA11y_Say(const char *text, int flags);
extern "C" const char *PortA11y_Label(const char *label);
static void say_quit_button(int which, int flags);

int Ui_Pauses(void) { /* the settings window is open and the game waits behind it (Dragon Rage's ui.h) */
    return sReady && sOpen && !sNet;
}

int Ui_QuitAsk(void) {
    if (!sReady) {
        return 0;
    }
    if (sQuit) { return 1; }
    if (sOpen) { set_open(false); }
    sQuit = true;
    PortQuit_Request();
    input_owner();
    const char *translated = MenuTranslation_PortText(PORT_TEXT_QUIT_CONFIRM);
    PortA11y_Say(translated ? translated : PortA11y_Label("Close the game? Progress that was not saved is lost."), 1);
    say_quit_button(1, 0);
    PortA11y_Say(PortA11y_Label("Left and right choose, Enter confirms, Escape goes back to the game"), 0);
    return 1;
}

// The screen reader (a11y.c): it cannot see this window's question, which is drawn and not one of the game's screens,
// so the question, the button under the keyboard's focus and the keys are said here. Nothing while it is off.
static void say_quit_button(int which, int flags) {
    char text[128], pos[32];
    snprintf(pos, sizeof(pos), PortA11y_Label(", %d of %d"), which + 1, 2);
    snprintf(text, sizeof(text), "%s%s", PortA11y_Label(which == 0 ? "Exit" : "Cancel"), pos);
    PortA11y_Say(text, flags);
}

static void build_quit(void) {
    static int said = -1; // the button last said: 0 Exit, 1 Cancel
    ImGuiIO &io = ImGui::GetIO();
    bool stay = sQuitNo;
    int focus = -1;

    sQuitNo = false;
    char title[128];
    snprintf(title, sizeof(title), "%s###exit-game", f1_text("Exit game"));
    if (!ImGui::IsPopupOpen(title)) {
        ImGui::OpenPopup(title);
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        const char *kAsk = f1_text("Close the game?\nUnsaved progress will be lost.");
        // the two buttons under the middle of the question (the window is as wide as its text)
        float line = ImGui::CalcTextSize(kAsk).x, pair = U(140.0f) * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        ImGui::TextUnformatted(kAsk);
        ImGui::Spacing();
        if (line > pair) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (line - pair) * 0.5f);
        }
        if (ImGui::Button(f1_text("Exit"), ImVec2(U(140.0f), 0.0f))) {
            exit(0);
        }
        focus = ImGui::IsItemFocused() ? 0 : focus;
        ImGui::SameLine();
        if (ImGui::Button(f1_text("Cancel"), ImVec2(U(140.0f), 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
            stay = true;
        }
        focus = ImGui::IsItemFocused() ? 1 : focus;
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetItemDefaultFocus(); // (on Cancel: a second press of the same button does not close the game)
            PortA11y_Say(kAsk, 1);
            say_quit_button(1, 0);
            PortA11y_Say(PortA11y_Label("Left and right choose, Enter confirms, Escape goes back to the game"), 0);
            said = 1;
        } else if (focus >= 0 && focus != said) {
            say_quit_button(focus, 1);
            said = focus;
        }
        if (stay) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (stay) {
        sQuit = false;
        PortQuit_Dismiss();
        input_owner();
        PortA11y_Say(PortA11y_Label("Back to the game"), 1);
    }
}

// Dragon Net Battle: the first online window of the port, behind the main menu's item 4. Tenkaichi Budokai Online's
// screens (src/menu/port_tbo.c) do everything it does; it is opened from F1 > Online > Advanced, and listed in the
// main menu only when the player asks for it there (setting net_dnb_menu). Since 0.1.39 a match with a friend is a
// lobby's (gs/net_lobby.c), as on the screens: the Host tab opens a lobby (with a code, or on a port) with its rules,
// the Join tab joins one (its code, or its owner's address); the lobby itself is the screens' page, which the main
// menu opens as soon as this game is in one. There is no room of two and no "Watch" any more: a spectator is a
// lobby's member in a spectator's place. Quick match is as it was.
extern "C" {
extern volatile int gPortNetMenuRequest;
extern volatile int gPortNetWindowClose; // gs/state.c
}

static void net_open(void) {
    set_open(true);
    sNet = true;
    Port_LobbyHelloAsk(); // (once per opening: is there a newer release, and is this one still let in)
    gPortInputCapture = 1; // the game takes no input at all while this is open (it sits on the main menu behind it)
}

// Quick match (the third tab of the online screen): what stands under the queues.
static bool sNetBusy;   // the lobby is waiting or connecting
static int sQueue = -1; // the queue chosen: 0 single, 1 team, 2..4 DP battle of 10 / 15 / 20 DP

static void quick_status(int state, int &lastState, const char *name) {
    // A search ends by itself after three minutes: a window left open does not go on asking the service for hours,
    // and the player is told that nobody was there instead of being left to wonder. One press searches again.
    static const Uint64 kSearchMs = 180000;
    static Uint64 since;
    if (state == 3 && since != 0 && SDL_GetTicks() - since > kSearchMs) {
        Port_LobbyCancel();
        lastState = -5;
        state = 0;
    }
    if (state == 3 || state == 1) {
        ImGui::TextUnformatted(f1_text(Port_LobbyCheckingData() ? "Checking your game data... The search begins when it is done."
                               : state == 1 ? "Found a player. Connecting..." : "Searching for a player in this queue..."));
        if (state == 3 && since != 0 && !Port_LobbyCheckingData()) {
            Uint64 left = kSearchMs - (SDL_GetTicks() - since);
            ImGui::TextDisabled(f1_text("Searching for %d:%02d more. Leave the window open."), (int)(left / 60000), (int)(left / 1000 % 60));
        }
        if (ImGui::Button(f1_text("Cancel"), ImVec2(U(120.0f), 0.0f))) {
            Port_LobbyCancel();
            lastState = 0;
        }
        return;
    }
    if (lastState == -3) {
        ImGui::TextWrapped("%s", Port_LobbyError());
    } else if (lastState == -4) {
        ImGui::TextWrapped(f1_text("The games could not establish a connection. Search again for another player."));
    } else if (lastState == -2) {
        ImGui::TextWrapped(f1_text("The other player's game is a different version. Search again."));
    } else if (lastState == -5) {
        ImGui::TextWrapped(f1_text("Nobody was searching this queue in the last three minutes. Search again, or try another queue."));
    }
    // Quick match is with strangers: the original disc's data only (gs/net.c refuses the search otherwise; said here
    // before it is asked for). With friends, by code or address, the same data on both sides is enough.
    const char *refused = Port_LobbyQuickRefusal();
    if (refused != NULL && refused[0] != '\0' && lastState != -3) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.35f, 1.0f));
        ImGui::TextWrapped("%s", refused);
        ImGui::PopStyleColor();
    } else if (refused != NULL) {
        ImGui::TextDisabled(f1_text("Checking your game data..."));
    }
    ImGui::BeginDisabled(refused != NULL && refused[0] != '\0');
    bool search = ImGui::Button(f1_text(lastState == -5 ? "Search again" : "Search for a match"), ImVec2(U(200.0f), 0.0f));
    ImGui::EndDisabled();
    if (search) {
        since = SDL_GetTicks() + 1;
        // The same for everyone in a quick match, whoever turns out to be the host: rollback of up to 4 frames and
        // the input delay measured from the connection. (The Host tab's own choices are for matches with someone
        // you can agree them with; a stranger's saved ones were not to be inherited unseen.)
        Port_NetOptions(Port_RollCan() ? 4 : 0, -2);
        Port_NetSpeedSet(Port_NetSpeedStandard()); // (every quick match: the competitive standard)
        Port_NetRulesSet(sQueue == 0 ? 0 : sQueue == 1 ? 1 : 2, sQueue >= 2 ? sQueue - 2 : 0, 4);
        Port_NetNameSet(name);
        Port_SettingTextSave("net_name", name);
        Port_SettingSave("net_queue", sQueue);
        Port_SettingsWrite();
        lastState = 0;
        Port_LobbyStartQuick(sQueue);
    }
}

static void build_net(void) {
    static char name[17] = "Player", address[64] = "", port[8] = "7000";
    static int tab, roll = -1, delay = 1, battle = -1, timeLimit = 3, speed = -1;
    static bool useCode = true;       // a lobby with a code (no port forwarding) or on a port
    static char roomCode[8] = "";     // the code to join with
    static int lastState = 0;         // what the search said last (for its messages after a failure)
    ImGuiIO &io = ImGui::GetIO();
    bool open = true;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(U(560.0f), 0.0f), ImGuiCond_Appearing);
    if (pad_pressed(ImGuiKey_GamepadFaceRight)) { // Circle / B: out of the window, as its close button
        open = false;
    }
    if (ImGui::Begin("Dragon Net Battle", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        {
            int min = 0, latest = 0, mine = Port_LobbyRelease(&min, &latest);
            if (min > mine) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.35f, 1.0f));
                ImGui::TextWrapped(f1_text("Update available. Online servers are unreachable until you update."));
                ImGui::PopStyleColor();
                ImGui::TextDisabled(f1_text("You have 0.%d.%d; 0.%d.%d or newer is needed. github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases"), mine / 100, mine % 100, min / 100, min % 100);
                ImGui::Separator();
            } else if (latest > mine) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Update available: release 0.%d.%d.", latest / 100, latest % 100);
                ImGui::TextDisabled("github.com/NauTaro/DBZ-BT4-Tag-Team-Decomp/releases");
                ImGui::Separator();
            }
        }
        ImGui::TextWrapped(f1_text("Fight a player on another computer. One of you hosts, the other joins with the host's address."));
        ImGui::Spacing();
        ImGui::SetNextItemWidth(U(260.0f));
        {
            static bool read;
            if (!read) { // the name used last time
                read = true;
                Port_SettingText("net_name", "Player", name, sizeof(name));
            }
        }
        ImGui::InputText(f1_text("Player name"), name, sizeof(name));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Port_SettingTextSave("net_name", name);
            Port_SettingsWrite();
        }
        ImGui::Spacing();
        if (ImGui::BeginTabBar("net")) {
            // BT3_UI_NET_TAB=host|quick|join (testing, with BT3_UI_OPEN=9): the online screen opens on that tab
            static const char *firstTab = getenv("BT3_UI_NET_TAB");
            static int firstFrames = 10; // (not on the tab bar's first frame alone: it has no tabs yet then)
            auto tabFlags = [](const char *which) {
                ImGuiTabItemFlags f = firstTab != NULL && strcmp(firstTab, which) == 0 ? ImGuiTabItemFlags_SetSelected : 0;
                return f;
            };
            if (ImGui::BeginTabItem(f1_text("Host"), NULL, tabFlags("host"))) {
                const char *const kRoll[] = {f1_text("Off (wait for each other)"), f1_text("Up to 2 frames"), f1_text("Up to 4 frames"), f1_text("Up to 6 frames"), f1_text("Up to 8 frames")};
                tab = 0;
                ImGui::Checkbox(f1_text("Use a code (no port forwarding needed)"), &useCode);
                ImGui::SetItemTooltip(f1_text("Your lobby gets a short code to tell the others, and the games find each other through a\n"
                                      "small matchmaking service. Works with most home routers. Without it the lobby is on a port,\n"
                                      "which must be reachable from the others (port forwarding or a virtual network)."));
                ImGui::BeginDisabled(useCode);
                ImGui::SetNextItemWidth(U(120.0f));
                ImGui::InputText(f1_text("Port"), port, sizeof(port), ImGuiInputTextFlags_CharsDecimal);
                ImGui::EndDisabled();
                if (roll < 0) { // the choices of last time (new names: the first ones' defaults were 4 frames and delay 1)
                    roll = Port_RollCan() ? Port_Setting("net_rollback2", 4) / 2 : 0;
                    delay = Port_Setting("net_delay2", -1) + 1; // 0 = automatic, else the delay + 1
                    if (roll < 0 || roll > 4) { roll = 4; }
                    if (delay < 0 || delay > 7) { delay = 0; }
                }
                ImGui::BeginDisabled(!Port_RollCan());
                ImGui::SetNextItemWidth(U(260.0f));
                ImGui::Combo(f1_text("Rollback"), &roll, kRoll, 5);
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip(f1_text("With rollback your own moves come out at once and the game corrects itself when the\n"
                                      "other player's buttons arrive. More frames cope with a slower connection and cost\n"
                                      "more processor time. Off: both games wait for each other every frame."));
                }
                ImGui::SetNextItemWidth(U(260.0f));
                {
                    const char *const kDelay[] = {f1_text("Automatic (from the connection)"), f1_text("0 frames"), f1_text("1 frame"), f1_text("2 frames"), f1_text("3 frames"),
                                                         f1_text("4 frames"), f1_text("5 frames"), f1_text("6 frames")};
                    ImGui::Combo(f1_text("Input delay"), &delay, kDelay, 8);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(f1_text("How long after a press your own move comes out. A slow connection needs more, or the game\n"
                                      "has to guess too far ahead and stutters. Automatic measures the connection when the match\n"
                                      "starts: 1 frame on a good one, more on a slow one."));
                }
                {
                    // The match's rules: the game's own versus menu is not shown online, so they are chosen here.
                    const char *const kBattle[] = {f1_text("Single battle"), f1_text("Team battle"), f1_text("DP battle (10 DP)"), f1_text("DP battle (15 DP)"), f1_text("DP battle (20 DP)")};
                    const char *const kTime[] = {f1_text("60 seconds"), f1_text("90 seconds"), f1_text("180 seconds"), f1_text("240 seconds"), f1_text("No limit")};
                    if (battle < 0) {
                        battle = Port_Setting("net_battle", 0);
                        timeLimit = Port_Setting("net_time", 3);
                        if (battle < 0 || battle > 4) { battle = 0; }
                        if (timeLimit < 0 || timeLimit > 4) { timeLimit = 3; }
                    }
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(U(260.0f));
                    ImGui::Combo(f1_text("Battle"), &battle, kBattle, 5);
                    ImGui::SetNextItemWidth(U(260.0f));
                    ImGui::Combo(f1_text("Time limit"), &timeLimit, kTime, 5);
                    {
                        if (speed < 0) {
                            speed = Port_Setting("net_speed_host", Port_NetSpeedStandard()) /* (a new name with 0.1.31: the standard went from 120 to 114) */;
                        }
                        ImGui::SetNextItemWidth(U(260.0f));
                        ImGui::SliderInt(f1_text("Game speed"), &speed, 100, 150, "%d%%", ImGuiSliderFlags_AlwaysClamp);
                        ImGui::SetItemTooltip(f1_text("How fast the match runs, for both players; the sound keeps its pitch.\n"
                                              "100%% is the game's own speed. %d%% is the competitive standard used by quick matches."), Port_NetSpeedStandard());
                    }
                }
                ImGui::TextDisabled(f1_text("The lobby's first rules; its owner changes them on its page. Everyone needs the same game data."));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(f1_text("Quick match"), NULL, tabFlags("quick"))) {
                const char *const kQueue[] = {f1_text("Single battle"), f1_text("Team battle"), f1_text("DP battle (10 DP)"), f1_text("DP battle (15 DP)"), f1_text("DP battle (20 DP)")};
                static Uint64 asked;
                bool busy = sNetBusy; // (a search is under way: the queue is not changed under it)
                tab = 2;
                if (sQueue < 0) {
                    sQueue = Port_Setting("net_queue", 0);
                    if (sQueue < 0 || sQueue > 4) { sQueue = 0; }
                }
                {
                    // How many wait: asked when the tab is opened and then every thirty seconds, but only while the
                    // player is doing something here (a window left open all day asks nothing after two minutes),
                    // and not during a search (the search hears from the service anyway).
                    static Uint64 active;
                    bool moved = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f || ImGui::IsAnyItemActive();
                    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END && !moved; k++) {
                        moved = ImGui::IsKeyDown((ImGuiKey)k);
                    }
                    if (moved || active == 0) {
                        active = SDL_GetTicks() + 1;
                    }
                    if (!sNetBusy && (asked == 0 || (SDL_GetTicks() - asked > 30000 && SDL_GetTicks() - active < 120000))) {
                        asked = SDL_GetTicks() + 1;
                        Port_LobbyQueuesAsk();
                    }
                }
                ImGui::TextWrapped(f1_text("Be matched with whoever is searching the same queue. All quick matches have no time limit and run at %d%% speed, "
                                   "between games with the original disc's data."), Port_NetSpeedStandard());
                ImGui::BeginDisabled(busy);
                for (int i = 0; i < 5; i++) {
                    char label[80];
                    int n = Port_LobbyQueueWaiting(i);
                    if (n < 0) {
                        snprintf(label, sizeof(label), "%s", kQueue[i]);
                    } else {
                        snprintf(label, sizeof(label), f1_text("%s   (%d searching)###q%d"), kQueue[i], n, i);
                    }
                    ImGui::RadioButton(label, &sQueue, i);
                    UiA11y_Radio(sQueue == i);
                }
                ImGui::EndDisabled();
                ImGui::TextDisabled(f1_text("The two games connect to each other directly when they can, as in most online fighting"));
                ImGui::TextDisabled(f1_text("games: a player who inspects their own network traffic can see the other's address."));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(f1_text("Join"), NULL, tabFlags("join"))) {
                tab = 1;
                ImGui::SetNextItemWidth(U(120.0f));
                ImGui::InputText(f1_text("Room code"), roomCode, sizeof(roomCode), ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
                ImGui::SameLine();
                if (ImGui::Button(f1_text("Paste"))) { // the letters and digits of what was copied, as capitals
                    char *clip = SDL_GetClipboardText();
                    size_t n = 0;
                    for (const char *c = clip; c != NULL && *c != '\0' && n + 1 < sizeof(roomCode); c++) {
                        if ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')) {
                            roomCode[n++] = (char)(*c >= 'a' ? *c - 32 : *c);
                        }
                    }
                    roomCode[n] = '\0';
                    SDL_free(clip);
                }
                ImGui::TextDisabled(f1_text("or, without a code, the host's address and port:"));
                ImGui::BeginDisabled(roomCode[0] != '\0');
                ImGui::SetNextItemWidth(U(260.0f));
                ImGui::InputText(f1_text("Host's address"), address, sizeof(address));
                ImGui::SetNextItemWidth(U(120.0f));
                ImGui::InputText(f1_text("Port"), port, sizeof(port), ImGuiInputTextFlags_CharsDecimal);
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
            if (firstTab != NULL && --firstFrames <= 0) {
                firstTab = NULL;
            }
        }
        ImGui::Spacing();
        {
            int state = Port_LobbyPoll();
            sNetBusy = state == 1 || state == 3;
            if (state == 5) { // the code or address was a lobby's: this game is its member; its screen is the game's
                Port_LobbyCancel(); // (the lobby has the socket now)
                const char *said = MenuTranslation_PortText(PORT_TEXT_LOBBY_JOINED);
                Port_UiNotice(said != NULL ? said : "You are in the lobby. Its screen opens from the main menu (Tenkaichi Budokai Online).");
                set_open(false);
                ImGui::End();
                return;
            }
            if (state == 2) {
                Port_LobbyLaunch(); // the other player is there: the game turns into the session at its next blank
                set_open(false);
                sNet = false;
            }
            bool byCode = tab == 0 ? useCode : roomCode[0] != '\0';
            if (state != 0) {
                lastState = state;
            }
            if (tab == 2) {
                quick_status(state, lastState, name);
            } else if (state == 3 || state == 1) { // joining a code: asking the service, then reaching the lobby's owner
                ImGui::TextUnformatted(f1_text(Port_LobbyCheckingData() ? "Checking your game data... Connecting begins when it is done."
                                       : state == 1 ? "Found the lobby. Connecting to its owner..." : "Looking for the lobby..."));
                if (ImGui::Button(f1_text("Cancel"), ImVec2(U(120.0f), 0.0f))) {
                    Port_LobbyCancel();
                    lastState = 0;
                }
            } else {
                if (lastState == -3) {
                    ImGui::TextWrapped("%s", Port_LobbyError());
                } else if (lastState == -4) {
                    ImGui::TextWrapped(f1_text("The games could not establish a connection. "
                                       "Hosting on a port (with port forwarding or a virtual network such as Tailscale) still works."));
                }
                bool can = Port_OnlineBlocked() == NULL && (byCode ? (tab == 0 || strlen(roomCode) == 6) : atoi(port) > 0 && atoi(port) < 65536 && (tab == 0 || address[0] != '\0'));
                bool lobby = false; // a lobby was opened or joined: its page is the screens' (the main menu opens it)
                ImGui::BeginDisabled(!can);
                if (ImGui::Button(f1_text(tab == 0 ? "Open a lobby" : "Join the lobby"), ImVec2(U(200.0f), 0.0f))) {
                    Port_NetNameSet(name);
                    Port_SettingTextSave("net_name", name);
                    lastState = 0;
                    if (tab == 0) {
                        if (roll < 0) {
                            roll = Port_RollCan() ? 4 : 0;
                        }
                        if (battle < 0) {
                            battle = 0;
                        }
                        if (speed < 0) {
                            speed = Port_NetSpeedStandard();
                        }
                        Port_SettingSave("net_speed_host", speed);
                        Port_SettingSave("net_battle", battle);
                        Port_SettingSave("net_time", timeLimit);
                        Port_SettingSave("net_rollback2", roll * 2);
                        Port_SettingSave("net_delay2", delay - 1);
                        Port_LobbyCancel();
                        Port_LobbyCreate(useCode ? 1 : 0, useCode ? 0 : atoi(port), battle, timeLimit, speed, roll * 2, delay); // (the lobby's delay: 0 automatic, else frames + 1, as here)
                        lobby = true;
                    } else if (byCode) {
                        Port_NetOptions(-1, -1);
                        Port_LobbyStartCode(0, roomCode); // (a lobby's code: Port_LobbyPoll says 5, below, and this game is its member)
                    } else {
                        Port_LobbyCancel();
                        Port_LobbyJoin(address, atoi(port));
                        lobby = true;
                    }
                    Port_SettingsWrite();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button(f1_text("Back"), ImVec2(U(120.0f), 0.0f))) {
                    open = false;
                }
                if (lobby) {
                    const char *said = MenuTranslation_PortText(PORT_TEXT_LOBBY_JOINED);
                    Port_UiNotice(said != NULL ? said : "You are in the lobby. Its screen opens from the main menu (Tenkaichi Budokai Online).");
                    set_open(false);
                    ImGui::End();
                    return;
                }
                if (state == -2) {
                    ImGui::TextWrapped(f1_text("The other game is a different version. Everyone needs the same release."));
                } else if (state < 0) {
                    ImGui::TextWrapped(f1_text("The search could not be started."));
                }
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled(f1_text("In a match both players get the same roster, with everything unlocked, and your own save is"));
        ImGui::TextDisabled(f1_text("left alone. When the match ends everyone is back on the lobby's page."));
    }
    ImGui::End();
    if (!open) {
        Port_LobbyCancel();
        set_open(false);
    }
}

void Ui_Toggle(void) {
    if (sReady && !sQuit && !sUpdPromptOpen) {
        set_open(!sOpen);
    }
}

static void bind(int value) {
    if (sCapAction == PORT_ACTIONS) { // the row "Open this window"
        if (sCapKind == 1) {
            *PortInput_OverlayKey() = value > 0 ? value : SDL_SCANCODE_F1; // (cleared = F1 again: a keyboard always has a way in)
        } else if (value < PORT_PAD_LT) { // (a button, not a trigger)
            *PortInput_OverlayPad() = value;
        }
    } else if (sCapAction == PORT_ACTIONS + 1) { // the row "Open online chat"
        if (sCapKind == 1) {
            *PortInput_ChatKey() = value > 0 ? value : SDL_SCANCODE_TAB;
        }
    } else if (sCapKind == 1) {
        PortInput_Keys(sCapPlayer)[sCapAction] = value;
    } else {
        PortInput_PadButtons(sCapPlayer)[sCapAction] = value;
    }
    PortInput_Save();
}

static bool chat_can_open(void) {
    return sReady && !sOpen && !sQuit && PortChat_Enabled() && Port_NetActive() && !Port_NetSpectating();
}

static void chat_open(void) {
    sChat = true;
    sChatFocus = true;
    sChatKeyHeld = true;
    sChatSoon = false;
    gPortChatTyping = 1;
    ImGui::GetIO().ClearInputKeys(); // (it saw none of the keys while the line was put away: no key is down for it)
}

static void chat_close(void) {
    sChat = false;
    gPortChatTyping = 0;
    PortInput_SwallowKeys(); // (Enter, which sent the line, is START: not the game's until it is let go)
}

// The chat while a line is typed: the keyboard is the line's; Esc puts it away. 1 = the event was used.
static int chat_event(const SDL_Event *ev) {
    int key = *PortInput_ChatKey();
    if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
        chat_close();
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !sChatKeyHeld && !ev->key.repeat && key > 0 && (int)ev->key.scancode == key) {
        chat_close();
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_UP && (int)ev->key.scancode == key) {
        sChatKeyHeld = false;
    }
    if (ev->type == SDL_EVENT_TEXT_INPUT && sChatKeyHeld) { // the letter of the key that opened the line
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT || ev->type == SDL_EVENT_TEXT_EDITING ||
        ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL ||
        ev->type == SDL_EVENT_MOUSE_MOTION) {
        ImGui_ImplSDL3_ProcessEvent(ev);
    }
    return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
           ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
}

int Ui_Event(const SDL_Event *ev) {
    PortVoice_Event(ev);
    if (!sReady) {
        return 0;
    }
    if (ev->type == SDL_EVENT_GAMEPAD_ADDED || ev->type == SDL_EVENT_GAMEPAD_REMOVED) { // (also while it is closed: its list of controllers)
        ImGui_ImplSDL3_ProcessEvent(ev);
        return 0;
    }
    if (sUpdPromptOpen) {
        ImGui_ImplSDL3_ProcessEvent(ev);
        return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
               ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL ||
               ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP || ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION;
    }
    if (!sOpen && !sQuit && !sChat && PortInput_TboEvent(ev)) { // typed on a keypad of Tenkaichi Budokai Online
        return 1;
    }
    if (sQuit && PortQuit_Active()) { // game dialog: keyboard and controller operate its native Yes / No plates
        if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
            SDL_Scancode key = ev->key.scancode;
            if (key == SDL_SCANCODE_ESCAPE) { PortQuit_Action(PORT_QUIT_CANCEL); }
            else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER || key == SDL_SCANCODE_SPACE) {
                PortQuit_Action(PORT_QUIT_CONFIRM);
            } else if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT ||
                       key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN) {
                if (PortQuit_Ready()) { say_quit_button(1 - PortQuit_Cursor(), 1); }
                PortQuit_Action(PORT_QUIT_MOVE);
            }
        } else if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            SDL_GamepadButton button = (SDL_GamepadButton)ev->gbutton.button;
            if (button == SDL_GAMEPAD_BUTTON_SOUTH) { PortQuit_Action(PORT_QUIT_CONFIRM); }
            else if (button == SDL_GAMEPAD_BUTTON_EAST) { PortQuit_Action(PORT_QUIT_CANCEL); }
            else if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ||
                     button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                if (PortQuit_Ready()) { say_quit_button(1 - PortQuit_Cursor(), 1); }
                PortQuit_Action(PORT_QUIT_MOVE);
            }
        }
        return 1;
    }
    if (sChat && !sOpen && !sQuit) {
        return chat_event(ev);
    }
    if (!sOpen && !sQuit && Port_NetSpectating() && ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
        gPortSpecMenuAsk = 1; // a spectator's own pause menu (the game's, over the match: only on this screen)
        return 1;
    }
    if (!sOpen && !sQuit && Port_NetSpectating() &&
        ((ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && ev->key.scancode == SDL_SCANCODE_V) ||
         (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && ev->gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))) {
        spec_view_next(); // (a spectator's keys and controller play nobody: the view is all they change)
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && *PortInput_ChatKey() > 0 && (int)ev->key.scancode == *PortInput_ChatKey() &&
        chat_can_open()) {
        chat_open();
        return 1;
    }
    if (sQuit) { // fallback when the native dialog resource cannot be loaded
        if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
            sQuitNo = sQuitNo || !ev->key.repeat; // (taken away where it is drawn)
            return 1;
        }
        ImGui_ImplSDL3_ProcessEvent(ev);
        return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
               ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
    }
    if (!sOpen) {
        return 0;
    }
    if (sCapKind != 0) { // the next key or button is the binding; Esc leaves it as it was, Delete clears it
        bool done = false;
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            if (ev->key.scancode == SDL_SCANCODE_DELETE || ev->key.scancode == SDL_SCANCODE_BACKSPACE) {
                bind(sCapKind == 1 ? 0 : -1);
            } else if (ev->key.scancode != SDL_SCANCODE_ESCAPE && sCapKind == 1) {
                bind((int)ev->key.scancode);
            }
            done = ev->key.scancode == SDL_SCANCODE_ESCAPE || ev->key.scancode == SDL_SCANCODE_DELETE ||
                   ev->key.scancode == SDL_SCANCODE_BACKSPACE || sCapKind == 1;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            bind((int)ev->gbutton.button);
            done = true;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && ev->gaxis.value > 20000 &&
                   (ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            bind(ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? PORT_PAD_LT : PORT_PAD_RT);
            done = true;
        }
        if (done) {
            sCapKind = 0;
            gPortInputCapture = 0;
        }
        if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP) {
            return 1;
        }
    } else if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
        set_open(false);
        return 1;
    }
    ImGui_ImplSDL3_ProcessEvent(ev);
    return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
           ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
}

static const char *pad_source_name(int src) {
    static const char *names[] = {"A / Cross", "B / Circle", "X / Square", "Y / Triangle", "Back / Select", "Guide", "Start",
                                  "Left stick click", "Right stick click", "Left shoulder", "Right shoulder", "D-pad up",
                                  "D-pad down", "D-pad left", "D-pad right", "Misc 1", "Right paddle 1", "Left paddle 1",
                                  "Right paddle 2", "Left paddle 2", "Touchpad"};
    static char other[24];
    if (src == PORT_PAD_LT) { return f1_text("Left trigger"); }
    if (src == PORT_PAD_RT) { return f1_text("Right trigger"); }
    if (src < 0) { return "-"; }
    if (src < (int)(sizeof(names) / sizeof(names[0]))) { return f1_text(names[src]); }
    snprintf(other, sizeof(other), f1_text("Button %d"), src);
    return other;
}

static bool tab(const char *label, int index) {
    char stable[192];
    snprintf(stable, sizeof(stable), "%s###settings-tab-%d", label, index);
    ImGuiTabItemFlags flags = sForceTab == index ? ImGuiTabItemFlags_SetSelected : 0;
    bool on = ImGui::BeginTabItem(stable, NULL, flags);
    if (on && sForceTab < 0) {
        sTab = index;
    }
    return on;
}

static void video_tab(PortVideo &v) {
    const char *hudLabel=PortHud_Hidden() ? f1_text("Show HUD") : f1_text("Hide HUD");
    if(ImGui::Button(hudLabel))PortHud_Toggle();
    ImGui::Separator();
    if (GsDraw_PerformanceMode()) {
        ImGui::TextWrapped("%s", f1_text("Performance mode: 1x resolution, reduced post-processing and no HD texture pack. Your other renderers keep their saved quality settings."));
        ImGui::Separator();
    }
    static const struct { const char *name; int milli; } aspects[] = {{"4:3 (original)", 1333}, {"16:10", 1600}, {"16:9", 1778}, {"21:9", 2389}, {"32:9", 3556}};
    char label[64];
    int best = 0, count = 0;

    snprintf(label, sizeof(label), "%gx  (%d x %d)", v.scale4 * 0.25, 512 * v.scale4 / 4, 448 * v.scale4 / 4); /* (quarters: Dragon Rage's ui.h) */
    ImGui::BeginDisabled(GsDraw_PerformanceMode() != 0);
    if (ImGui::BeginCombo(f1_text("Internal resolution"), label)) {
        for (int n = 1; n <= 8; n++) {
            snprintf(label, sizeof(label), "%dx  (%d x %d)", n, 512 * n, 448 * n);
            if (ImGui::Selectable(label, n * 4 == v.scale4)) { v.scale4 = n * 4; }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip(f1_text("How finely the game is drawn. 1x is the PlayStation 2's own resolution.\nHigher is sharper and uses more video memory."));
    for (int i = 1; i < (int)(sizeof(aspects) / sizeof(aspects[0])); i++) {
        if (abs(aspects[i].milli - v.aspectMilli) < abs(aspects[best].milli - v.aspectMilli)) { best = i; }
    }
    if (ImGui::BeginCombo(f1_text("Aspect ratio"), f1_text(aspects[best].name))) {
        for (int i = 0; i < (int)(sizeof(aspects) / sizeof(aspects[0])); i++) {
            if (ImGui::Selectable(f1_text(aspects[i].name), i == best)) { v.aspectMilli = aspects[i].milli; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip(f1_text("Wider than 4:3 shows more of the fight to the sides. Menus are stretched."));
    if (ImGui::BeginCombo(f1_text("2D filtering"), f1_text(v.smooth2d ? "Smooth" : "Sharp (console)"))) {
        if (ImGui::Selectable(f1_text("Sharp (console)"), !v.smooth2d)) { v.smooth2d = 0; }
        if (ImGui::Selectable(f1_text("Smooth"), v.smooth2d != 0)) { v.smooth2d = 1; }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip(f1_text("How the display of a fight, the menus and text are drawn above 1x internal resolution.\n"
                          "Sharp: each of the PlayStation 2's pixels is a block. Smooth: as an emulator shows them.\n"
                          "No difference at 1x."));
    bool full = v.fullscreen != 0;
    if (ImGui::Checkbox(f1_text("Full screen"), &full)) { v.fullscreen = full; }
    ImGui::SameLine();
    ImGui::TextDisabled("(F11)");
    {
        bool vs = v.vsync != 0, fs = v.frameSkip != 0;
        if (ImGui::Checkbox(f1_text("V-sync"), &vs)) { v.vsync = vs; }
        ImGui::SetItemTooltip(f1_text("Each picture waits for the display's refresh. Turn it on if the picture tears.\n"
                              "Off is the default: it adds a little delay."));
        if (ImGui::Checkbox(f1_text("Skip pictures instead of slowing down"), &fs)) { v.frameSkip = fs; }
        ImGui::SetItemTooltip(f1_text("When this machine cannot show a picture in time, the game normally runs slower.\n"
                              "With this on it stays at full speed and leaves pictures out instead (choppier,\n"
                              "not slower). It helps where the graphics chip is the slow part. On by default."));
        {
            bool late = Port_LateInput() != 0;
            if (ImGui::Checkbox(f1_text("Low input lag"), &late)) { Port_LateInputSet(late); }
            ImGui::SetItemTooltip(f1_text("The game reads the controller, computes, and then waits before the picture is shown:\n"
                                  "about 33 ms in a fight. With this on the wait comes first, so the picture follows\n"
                                  "your press by the few milliseconds the machine needs. The game itself is unchanged.\n"
                                  "With Smooth motion as well: its in-between picture then follows your press at once and\n"
                                  "the full one a refresh later. Not used online. Off by default: on a machine that\n"
                                  "cannot keep up it may stutter where the original order would not."));
        }
        {
            extern int Port_Setting(const char *name, int def);
            extern void Port_SettingSave(const char *name, int value);
            bool thr = Port_Setting("render_thread", 1) != 0;
            if (ImGui::Checkbox(f1_text("Render on a second processor core (restart required)"), &thr)) {
                Port_SettingSave("render_thread", thr ? 1 : 0);
                Port_SettingsWrite(); /* (kept outside the window's own values: written here, or it was lost at the end) */
            }
            ImGui::SetItemTooltip(f1_text("The picture is prepared and shown on a core of its own while the game computes the next\n"
                                  "moment: faster where the processor is the slow part (older or low-power machines).\n"
                                  "The same picture and the same game. Takes effect when the game is started again.\n"
                                  "Not with the OpenGL renderer, and not on machines with two cores or fewer."));
        }
    }
    SDL_DisplayID *displays = SDL_GetDisplays(&count);
    if (v.display > count) { v.display = 0; }
    snprintf(label, sizeof(label), "%s", v.display == 0 ? f1_text("Chosen by the desktop") : SDL_GetDisplayName(displays[v.display - 1]));
    if (ImGui::BeginCombo(f1_text("Display"), label)) {
        if (ImGui::Selectable(f1_text("Chosen by the desktop"), v.display == 0)) { v.display = 0; }
        for (int i = 0; i < count; i++) {
            snprintf(label, sizeof(label), "%d: %s", i + 1, SDL_GetDisplayName(displays[i]));
            if (ImGui::Selectable(label, v.display == i + 1)) { v.display = i + 1; }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip(f1_text("Takes effect the next time the game starts."));
    SDL_free(displays);
    {   // Persisted choice; the active device is reported separately after fallback.
        static const char *const kApi[] = {"Vulkan", "OpenGL",
#ifdef _WIN32
                                          "DirectX 12"
#endif
        };
        static int api = -1;
        const int count = IM_ARRAYSIZE(kApi);
        int active = sGL ? 1 : 0;
#ifdef _WIN32
        if (!sGL && sDevice && strcmp(SDL_GetGPUDeviceDriver(sDevice), "direct3d12") == 0) { active = 2; }
#endif
        if (api < 0) {
            api = Port_Setting("gpu_api", 0);
            if (api == 3 || api == 4) { api = 2; }
            if (api < 0 || api >= count) { api = 0; }
        }
        if (ImGui::Combo(f1_text("Renderer"), &api, kApi, count)) {
            Port_SettingSave("gpu_api", api);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip(f1_text("Takes effect after restarting the game. Performance depends on your GPU and drivers.\n"
                              "If the saved renderer is unavailable, the game tries Vulkan and then OpenGL."));
        ImGui::TextDisabled(f1_text("Running now: %s"), kApi[active]);
        static const int requestedAtStartup = Port_Setting("gpu_api", 0);
        if (api != active) {
            if (api == requestedAtStartup) { ImGui::TextWrapped(f1_text("The requested renderer could not start. Using %s."), kApi[active]); }
            else { ImGui::TextDisabled(f1_text("After restart: %s"), kApi[api]); }
        }
        ImGui::TextWrapped(f1_text("Active GPU: %s"), sGpuName);
    }
    {
        // In-between pictures (gs_draw.c): the fight is computed 30 times a second, as on the console; with this
        // on one more picture is drawn between each two. The game itself, its replays and online play are the same.
        const char *kSmooth[] = {f1_text("Off (30, as the console)"), f1_text("60 pictures a second"), f1_text("120 pictures a second"), f1_text("240 pictures a second")};
        int smooth = gsInterp < 0 ? 1 : gsInterp > 3 ? 3 : gsInterp;
        ImGui::SetNextItemWidth(U(416.0f));
        ImGui::BeginDisabled(GsDraw_PerformanceMode());
        if (ImGui::Combo(f1_text("Smooth motion"), &smooth, kSmooth, 4)) {
            gsInterp = smooth;
            Port_SettingSave("interp", gsInterp);
            Port_SettingsWrite();
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip(f1_text("The game computes a fight 30 times a second. With this, more pictures are\n"
                              "drawn between each two. The fight itself does not change, and neither does online play.\n"
                              "The newest picture reaches the screen later: 17 ms at 60, 25 ms at 120, 29 ms at 240.\n"
                              "Choose what your screen can show. F9 switches it off and on."));
    }
    {
        // The size of these windows, the chat and the notices: larger with a taller picture by itself (sUiScale)
        static int steps = 20; // (in fives of a percent; while it is dragged)
        char shown[16];
        snprintf(shown, sizeof(shown), "%d%%%%", steps * 5);
        ImGui::SetNextItemWidth(U(416.0f));
        ImGui::SliderInt(f1_text("Menu size"), &steps, 10, 40, shown, ImGuiSliderFlags_AlwaysClamp);
        int pct = steps * 5;
        if (ImGui::IsItemDeactivatedAfterEdit()) { // (applied on letting go: the slider would move under the pointer)
            sUiScalePct = pct;
            Port_SettingSave("ui_scale", pct);
            Port_SettingsWrite();
        } else if (!ImGui::IsItemActive()) {
            steps = (sUiScalePct < 0 ? 100 : sUiScalePct) / 5;
        }
        ImGui::SetItemTooltip(f1_text("The size of this window, the online windows, the chat and the notices. They already grow\n"
                              "with a picture taller than 1080 pixels; this makes them larger or smaller than that."));
    }
    {
        bool on = meter_on();
        if (ImGui::Checkbox(f1_text("Show frame rate, and the connection in an online match"), &on)) {
            sMeter = on;
            Port_SettingSave("meter", sMeter);
            Port_SettingsWrite();
        }
    }
    ImGui::Spacing();
    if (v.texPackCount > 0) {
        bool on = v.texPack != 0;
        snprintf(label, sizeof(label), f1_text("%s (%d textures)"), f1_text("Texture pack"), v.texPackCount);
        if (ImGui::Checkbox(label, &on)) { v.texPack = on; }
        ImGui::SetItemTooltip(f1_text("Replacement textures from the folder \"textures\" next to the game."));
    } else {
        ImGui::BeginDisabled();
        bool off = false;
        ImGui::Checkbox(f1_text("Texture pack (none found)"), &off);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip(f1_text("Put a ZIP pack or PNG/DDS files (PCSX2 naming) into the \"textures\" folder next to the game."));
    }
}

static void effects_tab(PortVideo &v) {
    static const struct { const char *name, *tip; } fx[7] = {
        {"Outline", "The black line around the fighters."},
        {"See-through tint", "The tint that shows a fighter behind scenery."},
        {"Depth tint", "The haze that colours distant things."},
        {"Glare and glow", "The bloom around bright things and the sky's glare."},
        {"Distance blur", "The soft focus on distant scenery."},
        {"Movement blur", "The blur of the scenery while the view turns or moves fast."},
        {"Weather", "The haze of dust or mist that drifts over some stages (Desert and Rocky Area at noon, Rocky Area\n"
                    "in the evening, Kami's Lookout; Glacier, Muscle Tower and the Time Chamber have a little of it too).\n"
                    "A faint tint, made of big soft sprites over the whole picture: off, those stages run as fast as the\n"
                    "others (a fifth faster at 3x)."},
    };
    for (int i = 0; i < 7; i++) {
        bool on = !((v.fxOff >> i) & 1);
        if (ImGui::Checkbox(f1_text(fx[i].name), &on)) { v.fxOff = (v.fxOff & ~(1 << i)) | (on ? 0 : 1 << i); }
        ImGui::SetItemTooltip("%s", f1_text(fx[i].tip));
        if (i == 6 && on) { /* Weather on: how much of it (fx_off bit 128: half the sprites) */
            const char *const amount[2] = {f1_text("Full"), f1_text("Light")};
            int light = (v.fxOff >> 7) & 1;
            ImGui::Indent();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
            if (ImGui::Combo(f1_text("Weather amount"), &light, amount, 2)) { v.fxOff = (v.fxOff & ~128) | (light ? 128 : 0); }
        ImGui::SetItemTooltip(f1_text("Light: half of the haze's sprites, a thinner haze; about half of what switching it off gains."));
            ImGui::Unindent();
        }
    }
    ImGui::Spacing();
    ImGui::SliderInt(f1_text("Glow strength"), &v.glow, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip(f1_text("The glow around characters and things (a Super Saiyan's hair, auras).\n"
                           "100%% is what the PlayStation 2 draws; the default here is 60%%."));
    ImGui::SliderInt(f1_text("Glare strength"), &v.glare, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip(f1_text("The glare of the sky and the stage's light.\n"
                           "100%% is what the PlayStation 2 draws; the default here is 60%%."));
    bool sharp = v.sharpBlur != 0;
    if (ImGui::Checkbox(f1_text("Sharp stage blur"), &sharp)) { v.sharpBlur = sharp; }
    ImGui::SetItemTooltip(f1_text("On: the stage's blur keeps the detail of the internal resolution (crisper, as other\n"
                           "emulators show it upscaled). Off: as soft as the PlayStation 2 draws it. No difference at 1x."));
}

// ---- the automatic update (gs/net_update.c) ----
// Check GitHub after startup, download and verify a signed release, then ask before restarting to apply it.
// Restart only after confirmation in an eligible offline menu, outside saves and fighter selection.
// BT3_UPDATE_CHECK=0 is reserved for offline build and renderer tests.
static bool sUpdAsked;       // the check was started (or decided against)
static bool sUpdAutomatic;
static Uint64 sUpdNextCheck;
static Uint64 sUpdSince;     // when the game was first ready (the check waits a few seconds after it)
static bool sUpdNotes;       // "What's new" is open
static bool sUpdTold;        // the result of the last start's update was put over the picture
static int sUpdAttempts;
static Uint64 sUpdRetryAt;
static char sUpdWhy[256];    // why an automatic install could not start
static void set_notice(const char *text, Uint64 ms);

// {version} and {percent} of a port catalog line put in (the English when the language has none)
static void upd_fill(char *out, size_t size, const char *id, const char *english, const char *version, int percent) {
    const char *line = f1_text(english);
    char num[16];
    size_t n = 0;

    (void)id;
    SDL_snprintf(num, sizeof(num), "%d", percent);
    for (size_t i = 0; line[i] != '\0' && n + 1 < size; i++) {
        const char *put = SDL_strncmp(line + i, "{version}", 9) == 0 ? version : SDL_strncmp(line + i, "{percent}", 9) == 0 ? num : NULL;
        if (put != NULL) {
            for (size_t k = 0; put[k] != '\0' && n + 1 < size; k++) {
                out[n++] = put[k];
            }
            i += 8;
        } else {
            out[n++] = line[i];
        }
    }
    out[n] = '\0';
}

static int upd_percent(const UpdateStatus &u) {
    return u.total > 0 ? (int)(u.got * 100 / u.total) : 0;
}

static bool upd_online(void) { // an online session or a lobby is on: it is never interrupted
    return Port_NetActive() || Lobby_Active() || Port_NetSpectating() || Port_TagNetwork();
}

static bool upd_menu(void) {
    // Character Reference's 3D viewer changes projection, but remains a menu.
    return UpdUi_MenuAllowed(gPortMenuMode||Port_GameMode()==60,Port_UpdateMenuScreen())!=0&&sSaving<=0;
}

static void update_summary_draw(void) {
    char summary[4096];
    static const char *const locales[]={"es","en","pt"};
    Update_NotesFor(locales[f1_language()],summary,sizeof(summary));
    ImGui::TextDisabled("%s",f1_text("What's new"));
    ImGui::BeginChild("update-summary",ImVec2(0,U(112.0f)),ImGuiChildFlags_Borders);
    ImGui::TextWrapped("%s",summary[0]?summary:f1_text("(The release has no notes.)"));
    ImGui::EndChild();
}

static void upd_apply(void) {
    sUpdWhy[0] = '\0';
    if (upd_online() || !upd_menu()) {
        SDL_strlcpy(sUpdWhy, "Restart the update from an offline menu.", sizeof(sUpdWhy));
        return;
    }
    sUpdPromptDismissed = true;
    Update_Install(); // relaunch is allowed only after the player accepts the prompt
}

static void update_tick(void) {
    if (!sReady) {
        return;
    }
    {
        UpdateStatus u;
        Update_Status(&u);
        if (u.state == UPD_INSTALLED) { // the files are swapped: the new version starts, this one closes
            static bool gone;
            if (!gone) {
                gone = true;
                Port_SettingsWrite();
                if (Update_Relaunch()) {
                    exit(0); // (as closing the window does)
                }
                SDL_strlcpy(sUpdWhy, "The update is installed, but the new version could not be started: close the game and start it again.",
                            sizeof(sUpdWhy));
            }
        }
    }
    // Migration runs once at a safe offline menu. No pack swap or 2.54 GB
    // download is started in a match, a character selector or a lobby.
    static bool texturesChecked;
    if(!texturesChecked&&!sOpen&&!sQuit&&upd_menu()&&!upd_online()){
        texturesChecked=true;
        if(Update_StartTextureMaintenance(f1_language())){Port_SettingsWrite();exit(0);}
    }
    if (!sUpdTold) { // the update installed before this start
        const char *ver;
        char line[256];
        sUpdTold = true;
        if (Update_LastResult(&ver) > 0) {
            upd_fill(line, sizeof(line), PORT_TEXT_UPDATE_DONE, "Updated to {version}.", ver, 0);
            set_notice(line, 10000);
        }
    }
    if (!sUpdAsked) {
        if (sUpdSince == 0) {
            sUpdSince = SDL_GetTicks();
        } else if (SDL_GetTicks() - sUpdSince >= 4000) {
            const char *drv = SDL_GetCurrentVideoDriver(), *env = SDL_getenv("BT3_UPDATE_CHECK");
            bool seen = drv == NULL || (SDL_strcmp(drv, "offscreen") != 0 && SDL_strcmp(drv, "dummy") != 0);
            sUpdAsked = true;
            sUpdAutomatic=env != NULL ? SDL_atoi(env) != 0 : seen&&Port_Setting("update_check",1)!=0;
            sUpdNextCheck=SDL_GetTicks()+300000;
            if (sUpdAutomatic) {
                Update_Check();
            }
        }
    }
    {
        UpdateStatus u;
        Update_Status(&u);
        if(sUpdAutomatic&&upd_menu()&&!upd_online()&&SDL_GetTicks()>=sUpdNextCheck&&
           (u.state==UPD_NONE||(u.state==UPD_FAILED&&!u.canInstall))){
            sUpdNextCheck=SDL_GetTicks()+300000;
            sUpdAttempts=0;Update_Recheck();
        }
        if (u.state == UPD_AVAILABLE && u.canInstall && sUpdAttempts == 0) {
            ++sUpdAttempts;
            sUpdRetryAt = SDL_GetTicks() + 5000;
            Update_Download();
        } else if (u.state == UPD_FAILED && u.canInstall && sUpdAttempts < 3 && SDL_GetTicks() >= sUpdRetryAt) {
            ++sUpdAttempts;
            sUpdRetryAt = SDL_GetTicks() + 15000;
            Update_Download();
        } else if (u.state == UPD_READY && !upd_online() && !sOpen && !sQuit && upd_menu() &&
                   !sUpdPromptOpen && !sUpdPromptDismissed) {
            sUpdPromptQueued = true;
        }
    }
}

// Update status only over eligible menus, never over fighter selection or gameplay.
static void update_line(char *out, size_t size) {
    UpdateStatus u;

    out[0] = '\0';
    if (!sReady || sOpen || sUpdPromptOpen || !upd_menu()) {
        return;
    }
    Update_Status(&u);
    if(Update_OnlineRequired() && u.state!=UPD_READY) {
        SDL_strlcpy(out,f1_text("Online play requires this update. Offline play remains available."),size);
    } else if (u.state == UPD_AVAILABLE && u.canInstall) {
        upd_fill(out, size, PORT_TEXT_UPDATE_AVAILABLE, "Update {version} found. Downloading automatically.", u.latest, 0);
    } else if (u.state == UPD_DOWNLOADING || u.state == UPD_VERIFYING) {
        upd_fill(out, size, PORT_TEXT_UPDATE_DOWNLOADING, "Downloading update {version}: {percent}%", u.latest, upd_percent(u));
    } else if (u.state == UPD_READY) {
        SDL_strlcpy(out, f1_text("New update available. Restart required."), size);
    }
}

// The top of the settings window: what there is to update, and the buttons.
static void update_panel(void) {
    UpdateStatus u;
    const char *ver;
    int last = Update_LastResult(&ver);
    const ImVec4 gold(1.0f, 0.85f, 0.3f, 1.0f), red(1.0f, 0.55f, 0.35f, 1.0f), green(0.45f, 0.85f, 0.5f, 1.0f);

    Update_Status(&u);
    if (last > 0 && u.state < UPD_AVAILABLE) {
        ImGui::TextColored(green, f1_text("Updated to %s."), ver);
        ImGui::Separator();
    }
    if(Update_OnlineRequired())ImGui::TextWrapped("%s",f1_text("Online play requires this update. Offline play remains available."));
    if (u.state < UPD_AVAILABLE) {
        return;
    }
    // the line that says where it stands
    if (u.state == UPD_AVAILABLE) {
        ImGui::TextColored(gold, f1_text("Update %s found. Downloading automatically."), u.latest);
    } else if (u.state == UPD_DOWNLOADING) {
        ImGui::TextColored(gold, f1_text("Downloading %s: %d%% (%.1f of %.1f MB). You can go on playing."), u.latest, upd_percent(u), (double)u.got / 1e6,
                           (double)u.total / 1e6);
        ImGui::ProgressBar((float)upd_percent(u) / 100.0f, ImVec2(-1.0f, 0.0f), "");
    } else if (u.state == UPD_VERIFYING) {
        ImGui::TextColored(gold, f1_text("Checking the download of %s (its signature)..."), u.latest);
        ImGui::ProgressBar(1.0f, ImVec2(-1.0f, 0.0f), "");
    } else if (u.state == UPD_READY) {
        ImGui::TextColored(green, "%s", f1_text("Restart required to finish installing the update."));
        update_summary_draw();
        bool safe = !upd_online() && upd_menu();
        ImGui::BeginDisabled(!safe);
        if (ImGui::Button(f1_text("Restart now"))) {
            upd_apply();
        }
        ImGui::EndDisabled();
        if (!safe) ImGui::TextWrapped("%s", f1_text("Return to an offline menu to apply the update."));
        ImGui::SameLine();
        if (ImGui::Button(f1_text("Later"))) {
            sUpdPromptDismissed = true;
        }
    } else if (u.state == UPD_INSTALLING || u.state == UPD_INSTALLED) {
        ImGui::TextColored(gold, f1_text("Installing %s..."), u.latest);
    } else if (u.state == UPD_FAILED) {
        ImGui::PushStyleColor(ImGuiCol_Text, red);
        if (u.latest[0]) ImGui::TextWrapped(f1_text("The update to %s was not installed: %s"), u.latest, u.why);
        else ImGui::TextWrapped("%s", f1_text(u.why));
        ImGui::PopStyleColor();
    }
    if (u.state == UPD_AVAILABLE && !u.canInstall && u.cannot[0] != '\0') {
        ImGui::TextWrapped(f1_text("Automatic installation is unavailable: %s"), u.cannot);
    }
    if (u.page[0] && ImGui::Button(f1_text(sUpdNotes ? "Hide what's new" : "What's new"))) {
        sUpdNotes = !sUpdNotes;
    }
    if (u.page[0]) ImGui::SameLine();
    if (u.page[0] && ImGui::Button(f1_text("Release page"))) {
        SDL_OpenURL(u.page);
    }
    ImGui::SetItemTooltip(f1_text("Opens the release page. A verified update is applied only after you confirm a restart from an offline menu."));
    if (!u.canInstall && u.cannot[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", f1_text(u.cannot));
        ImGui::PopStyleColor();
    }
    if (Port_OnlineBlocked() != NULL && u.state != UPD_INSTALLED) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", f1_text(Port_OnlineBlocked()));
        ImGui::PopStyleColor();
    }
    if (sUpdWhy[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, red);
        ImGui::TextWrapped("%s", f1_text(sUpdWhy));
        ImGui::PopStyleColor();
    }
    if (sUpdNotes) {
        char notes[8192];
        static const char *const locales[]={"es","en","pt"};
        Update_NotesFor(locales[f1_language()],notes,sizeof(notes));
        ImGui::BeginChild("notes", ImVec2(0.0f, U(180.0f)), ImGuiChildFlags_Borders);
        ImGui::TextWrapped("%s", notes[0] != '\0' ? notes : f1_text("(The release has no notes.)"));
        ImGui::EndChild();
    }
    ImGui::Separator();
}

static void updates_tab(void) {
    UpdateStatus u;
    Update_Status(&u);
    ImGui::Text(f1_text("Installed version: %s"), Update_Version());
    ImGui::TextWrapped("%s", f1_text("The updater checks for signed releases and downloads them in the background. In an offline menu, it asks before restarting to apply an update."));
    ImGui::Spacing();
    if (u.state == UPD_IDLE || u.state == UPD_NONE || u.state == UPD_FAILED) {
        if (ImGui::Button(f1_text("Check for updates"))) {
            sUpdAsked = true;
            sUpdAttempts = 0;
            sUpdNotes = sUpdPromptDismissed = false;
            sUpdWhy[0] = '\0';
            Update_Recheck();
        }
        if (u.state == UPD_FAILED && u.canInstall) {
            ImGui::SameLine();
            if (ImGui::Button(f1_text("Retry download"))) {
                sUpdAttempts = 1;
                sUpdRetryAt = SDL_GetTicks() + 15000;
                Update_Download();
            }
        }
    } else if (u.state == UPD_CHECKING) {
        ImGui::TextWrapped("%s", f1_text("Checking for updates..."));
    }
    if (u.state == UPD_NONE) ImGui::TextWrapped("%s", f1_text("No newer release is available for this build."));
    update_panel();
}

static void misc_tab(void) {
    {
        ImGui::TextDisabled(f1_text("Automatic updates: %s"), Update_Version());
        ImGui::SetItemTooltip(f1_text("The updater checks for signed releases and downloads them in the background. In an offline menu, it asks before restarting to apply an update."));
    }
    // the start of the game (PR #50): read by FirstRun_Main, so they count from the next start
    static int sSkipLogos = -1, sSkipIntro = -1, sSkipCopy = -1;
    if (sSkipLogos < 0) {
        sSkipLogos = Port_Setting("skip_logos", 0) != 0;
        sSkipIntro = Port_Setting("skip_intro", 0) != 0;
        sSkipCopy = Port_Setting("skip_copyright", 0) != 0;
    }
    bool logos = sSkipLogos != 0, intro = sSkipIntro != 0, copy = sSkipCopy != 0;
    ImGui::Spacing();
    if (ImGui::Checkbox(f1_text("Skip logos"), &logos)) {
        sSkipLogos = logos;
        Port_SettingSave("skip_logos", sSkipLogos);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip(f1_text("Start without the publisher and developer logos. From the next start of the game."));
    if (ImGui::Checkbox(f1_text("Skip intro video"), &intro)) {
        sSkipIntro = intro;
        Port_SettingSave("skip_intro", sSkipIntro);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip(f1_text("Start without the opening movie. From the next start of the game."));
    if (ImGui::Checkbox(f1_text("Skip copyright page"), &copy)) {
        sSkipCopy = copy;
        Port_SettingSave("skip_copyright", sSkipCopy);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip(f1_text("The page of legal text shown for five seconds before the title screen, every time."));
    bool hidePointer = Port_Setting("hide_pointer", 1) != 0;
    if (ImGui::Checkbox(f1_text("Hide the mouse pointer during play"), &hidePointer)) {
        Port_SettingSave("hide_pointer", hidePointer);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip(f1_text("The pointer is hidden over the game and shown in this window, the online windows, the\n"
                          "question before the game closes and while a chat line is typed."));
    bool quickSave = Port_Setting("save_quick", 1) != 0;
    if (ImGui::Checkbox(f1_text("Quick saving"), &quickSave)) {
        Port_SettingSave("save_quick", quickSave);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip(f1_text("Save and load without the memory card pages: no question, no waiting, a small card in the\n"
                          "corner while it saves, and the save loaded at start without a page. Load in the options\n"
                          "still asks first, then loads at once. A missing card, a full card or a failed save is\n"
                          "still shown. Online the game's own pages always run. Off: the game's own pages."));
}

// F1 > Online: the settings of online play (Tenkaichi Budokai Online's screens are the way in: the game's main menu), and
// under Advanced the old Dragon Net Battle window, which the screens have replaced (it is kept for testing and for
// whoever still wants it: the same connections, in a plain window).
// (Since 0.1.39 spectators are a lobby's members in its spectators' places, whom its owner lets in or removes: the
// row "Let others watch your matches", for the rooms of two, went with them.)
static void online_tab(void) {
    bool desyncEnds = Port_Setting("net_desync_end", 0) != 0;
    if (ImGui::Checkbox(f1_text("End an online match on a desync"), &desyncEnds)) {
        Port_SettingSave("net_desync_end", desyncEnds);
        Port_SettingsWrite();
    }
    ImGui::SetItemTooltip(f1_text("The two games compare what they compute. Off: when they no longer agree you are told, and the match\n"
                          "goes on (the two screens may then show different fights). On: the match ends there."));
    bool chat = PortChat_Enabled() != 0;
    if (ImGui::Checkbox(f1_text("Online chat"), &chat)) {
        PortChat_EnabledSet(chat);
    }
    ImGui::SetItemTooltip(f1_text("Text with the other player of an online match. Tab opens a line (setting key_chat), Enter sends it,\n"
                          "Esc puts it away; while it is typed the keyboard does not play, a controller does. Off: no lines\n"
                          "are shown or sent. A player without it sees nothing."));
    if (chat) {
        if (sChatSize < 0) {
            sChatSize = Port_Setting("chat_size", 1);
            if (sChatSize < 0 || sChatSize > 2) sChatSize = 1;
        }
        const char *const kChatSizes[] = {f1_text("Compact"), f1_text("Normal"), f1_text("Large")};
        int size = sChatSize;
        if (ImGui::Combo(f1_text("Chat size"), &size, kChatSizes, 3)) {
            sChatSize = size;
            Port_SettingSave("chat_size", sChatSize);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip(f1_text("How wide the online chat is and how many lines it shows (while typing / otherwise).\n"
                              "Compact: 4 / 2, narrower.\n"
                              "Normal: 6 / 4.\n"
                              "Large: 8 / 6, wider, for large screens."));
    }
    ImGui::Spacing();
    if (getenv("BT3_UI_ADVANCED") != NULL) { // (testing, with BT3_UI_OPEN=7: the section opened, for a picture of it)
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }
    if (ImGui::CollapsingHeader(f1_text("Advanced"))) {
        ImGui::TextWrapped("%s", f1_text("Dragon Net Battle is the first online window of this port. Tenkaichi Budokai Online (the main "
                           "menu) does everything it does: lobbies by code or by address, and quick matches."));
        ImGui::BeginDisabled(Port_NetActive() != 0);
        if (ImGui::Button(f1_text("Open Dragon Net Battle"))) {
            gPortNetMenuRequest = 1; // (opened in place of this window at the next frame: frame_build)
            set_open(false);
        }
        ImGui::EndDisabled();
        bool listed = Port_Setting("net_dnb_menu", 0) != 0;
        if (ImGui::Checkbox(f1_text("List Dragon Net Battle in the main menu"), &listed)) {
            Port_SettingSave("net_dnb_menu", listed ? 1 : 0);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip(f1_text("Shows the entry beside Tenkaichi Budokai Online the next time the main menu is opened."));
    }
}

// The existing free, tracking and overview camera controls.
static void spectator_tab(void) {
    bool cinematic=Port_SpecialMoveCameras(-1)!=0;
    if(ImGui::Checkbox(f1_text("Special-move cinematic cameras"),&cinematic))Port_SpecialMoveCameras(cinematic);
    ImGui::SetItemTooltip(f1_text("Turn off to keep the regular fight framing during special and ultimate moves. Attack animations, damage and timing are unchanged."));
    ImGui::Separator();
    int mode=PortSpectator_Mode();
    const char *modes[]={"Game camera","Free camera","Follow fighter","Fight overview"};
    for(int n=0;n<4;n++)if(ImGui::RadioButton(f1_text(modes[n]),mode==n)){PortSpectator_SetMode(n);mode=n;}
    ImGui::BeginDisabled(mode==0);
    if(mode==2) {
        int actor=PortSpectator_Target()+1,count=PortSpectator_Count();
        if(actor>count){actor=count;PortSpectator_SetTarget(actor-1);}
        if(ImGui::SliderInt(f1_text("Fighter"),&actor,1,count))PortSpectator_SetTarget(actor-1);
    }
    int speed=PortSpectator_Speed();
    if(ImGui::SliderInt(f1_text("Camera speed"),&speed,25,400,"%d%%"))PortSpectator_SetSpeed(speed);
    int zoom=PortSpectator_Zoom();
    if(ImGui::SliderInt(f1_text("Camera zoom"),&zoom,100,300,"%d%%",ImGuiSliderFlags_AlwaysClamp))PortSpectator_SetZoom(zoom);
    if(ImGui::Button(f1_text("Reset camera")))PortSpectator_ResetCamera();
    ImGui::EndDisabled();ImGui::Separator();
    bool hidden=PortHud_Hidden()!=0;
    if(ImGui::Checkbox(f1_text("Hide HUD"),&hidden))PortHud_SetHidden(hidden);
    ImGui::TextWrapped("%s",f1_text("Close F1 to control the camera. WASD: move; Q/E: descend/ascend; arrows: turn; Shift: accelerate; +/-: zoom."));
    ImGui::TextWrapped("%s",f1_text("Controller: left stick moves, right stick turns, L1/R1 descend/ascend, L2/R2 zoom, L3 accelerates."));
    ImGui::TextWrapped("%s",f1_text("Camera controls replace your local fighter controls during battle. Start still opens the pause menu."));
}

static void update_prompt_draw(void) {
    const char *popup = "###update-restart-required";
    if(sUpdPromptOpen&&(!upd_menu()||upd_online())){
        ImGui::ClosePopupToLevel(0,true);
        sUpdPromptOpen=false;input_owner();return;
    }
    if (sUpdPromptQueued) {
        sUpdPromptQueued = false;
        if (!upd_online() && !sOpen && !sQuit && upd_menu()) {
            sUpdPromptOpen = true;
            if (sChat) chat_close();
            input_owner();
            ImGui::OpenPopup(popup);
        }
    }
    if (!sUpdPromptOpen) return;
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(U(360.0f), 0.0f), ImVec2(U(560.0f), FLT_MAX));
    if (!ImGui::BeginPopupModal(popup, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar)) {
        return;
    }
    ImGui::TextWrapped("%s", f1_text("New update available. Restart required."));
    if(Update_OnlineRequired())ImGui::TextWrapped("%s",f1_text("Online play requires this update. Offline play remains available."));
    UpdateStatus status;Update_Status(&status);
    ImGui::TextDisabled("%s",status.latest);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", f1_text("This update is ready. Restart now to apply it, or choose Later to install it another time."));
    ImGui::Spacing();
    update_summary_draw();
    ImGui::Spacing();
    bool close = false;
    if (ImGui::Button(f1_text("Restart now"), ImVec2(U(180.0f), 0.0f))) {
        upd_apply();
        close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(f1_text("Later"), ImVec2(U(140.0f), 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
        sUpdPromptDismissed = true;
        close = true;
    }
    if (ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
    if (close) {
        ImGui::CloseCurrentPopup();
        sUpdPromptOpen = false;
        input_owner();
    }
    ImGui::EndPopup();
}

static void ui_tab(PortVideo &v) {
    {
        static const char *const names[] = {"Espa\xC3\xB1ol", "English", "Portugu\xC3\xAAs"};
        int language = f1_language();
        if (ImGui::BeginCombo(f1_text("Menu language"), names[language])) {
            for (int i = 0; i < 3; i++) {
                if (ImGui::Selectable(names[i], language == i)) f1_language_set(i);
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip(f1_text("Changes the language of this settings menu immediately. It does not change BT4 text or voices."));
        ImGui::Separator();
    }
    {
        static int choice = -1;
        if (choice < 0) choice = Port_GameProfileLanguage();
        const char *names[] = {f1_text("Spanish (text and voices)"), f1_text("English (text and voices)"), "Português (Brasil)"};
        ImGui::BeginDisabled(Port_NetActive() || Port_TagBattleRenderActive());
        if (ImGui::BeginCombo(f1_text("Game language"), names[choice])) {
            for (int i=0;i<3;i++) {
                ImGui::BeginDisabled(!Port_GameProfileAvailable(i));
                if (ImGui::Selectable(names[i],choice==i)) {
                    choice=i; Port_SettingTextSave("game_language",i==2?"pt-BR":i?"en":"es"); Port_SettingsWrite();
                }
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip(f1_text("Full disc profile: text and voices. Install each ISO once using the launcher. The startup selector confirms your choice before the game begins."));
        if (choice != Port_GameProfileLanguage()) ImGui::TextWrapped("%s", f1_text("Language saved for the next launch."));
        ImGui::Separator();
    }
    struct FontSetting { const char *label; int *choice; int role; const char *tip; };
    FontSetting fonts[] = {
        {"Menu font", &v.menuFont, MENU_TEXT_MENU,
         "Original uses the game's sprites. Other fonts replace matching menu labels immediately.\nBattle announcements have their own setting below."},
        {"Battle font", &v.battleFont, MENU_TEXT_BATTLE,
         "Choose original battle announcements and HUD text, or any battle font listed in fonts.tsv."},
        {"Dialogue / UI font", &v.textFont, MENU_TEXT_DIALOGUE,
         "Replace the shared font used by dialogue, subtitles and ordinary UI text.\nOriginal keeps the game's glyphs. Text placement, colors, shadows and icons are preserved."}
    };
    for (FontSetting &font : fonts) {
        if (ImGui::BeginCombo(f1_text(font.label), f1_text(MenuText_FontName(*font.choice)))) {
            for (int i = 0; i < MenuText_FontCount(); ++i) {
                if (!MenuText_FontAvailable(i, font.role)) continue;
                ImGui::PushID(i);
                if (ImGui::Selectable(f1_text(MenuText_FontName(i)), *font.choice == i)) *font.choice = i;
                if (*font.choice == i) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip(f1_text(font.tip));
    }
    static const char *const kHud[] = {"BT4 original, at the edges", "Extended health bars", "BT4 original, centered"};
    int bt4Hud = v.hudLayout >= 0 && v.hudLayout < 3 ? v.hudLayout : 0;
    const char *hudNames[] = {f1_text(kHud[0]), f1_text(kHud[1]), f1_text(kHud[2])};
    if (ImGui::Combo(f1_text("HUD Style"), &bt4Hud, hudNames, 3)) v.hudLayout = bt4Hud;
    ImGui::SetItemTooltip(f1_text("Where a fight's health bars and gauges sit in a picture wider than 4:3.\n"
                          "Long health bars: the bars reach from the edges to the timer.\n"
                          "In the middle: the original 4:3 arrangement, next to the timer."));
    {
        const char *const kMenus[] = {f1_text("Stretched"), "4:3", f1_text("Widescreen (beta)")};
        int m = Port_Menus43() ? 1 : Port_MenusWide() ? 2 : 0;
        if (ImGui::Combo(f1_text("Menus"), &m, kMenus, 3)) {
            Port_Menus43Set(m == 1);
            Port_MenusWideSet(m == 2);
            Port_SettingSave("menus_43", m == 1);
            Port_SettingSave("menus_wide", m == 2);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip(f1_text("Menu display on a picture wider than 4:3. Stretched fills the window. 4:3 keeps side bars. Widescreen (beta) anchors menus and HUD to the wider view while preserving artwork proportions."));
    }
    {
        static const char *const kBars[] = {"Black", "Border", "Fighter panels"};
        const char *barNames[] = {f1_text(kBars[0]), f1_text(kBars[1]), f1_text(kBars[2])};
        int b = sBars < 0 ? 0 : sBars;
        if (ImGui::Combo(f1_text("Side bars"), &b, barNames, 3)) {
            sBars = b;
            Port_SettingSave("side_bars", sBars);
            Port_SettingsWrite();
        }
        ImGui::SetItemTooltip(f1_text("What fills the sides where the picture is narrower than the window:\n"
                              "a 4:3 picture, or the menus with \"Menus: 4:3\", in a wide window.\n"
                              "Fighter panels: the border, and in a fight each side's fighter with its name and form.\n"
                              "Your own: a folder \"bars\" next to the game with trim.png (the strip along the\n"
                              "picture, kept in proportion) and fill.png (stretched over the rest). Read at start."));
    }
    if (sBars == 2) { // the fighter panels' faces
        font_picker("Name font", sPanelFont[0], "The fighter's name down the panel. The game's fonts, as for the Menu font\n"
                                                "(a font of your own: add it to fonts/fonts.tsv). Default: the first of the list.");
        font_picker("Form font", sPanelFont[1], "The form under the portrait. The game's fonts, as for the Menu font.\n"
                                                "Default: the first of the list.");
    }
}

extern "C" int gPortMusicFollows; // snd_adx.c: 1 = looping music is sped up or slowed down with the game

#include "voice_ui.inc"
static void audio_tab(PortVideo &v) {
    voice_settings_draw();
    ImGui::SeparatorText(f1_text("Game Audio"));
    ImGui::SliderInt(f1_text("Music"), &v.music, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderInt(f1_text("Sound effects"), &v.effects, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip(f1_text("Hits, blasts, menu sounds. Takes effect with the next sound that starts."));
    ImGui::SliderInt(f1_text("Voices"), &v.voices, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip(f1_text("The fighters' voices in a fight and the spoken lines outside one."));
    ImGui::SliderInt(f1_text("Stage sounds"), &v.stage, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip(f1_text("A stage's background sound (wind, water) and its own effects."));
    {
        bool follows = gPortMusicFollows > 0;
        ImGui::Spacing();
        if (ImGui::Checkbox(f1_text("Music follows game speed"), &follows)) {
            gPortMusicFollows = follows;
            Port_SettingSave("music_follows_speed", follows);
            Port_SettingsWrite(); // (in the file at once: it was lost at the end unless another setting was written after it)
        }
        ImGui::SetItemTooltip(f1_text("When the game runs at another speed than 100%% (an online match, the Cheats tab):\n"
                              "off, the music plays as it is; on, it is sped up or slowed down with the game,\n"
                              "at its own pitch, as an emulator does. Spoken lines always follow the game."));
    }
}

// Cheats. "Unlock everything" is the game's own leftover debug function (Save_UnlockAll, which nothing in the game
// calls): the request is set here and carried out on the game's side at the next vertical blank (headless.c).
extern "C" {
extern volatile int gPortUnlockAll, gPortUnlockDone;
}

extern "C" int Port_GameSpeed(void);          // plat_stub.c: the player's own game speed, percent; never used online
extern "C" void Port_GameSpeedSet(int percent);
extern "C" void Port_AudioRefresh(void);

// The community's fixes of the game's fighting mechanics (port/src/bt3_zcounter2.c, bt3_bombardment.c): each one on or
// off, read when a fight starts (src/battle/btl_char_mgr.c). Off, the game's own code runs, unchanged. Never online
// (both players would have to agree on them) and never in a replay (it was recorded with the game's own rules).
static void tech_tab(void) {
    ImGui::TextWrapped("%s",f1_text("Optional mechanics adjustments for offline duels. Disabled by default; not applied to online play, Tag Team or replays."));
    bool z=Port_Setting("tech_zcounter2",0)!=0,b=Port_Setting("tech_bombardment",0)!=0;
    if(ImGui::Checkbox(f1_text("Z Counter fix"),&z)){Port_SettingSave("tech_zcounter2",z);Port_SettingsWrite();}
    ImGui::SetItemTooltip("%s",f1_text("Retains a correctly timed Z Counter when movement changes the hitbox on the same frame."));
    if(ImGui::Checkbox(f1_text("Bombardment fix"),&b)){Port_SettingSave("tech_bombardment",b);Port_SettingsWrite();}
    ImGui::SetItemTooltip("%s",f1_text("Keeps a valid chain of smash Ki blasts from stopping one frame too early."));
    ImGui::TextDisabled("BT3 PS2 Port Gameplay Source v0.3.1 | Z Counter 2 v0.9.0 | Bombardment v2.2");
}

static void cheats_tab(void) {
    {
        int speed = Port_GameSpeed();
        if (ImGui::SliderInt(f1_text("Game speed"), &speed, 50, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
            Port_GameSpeedSet(speed);
            Port_AudioRefresh();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Port_SettingsWrite(); // (when the slider is let go: the speed was lost at the end unless another setting was written)
        }
        ImGui::SetItemTooltip(f1_text("How fast the game runs outside online play (the sound keeps its pitch). 100%% is the game's own;\n"
                              "114%% is what competitive play uses. An online match never uses this value:\n"
                              "its speed is the host's or the queue's."));
        ImGui::SameLine();
        if (ImGui::Button(f1_text("Reset"))) {
            Port_GameSpeedSet(100);
            Port_AudioRefresh();
            Port_SettingsWrite();
        }
        ImGui::Separator();
        ImGui::Spacing();
    }
    ImGui::TextWrapped("%s", f1_text("Unlock everything: all characters, stages, music and items, and the largest amount of Zenni. "
                       "This is a debug function the game's developers left in."));
    ImGui::Spacing();
    ImGui::TextWrapped("%s", f1_text("It also empties your records list, and it becomes permanent the next time the game saves. "
                       "Use it after your save has been loaded (from the main menu on)."));
    ImGui::Spacing();
    if (ImGui::Button(f1_text("Unlock everything..."))) {
        ImGui::OpenPopup(f1_text("Unlock everything?"));
    }
    if (gPortUnlockDone) {
        ImGui::SameLine();
        ImGui::TextUnformatted(f1_text("Done."));
    }
    if (ImGui::BeginPopupModal(f1_text("Unlock everything?"), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(f1_text("Your records list will be emptied. This cannot be undone once the game saves."));
        ImGui::Spacing();
        if (ImGui::Button(f1_text("Unlock"), ImVec2(U(140.0f), 0.0f))) {
            gPortUnlockAll = 1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(f1_text("Cancel"), ImVec2(U(140.0f), 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// The Controls tab's part for the sticks: each stick of the player's controller as it is now (the dot), its deadzone
// (the shaded middle: a circle, or the game's own square), what the game is given for it (the ring, and in numbers),
// the triggers, and the settings for these and for vibration. For seeing a stick that drifts and choosing a deadzone
// that covers it.
static void sticks_panel(int player) {
    static const char *const kStick[2] = {"Left stick", "Right stick"}, *const kTrigger[2] = {"Left trigger", "Right trigger"};
    static const char *const kDir[5] = {"", "up", "down", "left", "right"};
    const float R = U(42.0f), box = 2.0f * R + U(8.0f);
    const ImU32 colZone = ImGui::GetColorU32(ImGuiCol_Text, 0.22f);
    const ImU32 colRaw = ImGui::GetColorU32(ImGuiCol_Text), colGame = ImGui::GetColorU32(ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    int *dead = PortInput_Deadzone(player), *vib = PortInput_Vibration(player);
    PortStickView view;
    char label[64];

    PortInput_StickView(player, &view);
    if (!ImGui::BeginTable("sticks", 2)) {
        return;
    }
    float top = 0.0f;
    for (int s = 0; s < 2; s++) {
        ImGui::TableNextColumn();
        if (s == 0) {
            top = ImGui::GetCursorScreenPos().y;
        } else { // (the second cell would start lower by the text offset of the first one's last line)
            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, top));
        }
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 at = ImGui::GetCursorScreenPos(), c(at.x + box * 0.5f, at.y + box * 0.5f);
        ImGui::Dummy(ImVec2(box, box));
        dl->AddCircleFilled(c, R, ImGui::GetColorU32(ImGuiCol_FrameBg), 48);
        if (dead[s] < 0) { // the game's own: each axis by itself, 50 of 127.5
            float h = R * 50.0f / 127.5f;
            dl->AddRectFilled(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), colZone);
        } else if (dead[s] > 0) {
            dl->AddCircleFilled(c, R * (float)dead[s] / 100.0f, colZone, 48);
        }
        dl->AddLine(ImVec2(c.x - R, c.y), ImVec2(c.x + R, c.y), ImGui::GetColorU32(ImGuiCol_Border));
        dl->AddLine(ImVec2(c.x, c.y - R), ImVec2(c.x, c.y + R), ImGui::GetColorU32(ImGuiCol_Border));
        dl->AddCircle(c, R, ImGui::GetColorU32(ImGuiCol_Border), 48);
        if (view.connected) {
            dl->AddCircle(ImVec2(c.x + view.game[s][0] * R, c.y + view.game[s][1] * R), 5.5f, colGame, 16, 1.5f);
            dl->AddCircleFilled(ImVec2(c.x + view.raw[s][0] * R, c.y + view.raw[s][1] * R), 3.0f, colRaw, 12);
        }
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, top));
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding(); // (in both cells: the second one's lines start that much lower anyway)
        if (view.connected) {
            ImGui::Text(view.dir[s] != 0 ? "%s: %s" : "%s", f1_text(kStick[s]), f1_text(kDir[view.dir[s]]));
            ImGui::Text("%s  %+.2f %+.2f", f1_text("stick"), view.raw[s][0], view.raw[s][1]);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colGame), "%s   %+.2f %+.2f", f1_text("game"), view.game[s][0], view.game[s][1]);
        } else {
            ImGui::TextUnformatted(f1_text(kStick[s]));
            ImGui::TextDisabled("%s", f1_text("no controller"));
            ImGui::TextDisabled(" ");
        }
        ImGui::SetNextItemWidth(U(100.0f));
        snprintf(label, sizeof(label), "%s##%d", f1_text("Deadzone"), s);
        ImGui::SliderInt(label, &dead[s], -1, PORT_DEADZONE_MAX, dead[s] < 0 ? f1_text("Original") : "%d%%", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            PortInput_Save();
        }
        ImGui::SetItemTooltip(f1_text("How far the stick has to be moved before the game is told anything (the shaded middle).\n"
                              "Original: the game's own, 39%% for left / right and for up / down separately, a square.\n"
                              "0%% to 90%%: a round one of that size instead; from its edge the value grows evenly\n"
                              "to full at full tilt, and the stick counts as a direction half way between the two.\n"
                              "The white dot is the stick, the ring what the game is given; after the stick's name,\n"
                              "the direction the game takes it for. Choose a size that covers where the dot rests\n"
                              "when the stick is let go."));
        ImGui::EndGroup();
    }
    for (int s = 0; s < 2; s++) {
        ImGui::TableNextColumn();
        snprintf(label, sizeof(label), "%d%%%s", (int)(view.trigger[s] * 100.0f + 0.5f), view.triggerHeld[s] ? f1_text(" pressed") : "");
        ImGui::ProgressBar(view.trigger[s], ImVec2(U(144.0f), 0.0f), label);
        ImGui::SetItemTooltip(f1_text("A trigger counts as pressed from %d%% on. The game is told pressed or not, nothing between."),
                              (PORT_TRIGGER_AT * 100 + 16383) / 32767);
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(f1_text(kTrigger[s]));
    }
    ImGui::EndTable();
    bool on = vib[0] != 0;
    if (ImGui::Checkbox(f1_text("Vibration"), &on)) {
        vib[0] = on;
        PortInput_Save();
    }
    ImGui::SetItemTooltip(f1_text("The game's own vibration (hits, clashes, charging), on this player's controller.\n"
                          "The game's Options > Controller > Vibration has to be on as well: it is, unless you turned it off.\n"
                          "The game gives none in a replay or for a side the computer plays."));
    ImGui::BeginDisabled(!on);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(U(160.0f));
    ImGui::SliderInt(f1_text("Strength"), &vib[1], 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        PortInput_Save();
    }
    ImGui::SameLine();
    if (ImGui::Button(f1_text("Test"))) {
        PortInput_VibrationTest(player);
    }
    ImGui::SetItemTooltip(f1_text("Half a second of both motors at this strength."));
    ImGui::EndDisabled();
    if (view.connected && !view.canVibrate) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", f1_text("(this controller has none)"));
    }
}

static void controls_tab(void) {
    static int player;
    char label[96];
    int count = 0;

    ImGui::RadioButton(f1_text("Player 1"), &player, 0);
    UiA11y_Radio(player == 0);
    ImGui::SameLine();
    ImGui::RadioButton(f1_text("Player 2"), &player, 1);
    UiA11y_Radio(player == 1);
    int *slot = PortInput_PadSlot(player), *keys = PortInput_Keys(player), *pads = PortInput_PadButtons(player);
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (*slot < 0) {
        snprintf(label, sizeof(label), "%s", f1_text("None"));
    } else if (*slot < count) {
        snprintf(label, sizeof(label), "%d: %s", *slot + 1, SDL_GetGamepadNameForID(ids[*slot]));
    } else {
        snprintf(label, sizeof(label), f1_text("Controller %d (not connected)"), *slot + 1);
    }
    ImGui::SetNextItemWidth(U(300.0f));
    if (ImGui::BeginCombo(f1_text("Controller"), label)) {
        if (ImGui::Selectable(f1_text("None"), *slot < 0)) { *slot = -1; PortInput_Save(); }
        for (int i = 0; i < (count > 4 ? count : 4); i++) {
            if (i < count) {
                snprintf(label, sizeof(label), "%d: %s", i + 1, SDL_GetGamepadNameForID(ids[i]));
            } else {
                snprintf(label, sizeof(label), f1_text("Controller %d (not connected)"), i + 1);
            }
            if (ImGui::Selectable(label, *slot == i)) { *slot = i; PortInput_Save(); }
        }
        ImGui::EndCombo();
    }
    SDL_free(ids);
    ImGui::SameLine();
    if (ImGui::Button(f1_text("Reset to defaults"))) {
        PortInput_ResetDefaults(player);
        PortInput_Save();
    }
    if (ImGui::CollapsingHeader(f1_text("Sticks, triggers and vibration"), ImGuiTreeNodeFlags_DefaultOpen)) {
        sticks_panel(player);
    }
    {
        int *setting=PortInput_TargetStick(player);bool enabled=*setting!=0;
        if(ImGui::Checkbox(f1_text("Right stick enemy targeting"),&enabled)){
            *setting=enabled;PortInput_Save();
        }
        ImGui::SetItemTooltip(f1_text("Flick toward an enemy to switch targets in Tag Team. In a duel, lock onto your opponent. Release the stick before switching again. Original right-stick actions remain available when this is off."));
    }
    ImGui::TextDisabled("%s", f1_text("Choose a binding, then press the new key or button. Esc keeps it, Delete clears it;"));
    ImGui::TextDisabled("%s", f1_text("with a controller, press nothing for five seconds to keep it."));
    // (never lower than a few rows: in a small window the tab scrolls as a whole instead)
    if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y > U(140.0f) ? ImGui::GetContentRegionAvail().y : U(140.0f)))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(f1_text("Action"));
        ImGui::TableSetupColumn(f1_text("Keyboard"));
        ImGui::TableSetupColumn(f1_text("Controller"));
        ImGui::TableHeadersRow();
        {   // what opens and closes this window (not a player's: one for the program)
            const int a = PORT_ACTIONS;
            bool waitKey = sCapKind == 1 && sCapAction == a, waitPad = sCapKind == 2 && sCapAction == a;
            int key = *PortInput_OverlayKey(), pad = *PortInput_OverlayPad();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(f1_text("Open this window"));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? f1_text("press a key...") : key > 0 ? SDL_GetScancodeName((SDL_Scancode)key) : "-", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##p%d", waitPad ? f1_text("press a button...") : pad >= 0 ? pad_source_name(pad) : f1_text("START + SELECT"), a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(2, player, a);
            }
        }
        {   // what opens the online chat (not a player's: one for the program)
            const int a = PORT_ACTIONS + 1;
            bool waitKey = sCapKind == 1 && sCapAction == a;
            int key = *PortInput_ChatKey();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(f1_text("Open online chat"));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? f1_text("press a key...") : key > 0 ? SDL_GetScancodeName((SDL_Scancode)key) : "Tab", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            ImGui::TextDisabled("-");
        }
        for (int a = 0; a < PORT_ACTIONS; a++) {
            bool waitKey = sCapKind == 1 && sCapPlayer == player && sCapAction == a;
            bool waitPad = sCapKind == 2 && sCapPlayer == player && sCapAction == a;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(f1_text(PortInput_ActionLabel(a)));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? f1_text("press a key...") : keys[a] > 0 ? SDL_GetScancodeName((SDL_Scancode)keys[a]) : "-", a);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            if (a < PORT_PAD_BUTTONS) {
                snprintf(label, sizeof(label), "%s##p%d", waitPad ? f1_text("press a button...") : pad_source_name(pads[a]), a);
                if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.0f))) {
                    capture(2, player, a);
                }
            } else {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", f1_text(a < 20 ? "Left stick" : "Right stick"));
            }
        }
        ImGui::EndTable();
    }
}

static void build(void) {
    PortVideo v, was;
    ImGuiIO &io = ImGui::GetIO();
    bool open = true;

    GsGpu_GetSettings(&v);
    was = v;
    if (sCapKind == 2 && SDL_GetTicks() > sCapUntil) { // nothing was pressed: the binding stays
        sCapKind = 0;
        gPortInputCapture = 0;
    }
    if (pad_pressed(ImGuiKey_GamepadFaceRight)) { // Circle / B: out of the window
        open = false;
    }
    if (sForceTab < 0 && (pad_pressed(ImGuiKey_GamepadL1) || pad_pressed(ImGuiKey_GamepadR1))) {
        sForceTab = (sTab + (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) ? 11 : 1)) % 12;
        sPadTab = true;
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    // (760 high where the picture has the room: the Controls tab's sticks and its list of bindings together)
    { // (a large "Menu size" in a small window: no larger than the window)
        float sw = U(640.0f), sh = io.DisplaySize.y >= U(800.0f) ? U(760.0f) : U(600.0f);
        ImGui::SetNextWindowSize(ImVec2(sw < io.DisplaySize.x * 0.96f ? sw : io.DisplaySize.x * 0.96f,
                                        sh < io.DisplaySize.y * 0.94f ? sh : io.DisplaySize.y * 0.94f), ImGuiCond_Appearing);
    }
    char title[128];
    snprintf(title, sizeof(title), "%s###settings", f1_text("Settings"));
    if (ImGui::Begin(title, &open, ImGuiWindowFlags_NoCollapse)) {
        PortLobbyView lv;
        bool inLobby = Port_LobbyView(&lv) && lv.inMatch;
        if (inLobby) { // a lobby's match: the ways back (the owner: for everyone) and out
            ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), f1_text("Lobby%s%s"), lv.code[0] != '\0' ? " " : "", lv.code);
            if (lv.owner) {
                ImGui::SameLine();
                if (ImGui::Button(f1_text("Back to the lobby"))) {
                    Port_LobbyDo(LOBBY_BACK, 0, 0);
                    open = false;
                }
                ImGui::SetItemTooltip(f1_text("Ends the match for everyone: all go back to the lobby window, where the rules can be changed."));
            }
            ImGui::SameLine();
            if (ImGui::Button(f1_text(lv.owner ? "Close the lobby" : "Leave the lobby"))) {
                Port_LobbyDo(LOBBY_LEAVE, 0, 0);
                open = false;
            }
            ImGui::SetItemTooltip(f1_text(lv.owner ? "Ends the match and the lobby for everyone." : "Back to the main menu. A player leaving ends the match; a spectator leaving changes nothing for the others."));
            ImGui::Separator();
        }
        if (Port_NetSpectating()) { // watching a match: whose view, and the way out
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), f1_text("Spectating %s vs %s"), Port_NetName(0), Port_NetName(1));
            ImGui::SameLine();
            if (ImGui::Button(f1_text("Change view"))) {
                spec_view_next();
            }
            ImGui::SetItemTooltip(f1_text("Player 1's view, player 2's, or both (split screen). Also V or R1 while watching."));
            if (!inLobby) { // (in a lobby: "Leave the lobby", above)
                ImGui::SameLine();
                if (ImGui::Button(f1_text("Stop watching"))) {
                    Port_NetSpecLeave();
                    open = false;
                }
            }
            ImGui::TextDisabled(f1_text("View: %s"), f1_text(spec_view_name(Port_NetSpecView())));
            ImGui::Separator();
        }
        // the tabs in a region of their own, above a last line that is always there
        ImGui::BeginChild("body", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y), ImGuiChildFlags_NavFlattened);
        if (ImGui::BeginTabBar("tabs")) {
            if (tab(f1_text("Video"), 0)) { video_tab(v); ImGui::EndTabItem(); }
            if (tab(f1_text("Effects"), 1)) { ImGui::BeginDisabled(GsDraw_PerformanceMode() != 0); effects_tab(v); ImGui::EndDisabled(); ImGui::EndTabItem(); }
            if (tab(f1_text("Audio"), 2)) { audio_tab(v); ImGui::EndTabItem(); }
            if (tab(f1_text("UI"), 3)) { ui_tab(v); ImGui::EndTabItem(); }
            if (tab(f1_text("Controls"), 4)) {
                bool stickClash=Port_ClashStick()!=0;
                if(ImGui::Checkbox(f1_text("Use stick rotation in clashes"),&stickClash))Port_ClashStickSet(stickClash);
                ImGui::TextWrapped("%s", f1_text("Rotate the left stick during beam or melee clashes. Button controls remain available."));
                ImGui::Separator();
                controls_tab();
                ImGui::EndTabItem();
            }
            if (tab(f1_text("Cheats"), 5)) { cheats_tab(); ImGui::EndTabItem(); }
            if (tab(f1_text("Misc"), 6)) { misc_tab(); ImGui::EndTabItem(); }
            if (tab(f1_text("Online"), 7)) { online_tab(); ImGui::EndTabItem(); }
            if (tab(f1_text("Spectator"), 8)) { spectator_tab(); ImGui::EndTabItem(); }
            if (tab(f1_text("Mods"), 9)) { ModsUi_Tab(); ImGui::EndTabItem(); }
            if (tab(f1_text("Tech"), 10)) { tech_tab(); ImGui::EndTabItem(); }
            if (tab(f1_text("Updates"), 11)) { updates_tab(); ImGui::EndTabItem(); }
            ImGuiTabBar *bar=ImGui::GetCurrentContext()->CurrentTabBar;
            if(bar && bar->WidthAllTabsIdeal>bar->BarRect.GetWidth()+0.5f){
                ImGuiWindow *parent=ImGui::GetCurrentWindow()->ParentWindow;
                if(parent){
                    float width=ImMin(parent->Size.x+bar->WidthAllTabsIdeal-bar->BarRect.GetWidth()+U(4),io.DisplaySize.x*0.96f);
                    float x=ImClamp(parent->Pos.x-(width-parent->Size.x)*0.5f,0.0f,ImMax(0.0f,io.DisplaySize.x-width));
                    ImGui::SetWindowSize(parent,ImVec2(width,parent->Size.y));
                    ImGui::SetWindowPos(parent,ImVec2(x,parent->Pos.y));
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        if (sPadTab) { // (a tab chosen with the shoulder buttons)
            sPadTab = false;
            sTab = sForceTab;
            sForceTab = -1;
        } else if (ImGui::GetFrameCount() > 3) {
            sForceTab = -1;
        }
        ImGui::Separator();
        if (ImGui::Button(f1_text("Exit game"))) {
            Ui_QuitAsk();
        }
        if ((io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", f1_text("D-pad: move   Cross / A: choose   Circle / B: back   L1 / R1: tab"));
        }
    }
    ImGui::End();
    if (memcmp(&v, &was, sizeof(v)) != 0) {
        GsGpu_SetSettings(&v);
    }
    if (!open) {
        set_open(false);
    }
}

static bool sBuilt; // this frame has something to draw: the window, or the line over the picture

void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    ImDrawData *dd = sReady && sBuilt ? ImGui::GetDrawData() : NULL; // (sBuilt: Ui_Draw made a picture this frame)
    SDL_GPUColorTargetInfo ti;
    SDL_GPURenderPass *pass;

    if (dd == NULL || dd->CmdListsCount == 0 || dd->DisplaySize.x <= 0.0f || dd->DisplaySize.y <= 0.0f) {
        return;
    }
    ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
    SDL_zero(ti);
    ti.texture = target;
    ti.load_op = SDL_GPU_LOADOP_LOAD;
    ti.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(cmd, &ti, 1, NULL);
    ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
    SDL_EndGPURenderPass(pass);
}

static char sNotice[256];
static Uint64 sNoticeUntil;
static volatile int sNoticeNew;

static void set_notice(const char *text, Uint64 ms) { // (from this thread: shown at once, for `ms`)
    SDL_strlcpy(sNotice, text, sizeof(sNotice));
    sNoticeUntil = SDL_GetTicks() + ms;
}

extern "C" void Port_UiNotice(const char *text) {
    SDL_strlcpy(sNotice, text != NULL ? text : "", sizeof(sNotice));
    sNoticeNew = 1;
}

// A line in the middle of the screen, the picture behind it dimmed, while the game's thread waits inside a blank
// (the start of an online match, gs/net.c): drawn by the frames it shows meanwhile (Gs_HoldFrame). `ms` > 0: the
// wait is over; the line stays that long more, over the game's own pictures and without the dimming. NULL: gone.
static char sBanner[256];
static Uint64 sBannerUntil; // 0: while the game waits
extern "C" void Port_UiBanner(const char *text, int ms) {
    if (text != NULL) {
        SDL_strlcpy(sBanner, PortOnline_Text(text), sizeof(sBanner));
    }
    if (text == NULL && ms <= 0) {
        sBanner[0] = '\0';
    }
    sBannerUntil = ms > 0 ? SDL_GetTicks() + (Uint64)ms : 0;
}

#include "tag_team_menu_ui.inc"

// The save icon (quick saving, src/sys/mcflow_a.c): a small memory card in the bottom right corner of the picture
// while the game saves, at least a second, then it fades. Set from the game's thread: 1 saving, 0 saved, -1 the
// game's own window took over (an error: the icon goes at once).
static Uint64 sSaveFrom, sSaveEnd; // (0: not shown; the end, 0 while it saves)
extern "C" void Port_UiSaving(int state) {
    sSaving = state;
}

static bool save_icon_on(Uint64 now) {
    int state = sSaving;
    if (state > 0 && (sSaveFrom == 0 || sSaveEnd != 0)) {
        sSaveFrom = now;
        sSaveEnd = 0;
    }
    if (sSaveFrom != 0 && sSaveEnd == 0 && (state <= 0 || now - sSaveFrom > 15000)) { // (or a flow left half-way)
        sSaveEnd = state < 0 || now > sSaveFrom + 1000 ? now : sSaveFrom + 1000;
        if (state == 0) {
            PortA11y_Say(PortA11y_Label("Saved"), 0);
        }
    }
    if (sSaveEnd != 0 && now >= sSaveEnd + 400) {
        sSaveFrom = 0;
    }
    return sSaveFrom != 0;
}

static void save_icon_draw(Uint64 now) {
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    float h = (float)gUiPresentH * 0.055f, w = h * 0.78f;
    float x1 = (float)(gUiPresentX + gUiPresentW) - h * 0.6f, y1 = (float)(gUiPresentY + gUiPresentH) - h * 0.6f;
    float x0 = x1 - w, y0 = y1 - h;
    float a = sSaveEnd != 0 && now > sSaveEnd ? 1.0f - (float)(now - sSaveEnd) / 400.0f // fading
                                               : 0.7f + 0.3f * cosf((float)(now - sSaveFrom) * 0.006f); // saving: it pulses
    a = a < 0.0f ? 0.0f : a;
    auto col = [a](int r, int g, int b, int alpha) { return IM_COL32(r, g, b, (int)((float)alpha * a)); };
    ImVec2 card[5] = {ImVec2(x0, y0), ImVec2(x1 - w * 0.28f, y0), ImVec2(x1, y0 + w * 0.28f), ImVec2(x1, y1), ImVec2(x0, y1)};
    dl->AddConvexPolyFilled(card, 5, col(20, 24, 40, 210));
    dl->AddPolyline(card, 5, col(255, 255, 255, 235), ImDrawFlags_Closed, h * 0.06f > 1.5f ? h * 0.06f : 1.5f);
    for (int i = 0; i < 3; i++) { // the contacts
        float cx = x0 + w * (0.17f + 0.19f * (float)i);
        dl->AddRectFilled(ImVec2(cx, y0 + h * 0.1f), ImVec2(cx + w * 0.1f, y0 + h * 0.3f), col(255, 255, 255, 220));
    }
    dl->AddRectFilled(ImVec2(x0 + w * 0.18f, y0 + h * 0.48f), ImVec2(x1 - w * 0.18f, y1 - h * 0.14f), col(245, 140, 35, 235), h * 0.05f);
}

// Watching a match (gs/net.c, a spectator): a line at the top, whose match and whose view, and how to change it.
static const char *spec_view_name(int view) {
    static char text[48];
    if (view == 2) {
        return "both (split screen)";
    }
    SDL_snprintf(text, sizeof(text), "%s", Port_NetName(view)[0] != '\0' ? Port_NetName(view) : view == 0 ? "player 1" : "player 2");
    return text;
}

static void spec_view_next(void) {
    Port_NetSpecViewSet((Port_NetSpecView() + 1) % 3);
}

// For the players: who watches their match (small, in the top right corner).
static void watchers_line(void) {
    ImGuiIO &io = ImGui::GetIO();
    char text[200];
    int n = Port_NetWatchers();
    size_t at = (size_t)SDL_snprintf(text, sizeof(text), "%d watching: ", n);
    for (int i = 0; i < n && at < sizeof(text); i++) {
        at += (size_t)SDL_snprintf(text + at, sizeof(text) - at, "%s%s", i > 0 ? ", " : "", Port_NetWatcher(i));
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - U(6.0f), U(6.0f)), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("##watchers", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetWindowFontScale(0.85f);
        ImGui::TextDisabled("%s", text);
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();
}

// The meter: frames shown and the game's speed, and in an online match the connection. A setting (Video tab).
static bool meter_on(void) {
    if (sMeter < 0) {
        sMeter = getenv("BT3_METER") != NULL ? atoi(getenv("BT3_METER")) != 0 : Port_Setting("meter", 0) != 0;
    }
    return sMeter != 0;
}
static float sFps, sSpeed; // frames shown per second; the game's vertical blanks per second as a share of 60

static void meter_count(void) { // once per frame shown
    static Uint64 t0;
    static unsigned frames, blanks0;
    Uint64 now = SDL_GetTicks();
    frames++;
    if (t0 == 0) {
        t0 = now;
        blanks0 = gPortLiveBlanks;
    } else if (now - t0 >= 500) {
        sFps = (float)frames * 1000.0f / (float)(now - t0);
        sSpeed = (float)(gPortLiveBlanks - blanks0) * 1000.0f / (float)(now - t0) / 60.0f * 100.0f;
        frames = 0;
        blanks0 = gPortLiveBlanks;
        t0 = now;
    }
}

static void meter_draw(void) {
    int n[10];
    ImGui::SetNextWindowPos(ImVec2(U(8.0f), U(8.0f)), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("##meter", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
        // a fight shows 30 frames a second and the menus 60, as on the console; the speed is what should stay at 100%
        ImGui::Text("%.0f fps   speed %.0f%%", sFps, sSpeed);
        if (Port_NetStats(n)) {
            if (n[0] >= 0) {
                ImGui::Text("ping %d ms%s", n[0], Port_LobbyRelayed() ? " (relay)" : "");
            } else {
                ImGui::TextUnformatted("ping ...");
            }
            if (n[5] > 0) {
                ImGui::Text("rollbacks %d/s, %.1f frames", n[1], (float)n[2] / 10.0f);
            } else {
                ImGui::TextUnformatted("rollback off");
            }
            ImGui::Text("waited %d ms/s (%d times)", n[4], n[3]);
            if (n[8] == 1) { // gs/net.c wait_end: input packets came, without the frame this copy needed
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "  the other player's game is behind");
            } else if (n[8] == 2) { // nothing came for most of the wait
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "  nothing arrived from the other player");
            }
            ImGui::Text("delay %d%s, rollback up to %d", n[6], n[7] ? " (auto)" : "", n[5]);
            if (n[9] != 0) { // the desync check found the two games apart (the match goes on)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "desync since frame %d", n[9]);
            }
        } else if (Port_TagNetwork() && !Port_TagLocalBattle()) {
            int ping=Port_TagPing();
            if (ping >= 0) ImGui::Text("%s %d ms", PortOnline_Text("server ping"), ping);
            else ImGui::Text("%s ...", PortOnline_Text("server ping"));
        }
    }
    ImGui::End();
}

// The name of an added stage or song as text, in the rectangle the menu gives (the strip images' place): cream
// letters with a dark outline, as tall as the rectangle allows, centred for a stage and from the left for a song.
static void name_text(ImDrawList *dl, const char *text, float x, float y, float w, float h, bool centre) {
    ImFont *font = ImGui::GetFont();
    float size = h * (centre ? 0.42f : 0.72f);
    ImVec2 ext = font->CalcTextSizeA(size, 10000.0f, 0.0f, text);
    if (ext.x > w * 0.96f && ext.x > 0.0f) { // a long name: smaller, to fit
        size *= w * 0.96f / ext.x;
        ext = font->CalcTextSizeA(size, 10000.0f, 0.0f, text);
    }
    float tx = centre ? x + (w - ext.x) * 0.5f : x + h * 0.15f, ty = y + (h - ext.y) * 0.5f, o = size * 0.07f + 1.0f;
    for (int k = 0; k < 8; k++) {
        static const float dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        dl->AddText(font, size, ImVec2(tx + dx[k] * o, ty + dy[k] * o), IM_COL32(40, 20, 0, 255), text);
    }
    dl->AddText(font, size, ImVec2(tx, ty), IM_COL32(255, 236, 170, 255), text);
}

/* Names in the game's own lettering (namefont.c): a name with no picture in a strip is put together from the letters
   of the disc's name pictures, once, and kept as a texture while it is the one shown (a few are kept, for going up
   and down a list). */
struct FontName {
    SDL_GPUTexture *tex; // NULL: this name cannot be drawn that way (a character the disc's names do not have)
    int style, w, h, choice, resolution;
    char text[64];
    bool used;
};
static FontName sFontNames[12];
static unsigned sFontNext;

static const FontName *font_name(int style, const char *text) {
    int choice = MenuText_GetFont();
    if (choice == 0) {
        for (int i = 1; i < MenuText_FontCount(); i++) {
            if (MenuText_FontAvailable(i, MENU_TEXT_MENU)) { choice = i; break; }
        }
    }
    int resolution = choice ? 4 : 1; // (a font's strip: 4 times the disc's, for a window larger than the game's picture)
    if (text == NULL || text[0] == '\0' || (choice == 0 && !NameFont_Ready())) { // (not ready: asked again next frame)
        return NULL;
    }
    for (FontName &n : sFontNames) {
        if (n.used && n.style == style && n.choice == choice && n.resolution == resolution && strncmp(n.text, text, sizeof(n.text) - 1) == 0) {
            return n.tex != NULL ? &n : NULL;
        }
    }
    FontName &n = sFontNames[sFontNext++ % (sizeof(sFontNames) / sizeof(sFontNames[0]))];
    if (n.used && n.tex != NULL) {
        Ui_TextureFree(n.tex);
    }
    n.tex = NULL;
    n.w = n.h = 0;
    n.used = true;
    n.style = style;
    n.choice = choice; n.resolution = resolution;
    SDL_strlcpy(n.text, text, sizeof(n.text));
    unsigned char *pix;
    if (choice) {
        pix = style == NF_STAGE ? MenuText_RenderStage(n.text, resolution, &n.w, &n.h) :
                                  MenuText_RenderSong(n.text, style == NF_SONG_LIT, resolution, &n.w, &n.h);
    } else {
        pix = NameFont_Compose(style, n.text, &n.w, &n.h);
    }
    if (pix != NULL) {
        n.tex = make_strip(pix, (size_t)n.w * n.h * 4, (uint32_t)n.w, (uint32_t)n.h, 1); // (frees pix)
    }
    if (choice) { n.w /= resolution; n.h /= resolution; }
    return n.tex != NULL ? &n : NULL;
}

/* Draws such a name over the place of the disc's name picture: (x, y) is the corner of that picture's row in the
   window, kx and ky are window pixels per pixel of the row (512 wide). A name longer than the row is made smaller
   to fit, about the row's middle line; a stage name stays in the middle, a song name keeps its left edge. */
static void font_draw(ImDrawList *dl, const FontName *n, float x, float y, float kx, float ky, bool centre) {
    float k = n->w > 512 ? 512.0f / (float)n->w : 1.0f;
    float w = (float)n->w * kx * k, h = (float)n->h * ky * k;
    float x0 = centre ? x + (512.0f * kx - w) * 0.5f : x;
    float y0 = y + ((float)n->h * ky - h) * 0.5f;
    dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)n->tex), ImVec2(x0, y0), ImVec2(x0 + w, y0 + h));
}

// The online chat, in the lower left corner. Put away, it shows the newest lines for a few seconds each in a dark
// rounded card, fading out, and takes no input. While a line is typed it shows the last ones, input field and
// allows selecting/copying text with mouse.
enum { CHAT_SHOWN_MS = 8000, CHAT_FADE_MS = 2000 };

static bool chat_recent(void) {
    PortChatLine l;
    unsigned n = PortChat_Count();
    return n > 0 && PortChat_Line(n - 1, &l) && SDL_GetTicks() - l.atMs < CHAT_SHOWN_MS;
}

static void chat_draw(void) {
    ImGuiIO &io = ImGui::GetIO();
    if (sChatSize < 0) {
        sChatSize = Port_Setting("chat_size", 1);
        if (sChatSize < 0 || sChatSize > 2) sChatSize = 1;
    }
    int rowsTyping = 6, rowsPassive = 4; // Normal: as the chat always was
    float wFactor = 0.42f, minW = 320.0f, maxW = 100000.0f;
    if (sChatSize == 0) { // Compact
        rowsTyping = 4;
        rowsPassive = 2;
        wFactor = 0.28f;
        minW = 240.0f;
        maxW = 380.0f;
    } else if (sChatSize == 2) { // Large
        rowsTyping = 8;
        rowsPassive = 6;
        wFactor = 0.52f;
        minW = 400.0f;
    }
    float w = io.DisplaySize.x * wFactor;
    if (w < U(minW)) w = U(minW);
    if (w > U(maxW)) w = U(maxW);
    unsigned n = PortChat_Count(), rows = sChat ? rowsTyping : rowsPassive, first = n > rows ? n - rows : 0;
    Uint64 now = SDL_GetTicks();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (!sChat) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    // In the lower left corner (growing upwards) until it is dragged; then where it was left (off the screen: back).
    static ImVec2 sChatPos;
    static bool sChatMoved;
    if (sChatMoved && (sChatPos.x < 0.0f || sChatPos.y < 0.0f || sChatPos.x > io.DisplaySize.x - 50.0f ||
                       sChatPos.y > io.DisplaySize.y - 50.0f)) {
        sChatMoved = false;
    }
    if (!sChatMoved) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.02f, io.DisplaySize.y * 0.86f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    } else if (!sChat) {
        ImGui::SetNextWindowPos(sChatPos, ImGuiCond_Always);
    }

    float bgAlpha = 0.85f;
    if (!sChat) {
        float newestAlpha = 1.0f;
        PortChatLine lastLine;
        if (n > 0 && PortChat_Line(n - 1, &lastLine)) {
            Uint64 age = now - lastLine.atMs;
            if (age >= CHAT_SHOWN_MS) {
                return;
            }
            if (age > CHAT_SHOWN_MS - CHAT_FADE_MS) {
                newestAlpha = (float)(CHAT_SHOWN_MS - age) / (float)CHAT_FADE_MS;
            }
        }
        bgAlpha = 0.65f * newestAlpha;
    }
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(8.0f), U(6.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, sChat ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(U(4.0f), U(2.0f)));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.75f, 0.2f, 0.5f));

    if (sChat && sChatFocus) {
        ImGui::SetNextWindowFocus();
    }
    if (ImGui::Begin("##chat", NULL, flags)) {
        if (sChat && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            sChatMoved = true;
        }
        if (sChat && sChatMoved) {
            sChatPos = ImGui::GetWindowPos();
        }
        const char *names[2] = {Port_NetName(Port_NetMe()), Port_NetName(!Port_NetMe())};
        if (sChat) {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.28f, 1.0f), "Chat (Tab)");
            ImGui::SameLine();
            ImGui::TextDisabled("::");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Drag anywhere to move the chat window. Click a line to copy it.");
            }
            ImGui::SameLine(w - 70.0f);
            bool mute = PortChat_Muted() != 0;
            if (ImGui::Checkbox("Mute", &mute)) {
                PortChat_MutedSet(mute);
                sChatFocus = true;
            }
            ImGui::Separator();
        }

        for (unsigned i = first; i < n; i++) {
            PortChatLine l;
            float alpha = 1.0f;
            if (!PortChat_Line(i, &l)) {
                continue;
            }
            if (!sChat) {
                Uint64 age = now - l.atMs;
                if (age >= CHAT_SHOWN_MS) {
                    continue;
                }
                if (age > CHAT_SHOWN_MS - CHAT_FADE_MS) {
                    alpha = (float)(CHAT_SHOWN_MS - age) / (float)CHAT_FADE_MS;
                }
            }
            const char *sender = l.sender[0] != '\0' ? l.sender : (names[l.mine ? 0 : 1][0] != '\0' ? names[l.mine ? 0 : 1] : (l.mine ? "You" : "Rival"));
            ImGui::PushID((int)i);
            ImVec4 nameCol = l.mine ? ImVec4(0.35f, 0.85f, 1.0f, alpha) : ImVec4(1.0f, 0.82f, 0.28f, alpha);
            ImGui::TextColored(nameCol, "%s:", sender);
            ImGui::SameLine(0.0f, U(4.0f));

            ImVec2 at = ImGui::GetCursorScreenPos();
            if (!sChat) {
                ImGui::GetWindowDrawList()->AddText(NULL, 0.0f, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, (int)(230.0f * alpha)), l.text, NULL, w - (at.x - ImGui::GetWindowPos().x));
            }
            ImGui::PushTextWrapPos(w);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", l.text);
            ImGui::PopTextWrapPos();
            if (sChat) { // a click copies the line (a room code or an address is passed on this way); a drag does not
                static unsigned sCopiedIdx;
                static Uint64 sCopiedMs;
                bool copied = sCopiedIdx == i + 1 && now - sCopiedMs < 1500;
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(copied ? "Copied" : "Click to copy");
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 16.0f) {
                        SDL_SetClipboardText(l.text);
                        sCopiedIdx = i + 1;
                        sCopiedMs = now;
                        sChatFocus = true;
                    }
                }
            }
            ImGui::PopID();
        }

        if (sChat) {
            if (sChatFocus) {
                ImGui::SetKeyboardFocusHere();
                sChatFocus = false;
            }
            ImGui::SetNextItemWidth(w);
            if (ImGui::InputText("##chatline", sChatBuf, sizeof(sChatBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                const char *p = sChatBuf;
                while (*p == ' ') {
                    p++;
                }
                if (*p == '\0') {
                    chat_close();
                } else if (PortChat_Say(sChatBuf)) {
                    sChatBuf[0] = '\0';
                    chat_close();
                } else { // (said within a second of the line before: kept, to be sent again)
                    sChatSoon = true;
                    sChatFocus = true;
                }
            }
            ImGui::TextDisabled(sChatSoon ? "1 line/s: Enter to send" : "Enter: send   Esc/Tab: close   Ctrl+V: paste");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

// Builds this frame's picture of the overlay (the settings or the online window, the line over the picture).
// false: there is nothing to draw. The same for both back ends; each then draws ImGui's data its own way.
static bool frame_build(void) {
    PortInput_TboClipboard();
    { // a keypad of Tenkaichi Budokai Online takes text: the system's text input is on for it (letters as typed)
        static SDL_Window *on = NULL;
        SDL_Window *want = gPortTboTyping && !sOpen && !sQuit && !sChat ? SDL_GetKeyboardFocus() : NULL;
        if (want != on) {
            if (on != NULL) { SDL_StopTextInput(on); }
            if (want != NULL) { SDL_StartTextInput(want); }
            on = want;
        }
    }
    if (sQuit && !PortQuit_Active() && !PortQuit_Failed()) {
        sQuit = false;
        input_owner();
        PortA11y_Say(PortA11y_Label("Back to the game"), 1);
    }
    if (sReady && gPortNetWindowClose) { // back from an online session: the window it was started from closes
        gPortNetWindowClose = 0;
        gPortNetMenuRequest = 0;
        set_open(false);
    } else if (sReady && gPortNetMenuRequest) { // Dragon Net Battle was chosen in the game's main menu
        gPortNetMenuRequest = 0;
        net_open();
    }
    { // BT3_SPEC_PAUSE_AT=<seconds>: testing, a spectator's own pause menu (the game's) opens that long into its match
        static Uint64 since;
        static bool done;
        if (!done && getenv("BT3_SPEC_PAUSE_AT") != NULL && sReady && Port_NetSpectating() && !sOpen) {
            if (since == 0) {
                since = SDL_GetTicks();
            } else if (SDL_GetTicks() - since > (Uint64)atoi(getenv("BT3_SPEC_PAUSE_AT")) * 1000) {
                done = true;
                gPortSpecMenuAsk = 1;
            }
        }
    }
    update_tick();
    char updLine[256];
    update_line(updLine, sizeof(updLine)); // (the update's line over the main menu and the online screens)
    bool notice = sNotice[0] != '\0' && SDL_GetTicks() < sNoticeUntil;
    if (sNoticeNew) { // (set from the game's side: the time starts when it is first drawn)
        sNoticeNew = 0;
        sNoticeUntil = SDL_GetTicks() + 6000;
        notice = sNotice[0] != '\0';
    }
    // the name of a stage added from outside the disc, over the stage select (a strip of pre-rendered names)
    bool nameOn = gUiNameReady != 0 && gUiNameIdx >= 0 && gUiNameIdx < PortStages_Count();
    bool songOn = gUiSongReady != 0 && gUiSongIdx >= 0 && gUiSongIdx < PortSongs_Count(); // (the same for an added song)
    bool overlay = nameOn || songOn;
    bool meter = sReady && meter_on();
    // the players' names under the gauges of an online fight
    bool names = sReady && Port_NetActive() && BtlChar_FindByObjId(0) != NULL && (Port_NetName(0)[0] != '\0' || Port_NetName(1)[0] != '\0');
    bool bars = sReady && bars_on();
    bool online = sReady && Port_NetActive() && PortChat_Enabled() && !Port_NetSpectating(); // (a spectator has no one to talk to)
    if (sChat && !online) { // (the match ended while a line was typed)
        chat_close();
    }
    bool chat = online && (sChat || chat_recent());
    if (sBanner[0] != '\0' && sBannerUntil != 0 && SDL_GetTicks() >= sBannerUntil) {
        sBanner[0] = '\0';
    }
    bool tagMenu = sReady && sTagMenuOpen;
    bool updateAvailable=updLine[0]!=0;
    extern int gPortMenuMode;
    if(PortHud_Hidden() && !gPortMenuMode)meter=names=chat=bars=notice=false;
    PortVoiceOptions voiceOptions;PortVoice_Options(&voiceOptions);
    PortVoiceState voiceState;PortVoice_State(&voiceState);
    bool voice = sReady && voiceOptions.indicators && (voiceState.sessionMode!=0||(getenv("BT4_RENDER_TEST")&&getenv("BT4_VOICE_ICON_TEST")));
    bool banner = sReady && sBanner[0] != '\0' && !tagMenu;
    Uint64 now = SDL_GetTicks();
    bool saving = save_icon_on(now) && sReady && gUiPresentW > 0 && gUiPresentH > 0;
    bool watched = sReady && Port_NetWatchers() > 0 && !sOpen;  // a player's: who watches the match
    if(PortHud_Hidden() && !gPortMenuMode)voice=banner=saving=false;
    if(PortHud_Hidden() && !gPortMenuMode)watched=false;
    bool mods = sReady && Mods_HudActive() && !PortHud_Hidden();
    meter_count();
    sBuilt = false;
    UiA11y_Frame(sReady && sOpen && !sQuit, !sQuit);
    if (!sReady || (!sOpen && !sQuit && !sUpdPromptOpen && !sUpdPromptQueued && !notice && !overlay && !meter && !names && !bars && !chat && !banner && !saving && !tagMenu && !voice && !watched && !updateAvailable && !mods)) {
        return false;
    }
    if (sPadHold && sCapKind != 2) { // a binding was taken: the controller works the window again once nothing is held
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        bool held = false;
        for (int i = 0; i < count; i++) {
            SDL_Gamepad *g = SDL_GetGamepadFromID(ids[i]);
            for (int b = 0; g != NULL && b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
                held = held || SDL_GetGamepadButton(g, (SDL_GamepadButton)b);
            }
        }
        SDL_free(ids);
        sPadHold = held;
    }
    // (the button that becomes the binding would also press the row it is bound in; while a chat line is typed the
    // controller is the game's)
    if (sPadHold || (sChat && !sOpen)) {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    } else {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    }
    sBuilt = true;
#ifdef _WIN32
    if (sD3D9) { ImGui_ImplDX9_NewFrame(); }
    else
#endif
    if (sGL) {
        ImGui_ImplOpenGL3_NewFrame();
    } else {
        ImGui_ImplSDLGPU3_NewFrame();
    }
    ImGui_ImplSDL3_NewFrame();
    ui_scale_apply(); // (the window's size is known from here)
    fonts_apply(); // (a face chosen for the fight panels, between two frames)
    ImGui::NewFrame();
    update_prompt_draw();
    // The mouse pointer is hidden over the game's picture and shown with a window of this file that takes the mouse:
    // the settings, the online windows, the question before the game closes, a chat line being typed (setting
    // hide_pointer, on unless 0). The SDL back end hides it for this cursor at the next frame.
    if (!sOpen && !sQuit && !sChat && !sUpdPromptOpen && Port_Setting("hide_pointer", 1) != 0) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
    if (bars) {
        bars_draw();
    }
    if(mods)ModsUi_Draw(); // The player's Hide HUD choice also hides script overlays.
    if (sOpen) {
        if (sNet) {
            build_net();
        } else {
            build();
        }
    }
    UiA11y_End(); // (what the window's focus is on, said; the windows after this one are not read)
    if (sQuit && PortQuit_Failed()) {
        build_quit();
    }
    if (meter) {
        meter_draw();
    }
    if (chat) {
        chat_draw();
    }
    if (names && GsDraw_HudShown() && gUiPresentW > 0 && gUiPresentH > 0) { // (with the HUD: not over an intro)
        // Under each side's health bar (the game's 512 x 448 page: the bars end at about y 50). The left place is
        // side 0's, or this player's own side when the HUD's sides are swapped.
        ImDrawList *dl = ImGui::GetBackgroundDrawList();
        float sx = (float)gUiPresentW / 512.0f, sy = (float)gUiPresentH / 448.0f;
        float size = 15.0f * sy;
        ImFont *font = ImGui::GetFont();
        for (int place = 0; place < 2; place++) {
            const char *text = Port_NetName(place ^ Port_HudSwap());
            if (text[0] == '\0') {
                continue;
            }
            ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
            // 106 of the HUD's own pixels in from its outer edge: clear of the panel of a team's waiting fighters,
            // which reaches to about 100 (the name sat on it at 62: reported). The HUD is narrowed in a wide
            // picture and sits at the edges or about the middle (gs_draw.c), and the name goes with it.
            float narrow = Port_IsWide() ? 1333.333f / (float)Port_AspectMilli() : 1.0f;
            float in = Port_HudLayout() == 2 ? 256.0f + (106.0f - 256.0f) * narrow : 106.0f * narrow;
            float x = place == 0 ? (float)gUiPresentX + in * sx : (float)gUiPresentX + (512.0f - in) * sx - ts.x;
            float y = (float)gUiPresentY + 56.0f * sy;
            for (int ox = -1; ox <= 1; ox++) {
                for (int oy = -1; oy <= 1; oy++) {
                    if (ox != 0 || oy != 0) {
                        dl->AddText(font, size, ImVec2(x + (float)ox * 1.5f, y + (float)oy * 1.5f), IM_COL32(0, 0, 0, 220), text);
                    }
                }
            }
            dl->AddText(font, size, ImVec2(x, y), IM_COL32(255, 255, 255, 255), text);
        }
    }
    if (tagMenu)tag_menu_draw();
    if (banner) { // the start of an online match (Port_UiBanner): in the middle, large, outlined
        ImGuiIO &io = ImGui::GetIO();
        ImDrawList *dl = ImGui::GetForegroundDrawList();
        ImFont *font = ImGui::GetFont();
        float size = ImGui::GetFontSize() * (io.DisplaySize.y >= 900.0f ? 2.0f : 1.6f);
        float wrap = io.DisplaySize.x * 0.8f;
        ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, wrap, sBanner);
        float x = (io.DisplaySize.x - extent.x) * 0.5f, y = (io.DisplaySize.y - extent.y) * 0.5f;
        if (sBannerUntil == 0) { // still waiting: the picture dimmed
            dl->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, 150));
        }
        dl->AddRectFilled(ImVec2(x - size, y - size * 0.6f), ImVec2(x + extent.x + size, y + extent.y + size * 0.6f), IM_COL32(10, 14, 40, 210),
                          size * 0.4f);
        for (int ox = -1; ox <= 1; ox++) {
            for (int oy = -1; oy <= 1; oy++) {
                if (ox != 0 || oy != 0) {
                    dl->AddText(font, size, ImVec2(x + (float)ox * 2.0f, y + (float)oy * 2.0f), IM_COL32(0, 0, 0, 230), sBanner, NULL, wrap);
                }
            }
        }
        dl->AddText(font, size, ImVec2(x, y), IM_COL32(255, 236, 120, 255), sBanner, NULL, wrap);
    }
    if (saving) {
        save_icon_draw(now);
    }
    if (watched) {
        watchers_line();
    }
    if (updLine[0] != '\0') { // the update's line (update_line): at the top, over the main menu and the online screens
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.035f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.80f);
        if (ImGui::Begin("##update", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", updLine);
        }
        ImGui::End();
    }
    if (notice) { // a line over the picture for a few seconds (the end of an online match)
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.12f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::Begin("##notice", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted(sNotice);
        }
        ImGui::End();
    }
    if (overlay && gUiPresentW > 0 && gUiPresentH > 0) {
        /* Each name's rectangle is in the game's 512x448 pixels: map it through the picture's rectangle in the
           window (the letterbox gs_gpu.c just blitted into). */
        float sx = (float)gUiPresentW / 512.0f;
        float sy = (float)gUiPresentH / 448.0f;
        // Stage/song names belong to the game picture, behind settings and other windows,
        // including added names rendered while Menu font is Original.
        ImDrawList *dl = ImGui::GetBackgroundDrawList();

        if (nameOn) {
            const FontName *fn = font_name(NF_STAGE, PortStages_Name(gUiNameIdx));
            float x = (float)gUiPresentX + (float)gUiNameX * sx, y = (float)gUiPresentY + (float)gUiNameY * sy;
            if (fn != NULL) {
                float kx = (float)gUiNameW * sx / 384.0f, ky = (float)gUiNameH * sy / 48.0f;
                font_draw(dl, fn, x - 64.0f * kx, y + (1.5f - 14.0f) * ky, kx, ky, true);
            } else {
                name_text(dl, PortStages_Name(gUiNameIdx), x, y, (float)gUiNameW * sx, (float)gUiNameH * sy, true);
            }
        }
        if (songOn) {
            const FontName *fn = font_name(gUiSongLit ? NF_SONG_LIT : NF_SONG, PortSongs_Name(gUiSongIdx));
            float x = (float)gUiPresentX + (float)gUiSongX * sx, y = (float)gUiPresentY + (float)gUiSongY * sy;
            if (fn != NULL) {
                font_draw(dl, fn, x, y, (float)gUiSongW * sx / 512.0f, (float)gUiSongH * sy / 32.0f, false);
            } else {
                name_text(dl, PortSongs_Name(gUiSongIdx), x, y, (float)gUiSongW * sx, (float)gUiSongH * sy, false);
            }
        }
    }
    voice_indicator_draw();
    ImGui::Render();
    return true;
}

void Ui_DrawD3D9(void) {
#ifdef _WIN32
    if (sD3D9 && frame_build()) { ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData()); }
#endif
}
void Ui_Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    if (!sGL && frame_build()) {
        Ui_DrawAgain(cmd, target);
    }
}

// The OpenGL back end's turn: over the presented picture, just before the buffers are swapped.
void Ui_DrawGL(void) {
    if (sGL && frame_build()) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}
