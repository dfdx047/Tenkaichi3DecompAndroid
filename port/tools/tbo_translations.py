#!/usr/bin/env python3
"""The Tenkaichi Budokai Online screens in Portuguese and Spanish: writes port/translations/tbo/pt.json and es.json.

    python3 port/tools/tbo_translations.py          (after a line of src/menu/port_tbo.c changed)

The screens' language row (src/menu/port_tbo.c, the hub) loads one of these (menu_translation.c,
MenuTranslation_OnlineLanguage); English is the screens' own text. A line is found by its English, exactly as the
screen writes it, so every English line below is checked against the strings of port_tbo.c: one that is not there any
more is reported, and so is a line of the screen that has no translation. A translation keeps the %d / %s parts of its
English (menu_translation.c drops one that does not).
"""
import json, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCES = [ROOT / "src/menu/port_tbo.c", ROOT / "port/src/spec_menu.c"] # (the lines of tbo_strings.py)
OUT = ROOT / "port/translations/tbo"

# English: (Portuguese, Spanish)
LINES = {
    "Language": ("Idioma", "Idioma"),
    "Choose the language of these screens. Left and right change it.":
        ("Escolha o idioma destas telas. Esquerda e direita trocam.", "Elige el idioma de estas pantallas. Izquierda y derecha lo cambian."),
    "Searching": ("Procurando", "Buscando"),
    "Quick Match": ("Partida Rápida", "Partida Rápida"),
    "Ranked": ("Classificatória", "Clasificatoria"),
    "Play Against": ("Jogar contra", "Jugar contra"),
    "Fights for Elo points against Android and Switch players. Points change when both results agree.":
        ("Lutas valendo pontos Elo contra jogadores de Android e Switch. Os pontos mudam quando os dois resultados batem.",
         "Peleas por puntos Elo contra jugadores de Android y Switch. Los puntos cambian al confirmar ambos resultados."),
    "Choose who you play against: Android and Switch, or the PC.":
        ("Escolha contra quem você joga: Android e Switch, ou o PC.", "Elige contra quién juegas: Android y Switch, o el PC."),
    "Against PC players: rooms and quick match on the PC port's service.":
        ("Contra jogadores de PC: salas e partida rápida no serviço do port de PC.",
         "Contra jugadores de PC: salas y partida rápida en el servicio del port de PC."),
    "Against Android and Switch players, on Dragon Net.": ("Contra jogadores de Android e Switch, no Dragon Net.",
                                                           "Contra jugadores de Android y Switch, en Dragon Net."),
    "Against Android and Switch players. Online now: about %d.": ("Contra jogadores de Android e Switch. Online agora: cerca de %d.",
                                                                  "Contra jugadores de Android y Switch. En línea ahora: unos %d."),
    "RANKED IS AGAINST ANDROID + SWITCH: CHOOSE IT IN PLAY AGAINST": ("A CLASSIFICATÓRIA É CONTRA ANDROID + SWITCH: ESCOLHA EM JOGAR CONTRA",
                                                                      "LA CLASIFICATORIA ES CONTRA ANDROID + SWITCH: ELÍGELO EN JUGAR CONTRA"),
    "Search for a Rival": ("Buscar Rival", "Buscar Rival"),
    "Ranking": ("Classificação", "Clasificación"),
    "Update Profile": ("Atualizar Perfil", "Actualizar Perfil"),
    "Update": ("Atualizar", "Actualizar"),
    "Loading profile...": ("Carregando perfil...", "Cargando perfil..."),
    "%d ELO": ("%d ELO", "%d ELO"),
    "%d W / %d L": ("%d V / %d D", "%d V / %d D"),
    "Position %d": ("Posição %d", "Puesto %d"),
    "TEAM": ("EQUIPE", "EQUIPOS"),
    "SINGLE": ("INDIVIDUAL", "INDIVIDUAL"),
    "RANKED": ("CLASSIFICATÓRIA", "CLASIFICATORIA"),
    "RIVAL": ("RIVAL", "RIVAL"),
    "Bronze": ("Bronze", "Bronce"),
    "Silver": ("Prata", "Plata"),
    "Gold": ("Ouro", "Oro"),
    "Platinum": ("Platina", "Platino"),
    "Diamond": ("Diamante", "Diamante"),
    "Master": ("Mestre", "Maestro"),
    "Searching for a rival...": ("Procurando um rival...", "Buscando rival..."),
    "Searching for a rival of your level...": ("Procurando um rival do seu nível...", "Buscando rival de tu nivel..."),
    "Rival found. Connecting...": ("Rival encontrado. Conectando...", "Rival asignado. Conectando..."),
    "Result confirmed.": ("Resultado confirmado.", "Resultado confirmado."),
    "Search over. No points.": ("Busca encerrada. Sem pontos.", "Búsqueda finalizada. Sin puntos."),
    "Waiting for your rival's result.": ("Esperando o resultado do rival.", "Esperando confirmación del rival."),
    "The two results differ: no points.": ("Os dois resultados não batem: sem pontos.", "Resultados distintos: sin puntos."),
    "Match void: no points.": ("Partida anulada: sem pontos.", "Partida anulada: sin puntos."),
    "Ranked did not answer. Trying again...": ("O ranqueado não respondeu. Tentando de novo...", "Ranked sin respuesta. Reintentando..."),
    "The Ranked profile could not be made.": ("Não foi possível criar o perfil ranqueado.", "No se pudo registrar el perfil Ranked."),
    "The ranking could not be updated.": ("Não foi possível atualizar a classificação.", "No se pudo actualizar la clasificación."),
    "The ranked room could not be published.": ("Não foi possível publicar a sala ranqueada.", "No se pudo publicar la sala Ranked."),
    "Your last ranked match is still being confirmed.": ("Sua última partida ranqueada ainda está sendo confirmada.",
                                                         "Tu última partida Ranked aún se está confirmando."),
    "The Dragon Net service does not take this version yet.": ("O serviço Dragon Net ainda não aceita esta versão.",
                                                               "El servicio Dragon Net aún no acepta esta versión."),
    "Profile updated.": ("Perfil atualizado.", "Perfil actualizado."),
    "Too many requests: wait a moment.": ("Muitos pedidos: espere um pouco.", "Demasiadas solicitudes: espera un momento."),
    "Server busy (daily limit), try again later.": ("Servidor ocupado (limite diário), tente mais tarde.",
                                                    "Servidor ocupado (límite diario), inténtalo más tarde."),
    "Updating the ranking...": ("Atualizando a classificação...", "Actualizando clasificación..."),
    "The ranking did not answer: X tries again.": ("A classificação não respondeu: X tenta de novo.", "La clasificación no respondió: X reintenta."),
    "Single or team fights. No time limit, at the standard speed.": ("Lutas individuais ou em equipe. Sem limite de tempo, na velocidade padrão.",
                                                                     "Peleas individuales o por equipos. Sin límite de tiempo, a velocidad estándar."),
    "Find a rival of your level! Points change when both results agree.": ("Ache um rival do seu nível! Os pontos mudam quando os dois resultados batem.",
                                                                           "¡Encuentra un rival de tu nivel! Los puntos cambian al confirmar ambos resultados."),
    "The best fighters of the community, by their Elo points.": ("Os melhores lutadores da comunidade, pelos pontos Elo.",
                                                                 "Los mejores luchadores de la comunidad, por sus puntos Elo."),
    "Load your profile again from the service.": ("Carrega seu perfil de novo do serviço.", "Carga tu perfil otra vez desde el servicio."),
    "The table shows the community's Elo points. X updates it.": ("A tabela mostra os pontos Elo da comunidade. X atualiza.",
                                                                  "La tabla muestra los puntos Elo de la comunidad. X la actualiza."),
    ": team": (": equipe", ": equipos"),
    ": single": (": individual", ": individual"),
    "Rival found. Connecting": ("Rival encontrado. Conectando", "Rival asignado. Conectando"),
    "Ranked: searching for a rival": ("Classificatória: procurando um rival", "Clasificatoria: buscando rival"),
    "First: ": ("Primeiro: ", "Primero: "),
    "Create Room": ("Criar Sala", "Crear Sala"),
    "Join Room": ("Entrar em Sala", "Entrar a Sala"),
    "Lobby": ("Lobby", "Lobby"),
    "Friends": ("Amigos", "Amigos"),
    "Tournament": ("Torneio", "Torneo"),
    "Replays": ("Replays", "Repeticiones"),
    "Leaderboard": ("Ranking", "Clasificación"),
    "Profile": ("Perfil", "Perfil"),
    "Fight whoever is searching right now! No time limit, and you can train while you wait.":
        ("Lute com quem estiver procurando agora! Sem limite de tempo, e dá para treinar enquanto espera.",
         "¡Pelea con quien esté buscando ahora! Sin límite de tiempo, y puedes entrenar mientras esperas."),
    "Open a room with your own rules, then give your rival its code. No port forwarding needed!":
        ("Abra uma sala com suas regras e passe o código ao rival. Sem abrir portas no roteador!",
         "Abre una sala con tus reglas y pásale el código a tu rival. ¡Sin abrir puertos!"),
    "Got a room code from your rival? Type it in and jump right into their room!":
        ("Recebeu o código da sala do rival? Digite e entre direto na sala dele!",
         "¿Tu rival te pasó un código de sala? ¡Escríbelo y entra directo a su sala!"),
    "A lobby for up to eight: two fight, six watch. You pick who plays and the rules; the others join with its code.":
        ("Um lobby para até oito: dois lutam, seis assistem. Você escolhe quem joga e as regras; os outros entram com o código.",
         "Un lobby para hasta ocho: dos pelean, seis miran. Tú eliges quién juega y las reglas; los demás entran con el código."),
    "Your rivals, who is online, and invites. That one is coming soon!":
        ("Seus rivais, quem está online e convites. Essa chega em breve!", "Tus rivales, quién está en línea e invitaciones. ¡Esa llega pronto!"),
    "The Tenkaichi Budokai itself: brackets of 4 to 16 players. That one is coming soon!":
        ("O próprio Tenkaichi Budokai: chaves de 4 a 16 jogadores. Essa chega em breve!",
         "El propio Tenkaichi Budokai: llaves de 4 a 16 jugadores. ¡Esa llega pronto!"),
    "Your last matches, saved to watch again. That one is coming soon!":
        ("Suas últimas partidas, salvas para rever. Essa chega em breve!", "Tus últimas partidas, guardadas para volver a verlas. ¡Esa llega pronto!"),
    "The strongest in the world, among your friends, and with each fighter. Coming soon!":
        ("Os mais fortes do mundo, entre amigos e com cada lutador. Em breve!", "Los más fuertes del mundo, entre amigos y con cada luchador. ¡Pronto!"),
    "The name your rivals see.": ("O nome que seus rivais veem.", "El nombre que ven tus rivales."),
    "AUTO": ("AUTO", "AUTO"),
    "NORTH AMERICA": ("AMÉRICA DO NORTE", "AMÉRICA DEL NORTE"),
    "SOUTH AMERICA": ("AMÉRICA DO SUL", "AMÉRICA DEL SUR"),
    "EUROPE": ("EUROPA", "EUROPA"),
    "MIDDLE EAST": ("ORIENTE MÉDIO", "ORIENTE MEDIO"),
    "AFRICA": ("ÁFRICA", "ÁFRICA"),
    "ASIA": ("ÁSIA", "ASIA"),
    "OCEANIA": ("OCEANIA", "OCEANÍA"),
    "ANYWHERE": ("QUALQUER LUGAR", "CUALQUIER LUGAR"),
    "60 SECONDS": ("60 SEGUNDOS", "60 SEGUNDOS"),
    "90 SECONDS": ("90 SEGUNDOS", "90 SEGUNDOS"),
    "180 SECONDS": ("180 SEGUNDOS", "180 SEGUNDOS"),
    "240 SECONDS": ("240 SEGUNDOS", "240 SEGUNDOS"),
    "NO LIMIT": ("SEM LIMITE", "SIN LÍMITE"),
    "AUTOMATIC": ("AUTOMÁTICO", "AUTOMÁTICO"),
    "%d FRAME": ("%d QUADRO", "%d CUADRO"),
    "%d FRAMES": ("%d QUADROS", "%d CUADROS"),
    "NOT ON THIS PC": ("NÃO NESTE APARELHO", "NO EN ESTE EQUIPO"),
    "OFF": ("DESLIGADO", "APAGADO"),
    "UP TO %d FRAMES": ("ATÉ %d QUADROS", "HASTA %d CUADROS"),
    "CHECKING YOUR GAME DATA AGAINST THE DISC... TRY AGAIN IN A MOMENT":
        ("CONFERINDO SEUS DADOS COM O DISCO... TENTE DE NOVO EM INSTANTES", "COMPROBANDO TUS DATOS CON EL DISCO... PRUEBA DE NUEVO EN UN MOMENTO"),
    "Back": ("Voltar", "Atrás"),
    "THE NETWORK COULD NOT BE OPENED ON THIS PC": ("A REDE NÃO PÔDE SER ABERTA NESTE APARELHO", "NO SE PUDO ABRIR LA RED EN ESTE EQUIPO"),
    "THE TWO GAMES COULD NOT REACH EACH OTHER, EVEN THROUGH THE RELAY":
        ("OS DOIS JOGOS NÃO SE ALCANÇARAM, NEM PELO RELAY", "LOS DOS JUEGOS NO SE ALCANZARON, NI SIQUIERA POR EL RELÉ"),
    "THE ROOM COULD NOT BE OPENED": ("A SALA NÃO PÔDE SER ABERTA", "NO SE PUDO ABRIR LA SALA"),
    "Copy Code": ("Copiar Código", "Copiar Código"),
    "Train While Waiting": ("Treinar Esperando", "Entrenar Esperando"),
    "Cancel": ("Cancelar", "Cancelar"),
    "COPIED: PASTE THE CODE TO YOUR RIVAL": ("COPIADO: COLE O CÓDIGO PARA O RIVAL", "COPIADO: PÉGALE EL CÓDIGO A TU RIVAL"),
    "Change": ("Trocar", "Cambiar"),
    "Start Training": ("Começar Treino", "Empezar Entrenamiento"),
    "Search": ("Procurar", "Buscar"),
    "Enter": ("Entrar", "Entrar"),
    "NOT OPEN YET: IT NEEDS MORE FROM THE MATCHMAKING SERVICE THAN ROOM CODES":
        ("AINDA FECHADO: PRECISA DE MAIS DO SERVIÇO DE PARTIDAS QUE CÓDIGOS DE SALA",
         "AÚN NO ABIERTO: NECESITA DEL SERVICIO DE PARTIDAS MÁS QUE CÓDIGOS DE SALA"),
    "Open Room": ("Abrir Sala", "Abrir Sala"),
    "DELETE": ("APAGAR", "BORRAR"),
    "PASTE": ("COLAR", "PEGAR"),
    "JOIN": ("ENTRAR", "ENTRAR"),
    "SPACE": ("ESPAÇO", "ESPACIO"),
    "DONE": ("PRONTO", "LISTO"),
    "A ROOM CODE HAS SIX LETTERS AND NUMBERS": ("O CÓDIGO DA SALA TEM SEIS LETRAS E NÚMEROS", "UN CÓDIGO DE SALA TIENE SEIS LETRAS Y NÚMEROS"),
    "Delete": ("Apagar", "Borrar"),
    "Type Name": ("Digitar Nome", "Escribir Nombre"),
    "SAVED: YOUR RIVALS WILL SEE THIS NAME": ("SALVO: SEUS RIVAIS VÃO VER ESTE NOME", "GUARDADO: TUS RIVALES VERÁN ESTE NOMBRE"),
    "Battle": ("Batalha", "Batalla"),
    "Time Limit": ("Tempo", "Tiempo"),
    "Input Delay": ("Atraso", "Retraso"),
    "Rollback": ("Rollback", "Rollback"),
    "TEAMS (UP TO 5)": ("EQUIPES (ATÉ 5)", "EQUIPOS (HASTA 5)"),
    "ONE ON ONE": ("UM CONTRA UM", "UNO CONTRA UNO"),
    "ROOM SETTINGS": ("REGRAS DA SALA", "REGLAS DE LA SALA"),
    "QUICK MATCH": ("PARTIDA RÁPIDA", "PARTIDA RÁPIDA"),
    "TEAMS": ("EQUIPES", "EQUIPOS"),
    "YOUR RIVAL IS FROM": ("SEU RIVAL É DE", "TU RIVAL ES DE"),
    "NO ONE THERE:": ("NINGUÉM LÁ:", "NADIE ALLÍ:"),
    "SEARCHING EVERYWHERE": ("PROCURANDO EM TODO LUGAR", "BUSCANDO EN TODAS PARTES"),
    "PLAYERS IN": ("JOGADORES EM", "JUGADORES EN"),
    "FIRST, EVERYONE AFTER %d S": ("PRIMEIRO, TODOS DEPOIS DE %d S", "PRIMERO, TODOS DESPUÉS DE %d S"),
    "NO ONE NEARBY:": ("NINGUÉM POR PERTO:", "NADIE CERCA:"),
    "LOOKING FOR A RIVAL": ("PROCURANDO UM RIVAL", "BUSCANDO UN RIVAL"),
    "NEAR YOU FIRST": ("PERTO DE VOCÊ PRIMEIRO", "CERCA DE TI PRIMERO"),
    "THE SERVICE PAIRS YOU": ("O SERVIÇO TE JUNTA", "EL SERVICIO TE EMPAREJA"),
    "WITH THE NEXT RIVAL": ("COM O PRÓXIMO RIVAL", "CON EL SIGUIENTE RIVAL"),
    "%d IN THIS QUEUE": ("%d NESTA FILA", "%d EN ESTA COLA"),
    "ROOM CODE": ("CÓDIGO DA SALA", "CÓDIGO DE SALA"),
    "GETTING A CODE...": ("PEGANDO UM CÓDIGO...", "OBTENIENDO UN CÓDIGO..."),
    "GIVE THIS CODE TO": ("PASSE ESTE CÓDIGO PARA", "PÁSALE ESTE CÓDIGO A"),
    "ASKING THE": ("PEDINDO AO", "PIDIENDO AL"),
    "YOUR RIVAL": ("SEU RIVAL", "TU RIVAL"),
    "MATCHMAKING SERVICE": ("SERVIÇO DE PARTIDAS", "SERVICIO DE PARTIDAS"),
    "ANOTHER VERSION TRIED": ("OUTRA VERSÃO TENTOU", "OTRA VERSIÓN LO INTENTÓ"),
    "JOINING ROOM": ("ENTRANDO NA SALA", "ENTRANDO A LA SALA"),
    "LOOKING FOR": ("PROCURANDO", "BUSCANDO"),
    "THE HOST...": ("O ANFITRIÃO...", "AL ANFITRIÓN..."),
    "YOUR RIVAL IS HERE!": ("SEU RIVAL CHEGOU!", "¡TU RIVAL LLEGÓ!"),
    "SEARCHING...  %d:%02d": ("PROCURANDO...  %d:%02d", "BUSCANDO...  %d:%02d"),
    "Cannot Go Online": ("Sem Acesso Online", "No Puedes Jugar en Línea"),
    "Online matches are only between": ("Partidas online são só entre", "Las partidas en línea son solo entre"),
    "two unmodded games.": ("dois jogos sem mods.", "dos juegos sin mods."),
    "modified:": ("modificado:", "modificado:"),
    "Changed: ": ("Alterado: ", "Cambiado: "),
    "Your game data is not the disc's.": ("Seus dados não são os do disco.", "Tus datos no son los del disco."),
    "Your game data could not be checked.": ("Não deu para conferir seus dados.", "No se pudieron comprobar tus datos."),
    "Take out what you added in gamedata/mods.": ("Tire o que você pôs em gamedata/mods.", "Quita lo que pusiste en gamedata/mods."),
    "(Fighter mods switch off by themselves online.)": ("(Mods de lutadores desligam sozinhos online.)", "(Los mods de luchadores se apagan solos en línea.)"),
    "BACK": ("VOLTAR", "ATRÁS"),
    "TYPE YOUR NAME": ("DIGITE SEU NOME", "ESCRIBE TU NOMBRE"),
    "SPARRING AT THE TOURNAMENT": ("TREINO NO TORNEIO", "ENTRENAMIENTO EN EL TORNEO"),
    "You": ("Você", "Tú"),
    "Partner": ("Parceiro", "Compañero"),
    "QUICK MATCH SEARCH": ("BUSCA DE PARTIDA RÁPIDA", "BÚSQUEDA DE PARTIDA RÁPIDA"),
    "YOUR ROOM IS OPEN": ("SUA SALA ESTÁ ABERTA", "TU SALA ESTÁ ABIERTA"),
    "JOINING A ROOM": ("ENTRANDO EM UMA SALA", "ENTRANDO A UNA SALA"),
    "SEARCHING  %d:%02d": ("PROCURANDO  %d:%02d", "BUSCANDO  %d:%02d"),
    "THE MATCH STARTS THE MOMENT": ("A PARTIDA COMEÇA ASSIM QUE", "LA PARTIDA EMPIEZA EN CUANTO"),
    "YOUR RIVAL ARRIVES": ("SEU RIVAL CHEGAR", "LLEGUE TU RIVAL"),
    "PICK YOUR BATTLE": ("ESCOLHA SUA BATALHA", "ELIGE TU BATALLA"),
    "Region": ("Região", "Región"),
    "WAITING RIGHT NOW": ("ESPERANDO AGORA", "ESPERANDO AHORA"),
    "Teams": ("Equipes", "Equipos"),
    "One on One": ("Um contra Um", "Uno contra Uno"),
    "%d WAITING": ("%d ESPERANDO", "%d ESPERANDO"),
    "PLAYERS THERE FIRST, EVERYONE AFTER %d S": ("JOGADORES DE LÁ PRIMEIRO, TODOS DEPOIS DE %d S", "JUGADORES DE ALLÍ PRIMERO, TODOS DESPUÉS DE %d S"),
    "RIVALS NEAR YOU FIRST, THEN ANYWHERE": ("RIVAIS PERTO PRIMEIRO, DEPOIS QUALQUER LUGAR", "RIVALES CERCA PRIMERO, LUEGO CUALQUIER LUGAR"),
    "THE NEXT RIVAL, WHEREVER: THE FASTEST": ("O PRÓXIMO RIVAL, DE ONDE FOR: O MAIS RÁPIDO", "EL SIGUIENTE RIVAL, DE DONDE SEA: LO MÁS RÁPIDO"),
    "Name": ("Nome", "Nombre"),
    "PLAYER CARD": ("CARTÃO DO JOGADOR", "TARJETA DE JUGADOR"),
    "SINGLE BATTLE": ("BATALHA SIMPLES", "BATALLA SIMPLE"),
    "TEAM BATTLE": ("BATALHA EM EQUIPE", "BATALLA POR EQUIPOS"),
    "DP BATTLE: 10 DP": ("BATALHA DP: 10 DP", "BATALLA DP: 10 DP"),
    "DP BATTLE: 15 DP": ("BATALHA DP: 15 DP", "BATALLA DP: 15 DP"),
    "DP BATTLE: 20 DP": ("BATALHA DP: 20 DP", "BATALLA DP: 20 DP"),
    "Game Speed": ("Velocidade", "Velocidad"),
    "The owner went back to the lobby.": ("O dono voltou ao lobby.", "El dueño volvió al lobby."),
    "The two players could not connect to each other.": ("Os dois jogadores não conseguiram se conectar.", "Los dos jugadores no pudieron conectarse."),
    "The lobby was closed by its owner.": ("O dono fechou o lobby.", "El dueño cerró el lobby."),
    "The lobby's owner removed you from the lobby.": ("O dono do lobby te removeu.", "El dueño del lobby te sacó."),
    "The connection to the lobby's owner was lost: the lobby is closed.":
        ("A conexão com o dono do lobby caiu: o lobby fechou.", "Se perdió la conexión con el dueño del lobby: el lobby se cerró."),
    "This lobby is full (8 places).": ("Este lobby está cheio (8 lugares).", "Este lobby está lleno (8 lugares)."),
    "The lobby owner's game is a different version: joining needs the same release.":
        ("O jogo do dono do lobby é de outra versão: para entrar, as versões têm que ser iguais.",
         "El juego del dueño del lobby es de otra versión: para entrar hace falta la misma."),
    "The lobby owner's game data are not the same as yours (a modified disc image or mods on one side): you cannot join.":
        ("Os dados do jogo do dono do lobby são diferentes dos seus (imagem modificada ou mods de um lado): não dá para entrar.",
         "Los datos del juego del dueño del lobby no son como los tuyos (imagen modificada o mods en un lado): no puedes entrar."),
    "That address is not known.": ("Esse endereço não é conhecido.", "Esa dirección no se conoce."),
    "No socket could be opened.": ("Não deu para abrir a conexão de rede.", "No se pudo abrir la conexión de red."),
    "Lobbies need the game's state list (the .mem file beside the program): copy all the files of the release.":
        ("Lobbies precisam da lista de estado do jogo (o arquivo .mem ao lado do programa): copie todos os arquivos da versão.",
         "Los lobbies necesitan la lista de estado del juego (el archivo .mem junto al programa): copia todos los archivos de la versión."),
    "%s joined the lobby": ("%s entrou no lobby", "%s entró al lobby"),
    "%s left the lobby": ("%s saiu do lobby", "%s salió del lobby"),
    "%s was removed from the lobby": ("%s foi removido do lobby", "%s fue sacado del lobby"),
    "%s lost the connection to the lobby": ("%s perdeu a conexão com o lobby", "%s perdió la conexión con el lobby"),
    "Match ": ("Partida ", "Partida "),
    "Match %d: %s vs %s": ("Partida %d: %s vs %s", "Partida %d: %s vs %s"),
    "%d%% (STANDARD)": ("%d%% (PADRÃO)", "%d%% (ESTÁNDAR)"),
    "Match On": ("Em Partida", "En Partida"),
    "Start Match": ("Começar", "Empezar"),
    "Not Ready": ("Não Pronto", "No Listo"),
    "Ready": ("Pronto", "Listo"),
    "Chat": ("Chat", "Chat"),
    "Close Lobby": ("Fechar Lobby", "Cerrar Lobby"),
    "Leave Lobby": ("Sair do Lobby", "Salir del Lobby"),
    "A MATCH IS ON ALREADY": ("JÁ TEM UMA PARTIDA ROLANDO", "YA HAY UNA PARTIDA EN CURSO"),
    "TWO PLAYERS ARE NEEDED: ONE IN 1P, ONE IN 2P": ("PRECISA DE DOIS JOGADORES: UM NO 1P, UM NO 2P", "HACEN FALTA DOS JUGADORES: UNO EN 1P, UNO EN 2P"),
    "Send": ("Enviar", "Enviar"),
    "Yes": ("Sim", "Sí"),
    "No": ("Não", "No"),
    "Move": ("Mover", "Mover"),
    "Remove from the Lobby": ("Remover do Lobby", "Sacar del Lobby"),
    "Move Here": ("Mover para Cá", "Mover Aquí"),
    "THE RULES CANNOT CHANGE DURING A MATCH": ("AS REGRAS NÃO MUDAM DURANTE A PARTIDA", "LAS REGLAS NO CAMBIAN DURANTE LA PARTIDA"),
    "NOT DURING A MATCH: THE PLACES CHANGE BETWEEN MATCHES": ("NÃO DURANTE A PARTIDA: OS LUGARES MUDAM ENTRE PARTIDAS", "NO DURANTE LA PARTIDA: LOS LUGARES CAMBIAN ENTRE PARTIDAS"),
    "THE LOBBY'S OWNER CHOOSES WHO PLAYS": ("O DONO DO LOBBY ESCOLHE QUEM JOGA", "EL DUEÑO DEL LOBBY ELIGE QUIÉN JUEGA"),
    "COPIED: PASTE THE CODE TO THE OTHERS": ("COPIADO: COLE O CÓDIGO PARA OS OUTROS", "COPIADO: PÉGALES EL CÓDIGO A LOS DEMÁS"),
    "PLAYERS": ("JOGADORES", "JUGADORES"),
    "SPECTATORS  %d / 6": ("ESPECTADORES  %d / 6", "ESPECTADORES  %d / 6"),
    "OWNER": ("DONO", "DUEÑO"),
    "YOU": ("VOCÊ", "TÚ"),
    "READY": ("PRONTO", "LISTO"),
    "EMPTY": ("VAZIO", "VACÍO"),
    "LOBBY CODE": ("CÓDIGO DO LOBBY", "CÓDIGO DEL LOBBY"),
    "GIVE IT TO THE OTHERS": ("PASSE PARA OS OUTROS", "PÁSALO A LOS DEMÁS"),
    "JOIN ROOM WITH IT": ("ENTRAR EM SALA COM ELE", "ENTRAR A SALA CON ÉL"),
    "OTHERS JOIN WITH YOUR ADDRESS AND PORT": ("OS OUTROS ENTRAM COM SEU ENDEREÇO E PORTA", "LOS DEMÁS ENTRAN CON TU DIRECCIÓN Y PUERTO"),
    "LOBBY": ("LOBBY", "LOBBY"),
    "JOINED BY ADDRESS": ("ENTROU PELO ENDEREÇO", "ENTRÓ POR DIRECCIÓN"),
    "A MATCH IS ON": ("PARTIDA ROLANDO", "PARTIDA EN CURSO"),
    "RULES": ("REGRAS", "REGLAS"),
    "RULES (THE OWNER'S)": ("REGRAS (DO DONO)", "REGLAS (DEL DUEÑO)"),
    "Opening the Lobby": ("Abrindo o Lobby", "Abriendo el Lobby"),
    "Joining the Lobby": ("Entrando no Lobby", "Entrando al Lobby"),
    "Asking the matchmaking service for a code...": ("Pedindo um código ao serviço de partidas...", "Pidiendo un código al servicio de partidas..."),
    "Looking for the lobby's owner...": ("Procurando o dono do lobby...", "Buscando al dueño del lobby..."),
    "No Longer in the Lobby": ("Fora do Lobby", "Fuera del Lobby"),
    "The lobby is closed.": ("O lobby fechou.", "El lobby se cerró."),
    "Close the Lobby?": ("Fechar o Lobby?", "¿Cerrar el Lobby?"),
    "Leave the Lobby?": ("Sair do Lobby?", "¿Salir del Lobby?"),
    "Everyone in it goes back to the menus.": ("Todos voltam para os menus.", "Todos vuelven a los menús."),
    "You go back to the online menu.": ("Você volta para o menu online.", "Vuelves al menú en línea."),
    "SAY SOMETHING TO THE LOBBY": ("DIGA ALGO PARA O LOBBY", "DILE ALGO AL LOBBY"),
    "Opening the lobby": ("Abrindo o lobby", "Abriendo el lobby"),
    "Joining the lobby": ("Entrando no lobby", "Entrando al lobby"),
    "Close the lobby? ": ("Fechar o lobby? ", "¿Cerrar el lobby? "),
    "Leave the lobby? ": ("Sair do lobby? ", "¿Salir del lobby? "),
    "Player %d": ("Jogador %d", "Jugador %d"),
    "Spectator %d": ("Espectador %d", "Espectador %d"),
    "Move to ": ("Mover para ", "Mover a "),
    "empty": ("vazio", "vacío"),
    ", owner": (", dono", ", dueño"),
    ", ready": (", pronto", ", listo"),
    "How late your moves come out. Automatic measures the line when the match starts.":
        ("Quanto seus golpes atrasam. Automático mede a conexão quando a partida começa.",
         "Cuánto tardan tus golpes. Automático mide la conexión al empezar la partida."),
    "Your moves come out at once and the game corrects itself. More frames cope with a slower line.":
        ("Seus golpes saem na hora e o jogo se corrige. Mais quadros aguentam uma conexão mais lenta.",
         "Tus golpes salen al instante y el juego se corrige. Más cuadros aguantan una conexión más lenta."),
    "Set the rules of your tournament! Then your rival joins with the room code.":
        ("Defina as regras do seu torneio! Depois o rival entra com o código da sala.",
         "¡Define las reglas de tu torneo! Luego tu rival entra con el código de sala."),
    "Enter your rival's room code, or Paste the one they sent you! Then Join.":
        ("Digite o código da sala do rival, ou Cole o que ele mandou! Depois Entrar.",
         "¡Escribe el código de sala de tu rival, o Pega el que te mandó! Luego Entrar."),
    "Here we go! The match is starting!": ("Lá vamos nós! A partida vai começar!", "¡Allá vamos! ¡La partida empieza!"),
    "Looking for a worthy challenger... Warm up in Training meanwhile!":
        ("Procurando um desafiante à altura... Aqueça no Treino enquanto isso!", "Buscando un rival digno... ¡Calienta en el Entrenamiento mientras tanto!"),
    "Sorry! Online fights have to be fair, so only the game straight off the disc can play.":
        ("Desculpe! As lutas online têm que ser justas, então só o jogo igual ao do disco pode jogar.",
         "¡Lo siento! Las peleas en línea deben ser justas, así que solo juega el juego tal como viene en el disco."),
    "Warm up at the tournament while we search! Pick yourself and a sparring partner.":
        ("Aqueça no torneio enquanto procuramos! Escolha você e um parceiro de treino.",
         "¡Calienta en el torneo mientras buscamos! Elige a tu luchador y un compañero de entrenamiento."),
    "Auto: rivals near you first. A region: players there first, everyone after 40 s. Anywhere: the fastest.":
        ("Auto: rivais perto primeiro. Uma região: jogadores de lá primeiro, todos depois de 40 s. Qualquer lugar: o mais rápido.",
         "Auto: rivales cerca primero. Una región: jugadores de allí primero, todos tras 40 s. Cualquier lugar: lo más rápido."),
    "Pick a battle and we'll find you a rival! No time limit, and the game runs a little faster.":
        ("Escolha a batalha e achamos um rival! Sem limite de tempo, e o jogo roda um pouco mais rápido.",
         "¡Elige una batalla y te buscamos un rival! Sin límite de tiempo, y el juego va un poco más rápido."),
    "Pick your letters, then Done! Square deletes, Start finishes.":
        ("Escolha as letras e depois Pronto! Quadrado apaga, Start termina.", "¡Elige tus letras y luego Listo! Cuadrado borra, Start termina."),
    "Show everyone who you are! This is the name your rivals see.":
        ("Mostre a todos quem você é! Este é o nome que seus rivais veem.", "¡Muestra a todos quién eres! Este es el nombre que ven tus rivales."),
    "Opening your lobby! The others join it with its code.": ("Abrindo seu lobby! Os outros entram com o código.", "¡Abriendo tu lobby! Los demás entran con su código."),
    "Knocking at the lobby's door...": ("Batendo na porta do lobby...", "Tocando la puerta del lobby..."),
    "You are out of that lobby. OK takes you back to the online menu.":
        ("Você saiu daquele lobby. OK volta para o menu online.", "Ya no estás en ese lobby. OK te lleva al menú en línea."),
    "Type a line for everyone in the lobby, then Done! Start sends it too.":
        ("Escreva algo para todos no lobby e depois Pronto! Start também envia.", "¡Escribe algo para todos en el lobby y luego Listo! Start también lo envía."),
    "Welcome to the lobby! Its owner picks who plays and starts the match. Chat while you wait!":
        ("Bem-vindo ao lobby! O dono escolhe quem joga e começa a partida. Converse enquanto espera!",
         "¡Bienvenido al lobby! El dueño elige quién juega y empieza la partida. ¡Chatea mientras esperas!"),
    "Your lobby is open! Give the others its code: they join with Join Room. Then put two of you in 1P and 2P.":
        ("Seu lobby está aberto! Passe o código: os outros entram por Entrar em Sala. Depois ponha dois no 1P e no 2P.",
         "¡Tu lobby está abierto! Pasa el código: los demás entran con Entrar a Sala. Luego pon a dos en 1P y 2P."),
    "Your lobby is open! The others join with your address and port. Then put two of you in 1P and 2P.":
        ("Seu lobby está aberto! Os outros entram com seu endereço e porta. Depois ponha dois no 1P e no 2P.",
         "¡Tu lobby está abierto! Los demás entran con tu dirección y puerto. Luego pon a dos en 1P y 2P."),
    ", soon": (", em breve", ", pronto"),
    "Teams, up to 5": ("Equipes, até 5", "Equipos, hasta 5"),
    "One on one": ("Um contra um", "Uno contra uno"),
    "Your rival is here! The match begins.": ("Seu rival chegou! A partida começa.", "¡Tu rival llegó! La partida empieza."),
    " Your rival is from ": (" Seu rival é de ", " Tu rival es de "),
    "Quick match, teams: searching": ("Partida rápida, equipes: procurando", "Partida rápida, equipos: buscando"),
    "Quick match, one on one: searching": ("Partida rápida, um contra um: procurando", "Partida rápida, uno contra uno: buscando"),
    "Your room code: ": ("Código da sua sala: ", "Código de tu sala: "),
    "Getting a room code": ("Pegando um código de sala", "Obteniendo un código de sala"),
    "Joining room ": ("Entrando na sala ", "Entrando a la sala "),
    "Battle: teams": ("Batalha: equipes", "Batalla: equipos"),
    "Battle: one on one": ("Batalha: um contra um", "Batalla: uno contra uno"),
    "Region: ": ("Região: ", "Región: "),
    "You: ": ("Você: ", "Tú: "),
    "Partner: ": ("Parceiro: ", "Compañero: "),
    "Start training": ("Começar treino", "Empezar entrenamiento"),
    "Cannot go online. Online matches are only between two unmodded games.":
        ("Sem acesso online. Partidas online são só entre dois jogos sem mods.", "No puedes jugar en línea. Las partidas en línea son solo entre dos juegos sin mods."),
    "Name: ": ("Nome: ", "Nombre: "),
    "Code: ": ("Código: ", "Código: "),
    "Searching for a rival...  %d:%02d": ("Procurando um rival...  %d:%02d", "Buscando un rival...  %d:%02d"),
    "Quick Match: ": ("Partida Rápida: ", "Partida Rápida: "),
    "Your room: ": ("Sua sala: ", "Tu sala: "),
    "PLAYER 1": ("JOGADOR 1", "JUGADOR 1"),
    "PLAYER 2": ("JOGADOR 2", "JUGADOR 2"),
    "SPLIT SCREEN": ("TELA DIVIDIDA", "PANTALLA DIVIDIDA"),
    "Spectating %s vs %s": ("Assistindo %s vs %s", "Viendo %s vs %s"),
    "CATCHING UP WITH THE MATCH... %d%%": ("ALCANÇANDO A PARTIDA... %d%%", "ALCANZANDO LA PARTIDA... %d%%"),
    "NO LONGER THE PLAYERS' FIGHT (SINCE FRAME %d)": ("NÃO É MAIS A LUTA DOS JOGADORES (DESDE O QUADRO %d)", "YA NO ES LA PELEA DE LOS JUGADORES (DESDE EL CUADRO %d)"),
    "VIEW: ": ("CÂMERA: ", "CÁMARA: "),
    "   R1: CHANGE   START: MENU": ("   R1: TROCAR   START: MENU", "   R1: CAMBIAR   START: MENÚ"),
    # the spectator's pause menu (port/src/spec_menu.c)
    "Player 1": ("Jogador 1", "Jugador 1"),
    "Player 2": ("Jogador 2", "Jugador 2"),
    "Split Screen": ("Tela Dividida", "Pantalla Dividida"),
    "Stop Watching": ("Parar de Assistir", "Dejar de Ver"),
    "Close the Lobby": ("Fechar o Lobby", "Cerrar el Lobby"),
    "Leave the Lobby": ("Sair do Lobby", "Salir del Lobby"),
    "Your screen only: the fight goes on": ("Só a sua tela: a luta continua", "Solo tu pantalla: la pelea sigue"),
    "Continue": ("Continuar", "Continuar"),
    "View: ": ("Câmera: ", "Cámara: "),
    "Back to the Lobby": ("Voltar ao Lobby", "Volver al Lobby"),
}

