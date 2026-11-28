#!/usr/bin/env python3
"""Z Warriors in Portuguese, English and Spanish: writes src/menu/port_zw_lang.inc, the table the mod's screens look
their texts up in as they are drawn (port_zw.c Zw_Tr). A text's key is the very string literal in the code (with its
C escapes); a text not listed is shown as written. Formats keep their % in the same order."""
import pathlib
import re

# blocks of lines "key | English | Spanish" (key written in Portuguese) and "key | Portuguese | Spanish" (in English)
FROM_PT = r'''
Nome|Name|Nombre
Título|Title|Título
Corpo|Body|Cuerpo
Postura|Stance|Postura
Voz|Voice|Voz
Retrato / ícone / rosto do HUD|Portrait / icon / HUD face|Retrato / icono / cara del HUD
Esculpir|Sculpt|Esculpir
Cores|Colours|Colores
Sombreamento|Shading|Sombreado
Pintar texturas|Paint textures|Pintar texturas
Cabelo|Hair|Pelo
Tamanho da cabeça|Head size|Tamaño de la cabeza
Golpes corpo a corpo|Melee moves|Golpes cuerpo a cuerpo
Finalizadores|Finishers|Remates
Ajuste fino da Ultimate|Ultimate fine-tune|Ajuste de la Ultimate
Forma 1|Form 1|Forma 1
Comando|Command|Comando
Custo de ki|Ki cost|Coste de ki
Tipo|Type|Tipo
Cor da aura|Aura colour|Color del aura
Forma 2|Form 2|Forma 2
GUERREIRO|WARRIOR|GUERRERO
APARÊNCIA|LOOKS|APARIENCIA
Pose própria|Own pose|Pose propia
RETRATOS|PORTRAITS|RETRATOS
Retrato|Portrait|Retrato
Ícone de seleção|Select icon|Icono de selección
Rosto do HUD|HUD face|Cara del HUD
Prévia|Preview|Vista previa
TRAJES|COSTUMES|TRAJES
Trajes|Costumes|Trajes
Editar traje|Edit costume|Editar traje
DANOS DE BATALHA|BATTLE DAMAGE|DAÑOS DE BATALLA
Aparência danificada|Damaged look|Aspecto dañado
FERRAMENTAS|TOOLS|HERRAMIENTAS
Sombreamento emprestado|Borrowed shading|Sombreado prestado
Brilho Super Saiyajin|Super Saiyan glow|Brillo Super Saiyajin
Sortear|Surprise|Sortear
Cabeça inteira|Whole head|Cabeza entera
Roupa inteira|Whole outfit|Ropa completa
Redefinir peças|Reset parts|Restablecer piezas
CAMADAS EXTRAS|EXTRA LAYERS|CAPAS EXTRA
Camada 1|Layer 1|Capa 1
Camada 2|Layer 2|Capa 2
Camada 3|Layer 3|Capa 3
Camada 4|Layer 4|Capa 4
PORTE|BUILD|COMPLEXIÓN
Tronco|Torso|Torso
Braços|Arms|Brazos
Pernas|Legs|Piernas
CABEÇA|HEAD|CABEZA
Cabeça|Head|Cabeza
Rosto|Face|Cara
Tamanho do cabelo|Hair size|Tamaño del pelo
PARTE DE CIMA|UPPER BODY|PARTE SUPERIOR
Peito|Chest|Pecho
Cintura|Waist|Cintura
Antebraços|Forearms|Antebrazos
Mãos|Hands|Manos
Tamanho das mãos|Hand size|Tamaño de las manos
PARTE DE BAIXO|LOWER BODY|PARTE INFERIOR
Quadril e coxas|Hips and thighs|Cadera y muslos
Canelas|Shins|Espinillas
Tamanho dos pés|Foot size|Tamaño de los pies
EXTRAS|EXTRAS|EXTRAS
Acessórios|Accessories|Accesorios
LUTA|FIGHT|LUCHA
Atributos|Stats|Atributos
Rajadas de ki|Ki blasts|Ráfagas de ki
ANIMAÇÕES|ANIMATIONS|ANIMACIONES
Andar e voar|Walk and fly|Andar y volar
Carregar|Charge|Cargar
Disparar ki|Fire ki|Disparar ki
Agarrões|Grabs|Agarres
Defesa e esquiva|Guard and dodge|Defensa y esquiva
Entrada|Intro|Entrada
Vitória|Victory|Victoria
Provocação|Taunt|Provocación
Técnica|Technique|Técnica
Visual|Look|Aspecto
Ajuste fino|Fine-tune|Ajuste fino
Brilho do corpo|Body glow|Brillo del cuerpo
Efeitos|Effects|Efectos
Câmera|Camera|Cámara
Câmera de abertura|Opening camera|Cámara de apertura
Câmera final|Final camera|Cámara final
Guerreiro|Warrior|Guerrero
FORMA 1|FORM 1|FORMA 1
FORMA 2|FORM 2|FORMA 2
FORMA 3|FORM 3|FORMA 3
FORMA 4|FORM 4|FORMA 4
Sortear tudo|Surprise me|Sortear todo
Animações e aura|Animations and aura|Animaciones y aura
Formas 3 e 4|Forms 3 and 4|Formas 3 y 4
PEÇAS|PARTS|PIEZAS
GOLPES|MOVES|GOLPES
FORMAS|FORMS|FORMAS
Sem nome|No name|Sin nombre
  \342\200\242  não salvo|  \342\200\242  not saved|  \342\200\242  sin guardar
todas as opções  \342\200\242  %s%s|all options  \342\200\242  %s%s|todas las opciones  \342\200\242  %s%s
Peças|Parts|Piezas
Golpes|Moves|Golpes
Transformações|Transformations|Transformaciones
 1 de 2| 1 of 2| 1 de 2
 2 de 2| 2 of 2| 2 de 2
EDITAR GUERREIRO|EDIT WARRIOR|EDITAR GUERRERO
SALVAR|SAVE|GUARDAR
modelo 3D do guerreiro|the warrior in 3D|el guerrero en 3D
3 itens|3 items|3 elementos
Seleção de personagens|Character select|Selección de personajes
Luta de teste|Test fight|Lucha de prueba
Desfazer|Undo|Deshacer
Montando seu guerreiro...|Building your warrior...|Montando tu guerrero...
O comando de transformação é o do jogo.|The transform input is the game's.|El comando de transformación es el del juego.
Do jogo|From the game|Del juego
Dos mods|From mods|De mods
Nenhum|None|Ninguno
Traje %d|Costume %d|Traje %d
Traje: do guerreiro|Costume: the warrior's|Traje: del guerrero
para: %s%s|for: %s%s|para: %s%s
camada em |layer on |capa en 
APRENDER TÉCNICA|LEARN TECHNIQUE|APRENDER TÉCNICA
ESCOLHER LUTADOR|CHOOSE FIGHTER|ELEGIR LUCHADOR
APRENDER|LEARN|APRENDER
USAR|USE|USAR
LUTADOR|FIGHTER|LUCHADOR
<  Anterior|<  Previous|<  Anterior
Próxima  >|Next  >|Siguiente  >
%s %d de %d|%s %d of %d|%s %d de %d
Personagens|Characters|Personajes
escolha a técnica|choose the technique|elige la técnica
investida: não serve aqui|rush: cannot go here|embestida: aquí no sirve
outro tipo: não serve aqui|another kind: cannot go here|otro tipo: aquí no sirve
(nenhuma)|(none)|(ninguna)
Cancelar|Cancel|Cancelar
experimentando: %s|trying: %s|probando: %s
Remover \"%.40s\"?|Remove \"%.40s\"?|¿Quitar \"%.40s\"?
A pasta vai para mods_removed.|Its folder goes to mods_removed.|La carpeta va a mods_removed.
Nada é apagado.|Nothing is deleted.|No se borra nada.
REMOVER|REMOVE|QUITAR
CANCELAR|CANCEL|CANCELAR
Editar o Traje 2?|Edit Costume 2?|¿Editar el Traje 2?
O que você tem agora vira o Traje 1.|What you have now becomes Costume 1.|Lo que tienes ahora será el Traje 1.
O Traje 2 começa como uma cópia.|Costume 2 starts as a copy.|El Traje 2 empieza como copia.
COMEÇAR|START|EMPEZAR
Desligar os trajes?|Turn costumes off?|¿Desactivar los trajes?
Os trajes 2 a 4 saem ao salvar.|Costumes 2 to 4 go when you save.|Los trajes 2 a 4 se quitan al guardar.
O Traje 1 fica.|Costume 1 stays.|El Traje 1 se queda.
DESLIGAR|TURN OFF|DESACTIVAR
Tirar da Oficina?|Take down from the Workshop?|¿Quitar del Taller?
Ele sai da Oficina.|It leaves the Workshop.|Sale del Taller.
O seu guerreiro continua com você.|Your warrior stays with you.|Tu guerrero sigue contigo.
TIRAR|TAKE DOWN|QUITAR
Sair sem salvar?|Leave without saving?|¿Salir sin guardar?
Há mudanças que não foram salvas.|There are unsaved changes.|Hay cambios sin guardar.
NÃO SALVAR|DON'T SAVE|NO GUARDAR
'''

