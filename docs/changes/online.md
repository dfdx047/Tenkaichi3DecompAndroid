# Online

Regras e formato: [README](README.md).

### 1.0.0 (este commit): `Tb_Vanilla` sem o desvio morto da mescla
- **Onde:** `src/menu/port_tbo.c`, `Tb_Vanilla`, linhas 628-634
- **O que e por quê:** a mescla automática juntou o `state < 0` do Regulus (deixar entrar quando o fingerprint não foi
  calculado) com o `state != 1` da 0.1.39, que já retorna antes. O bloco nunca rodava. Fica o da 0.1.39:
  `Port_DataFingerprintNeed` calcula o fingerprint no Android e o jogador espera por ele.
- **Medido/provado:** leitura; a partida rápida é testada na bateria.
- **O que pode quebrar:** Partida Rápida e Ranked no Android, se o fingerprint não bater com o do disco: a tela
  "Sem Partida Rápida" apareceria para todos.
- **Como testar de novo:** Dragon Net Online › Partida Rápida com a ISO original: a busca tem de começar.

