# Launcher

Regras e formato: [README](README.md).


### 1.0.0 (este commit): controle volta a navegar no launcher
- **Onde:** `android/.../MainActivity.kt`, `dispatchKeyEvent`/`dispatchGenericMotionEvent` removidos
- **O que e por quê:** veio do BT2 (Regulus): o launcher engolia todo evento de controle, inclusive no RG Cube e no RP5, em que o direcional é o único jeito de navegar sem toque. Na 0.9.0 isso não existia, e o jogo roda em outra Activity, que recebe o controle dela.
- **Medido/provado:** compila; teste em aparelho: ver a bateria em `docs/release-1.0.md`.
- **O que pode quebrar:** um botão de controle agir no launcher e no jogo ao mesmo tempo, o que não acontece porque são Activities diferentes.
- **Como testar de novo:** no launcher, navegar com direcional e confirmar com A; abrir o jogo e conferir que o controle só age no jogo.
