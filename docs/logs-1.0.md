# Logs dos jogadores para a 1.0

Fonte: 208 logs enviados por jogadores (485 execuções, 84 aparelhos), lidos com `port/tools/logscan.py`. Os arquivos ficam
fora do repositório; aqui só há modelos de aparelho e trechos curtos. A contagem é por modelo de aparelho, e a mesma linha pode
vir de mais de uma versão (dev.45 a 0.9.0).

```sh
python3 port/tools/logscan.py "<pasta dos logs>" --csv runs.csv > summary.txt
```

O CSV tem uma linha por execução: arquivo, versão, jogo, aparelho, SoC, GPU, driver, Android, renderizador, resolução, perfil,
Lite, velocidade e fps médios, assinatura do crash, erros de import, de rede e avisos repetidos.

## Os 15 maiores, por aparelhos afetados

| # | Problema | Aparelhos | Versões | Estado na 1.0 |
|---|---|---|---|---|
| 1 | Menu font e letreiro: "dados do jogo" faltando | 69 | todas | corrigido |
| 2 | Aviso falso de pacote de textura | 46 | dev.54, 0.9.0 | corrigido (Regulus) |
| 3 | BT4: outra versão do mod ou ISO de outro jogo | 18 | 0.9.0 | recusa melhorada |
| 4 | `.7z` e `.rar` no lugar de `.zip` | 18 | 0.8.1, 0.9.0 | recusa clara, sem suporte novo |
| 5 | Dragon Net cai do servidor A para o B | 18 | todas | menos pedidos; limite do plano continua |
| 6 | Verificação de atualização sem internet | 16 | todas | não é mais aviso |
| 7 | Dessincronia no online | 14 | todas | correções parciais; aberto |
| 8 | Imagens da tela online brancas | 11 | até 0.9.0 | corrigido (Regulus) |
| 9 | HTTP 429 / erro 1027 | 11 | todas | recuo e mensagem nos três serviços |
| 10 | `BtlObjMdl_BindFiles+0x30` | 10 | dev.51, 0.8.1, 0.9.0 | mitigado; causa raiz sem prova |
| 11 | "Não é uma ISO válida" | 7 | 0.8.1, 0.9.0 | mensagem já clara |
| 12 | BT4/ARMSX2: superfície perdida na volta do segundo plano | 4 | 0.9.0 | aberto |
| 13 | ISO incompleta | 3 | 0.8.1, 0.9.0 | mensagem já clara |
| 14 | "Sem memória" com tamanho negativo | 3 | dev.51, 0.9.0 | corrigido |
| 15 | `VULKAN_INTERNAL_PrepareTextureSubresourceForWrite` e outros crashes de poucos aparelhos | 2 cada | várias | aberto |

## Detalhes

### 1. Menu font e letreiro

```
bt3: menu text: no list of labels was made: gamedata/pzs3us1/00005.bin is missing (the game data of the USA release...
bt3: main menu: the lettering of the PC's item was not made from the disc (gamedata/pzs3us1/00448.bin is missing...
```

**Causa:** o Android guarda o AFS inteiro (`layout=afs`), e `ml_read_one` em `port/src/gs/menu_labels.c` procurava só arquivos
soltos. A linha aparecia para todos os jogadores, e o Menu font só oferecia "Original". **Correção:** a leitura cai no AFS por
`Port_AfsSpanShared`. Ver [`changes/bt3-engine.md`](changes/bt3-engine.md).

### 2. Aviso falso de textura

```
[game] the last run ended badly: TEXTURES: texture pack: no replacement textures (PCSX2 names) found in <pasta>
```

338 eventos, o maior número de todos. **Causa:** a chave de pacote vem ligada, e a linha sai também com a pasta vazia. **Correção:**
`f689660f` (Regulus): `LastRun.kt` só avisa se a pasta tem arquivos.

### 3. BT4: outra versão do mod

```
Esta imagem é outra versão do mod: o jogo foi recompilado para a Beta 14 Rev2 (SLUS-219.78).
bt4: no SLUS_219.xx; top of the image: [SYSTEM.CNF, SLUS_216.xx, BIN, IRX, DATA]
```

**Causa:** o runner é compilado para um único executável, e jogadores dão outra Beta ou a ISO do BT3 ou do SLES na vaga do BT4.
**Correção:** a recusa diz o executável achado. A recusa continua certa: outro executável não roda neste código.

### 4. `.7z` e `.rar`

```
Escolha um pack em .zip (arquivos .7z e .rar ainda não são aceitos).
This file is a compressed 7Z, not the disc image itself. Extract it first (ZArchiver, for example)
```

A mensagem já diz o que fazer. Aceitar `.7z` traria um descompactador para o APK; deixei de fora.

### 5. Servidor A para B

```
dragonnet: server A failed, using B until #:# UTC
```

**Causa:** `mitigo-rooms` estoura o limite do plano grátis. **Correção:** ver o item 9. O limite diário é da conta do Worker e não se
resolve no app: recomendação em `docs/release-1.0.md`.

### 6. Atualização sem internet