FROM_EN = r'''
Aura|Aura|Aura
Blast 1 (1)|Blast 1 (1)|Blast 1 (1)
Blast 1 (2)|Blast 1 (2)|Blast 1 (2)
Blast 2 (1)|Blast 2 (1)|Blast 2 (1)
Blast 2 (2)|Blast 2 (2)|Blast 2 (2)
ULTIMATE|ULTIMATE|ULTIMATE
Ultimate|Ultimate|Ultimate
own colours|cores próprias|colores propios
%d painted|%d pintadas|%d pintadas
(save first)|(salve antes)|(guarda antes)
%s (own colour)|%s (cor própria)|%s (color propio)
Random parts|Peças aleatórias|Piezas al azar
Turns into %s|Vira %s|Se vuelve %s
(a warrior that is gone)|(um guerreiro que não existe mais)|(un guerrero que ya no está)
R3 + Left|R3 + Esquerda|R3 + Izquierda
R3 + Right|R3 + Direita|R3 + Derecha
R3 + Down|R3 + Baixo|R3 + Abajo
R3 (or %s)|R3 (ou %s)|R3 (o %s)
Back to base form|Voltar à forma base|Volver a la forma base
Power up|Fortalecimento|Potenciación
From borrowed shading|Da sombra emprestada|De la sombra prestada
From %s|De %s|De %s
%d bones posed|%d ossos posados|%d huesos posados
Draw or add pictures|Desenhar ou pôr imagens|Dibujar o poner imágenes
%d stat changed|%d atributo mudado|%d atributo cambiado
%d stats changed|%d atributos mudados|%d atributos cambiados
%d custom part|%d peça própria|%d pieza propia
%d custom parts|%d peças próprias|%d piezas propias
Holes patched|Buracos tapados|Huecos tapados
Stretch, hide or move pieces|Esticar, esconder ou mover peças|Estirar, ocultar o mover piezas
%d pieces edited, patched|%d peças editadas, tapadas|%d piezas editadas, tapadas
%d pieces edited|%d peças editadas|%d piezas editadas
On (four outfits)|Ligado (quatro trajes)|Activado (cuatro trajes)
Off (one outfit)|Desligado (um traje)|Desactivado (un traje)
Hips|Quadril|Cadera
Tail|Cauda|Cola
Costume 1 (costumes off)|Traje 1 (trajes desligados)|Traje 1 (trajes desactivados)
Costume %d of 4|Traje %d de 4|Traje %d de 4
The fighters' own|O dos lutadores|El de los luchadores
Scuffed|Arranhado|Rasguñado
Battle-torn|Rasgado de luta|Destrozado
Stays clean|Fica limpo|Queda limpio
Looks like %s|Visual de %s|Aspecto de %s
%s  (strength %d%%)|%s  (força %d%%)|%s  (fuerza %d%%)
Kaio-ken red|Vermelho Kaio-ken|Rojo Kaio-ken
  (a giant!)|  (um gigante!)|  (¡un gigante!)
%d%%  (locked)|%d%%  (travado)|%d%%  (bloqueado)
%s  (locked)|%s  (travado)|%s  (bloqueado)
The body's own|O do corpo|El del cuerpo
 (%s aura)| (aura de %s)| (aura de %s)
Too many fighters' shading (16 at most): some parts borrow another's and look off. Fewer hair layers fix it.|Sombras de lutadores demais (máximo 16): algumas peças pegam outra e ficam estranhas. Menos camadas de cabelo resolve.|Demasiadas sombras (máximo 16): algunas piezas toman otra y se ven mal. Menos capas de pelo lo arregla.
Start editing Costume 2?|Começar a editar o Traje 2?|¿Editar el Traje 2?
Turn costumes off?|Desligar os trajes?|¿Desactivar los trajes?
It leaves the Workshop. Your own warrior and the ones others got stay.|Sai da Oficina. O seu guerreiro e os que outros baixaram ficam.|Sale del Taller. Tu guerrero y los descargados se quedan.
What you have now is kept as Costume 1. Costume 2 starts as a copy of it.|O atual vira o Traje 1. O Traje 2 começa como cópia dele.|Lo actual queda como Traje 1. El Traje 2 empieza como copia.
Costumes 2 to 4 go when you save. Costume 1 stays.|Os trajes 2 a 4 somem ao salvar. O Traje 1 fica.|Los trajes 2 a 4 se van al guardar. El 1 se queda.
Off to the Other Other World with you!|Para o Outro Mundo do Outro Mundo!|¡Al Otro Otro Mundo!
Costume 2! Change anything on its look: parts, colours, edits. Left and right on Editing for the others.|Traje 2! Mude peças, cores e edições. Use Editar traje para os outros.|¡Traje 2! Cambia piezas, colores y ediciones. Usa Editar traje para los otros.
One outfit again. Save to make it so.|Um traje de novo. Salve para valer.|Un solo traje otra vez. Guarda para aplicarlo.
Taken down from the Workshop.|Retirado da Oficina.|Quitado del Taller.
Saiyan|Saiyajin|Saiyan
Fan form|Forma de fã|Forma de fan
Fusion|Fusão|Fusión
Xeno|Xeno|Xeno
What-if|E se...|¿Y si...?
Recipe received|Receita recebida|Receta recibida
Drawings and photos|Desenhos e fotos|Dibujos y fotos
Building from your game...|Montando do seu jogo...|Construyendo desde tu juego...
Built from your own copy of the game: only the recipe is downloaded.|Montado da sua cópia do jogo: só a receita é baixada.|Hecho con tu copia del juego: solo se descarga la receta.
Here he is! He's in your warriors now, ready to fight.|Chegou! Já está nos seus guerreiros, pronto para lutar.|¡Ya llegó! Está en tus guerreros, listo.
Hmm, that one could not be brought here.|Hmm, não deu para trazer esse.|Hmm, no se pudo traer.
Upload a warrior from MY WARRIORS (Start).|Envie um guerreiro em MEUS GUERREIROS (Start).|Sube un guerrero desde MIS GUERREROS (Start).
Created by %s|Criado por %s|Creado por %s
Downloads %d    Likes %d|Downloads %d    Curtidas %d|Descargas %d    Me gusta %d
IN YOUR WARRIORS|NOS SEUS GUERREIROS|EN TUS GUERREROS
Here he comes! Hold on to your antennae!|Lá vem! Segure as antenas!|¡Ahí viene! ¡Sujeta las antenas!
Pick a warrior from any world, heh heh!|Escolha um guerreiro de qualquer mundo, heh heh!|¡Elige un guerrero de cualquier mundo, je je!
Already in your warriors! Look in MY WARRIORS.|Já está nos seus guerreiros! Veja em MEUS GUERREIROS.|¡Ya está en tus guerreros! Mira en MIS GUERREROS.
Downloads %d    Likes %d%s    Uploaded %s    Version %d|Downloads %d    Curtidas %d%s    Enviado %s    Versão %d|Descargas %d    Me gusta %d%s    Subido %s    Versión %d
 (you)| (você)| (tú)
(no bio)|(sem descrição)|(sin descripción)
Tags: %s|Marcas: %s|Etiquetas: %s
CONTAINS|CONTÉM|CONTIENE
BUILT FROM|FEITO COM|HECHO CON
Ooh, a popular one! Read up before you take him home.|Ooh, um popular! Leia antes de levar.|¡Uno popular! Lee antes de llevarlo.
A fresh one! Be the first to try him out.|Novinho! Seja o primeiro a testar.|¡Nuevo! Sé el primero en probarlo.
Bio:|Descrição:|Descripción:
(Cross to write one)|(Cross para escrever)|(Cross para escribir)
Tags:|Marcas:|Etiquetas:
READY CHECK|CONFERÊNCIA|COMPROBACIÓN
- It does not build yet: fix it first|- Ainda não monta: corrija antes|- Aún no se construye: corrígelo
+ Builds without errors|+ Monta sem erros|+ Se construye sin errores
+ Drawings, photos and costumes included|+ Desenhos, fotos e trajes incluídos|+ Dibujos, fotos y trajes incluidos
Only your recipe is uploaded, never the game's files.|Só a receita é enviada, nunca arquivos do jogo.|Solo se sube la receta, nunca archivos del juego.
Show the whole Other World what you made!|Mostre ao Outro Mundo o que você fez!|¡Muestra al Otro Mundo lo que hiciste!
It has to build first! Fix it in the editor, then upload.|Precisa montar antes! Corrija no editor e envie.|¡Primero debe construirse! Corrígelo y súbelo.
It's up! The whole Other World can get it now.|Enviado! Todo o Outro Mundo já pode pegar.|¡Subido! Todo el Otro Mundo puede tenerlo.
Hmm, the upload did not work. Try again?|Hmm, o envio falhou. Tentar de novo?|Hmm, la subida falló. ¿Otra vez?
Z WARRIORS|Z WARRIORS|Z WARRIORS
New Warrior|Novo Guerreiro|Nuevo Guerrero
Z Warrior|Guerreiro Z|Guerrero Z
The second line on the select screen. Make it catchy!|A segunda linha na seleção. Capriche!|La segunda línea en la selección. ¡Que enganche!
Hair, head and face from one fighter, all at once!|Cabelo, cabeça e rosto de um lutador, de uma vez!|¡Pelo, cabeza y rostro de un luchador, de una vez!
Everything from the neck down, accessories too. A whole new outfit!|Tudo do pescoço para baixo, com acessórios. Roupa nova!|Todo del cuello abajo, con accesorios. ¡Traje nuevo!
Whose hair? Spikes, bangs, the lot! Circle layers another on top.|De quem é o cabelo? Espetado, franja, tudo!|¿De quién es el pelo? ¡Púas, flequillo, todo!
Whose head and ears? The skull under the hair.|De quem são cabeça e orelhas? O crânio sob o cabelo.|¿De quién la cabeza y orejas? El cráneo bajo el pelo.
Whose face? The eyes and expressions come along!|De quem é o rosto? Olhos e expressões vêm junto!|¿De quién el rostro? ¡Con ojos y expresiones!
Whose chest and shoulders? Armour, a gi, a bare chest...|De quem são peito e ombros? Armadura, kimono, peito nu...|¿De quién pecho y hombros? Armadura, gi, torso desnudo...
Whose waist? The belt or sash.|De quem é a cintura? A faixa ou cinto.|¿De quién la cintura? La faja o cinturón.
Whose upper arms? Sleeves or muscles!|De quem são os braços? Mangas ou músculos!|¿De quién los brazos? ¡Mangas o músculos!
Whose forearms? Wristbands, gloves' cuffs...|De quem são os antebraços? Munhequeiras, punhos...|¿De quién los antebrazos? Muñequeras, puños...
Whose hands? Gloves, claws, fingers and all!|De quem são as mãos? Luvas, garras, dedos e tudo!|¿De quién las manos? ¡Guantes, garras, dedos y todo!
Whose hips and thighs? Trousers, armour skirts...|De quem são quadril e coxas? Calças, saias de armadura...|¿De quién cadera y muslos? Pantalones, faldas de armadura...
Whose shins? The bottom of the trousers or the boots' tops.|De quem são as canelas? A barra da calça ou o topo das botas.|¿De quién las espinillas? El bajo del pantalón o las botas.
Whose feet? A warrior needs good boots!|De quem são os pés? Guerreiro precisa de boas botas!|¿De quién los pies? ¡Un guerrero necesita buenas botas!
Whose accessories? Capes, swords, halos, tails of cloth!|De quem são os acessórios? Capas, espadas, auréolas!|¿De quién los accesorios? ¡Capas, espadas, aureolas!
Left and right: a bigger or smaller head!|Cabeça maior ou menor!|¡Cabeza más grande o pequeña!
Left and right: wilder hair, or tamer!|Cabelo mais rebelde ou mais comportado!|¡Pelo más salvaje o más domado!
Left and right: the hands' size. Big fists hit hard!|Tamanho das mãos. Punho grande bate forte!|Tamaño de las manos. ¡Puños grandes pegan fuerte!
Left and right: the feet's size.|Tamanho dos pés.|Tamaño de los pies.
Left and right: skinny arms or huge ones!|Braços finos ou enormes!|¡Brazos finos o enormes!
Left and right: a broader chest, or a leaner one.|Peito mais largo ou mais magro.|Pecho más ancho o más delgado.
Left and right: the legs' build. Never skip leg day!|Físico das pernas. Não pule o treino!|Complexión de piernas. ¡No saltes el día de piernas!
A warrior made of random parts. Feeling lucky?|Um guerreiro de peças aleatórias. Sente a sorte?|Un guerrero de piezas al azar. ¿Con suerte?
Resets every part, size and layer to the body's default.|Volta peças, tamanhos e camadas ao padrão do corpo.|Restaura piezas, tamaños y capas del cuerpo.
Every warrior needs a battle cry. Whose shall it be?|Todo guerreiro precisa de um grito. De quem será?|Todo guerrero necesita un grito. ¿De quién?
A splash of colour! Paint any part of your warrior.|Um toque de cor! Pinte qualquer parte.|¡Un toque de color! Pinta cualquier parte.
Transforms into another warrior you made. Left and right to choose. In battle, use the normal transform input.|Vira outro guerreiro seu. Escolha qual. Na luta, use o comando normal de transformação.|Se transforma en otro guerrero tuyo. En combate usa el comando normal.
How many ki bars the transformation costs.|Quantas barras de ki a transformação custa.|Cuántas barras de ki cuesta la transformación.
In battle: build up the ki it costs, then press these buttons.|Na luta: junte o ki e aperte estes botões.|En combate: junta el ki y pulsa estos botones.
The aura colour when it transforms. Left and right to change it.|A cor da aura ao se transformar.|El color del aura al transformarse.
Power up, or return to base form. Give the transformed warrior a return form to change back.|Fortalecer ou voltar à base. Dê à forma um caminho de volta.|Potenciar o volver a la base. Dale a la forma un camino de vuelta.
The glow transformed fighters have, like Super Saiyans. Left and right to change it.|O brilho dos transformados, como Super Saiyajins.|El brillo de los transformados, como Super Saiyan.
Borrow another fighter's shading and glow for every part. Try a Super Saiyan's on a base form!|Pegue sombra e brilho de outro lutador. Tente de um Super Saiyajin na forma base!|Toma sombra y brillo de otro luchador. ¡Prueba uno de Super Saiyan!
Pose your warrior's stance: turn the arms, legs, head... It still breathes!|Pose da postura: gire braços, pernas, cabeça... Ainda respira!|Posa la postura: gira brazos, piernas, cabeza... ¡Aún respira!
Draw on your warrior's textures, or put a picture of your own on one! A mouse works too.|Desenhe nas texturas ou ponha uma imagem sua!|¡Dibuja en las texturas o pon una imagen tuya!
The big portrait: Circle for a photo of your warrior, or drop a picture on the window!|O retrato grande: tire uma foto do guerreiro.|El retrato grande: toma una foto del guerrero.
The little face on the grids: crop it from the render, or Circle for a photo!|O rostinho das grades: recorte do retrato ou fotografe!|La carita de las rejillas: recorta o fotografía.
No guessing! Cross: the real character select. Circle: a test fight, health bar and all.|Sem adivinhar! Veja na seleção real ou numa luta de teste.|¡Sin adivinar! Míralo en la selección o en un combate de prueba.
The face on the health bar in battle. Or take a photo of your warrior's face!|O rosto na barra de vida. Ou fotografe o seu!|La cara de la barra de vida. ¡O fotografía la tuya!
Whose punches, kicks and rushes? The techniques below too, unless you swap them.|De quem são socos, chutes e investidas? As técnicas abaixo também, se não trocar.|¿De quién puños, patadas y embestidas? Y las técnicas si no las cambias.
Whose ki blasts and charge? Each technique keeps its own effects.|De quem são rajadas e carga? Cada técnica mantém seus efeitos.|¿De quién ráfagas y carga? Cada técnica mantiene sus efectos.
Add another fighter's part on top, like a second hairstyle or a scouter. Cross: choose the fighter. Left and right: choose the part.|Ponha a peça de outro por cima, como um segundo cabelo ou um rastreador.|Pon la pieza de otro encima, como otro peinado o un rastreador.
Four outfits of your own! Turn it on, then pick a costume below and change its look.|Quatro trajes seus! Ligue, escolha um traje e mude o visual.|¡Cuatro trajes tuyos! Actívalo, elige uno y cambia su aspecto.
The costume you are changing now. Left and right for another; Square starts it again from Costume 1.|O traje que está mudando. Pode recomeçar a partir do Traje 1.|El traje que estás cambiando. Puedes recomenzarlo desde el 1.
Turn Costumes on first: then each costume gets its own look!|Ligue os Trajes antes: cada um terá seu visual!|¡Activa los Trajes primero: cada uno tendrá su aspecto!
How your warrior looks beaten up, in every costume: the fighters' own, or wear painted on!|Como fica machucado: o dos lutadores ou desgaste pintado!|Cómo se ve herido: el de los luchadores o desgaste pintado.
Your warrior beaten up: the look of another warrior you made. Make one with torn clothes!|Machucado com o visual de outro guerreiro seu. Faça um de roupa rasgada!|Herido con el aspecto de otro guerrero tuyo. ¡Haz uno con la ropa rota!
Another outfit: wear the look of another warrior you made. Left and right to choose.|Outro traje: o visual de outro guerreiro seu.|Otro traje: el aspecto de otro guerrero tuyo.
Borrow another Blast 1's look. Only the look changes, not what it does.|Pegue o visual de outro Blast 1. Só o visual muda.|Toma el aspecto de otro Blast 1. Solo cambia lo visual.
Whose way of moving? Walking, dashing, jumping, flying up and down.|De quem é o jeito de andar, correr, pular e voar?|¿De quién la forma de andar, correr, saltar y volar?
How your warrior charges ki and powers up!|Como carrega o ki e se fortalece!|¡Cómo carga el ki y se potencia!
Whose punches and kicks look? The combos and their damage stay the same.|De quem é o visual de socos e chutes? Combos e dano iguais.|¿De quién el aspecto de puños y patadas? Combos y daño iguales.
Whose way of throwing ki blasts? Only the motion, not the blasts themselves.|De quem é o jeito de lançar ki? Só o movimento.|¿De quién la forma de lanzar ki? Solo el movimiento.
Whose grabs and throws?|De quem são agarrões e arremessos?|¿De quién los agarres y lanzamientos?
Whose guard, dodges and vanishes?|De quem são defesa, esquivas e sumiços?|¿De quién la guardia, esquivas y desapariciones?
Whose aura? Its flames, the light on your warrior and the ki trails. Left and right or Select: its colour!|De quem é a aura? Chamas, luz e rastros de ki. Toque em COR para a cor!|¿De quién el aura? Llamas, luz y estelas. ¡Toca COR para el color!
Whose health, stats and ki rates should your warrior have?|De quem o guerreiro tem vida, atributos e ki?|¿De quién la vida, atributos y ki?
How your warrior stands: the pose and the battle idle. Its own, or anyone's!|Como fica parado: pose e posição de luta. A sua ou de qualquer um!|Cómo se planta: pose y guardia. ¡La suya o la de cualquiera!
Sculpt every piece of your warrior: stretch it, move it or hide it.|Esculpa cada peça: estique, mova ou esconda.|Esculpe cada pieza: estira, mueve u oculta.
The look of this technique, from another of the same kind. A purple Kamehameha, anyone?|O visual desta técnica, de outra do mesmo tipo. Um Kamehameha roxo?|El aspecto de otra técnica del mismo tipo. ¿Un Kamehameha morado?
The camera for the start and charge, borrowed from any technique. Rushes keep their own.|A câmera do início e da carga, de qualquer técnica.|La cámara del inicio y la carga, de cualquier técnica.
The camera for the Ultimate's opening pose and charge, borrowed from any technique.|A câmera da abertura e carga do Ultimate.|La cámara de la apertura y carga del Ultimate.
The camera once the Ultimate fires: the beam, the hit, the rush. Borrowed from any technique.|A câmera quando o Ultimate dispara: raio, impacto, investida.|La cámara al disparar el Ultimate: rayo, impacto, embestida.
How the rush combos end. Pow!|Como os combos de investida terminam. Pow!|Cómo terminan los combos. ¡Pum!
Fold a section away, or open it again. Less scrolling!|Feche ou abra uma seção. Menos rolagem!|Pliega o abre una sección. ¡Menos desplazamiento!
Copy how another technique behaves: any Blast 2 or Ultimate, except rushes.|Copie o comportamento de outra técnica: Blast 2 ou Ultimate, menos investidas.|Copia el comportamiento de otra técnica, salvo embestidas.
Fine-tune this technique: damage, hits, size, speed, unblockable and more.|Ajuste fino: dano, acertos, tamanho, velocidade, indefensável e mais.|Ajuste fino: daño, golpes, tamaño, velocidad, imbloqueable y más.
Default (%s)|Padrão (%s)|Por defecto (%s)
PARTS|PEÇAS|PIEZAS
%d hidden|%d escondidas|%d ocultas
L3 undo   R3 redo|L3 desfazer   R3 refazer|L3 deshacer   R3 rehacer
L3 undo|L3 desfazer|L3 deshacer
R3 redo|R3 refazer|R3 rehacer
Costume 1 is the warrior's own look: change it with the parts!|O Traje 1 é o visual do guerreiro: mude pelas peças!|El Traje 1 es el aspecto del guerrero: ¡cámbialo con las piezas!
Saving your warrior first, then your picture goes on!|Salvando antes, depois a imagem entra!|¡Guardando primero, luego entra la imagen!
Choose whose first: Cross!|Escolha de quem primeiro!|¡Primero elige de quién!
Make another warrior first: that one is the form it turns into!|Faça outro guerreiro antes: ele é a forma em que vira!|¡Crea otro guerrero: será su transformación!
Beaten up the way the fighters are on the disc.|Machucado como os lutadores do disco.|Herido como los luchadores del disco.
A few scuffs and dirt: still standing proud!|Uns arranhões e sujeira: ainda de pé!|Unos rasguños y polvo: ¡aún en pie!
Torn and battered: a real fight happened here!|Rasgado e surrado: foi luta de verdade!|Roto y golpeado: ¡hubo pelea de verdad!
Not a scratch, ever. Show-off!|Nem um arranhão, nunca. Exibido!|Ni un rasguño, nunca. ¡Presumido!
Make another warrior first: its look is what this costume wears!|Faça outro guerreiro antes: o visual dele vai neste traje!|¡Crea otro guerrero: su aspecto irá en este traje!
Saved with the warrior: that costume wears this look.|Salvo no guerreiro: o traje usa esse visual.|Guardado: ese traje usa ese aspecto.
A %s aura! Power up in a fight to see it.|Aura %s! Carregue o ki numa luta para ver.|¡Aura %s! Carga ki en combate para verla.
The aura in its %s.|A aura no seu %s.|El aura en su %s.
%s, in %s! Fire it in a fight to see it.|%s, em %s! Dispare numa luta para ver.|%s, en %s. ¡Dispáralo para verlo!
%s, in its %s.|%s, no seu %s.|%s, en su %s.
Ta-daa! Heh heh, a one of a kind warrior!|Tcharam! Heh heh, um guerreiro único!|¡Tachán! Je je, ¡un guerrero único!
LAYER ON TOP|CAMADA POR CIMA|CAPA ENCIMA
TRY IT ON|PROVAR|PROBAR
None: taken away|Nenhum: removido|Ninguno: quitado
Costume %d|Traje %d|Traje %d
Costume: the warrior's own|Traje: o do guerreiro|Traje: el del guerrero
LEARN A TECHNIQUE|APRENDER TÉCNICA|APRENDER TÉCNICA
CHOOSE A FIGHTER|ESCOLHER LUTADOR|ELEGIR LUCHADOR
for %s|para %s|para %s
 - rush| - investida| - embestida
 - won't fit| - não serve| - no sirve
the third is its Ultimate|a terceira é o Ultimate|la tercera es el Ultimate
Cross: choose from its techniques|Escolha entre as técnicas|Elige entre sus técnicas
Which one? The third one is the big finisher!|Qual? A terceira é o grande golpe!|¿Cuál? ¡La tercera es el remate!
Layered on top! Mix and match, heh heh.|Em camada! Misture à vontade, heh heh.|¡En capa! Mezcla a gusto, je je.
On top! Left and right on its row for which part: hair, head, accessories, a tail...|Por cima! Escolha qual peça: cabelo, cabeça, acessórios, cauda...|¡Encima! Elige qué pieza: pelo, cabeza, accesorios, cola...
Its own shading again.|Voltou à própria sombra.|Su propia sombra otra vez.
Shaded like that fighter! Give the hair its colour and it is a real transformation.|Sombreado como aquele lutador! Pinte o cabelo e vira transformação.|¡Sombreado como él! Pinta el pelo y es una transformación.
%s! Good choice!|%s! Boa escolha!|¡%s! ¡Buena elección!
Rushes can't lend their rules. Pick a beam, a ball or a barrage!|Investidas não emprestam regras. Escolha raio, esfera ou rajada!|Las embestidas no prestan reglas. ¡Elige rayo, esfera o ráfaga!
That one is another kind of technique: its effects won't fit!|Esse é outro tipo de técnica: os efeitos não servem!|Es otro tipo de técnica: ¡sus efectos no sirven!
How many times it hits per shot: a beam's multi-hit. The power is spread over them.|Quantas vezes acerta por disparo. O poder se divide.|Cuántas veces golpea por disparo. El poder se reparte.
How many shots it fires: a barrage's count. Best on barrage techniques!|Quantos tiros dispara. Melhor em rajadas!|Cuántos disparos hace. ¡Mejor en ráfagas!
How big the blast is: a needle-thin beam or a planet-sized bomb!|O tamanho do golpe: de uma agulha a um planeta!|El tamaño: ¡de una aguja a un planeta!
How hard it turns after its target. 0 flies straight.|Quanto persegue o alvo. 0 vai reto.|Cuánto persigue. 0 va recto.
How long it flies before it fades: its range.|Quanto voa antes de sumir: o alcance.|Cuánto vuela antes de apagarse: su alcance.
How far it sends the foe flying.|Quão longe lança o inimigo.|Qué tan lejos lanza al rival.
Its power when fully charged: x1.50 is half as strong again.|Poder com carga total: x1.50 é metade mais forte.|Poder a carga completa: x1.50 es la mitad más.
Unblockable: guarding does nothing, only a super guard saves the foe!|Indefensável: defesa comum não adianta!|Imbloqueable: ¡la guardia normal no sirve!
Copy how any technique behaves: piercing like Special Beam Cannon, exploding like a Death Ball... Cross to choose.|Copie como outra técnica age: perfurar, explodir...|Copia cómo actúa otra técnica: perforar, explotar...
How many Blast stocks it uses.|Quantos estoques de Blast gasta.|Cuántas reservas de Blast gasta.
How long the boost or the stun lasts.|Quanto dura o bônus ou o atordoamento.|Cuánto dura el bono o el aturdimiento.
Stronger punches, kicks and ki blasts while it lasts.|Socos, chutes e ki mais fortes enquanto dura.|Puños, patadas y ki más fuertes mientras dura.
Takes less damage while it lasts.|Leva menos dano enquanto dura.|Recibe menos daño mientras dura.
Faster ki charging and faster movement while it lasts.|Carrega ki e se move mais rápido enquanto dura.|Carga ki y se mueve más rápido mientras dura.
Stronger Blast 2 attacks and Ultimates while it lasts.|Blast 2 e Ultimate mais fortes enquanto dura.|Blast 2 y Ultimate más fuertes mientras dura.
Health restored, in percent. Below zero, it costs health.|Vida recuperada em %. Abaixo de zero, custa vida.|Vida recuperada en %. Bajo cero, cuesta vida.
Ki restored. 20,000 is one bar.|Ki recuperado. 20.000 é uma barra.|Ki recuperado. 20.000 es una barra.
Damage, for skills that hit. 10,000 is a full health bar.|Dano, para habilidades que acertam. 10.000 é uma barra.|Daño, para habilidades que golpean. 10.000 es una barra.
Yes: using it also puts your warrior into Max Power mode!|Sim: usar também ativa o Poder Máximo!|Sí: ¡usarlo también activa el Poder Máximo!
That technique's numbers could not be read.|Não deu para ler os números da técnica.|No se pudieron leer los datos de la técnica.
%d.%d bars|%d.%d barras|%d.%d barras
%d stock|%d estoque|%d reserva
%d stocks|%d estoques|%d reservas
%s%d.%d bars|%s%d.%d barras|%s%d.%d barras
Blast 2|Blast 2|Blast 2
Blast 1|Blast 1|Blast 1
Lasts  %d.%d s|Dura  %d.%d s|Dura  %d.%d s
Cost  %d stock|Custa  %d estoque|Cuesta  %d reserva
Cost  %d stocks|Custa  %d estoques|Cuesta  %d reservas
Total boost  +%d levels|Bônus total  +%d níveis|Bono total  +%d niveles
Total boost  %d levels|Bônus total  %d níveis|Bono total  %d niveles
Heal  %d%%    Ki  %d.%d bars|Cura  %d%%    Ki  %d.%d barras|Cura  %d%%    Ki  %d.%d barras
Very strong! Your opponents will not be happy.|Muito forte! Os rivais não vão gostar.|¡Muy fuerte! A los rivales no les gustará.
Boosts last for the duration.|Os bônus duram pela duração.|Los bonos duran lo indicado.
Total hits  %d|Acertos totais  %d|Golpes totales  %d
Per hit  %d|Por acerto  %d|Por golpe  %d
Ki per 1,000 damage  %d|Ki por 1.000 de dano  %d|Ki por 1.000 de daño  %d
Damage for its cost: %d%% of the original|Dano pelo custo: %d%% do original|Daño por coste: %d%% del original
Overpowered! Your opponents will not be happy.|Apelão! Os rivais não vão gostar.|¡Abusivo! A los rivales no les gustará.
Balanced, like the game's own techniques.|Equilibrado, como as técnicas do jogo.|Equilibrado, como las del juego.
Name your technique! Shout it loud! (Leave it empty for its own name.)|Nomeie a técnica! (Vazio mantém o nome original.)|¡Ponle nombre! (Vacío mantiene el original.)
Every great warrior needs a great name! Heh heh. (Your keyboard works too.)|Todo grande guerreiro precisa de um grande nome! Heh heh.|¡Todo gran guerrero necesita un gran nombre! Je je.
Top|Parte de cima|Parte de arriba
Belt|Cinto|Cinturón
Sleeves|Mangas|Mangas
Trousers|Calças|Pantalones
Boots|Botas|Botas
Extras|Extras|Extras
Other|Outro|Otro
Dark |Escuro |Oscuro 
Pale |Claro |Claro 
, %s & more|, %s e mais|, %s y más
Colour %d - %s|Cor %d - %s|Color %d - %s
Hair - %s|Cabelo - %s|Pelo - %s
Eyebrows - %s|Sobrancelhas - %s|Cejas - %s
Eyes - %s|Olhos - %s|Ojos - %s
Face - %s|Rosto - %s|Rostro - %s
 (own)| (próprio)| (propio)
Fine brush.|Pincel fino.|Pincel fino.
Medium brush.|Pincel médio.|Pincel medio.
Big brush.|Pincel grosso.|Pincel grueso.
Drawing wiped: the texture is as it was.|Desenho apagado: a textura voltou ao que era.|Dibujo borrado: la textura vuelve a como era.
Picture on! Fitted to the texture in 16 colours. R1 to draw on it more.|Imagem aplicada em 16 cores.|Imagen puesta en 16 colores.
That picture could not be read. Try a PNG or JPG.|Não deu para ler a imagem. Tente PNG ou JPG.|No se pudo leer la imagen. Prueba PNG o JPG.
Colour under the cursor:|Cor tocada:|Color tocado:
This part's own colours  (Cross: shared again)|Cores próprias desta peça (toque: voltar a compartilhar)|Colores propios de la pieza (toca: compartir)
< Everywhere >|< Todo lugar >|< En todas partes >
< %s only >  (Cross: its own colours)|< Só %s >  (cores próprias)|< Solo %s >  (colores propios)
One part only? It gets colours of its own, heh heh!|Só uma parte? Ela ganha cores próprias, heh heh!|¿Solo una parte? ¡Tendrá colores propios!
Four parts with colours of their own is the most!|No máximo quatro peças com cores próprias!|¡Máximo cuatro piezas con colores propios!
That part has colours of its own now!|Essa peça agora tem cores próprias!|¡Esa pieza ya tiene colores propios!
HAIR|CABELO|PELO
FACE|ROSTO|ROSTRO
CHEST|PEITO|PECHO
WAIST|CINTURA|CINTURA
UPPER ARMS|BRAÇOS|BRAZOS
FOREARMS|ANTEBRAÇOS|ANTEBRAZOS
HANDS|MÃOS|MANOS
SHINS|CANELAS|ESPINILLAS
FEET|PÉS|PIES
ACCESSORIES|ACESSÓRIOS|ACCESORIOS
OTHER|OUTROS|OTROS
Thirty-two edits is the most a warrior can hold!|No máximo 32 edições por guerreiro!|¡Máximo 32 ediciones por guerrero!
Pick one piece to copy - a whole part can't be copied!|Escolha uma peça para copiar, não a parte inteira!|¡Elige una pieza para copiar, no la parte entera!
Copied! It sits right on the piece: move it where you want it.|Copiada! Está sobre a peça: mova para onde quiser.|¡Copiada! Está sobre la pieza: muévela donde quieras.
Piece %d|Peça %d|Pieza %d
Strand %d.%d|Mecha %d.%d|Mechón %d.%d
Copy of strand %d.%d|Cópia da mecha %d.%d|Copia del mechón %d.%d
Copy of piece %d|Cópia da peça %d|Copia de la pieza %d
pieces folded|peças recolhidas|piezas plegadas
%d pts|%d pts|%d pts
Patch holes (EXPERIMENTAL): ON|Tapar buracos (EXPERIMENTAL): LIGADO|Tapar huecos (EXPERIMENTAL): SÍ
Patch holes (EXPERIMENTAL): OFF|Tapar buracos (EXPERIMENTAL): DESLIGADO|Tapar huecos (EXPERIMENTAL): NO
%+d deg|%+d graus|%+d grados
Putting your warrior together... hold still!|Montando seu guerreiro...|Montando tu guerrero...
Stretch it, move it, turn it! Your warrior changes as you go.|Estique, mova, gire! O guerreiro muda na hora.|¡Estira, mueve, gira! Cambia al momento.
Every piece of your warrior! The one under the cursor blinks.|Cada peça do guerreiro! A escolhida pisca.|¡Cada pieza! La elegida parpadea.
Only this piece of the layer is kept! Mark more with Circle to keep them too.|Só esta peça da camada fica! Marque outras para manter.|¡Solo queda esta pieza de la capa! Marca más para conservarlas.
Holes left open.|Buracos abertos.|Huecos abiertos.
Holes patched (experimental: it can pull parts out of shape; the face is left alone). Check the model!|Buracos tapados (experimental: pode deformar; o rosto fica). Confira o modelo!|Huecos tapados (experimental: puede deformar; la cara no). ¡Revisa el modelo!
ON THE GRIDS|NAS GRADES|EN LAS REJILLAS
NO PORTRAIT YET|SEM RETRATO AINDA|AÚN SIN RETRATO
Frame that face! A little to the left... perfect!|Enquadre o rosto! Um pouco à esquerda... perfeito!|¡Encuadra la cara! Un poco a la izquierda... ¡perfecto!
A face fit for the Other World Tournament!|Um rosto digno do Torneio do Outro Mundo!|¡Una cara digna del Torneo del Otro Mundo!
T-pose (a clear look at the face)|Pose T (rosto bem visível)|Pose T (cara bien visible)
Full power|Poder total|Máximo poder
Blast 2 (1): charge|Blast 2 (1): carga|Blast 2 (1): carga
Blast 2 (1): fire|Blast 2 (1): disparo|Blast 2 (1): disparo
Blast 2 (2): charge|Blast 2 (2): carga|Blast 2 (2): carga
Blast 2 (2): fire|Blast 2 (2): disparo|Blast 2 (2): disparo
Ultimate: opening pose|Ultimate: pose inicial|Ultimate: pose inicial
Ultimate: charge|Ultimate: carga|Ultimate: carga
Ultimate: fire|Ultimate: disparo|Ultimate: disparo
Transforming|Transformando|Transformándose
Sensing ki|Sentindo o ki|Sintiendo el ki
What a pose! That's its portrait in the character select now.|Que pose! Já é o retrato na seleção.|¡Qué pose! Ya es el retrato en la selección.
Snap! That little face is on the grids now.|Clique! O rostinho já está nas grades.|¡Clic! La carita ya está en las rejillas.
Say cheese! That face is on its health bar now.|Diga xis! O rosto já está na barra de vida.|¡Di whisky! La cara ya está en la barra de vida.
Photo gone: the little face is cropped from the render again.|Foto removida: o rostinho volta a vir do retrato.|Foto quitada: la carita vuelve a salir del retrato.
Photo gone: the health bar gets the HUD face fighter's again.|Foto removida: a barra usa o rosto original.|Foto quitada: la barra usa la cara original.
HUD FACE PHOTO|FOTO DA BARRA DE VIDA|FOTO DE LA BARRA
RENDER PHOTO|FOTO DO RETRATO|FOTO DEL RETRATO
SMALL PORTRAIT PHOTO|FOTO DO RETRATO PEQUENO|FOTO DEL RETRATO PEQUEÑO
Eyes in the middle, looking to the right: the other side gets it mirrored.|Olhos no meio, olhando para a direita: o outro lado é espelhado.|Ojos al centro, mirando a la derecha: el otro lado se refleja.
Fill the frame like the game's renders: hair at the top, knees at the bottom. The background is cut away.|Preencha o quadro: cabelo no topo, joelhos embaixo. O fundo é recortado.|Llena el cuadro: pelo arriba, rodillas abajo. El fondo se recorta.
The face, nice and big: it goes on the fighter grids.|O rosto bem grande: vai nas grades de lutadores.|La cara bien grande: va en las rejillas.
name plates|placas de nome|placas de nombre
fighter reel|carrossel de lutadores|carrusel de luchadores
Strike a pose! Frame the whole warrior, then snap.|Faça pose! Enquadre o guerreiro e fotografe.|¡Posa! Encuadra al guerrero y dispara.
Smile! Frame its face, then snap. Heh heh!|Sorria! Enquadre o rosto e fotografe. Heh heh!|¡Sonríe! Encuadra la cara y dispara.
Hmm, it is not in the mod grid. Is it switched on in the Mod Manager?|Não está na grade de mods. Está ligado no Gerenciador?|No está en la rejilla de mods. ¿Está activado?
Turn the camera (right stick) to pose from the side.|Gire a câmera para posar de lado.|Gira la cámara para posar de lado.
Free (press X, Y or Z to keep to one axis)|Livre (X, Y ou Z para travar num eixo)|Libre (X, Y o Z para fijar un eje)
Kept to X|Preso em X|Fijo en X
Kept to Y|Preso em Y|Fijo en Y
Kept to Z|Preso em Z|Fijo en Z
The other side posed the same, mirrored.|O outro lado posado igual, espelhado.|El otro lado igual, reflejado.
Face: blink|Rosto: piscar|Cara: parpadeo
Face: hurt|Rosto: ferido|Cara: herido
Face: expression %d|Rosto: expressão %d|Cara: expresión %d
Area picked! Give it any colour, saturation and brightness.|Área escolhida! Dê a cor que quiser.|¡Área elegida! Dale el color que quieras.
Choose a picture: then move it, size it and put it on.|Escolha uma imagem: depois mova, ajuste e aplique.|Elige una imagen: muévela, ajústala y aplícala.
Drawing wiped: this texture is as it was.|Desenho apagado: a textura voltou ao que era.|Dibujo borrado: la textura vuelve a como era.
Move it, size it, turn it, then put it on! It takes the texture's own colours.|Mova, ajuste, gire e aplique! Ela usa as cores da textura.|¡Mueve, ajusta, gira y aplica! Usa los colores de la textura.
Picture taken off.|Imagem removida.|Imagen quitada.
Picture on! R3 picks it up again any time: move it, size it, turn it or take it off.|Imagem aplicada! Toque em Imagem para ajustar ou tirar.|¡Imagen puesta! Toca Imagen para ajustarla o quitarla.
Mouse drawing: left button uses the tool, right takes a colour, the wheel picks colours.|Desenho com mouse: esquerdo usa, direito pega cor, roda troca.|Ratón: izquierdo usa, derecho toma color, rueda cambia.
Pad drawing: the D-pad or stick moves, Cross paints.|Desenho no controle: direcional move, Cross pinta.|Mando: la cruceta mueve, Cross pinta.
Mouse drawing! Select goes back to the pad.|Desenho com mouse! Select volta ao controle.|¡Ratón! Select vuelve al mando.
Texture, then the tools below|Textura, depois as ferramentas|Textura, luego las herramientas
Draw with the mouse|Desenhar com mouse|Dibujar con ratón
New colour (any)|Cor nova (qualquer)|Color nuevo (cualquiera)
Move (stick: gently)|Mover (analógico: devagar)|Mover (stick: suave)
Back to the list|Voltar à lista|Volver a la lista
Magic wand: area's colour|Varinha: cor da área|Varita: color del área
Left button: paint|Botão esquerdo: pintar|Botón izquierdo: pintar
Right button: take colour|Botão direito: pegar cor|Botón derecho: tomar color
Middle button: fill|Botão do meio: preencher|Botón central: rellenar
Wheel: colour|Roda: cor|Rueda: color
Draw with the pad|Desenhar com controle|Dibujar con mando
Put it on|Aplicar|Aplicar
Take it off (a picture on)|Tirar (imagem aplicada)|Quitar (imagen puesta)
Another of its pictures|Outra imagem dela|Otra de sus imágenes
A new picture|Imagem nova|Imagen nueva
Mouse: drag, wheel to size|Mouse: arraste, roda muda o tamanho|Ratón: arrastra, rueda cambia el tamaño
Right button: cancel|Botão direito: cancelar|Botón derecho: cancelar
No textures to draw on.|Nenhuma textura para pintar.|No hay texturas.
Size %d|Tamanho %d|Tamaño %d
Picture|Imagem|Imagen
Wipe|Limpar|Limpiar
its own|a própria|la propia
Like the whole warrior|Como o guerreiro todo|Como todo el guerrero
Cel (two tones)|Cel (dois tons)|Cel (dos tonos)
Soft|Suave|Suave
Deep|Profunda|Profunda
Glow|Brilho|Brillo
Flat (no shadow)|Plana (sem sombra)|Plana (sin sombra)
The game's own|A do jogo|La del juego
Tiny|Mínima|Mínima
Small|Pequena|Pequeña
Medium|Média|Media
Large|Grande|Grande
Huge|Enorme|Enorme
Auto|Automática|Automática
Cool|Fria|Fría
Warm|Quente|Cálida
Style|Estilo|Estilo
Shadow size|Tamanho da sombra|Tamaño de sombra
Shadow tint|Tom da sombra|Tono de sombra
Outline|Contorno|Contorno
Whole warrior|Guerreiro todo|Todo el guerrero
 (layer)| (camada)| (capa)
The game's own shading|A sombra do jogo|La sombra del juego
Sixteen textures with shading of their own is the most!|No máximo 16 texturas com sombra própria!|¡Máximo 16 texturas con sombra propia!
BY PART|POR PARTE|POR PARTE
BY TEXTURE|POR TEXTURA|POR TEXTURA
ITS SHADERS|SUAS SOMBRAS|SUS SOMBRAS
(seen once the warrior is made)|(aparece depois de montado)|(se ve al montarlo)
Shadow colour|Cor da sombra|Color de sombra
the game's|a do jogo|la del juego
Pick a texture: its shaders show on the right. Left and right give its shadow a colour of your own!|Escolha uma textura e dê uma cor à sombra dela!|¡Elige una textura y da color a su sombra!
All parts|Todas as peças|Todas las piezas
your colour|sua cor|tu color
as the part it copies|a da parte copiada|la de la parte copiada
each part the game's|cada parte a do jogo|cada parte la del juego
Same shading as|Sombra igual a|Sombra igual a
no: its own|não: a própria|no: la propia
each its own|cada uma a sua|cada una la suya
group %d|grupo %d|grupo %d
with the body|junto do corpo|con el cuerpo
Pick a part, then give its shadow a colour of your own, or the same shading as another part!|Escolha uma parte e dê cor à sombra, ou iguale a outra!|¡Elige una parte y da color a su sombra, o iguálala!
Cross: the colour the shadow shows. It falls where the game puts it.|A cor que a sombra mostra.|El color que muestra la sombra.
Left and right: shaded just like another part. All the way round: its own again.|Sombreada igual a outra parte.|Sombreada igual que otra parte.
The glow of a transformed warrior: none, soft, bright, blazing.|O brilho de um transformado: nenhum, suave, forte, intenso.|El brillo de un transformado: ninguno, suave, fuerte, intenso.
Parts in one group are one surface; a line is drawn where groups meet.|Partes de um grupo viram uma superfície; a linha fica entre grupos.|Las partes de un grupo se unen; la línea va entre grupos.
Its own colours|Próprias cores|Colores propios
Glow while doing it (brighter: stronger)|Brilho durante o golpe (mais claro: mais forte)|Brillo al hacerlo (más claro: más fuerte)
A colour for this texture|Uma cor para esta textura|Un color para esta textura
Whole warrior: shadow colour|Guerreiro todo: cor da sombra|Todo el guerrero: color de sombra
Aura, lightning and charge|Aura, raios e carga|Aura, rayos y carga
Any colour you like for the aura! The lightning and the charge glow take it too.|Qualquer cor para a aura! Raios e brilho de carga também.|¡Cualquier color para el aura! También rayos y carga.
Any colour you like! Fire it in a fight to see it.|Qualquer cor! Dispare numa luta para ver.|¡Cualquier color! Dispáralo en combate.
WARRIOR|GUERREIRO|GUERRERO
Name|Nome|Nombre
Title|Título|Título
LOOK|VISUAL|ASPECTO
Body|Corpo|Cuerpo
Custom pose|Pose própria|Pose propia
Voice|Voz|Voz
PORTRAITS|RETRATOS|RETRATOS
Portrait|Retrato|Retrato
Small portrait|Retrato pequeno|Retrato pequeño
Health bar face|Rosto da barra de vida|Cara de la barra de vida
Preview|Prévia|Vista previa
COSTUMES|TRAJES|TRAJES
Costumes|Trajes|Trajes
Edit costume|Editar traje|Editar traje
BATTLE DAMAGE|DANO DE BATALHA|DAÑO DE BATALLA
Damaged look|Visual danificado|Aspecto dañado
FIGHTING|LUTA|COMBATE
Melee moves|Golpes corpo a corpo|Golpes cuerpo a cuerpo
Finishers|Finalizadores|Remates
Stats|Atributos|Atributos
Ki blasts|Rajadas de ki|Ráfagas de ki
ANIMATIONS|ANIMAÇÕES|ANIMACIONES
Moving & flying|Andar e voar|Moverse y volar
Charging|Carregando ki|Cargando ki
Firing ki blasts|Disparando ki|Disparando ki
Grabs & throws|Agarrões e arremessos|Agarres y lanzamientos
Guard & dodge|Defesa e esquiva|Guardia y esquiva
Intro|Entrada|Entrada
Victory|Vitória|Victoria
Taunt|Provocação|Burla
  Look|  Visual|  Aspecto
  Fine-tune|  Ajuste fino|  Ajuste fino
  Body glow|  Brilho do corpo|  Brillo del cuerpo
  Effects|  Efeitos|  Efectos
  Camera|  Câmera|  Cámara
  Opening camera|  Câmera inicial|  Cámara inicial
  Finish camera|  Câmera final|  Cámara final
TRANSFORMATIONS|TRANSFORMAÇÕES|TRANSFORMACIONES
Form 1|Forma 1|Forma 1
Form 2|Forma 2|Forma 2
Form 3|Forma 3|Forma 3
Form 4|Forma 4|Forma 4
  Input|  Comando|  Comando
  Ki cost|  Custo de ki|  Coste de ki
  Type|  Tipo|  Tipo
  Aura colour|  Cor da aura|  Color del aura
TOOLS|FERRAMENTAS|HERRAMIENTAS
Sculpt|Esculpir|Esculpir
Shading|Sombreamento|Sombreado
Paint textures|Pintar texturas|Pintar texturas
Borrowed shading|Sombra emprestada|Sombra prestada
Super Saiyan glow|Brilho Super Saiyajin|Brillo Super Saiyan
Randomize|Sortear|Al azar
Whole head|Cabeça inteira|Cabeza completa
Whole outfit|Roupa inteira|Traje completo
Reset all parts|Restaurar peças|Restaurar piezas
EXTRA LAYERS|CAMADAS EXTRAS|CAPAS EXTRA
Extra layer 1|Camada extra 1|Capa extra 1
Extra layer 2|Camada extra 2|Capa extra 2
Extra layer 3|Camada extra 3|Capa extra 3
Extra layer 4|Camada extra 4|Capa extra 4
BUILD|FÍSICO|COMPLEXIÓN
Torso build|Tronco|Torso
Arm build|Braços|Brazos
Leg build|Pernas|Piernas
HEAD|CABEÇA|CABEZA
Hair|Cabelo|Pelo
Head|Cabeça|Cabeza
Face|Rosto|Rostro
Head size|Tamanho da cabeça|Tamaño de cabeza
Hair size|Tamanho do cabelo|Tamaño del pelo
UPPER BODY|PARTE DE CIMA|PARTE SUPERIOR
Chest|Peito|Pecho
Waist|Cintura|Cintura
Upper arms|Braços|Brazos
Forearms|Antebraços|Antebrazos
Hands|Mãos|Manos
Hand size|Tamanho das mãos|Tamaño de manos
LOWER BODY|PARTE DE BAIXO|PARTE INFERIOR
Hips & thighs|Quadril e coxas|Cadera y muslos
Shins|Canelas|Espinillas
Feet|Pés|Pies
Foot size|Tamanho dos pés|Tamaño de pies
Accessories|Acessórios|Accesorios
Red|Vermelho|Rojo
Orange|Laranja|Naranja
Yellow|Amarelo|Amarillo
Green|Verde|Verde
Cyan|Ciano|Cian
Blue|Azul|Azul
Purple|Roxo|Morado
Pink|Rosa|Rosa
White|Branco|Blanco
Black|Preto|Negro
Gold|Dourado|Dorado
Lime|Verde-limão|Lima
Teal|Verde-azulado|Verde azulado
Sky blue|Azul-céu|Celeste
Light grey|Cinza claro|Gris claro
Dark grey|Cinza escuro|Gris oscuro
Skin|Pele|Piel
None|Nenhum|Ninguno
Off|Desligado|Desactivado
Own|Próprio|Propio
Mixed|Misturado|Mezclado
Your colour|Sua cor|Tu color
Your picture|Sua imagem|Tu imagen
Your photo|Sua foto|Tu foto
Cropped|Recortado|Recortado
Default pose|Pose padrão|Pose por defecto
Default damage|Dano padrão|Daño por defecto
Default costume|Traje padrão|Traje por defecto
NO COLOURS FOUND|NENHUMA COR|SIN COLORES
Hue|Tom|Tono
Strength|Força|Fuerza
Light|Luz|Luz
Lately|Recentes|Recientes
Only on|Só em|Solo en
Was|Era|Antes
Saturation|Saturação|Saturación
Brightness|Brilho|Brillo
(none yet)|(nenhuma ainda)|(ninguno aún)
Everywhere it is|Em todo lugar|En todas partes
BODY|CORPO|CUERPO
DETAILS|DETALHES|DETALLES
NOT SAVED|NÃO SALVO|SIN GUARDAR
Brush|Pincel|Pincel
Undo|Desfazer|Deshacer
Redo|Refazer|Rehacer
Shown|Visível|Visible
Whole part|Peça inteira|Pieza completa
Stretch X|Esticar X|Estirar X
Stretch Y|Esticar Y|Estirar Y
Stretch Z|Esticar Z|Estirar Z
Move X|Mover X|Mover X
Move Y|Mover Y|Mover Y
Move Z|Mover Z|Mover Z
Turn X|Girar X|Girar X
Turn Y|Girar Y|Girar Y
Turn Z|Girar Z|Girar Z
Back to as it was|Voltar como era|Volver como era
ONLY THIS|SÓ ESTA|SOLO ESTA
taken off|removida|quitada
Neck|Pescoço|Cuello
Upper body|Tronco|Torso
Left upper arm|Braço esquerdo|Brazo izquierdo
Left forearm|Antebraço esquerdo|Antebrazo izquierdo
Left hand|Mão esquerda|Mano izquierda
Right upper arm|Braço direito|Brazo derecho
Right forearm|Antebraço direito|Antebrazo derecho
Right hand|Mão direita|Mano derecha
Left thigh|Coxa esquerda|Muslo izquierdo
Left shin|Canela esquerda|Espinilla izquierda
Left foot|Pé esquerdo|Pie izquierdo
Right thigh|Coxa direita|Muslo derecho
Right shin|Canela direita|Espinilla derecha
Right foot|Pé direito|Pie derecho
Left elbow|Cotovelo esquerdo|Codo izquierdo
Right elbow|Cotovelo direito|Codo derecho
Left knee|Joelho esquerdo|Rodilla izquierda
Right knee|Joelho direito|Rodilla derecha
Advanced (numbers)|Avançado (números)|Avanzado (números)
Drag the dots on your warrior.|Arraste os pontos no guerreiro.|Arrastra los puntos del guerrero.
Damage|Dano|Daño
Ki cost|Custo de ki|Coste de ki
Hits|Acertos|Golpes
Shots|Disparos|Disparos
Speed|Velocidade|Velocidad
Homing|Perseguição|Persecución
Range|Alcance|Alcance
Knock-back|Empurrão|Empuje
Charge boost|Bônus de carga|Bono de carga
Recoil|Recuo|Retroceso
Unblockable|Indefensável|Imbloqueable
Copy rules from|Copiar regras de|Copiar reglas de
Cost|Custo|Coste
Duration|Duração|Duración
Attack boost|Bônus de ataque|Bono de ataque
Defense boost|Bônus de defesa|Bono de defensa
Ki & speed boost|Ki e velocidade|Ki y velocidad
Blast boost|Bônus de Blast|Bono de Blast
Heal|Cura|Curación
Ki restored|Ki recuperado|Ki recuperado
Max Power|Poder máximo|Poder máximo
STAT|ATRIBUTO|ATRIBUTO
VALUE|VALOR|VALOR
READOUT|RESUMO|RESUMEN
Straight|Reto|Recto
Yes|Sim|Sí
How much damage it does. 10,000 is a full health bar.|Quanto dano causa. 10.000 é uma barra de vida.|Cuánto daño hace. 10.000 es una barra de vida.
The ki it takes. 20,000 is one bar of ki.|O ki que gasta. 20.000 é uma barra.|El ki que gasta. 20.000 es una barra.
How fast it flies.|Quão rápido voa.|Qué tan rápido vuela.
Damage to your own warrior for firing it. Power has a price!|Dano no seu guerreiro ao disparar. Poder tem preço!|Daño a tu guerrero al disparar. ¡El poder cuesta!
Name: %s|Nome: %s|Nombre: %s
Title: %s|Título: %s|Título: %s
NAME|NOME|NOMBRE
TITLE|TÍTULO|TÍTULO
SPACE|ESPAÇO|ESPACIO
DELETE|APAGAR|BORRAR
What do we call this fine fellow?|Como vamos chamar este guerreiro?|¿Cómo se llamará este guerrero?
Save the warrior first, then see it for real!|Salve o guerreiro primeiro!|¡Guarda el guerrero primero!
Save the warrior first, then crop its face.|Salve antes de recortar o rosto.|Guarda antes de recortar la cara.
Building your warrior... hold still!|Montando seu guerreiro...|Montando tu guerrero...
Saving your warrior... hold still!|Salvando seu guerreiro...|Guardando tu guerrero...
Painting your warrior... don't smudge it!|Pintando seu guerreiro...|Pintando tu guerrero...
Locked! Surprises leave this one alone.|Travado! O sorteio não mexe nesta.|¡Bloqueado! El azar no la cambia.
Unlocked. Anything goes!|Destravado!|¡Desbloqueado!
Back to the body it came with!|Corpo original de volta!|¡Cuerpo original!
Pick the body your warrior fights in!|Escolha o corpo do guerreiro!|¡Elige el cuerpo del guerrero!
Whose technique should your warrior learn?|De quem aprender a técnica?|¿De quién aprender la técnica?
Pick a texture of your warrior, then the colour to change on it!|Escolha a textura e depois a cor!|¡Elige la textura y luego el color!
Slide it around! Your warrior changes as you go.|Deslize! O guerreiro muda na hora.|¡Desliza! El guerrero cambia al momento.
Point at a colour and press Cross! Circle paints the whole texture.|Toque numa cor da imagem.|Toca un color de la imagen.
Forty-eight colours is the most a warrior can hold!|Um guerreiro guarda no máximo 48 cores!|¡Máximo 48 colores por guerrero!
Take this warrior down from the Workshop?|Tirar este guerreiro da Oficina?|¿Quitar este guerrero del Taller?
Send this warrior away?|Remover este guerreiro?|¿Quitar este guerrero?
You have changes that are not saved!|Há mudanças não salvas!|¡Hay cambios sin guardar!
Keep them before you go?|Guardar antes de sair?|¿Guardar antes de salir?
Keep it|Manter|Mantener
Don't save|Não salvar|No guardar
Keep editing|Continuar editando|Seguir editando
Yes, start it|Sim, começar|Sí, empezar
Not now|Agora não|Ahora no
Turn them off|Desligar|Desactivar
Keep them|Manter|Mantener
Take it down|Tirar|Quitar
Keep it up|Deixar|Dejar
MOST RECENT|MAIS RECENTES|MÁS RECIENTES
MOST DOWNLOADED|MAIS BAIXADOS|MÁS DESCARGADOS
FEATURED|DESTAQUES|DESTACADOS
MY UPLOADS|MEUS ENVIOS|MIS ENVÍOS
OVERVIEW|RESUMO|RESUMEN
MOVES|GOLPES|GOLPES
FORMS|FORMAS|FORMAS
MY WARRIORS|MEUS GUERREIROS|MIS GUERREROS
DOWNLOAD|BAIXAR|DESCARGAR
DOWNLOADING|BAIXANDO|DESCARGANDO
LIKE|CURTIR|ME GUSTA
LIKED|CURTIDO|TE GUSTA
DETAILS:|DETALHES:|DETALLES:
Ready to fight|Pronto para lutar|Listo para luchar
Nothing here yet!|Nada aqui ainda!|¡Nada aquí todavía!
WORKSHOP|OFICINA|TALLER
Wait|Aguarde|Espera
Details|Detalhes|Detalles
Download|Baixar|Descargar
Take down|Retirar|Retirar
Sort|Ordenar|Ordenar
Page|Página|Página
Back|Voltar|Volver
Unlike|Descurtir|Quitar me gusta
Like|Curtir|Me gusta
Tab|Aba|Pestaña
UPLOAD|ENVIAR|SUBIR
Write|Escrever|Escribir
Upload|Enviar|Subir
Tag|Etiqueta|Etiqueta
Cancel|Cancelar|Cancelar
Online|Online|En línea
Create|Criar|Crear
Edit|Editar|Editar
Copy|Copiar|Copiar
Remove|Remover|Quitar
Share|Compartilhar|Compartir
Paste a code|Colar código|Pegar código
Bug report|Relatar erro|Informar error
Create a New Warrior|Criar novo guerreiro|Crear un guerrero
A NEW WARRIOR|NOVO GUERREIRO|NUEVO GUERRERO
This warrior could not be made. Check its parts!|Não deu para montar este guerreiro. Confira as peças!|No se pudo montar. ¡Revisa sus piezas!
Welcome! Shall we make a warrior?|Bem-vindo! Vamos criar um guerreiro?|¡Bienvenido! ¿Creamos un guerrero?
A fine warrior you have there.|Um belo guerreiro o seu.|Buen guerrero el que tienes.
A new warrior? Let us begin.|Um novo guerreiro? Vamos começar.|¿Un guerrero nuevo? Empecemos.
Turn / look|Girar / olhar|Girar / mirar
Zoom|Zoom|Zoom
T-pose|Pose T|Pose T
Stance|Postura|Postura
MAKING...|MONTANDO...|MONTANDO...
LOADING...|CARREGANDO...|CARGANDO...
Ah, your unsaved changes from last time! Picked up right where you left off.|Suas mudanças não salvas voltaram. Continue de onde parou.|Volvieron tus cambios sin guardar. Sigue donde lo dejaste.
A warrior from a friend! Built from your own game, ready to fight.|Um guerreiro de um amigo! Montado com o seu jogo, pronto para lutar.|¡Un guerrero de un amigo! Montado con tu juego, listo para luchar.
No warrior code on the clipboard. Copy the whole line your friend sent (it starts with ZW1:).|Nenhum código copiado. Copie a linha inteira do seu amigo (começa com ZW1:).|No hay código copiado. Copia toda la línea de tu amigo (empieza con ZW1:).
The clipboard didn't answer. Try again!|A área de transferência não respondeu. Tente de novo!|El portapapeles no respondió. ¡Reintenta!
Code copied! Send it to a friend: they paste it here with R1.|Código copiado! Envie a um amigo: ele cola com R1.|¡Código copiado! Envíalo a un amigo: lo pega con R1.
That warrior's code couldn't be made.|Não deu para gerar o código.|No se pudo crear el código.
Two of them! Now that's a training partner.|Agora são dois! Um parceiro de treino.|¡Ya son dos! Un compañero de entrenamiento.
A report, all zipped up in the reports folder! Send it with what happened.|Relatório salvo na pasta reports! Envie contando o que houve.|¡Informe guardado en reports! Envíalo contando qué pasó.
Hmm, the report could not be made.|Não deu para criar o relatório.|No se pudo crear el informe.
EDIT WARRIOR|EDITAR GUERREIRO|EDITAR GUERRERO
Open section|Abrir seção|Abrir sección
Fold section|Fechar seção|Cerrar sección
Size|Tamanho|Tamaño
Character select|Seleção|Selección
Crop|Recortar|Recortar
Paint|Pintar|Pintar
Pick technique|Escolher técnica|Elegir técnica
Change|Mudar|Cambiar
Default|Padrão|Predeterminado
Aura colour|Cor da aura|Color del aura
Colour|Cor|Color
Lock|Travar|Bloquear
Layer|Camada|Capa
Rename|Renomear|Renombrar
Photo of the model|Foto do modelo|Foto del modelo
Test fight|Luta de teste|Lucha de prueba
Save|Salvar|Guardar
Ooh, a portrait of your own! Very artistic.|Um retrato próprio! Que artístico.|¡Un retrato propio! Muy artístico.
That picture could not be used. Try a PNG or JPG.|Não deu para usar a imagem. Tente PNG ou JPG.|No se pudo usar la imagen. Prueba PNG o JPG.
Hmm, something went wrong. Look at the library for why.|Algo deu errado. Veja o motivo na lista.|Algo salió mal. Mira el motivo en la lista.
Saved! Your warrior is ready to fight.|Salvo! Seu guerreiro está pronto.|¡Guardado! Tu guerrero está listo.
It could not be written. Is the mods folder read-only?|Não deu para gravar. A pasta mods é só leitura?|No se pudo escribir. ¿La carpeta mods es de solo lectura?
This costume starts again from Costume 1's look.|Este traje recomeça pelo visual do Traje 1.|Este traje vuelve al aspecto del Traje 1.
Back to the way it was!|Voltou a ser como era!|¡Como antes!
Save to make it so. Give that warrior a way back too!|Salve para valer. Dê a esse guerreiro um jeito de voltar!|Guarda para aplicarlo. ¡Dale también una vuelta atrás!
Fighter|Lutador|Luchador
Wear it|Vestir|Ponerlo
Technique|Técnica|Técnica
Costume|Traje|Traje
Its techniques|Técnicas dele|Sus técnicas
Take it|Pegar|Tomar
Other fighter|Outro lutador|Otro luchador
Gone! Clean as a whistle.|Removido! Tudo limpo.|¡Quitado! Todo limpio.
Looking sharp!|Ficou ótimo!|¡Se ve genial!
Layer gone.|Camada removida.|Capa quitada.
A new body! Its colours start fresh.|Corpo novo! As cores começam do zero.|¡Cuerpo nuevo! Los colores empiezan de cero.
FINE-TUNE|AJUSTE FINO|AJUSTE FINO
Choose|Escolher|Elegir
Adjust|Ajustar|Ajustar
Reset|Redefinir|Restablecer
Reset all|Redefinir tudo|Restablecer todo
All stats reset to default.|Atributos de volta ao padrão.|Atributos restablecidos.
WORKSHOP BIO|BIO DA OFICINA|BIO DEL TALLER
TECHNIQUE NAME|NOME DA TÉCNICA|NOMBRE DE LA TÉCNICA
WARRIOR TITLE|TÍTULO DO GUERREIRO|TÍTULO DEL GUERRERO
WARRIOR NAME|NOME DO GUERREIRO|NOMBRE DEL GUERRERO
Type|Tipo|Tipo
Delete|Apagar|Borrar
Name / title|Nome / título|Nombre / título
OK|OK|OK
Move|Mover|Mover
Fill|Preencher|Rellenar
Take colour|Pegar cor|Tomar color
Next colour|Próxima cor|Color siguiente
Brush size|Pincel|Pincel
Wipe drawing|Apagar desenho|Borrar dibujo
Done|Pronto|Listo
This colour|Esta cor|Este color
Whole texture|Textura inteira|Textura entera
Original|Original|Original
Colours|Cores|Colores
COLOURS|CORES|COLORES
Open texture|Abrir textura|Abrir textura
All original|Tudo original|Todo original
Contrast|Contraste|Contraste
Keep|Manter|Mantener
Back to the colours it came with!|De volta às cores originais!|¡De vuelta a sus colores!
Fold pieces|Fechar peças|Cerrar piezas
Show pieces|Ver peças|Ver piezas
Show / hide|Mostrar / ocultar|Mostrar / ocultar
Duplicate|Duplicar|Duplicar
Remove copy|Remover cópia|Quitar copia
Undo all of it|Desfazer tudo|Deshacer todo
Taken off.|Retirado.|Retirado.
Back on.|De volta.|De vuelta.
SMALL PORTRAIT|RETRATO PEQUENO|RETRATO PEQUEÑO
Automatic|Automático|Automático
Take photo|Tirar foto|Hacer foto
Use a picture file|Usar imagem|Usar imagen
Remove photo|Remover foto|Quitar foto
Save the warrior first, then take its photo.|Salve o guerreiro antes de fotografar.|Guarda el guerrero antes de la foto.
Hmm, the photo did not come out. Try again?|A foto não saiu. Tentar de novo?|La foto no salió. ¿Reintentar?
Undone! Back a step.|Desfeito! Um passo atrás.|¡Deshecho! Un paso atrás.
Nothing more to undo!|Nada mais a desfazer!|¡Nada más que deshacer!
Redone!|Refeito!|¡Rehecho!
STANCE MAKER|CRIAR POSTURA|CREAR POSTURA
Pull the dot|Puxe o ponto|Tira del punto
Dot|Ponto|Punto
Axis|Eixo|Eje
Reset its bone|Redefinir osso|Restablecer hueso
Advanced|Avançado|Avanzado
Bone|Osso|Hueso
Turn|Girar|Girar
Reset bone|Redefinir osso|Restablecer hueso
Mirror to the other side|Espelhar no outro lado|Reflejar al otro lado
Simple|Simples|Simple
Nothing to redo.|Nada a refazer.|Nada que rehacer.
Nothing to undo.|Nada a desfazer.|Nada que deshacer.
TEXTURES|TEXTURAS|TEXTURAS
Use the pad|Usar o controle|Usar el mando
Use the mouse|Usar o mouse|Usar el ratón
Texture|Textura|Textura
Draw on it|Desenhar|Dibujar
Pictures: add / edit|Imagens: pôr / editar|Imágenes: poner / editar
List|Lista|Lista
SHADING|SOMBREAMENTO|SOMBREADO
Texture / shadow colour|Textura / cor da sombra|Textura / color de sombra
Any colour|Qualquer cor|Cualquier color
Gradient|Degradê|Degradado
By part|Por parte|Por parte
The game's|A do jogo|El del juego
Part|Parte|Parte
Change it|Mudar|Cambiarlo
As it was|Como era|Como estaba
Pick the colour|Escolher a cor|Elegir el color
Shaded as the game has it again.|Sombreado como no jogo.|Sombreado como en el juego.
BODY GLOW|BRILHO DO CORPO|BRILLO DEL CUERPO
NEW COLOUR|COR NOVA|COLOR NUEVO
BLAST 1 COLOUR|COR DO BLAST 1|COLOR DEL BLAST 1
SHADE COLOUR|COR DA SOMBRA|COLOR DE SOMBRA
AURA COLOUR|COR DA AURA|COLOR DEL AURA
TECHNIQUE COLOUR|COR DA TÉCNICA|COLOR DE LA TÉCNICA
Its own|A própria|El propio
Its own colours again!|Voltou às próprias cores!|¡Sus colores de nuevo!
'''


def lit(s):
    """UTF-8 as C octal escapes; the C escapes already written stay."""
    out = []
    for ch in s:
        if ord(ch) < 128:
            out.append(ch)
        else:
            out.append("".join("\\%03o" % b for b in ch.encode()))
    text = "".join(out)
    return re.sub(r'(?<!\\)"', r'\\"', text)


def main():
    rows = {}
    for block, source_is_pt in ((FROM_PT, True), (FROM_EN, False)):
        for line in block.split("\n"):
            if not line.strip():
                continue
            key, a, b = line.split("|")
            pt, en, es = (key, a, b) if source_is_pt else (a, key, b)
            rows.setdefault(lit(key), (lit(en), lit(pt), lit(es)))
    out = pathlib.Path(__file__).resolve().parents[2] / "src/menu/port_zw_lang.inc"
    body = "".join('    {"%s", {"%s", "%s", "%s"}},\n' % ((k,) + v) for k, v in rows.items())
    out.write_text("/* Z Warriors' texts: {as written in the code, {English, Portuguese, Spanish}}. Made by port/tools/zw_lang.py. */\n" + body)
    print(len(rows), "texts")


if __name__ == "__main__":
    main()
