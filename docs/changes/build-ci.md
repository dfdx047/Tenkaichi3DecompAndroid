# Build e CI

Regras e formato: [README](README.md).


### 1.0.0 (este commit): ferramentas de laboratório e logscan
- **Onde:** `port/tools/logscan.py` (crash sem repetição por aparelho); `port/tools/lab/ppmcmp.py` (novo)
- **O que e por quê:** o `logscan` contava o mesmo crash duas vezes (linha do log e linha do relatório do app). `ppmcmp.py` compara as pastas de quadros `BT3_SHOT` pixel a pixel.
- **Medido/provado:** rodado nos 208 logs dos jogadores.
- **O que pode quebrar:** contagens dos relatórios antigos.
- **Como testar de novo:** `python3 port/tools/lab/ppmcmp.py <pasta a> <pasta b>`: 0 pixels entre duas rodadas da mesma gravação.
