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
