# Launcher

Regras e formato: [README](README.md).


### 1.0.0 (este commit): controle volta a navegar no launcher
- **Onde:** `android/.../MainActivity.kt`, `dispatchKeyEvent`/`dispatchGenericMotionEvent` removidos
- **O que e por quê:** veio do BT2 (Regulus): o launcher engolia todo evento de controle, inclusive no RG Cube e no RP5, em que o direcional é o único jeito de navegar sem toque. Na 0.9.0 isso não existia, e o jogo roda em outra Activity, que recebe o controle dela.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** um botão de controle agir no launcher e no jogo ao mesmo tempo, o que não acontece porque são Activities diferentes.
- **Como testar de novo:** no launcher, navegar com direcional e confirmar com A; abrir o jogo e conferir que o controle só age no jogo.

### 1.0.0 (este commit): equipe na tela Sobre, licenças no app
- **Onde:** `ui/screens/SettingsScreen.kt` (`about_team`, botão Licenças); `port/tools/android.py` (gera `assets/licenses.txt`); `README.md`, `README.pt.md`, `README.es.md`, "Equipe e créditos"
- **O que e por quê:** os avisos de licença ficavam fora do APK (`android.py` excluía os arquivos) e o app não tinha tela para eles. sse2neon, pl_mpeg, Kanit, fontes, botões CC BY 4.0 e demais agora aparecem no app, juntados na compilação.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** nada de jogo; `licenses.txt` é gerado e fica no `.gitignore`. Se faltar um arquivo de licença, `android.py` falha na compilação.
- **Como testar de novo:** Settings > Sobre > Licenças: a lista abre e rola.