# Lines that are not words to translate: keys of the settings, the keypads' rows, names and codes.
KEEP = re.compile(r"^(tbo_|net_)|^[a-z]{2,3}$|^[A-Z]{2}$|^(ABCDEFGHI|JKLMNOPQR|STUVWXYZ0|abcdefghi|jklmnopqr|stuvwxyz0|Aa|auto|any|%dP|%d%%| vs |OK|"
                  r"Chat: |Rollback|Lobby|LOBBY|Chat|Dragon Net Online|ANDROID \+ SWITCH|PC|Bronce|Plata|Oro|Platino|Diamante|Maestro|AUTO|EUROPA|dr_lang|ENGLISH|PORTUGU.xC3.x8AS|ESPA.xC3.x91OL)$")


def screen_lines():
    s = "\n".join(p.read_text(encoding="utf-8") for p in SOURCES)
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    s = re.sub(r"#include[^\n]*", "", s)
    a, b = s.find('"Goku (Early)"'), s.find('"Future Gohan Super Saiyan"')
    if a >= 0 and b >= 0:  # the fighters' names: as they are
        s = s[:a] + s[b:].split("\n", 1)[1]
    out = []
    for l in re.findall(r'"((?:[^"\\]|\\.)*)"', s):
        if re.search(r"[A-Za-z]{2}", l) and l not in out:
            out.append(l)
    return out


def slots(t):
    return re.findall(r"%[-0-9]*[ds%]|\{[a-z]+\}", t)


def main():
    have = screen_lines()
    bad = 0
    for en, (pt, es) in LINES.items():
        if en not in have:
            print("not on the screen any more:", repr(en))
        for t in (pt, es):
            if slots(t) != slots(en):
                print("lost a slot:", repr(en), "->", repr(t))
                bad = 1
    for l in have:
        if l not in LINES and not KEEP.search(l):
            print("no translation:", repr(l))
    if bad:
        return 1
    OUT.mkdir(parents=True, exist_ok=True)
    for i, (loc, name) in enumerate((("pt", "Português"), ("es", "Español"))):
        labels = {f"tbo/{n:03d}": {"original": en, "text": tr[i]} for n, (en, tr) in enumerate(LINES.items())}
        doc = {"schema_version": 1, "locale": loc, "name": name, "labels": labels}
        (OUT / f"{loc}.json").write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("wrote", OUT.relative_to(ROOT), len(LINES), "lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
