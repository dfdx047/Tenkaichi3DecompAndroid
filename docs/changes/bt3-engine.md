# Motor do BT3 (jogo descompilado, loader, memória)

Regras e formato: [README](README.md).


### 1.0.0 (este commit): leitura de arquivo que falha: diz o motivo, tenta de novo, luta segue
- **Onde:** `port/src/plat_fcache.c`, `Port_FileReadAt`; `src/battle/btl_obj.c`, `BtlObj_Rebind` e `BtlRes_PortModelReady`; `src/sys/gfxm_e_b.c`, `BtlObjMdl_Init`
- **O que e por quê:** maior crash dos logs: `BtlObjMdl_BindFiles+0x30`, endereço 0x6c, 10 aparelhos e 57 eventos, sempre em `BtlLoad_StepChara > BtlObj_Rebind` no meio da luta. `meshOfs` fica em 0x6c de um `file` nulo, e `ADXF_ReadNw` preenche com zeros o que não foi lido sem dizer nada. Conferi os arquivos 1994, 2824 e 2924 no AFS de um aparelho: a tabela deles está certa, então o disco não é a causa; a releitura da 0.9.0 lê do mesmo `FILE*` e também não resolvia. A causa exata da falha de leitura não está provada, porque o log não tinha o `errno`.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`. A chave de teste `BT3_TEST_READ_FAIL=<n>` faz a n-ésima leitura não entregar nada.
- **O que pode quebrar:** `exit(2)` novo quando o modelo não existe no início da batalha; a troca de forma que falha deixa o lutador com o modelo antigo e as animações novas.
- **Como testar de novo:** jogar com `BT3_TEST_READ_FAIL=2000` em `bt3_env.txt`: o log diz `reading ...: 0 of N bytes (I/O error); after opening it again N` e a luta segue. O próximo log real de jogador diz o `errno`.
