# Toque e controles

Regras e formato: [README](README.md).


### 1.0.0 (este commit): mapeamento de controle relido no Android
- **Onde:** `port/src/gs/gs_input.c`, início de `poll_pads`, `SDL_AddGamepadMappingsFromFile`
- **O que e por quê:** a leitura usava caminho relativo, que no Android o SDL procura nos assets do APK, e a escrita usava o diretório do jogo. Quem fez a configuração automática perdia o mapeamento a cada abertura, e cada nova configuração só acrescentava a mesma linha.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** controle com mapeamento salvo errado passa a valer de verdade na próxima abertura.
- **Como testar de novo:** configurar um controle sem mapeamento, fechar, abrir: continua configurado.