```
check failed: UnknownHostException: Unable to resolve host "api.github.com"
```

Portátil sem Wi-Fi é o caso normal. **Correção:** virou linha de informação em `AppViewModel.kt`. A verificação manual continua
dizendo que falhou.

### 7. Dessincronia

```
bt3: net: The two games no longer agree (desync): the match has ended. (blank #)
```

15 eventos em 8 aparelhos no resumo; 14 aparelhos contando as linhas de versões antigas. **Causas lidas:** música aleatória que
depende das músicas instaladas (`battle_load.c`, sem correção) e a opção `CLASH_QTE` local (corrigida em `c98c4c6b`). O log
de jogador não diz qual campo divergiu. **Estado:** aberto; o teste entre os aparelhos está em `docs/release-1.0.md`.

### 8. Imagens brancas

```
bt3: artwork: cannot read port/tbo/ui/bg.png
```

**Causa:** no Android o SDL abre caminho relativo dentro dos assets do APK. **Correção:** `5d3eea11` (Regulus), caminho absoluto.

### 9. HTTP 429

```
net: HTTP 429 from hello: error code: 1027
```

**Correção:** Dragon Net já recuava (`net_match.c`). Na 1.0 o Tag Team e o BT2 também recuam de 30 s a 10 min, com "Servidor
ocupado"; as salas consultam a cada 2 s em vez de 1 s e nada é pedido com o app em segundo plano. O "429" procurado na
resposta inteira, que recusava um join válido, foi corrigido.

### 10. `BtlObjMdl_BindFiles+0x30`

```
bt3: model of slot N was not in its buffer (file 2824); read again
CRASH: signal 11 (code 1) at BtlObjMdl_BindFiles+0x30, fault address 0x6c
```

57 eventos em 10 aparelhos, na maioria em `BtlObj_Rebind`, na troca de forma no meio da luta. **Lido no código:** `ADXF_ReadNw`
preenche com zeros o que `Port_FileReadAt` não leu, sem avisar; `meshOfs` fica em 0x6c de um `file` nulo. A tabela dos arquivos citados
está certa, então o disco não é a causa. O arquivo aberto parece parar de funcionar, porque uma releitura pelo mesmo objeto também
falhava. **Isso é hipótese:** o log não tinha o `errno`. **Correção:** a leitura curta agora diz o motivo, reabre o arquivo e tenta até
3 vezes; se ainda falhar, o lutador fica com o modelo que tem, e no início da batalha o jogo diz para reinstalar a ISO.

### 11. ISO inválida e 13. ISO incompleta

```
The file is not a valid ISO disc image. Use the game's ISO (not CSO, CHD or ZIP).
The image ends early (incomplete file?)
```

Recusas certas, com mensagem que diz o que fazer.

### 12. Superfície perdida no BT4

```
VK: Surface lost, attempting to recreate
(CreateVulkanSurface) vkCreateAndroidSurfaceKHR failed: (-1000000001: VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)
```

Dois aparelhos terminam em crash em `vulkan::driver::CreateAndroidSurfaceKHR`. **Causa provável:** o ARMSX2 recria a superfície antes de
liberar a antiga, ao voltar do segundo plano. **Estado:** aberto, não reproduzido.

### 14. Tamanho negativo

```
no memory below 4 GB for 18446744073709531552 bytes
```

É -20064 pedido por dados de disco modificado, em imagens com 3400 arquivos no AFS (o original tem 3399). **Correção:** `Port_LowAlloc`
diz que não é o disco original, e `LastRun.kt` mostra isso como problema de dados, não de memória.

### 15. Crashes de poucos aparelhos

| Assinatura | Aparelhos | Leitura |
|---|---|---|
| `libSDL3.so VULKAN_INTERNAL_PrepareTextureSubresourceForWrite` | 2 | aberto |
| `libbt4.so std::recursive_mutex::unlock` (moto g20) | 2 | aberto |
| `Res_RelocateOffsets` | 2 | endereços inválidos; provável disco modificado |
| `BtlScene_TestCharPackBit+0x34` | 1 | 0.8.1; sem repetição |
| `BootCard_Init+0x94` | 1 | 0.8.1; sem repetição |
| `HudGauge_DrawFace` | 0 | não aparece nesta pasta de logs |

Logs gigantes de "file not found": nenhum nesta pasta. A linha já sai só nas primeiras vezes e nas potências de dois (`plat_fcache.c`).

## Desempenho por GPU (lutas, 0.8.1 e 0.9.0)

| GPU | Velocidade | fps | Aparelhos |
|---|---|---|---|
| Mali-G52 MC2 | 94,6% | 26,5 | 6 |
| Mali-G57 MC2 | 91,6% | 21,6 | 5 |
| Adreno 610 | 83,9% | 18,8 | 5 |
| Mali-G615 MC2 | 80,4% | 26,9 | 2 |
| Adreno 630 | 81,9% | 24,6 | 1 |
| Mali-G720 MC8 | 82,8% | 39,5 | 1 |

Médias de poucos aparelhos e de resoluções escolhidas por cada jogador: servem para ordenar, não para medir.
