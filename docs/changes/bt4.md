# BT4 (runner recompilado)

Regras e formato: [README](README.md).

### 1.0.0 (este commit): símbolos novos do `gs_input.c` no shim
- **Onde:** `port/bt4/host/bt4_shim.c`, depois de `gPortNetWaitNs`
- **O que e por quê:** a mescla do PC 0.1.39 e do Tag Team fez o `gs_input.c` pedir `Port_NetSpectating`,
  `Port_TagTestCheck`, `TagNet_Enabled`, `TagNet_Role`, `TagTeam_ViewActor` e `gNativePad`. O `check_symbols` parou o build;
  sem ele, o BT4 cairia no aparelho com "cannot locate symbol", como na mescla da 0.1.38.
- **Medido/provado:** `build_bt4.py` passa a checagem. Abertura e luta: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** BT4 nos dois renderizadores. Novo código de BT3 em `port/src/gs` pode pedir mais símbolos.
- **Como testar de novo:** `python3 port/bt4/build_bt4.py --armsx2`; `port/tools/lab/bt4fight.sh`.

