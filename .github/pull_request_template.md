## O que muda
<!-- TU(s) e funções tocadas -->

## Checklist
- [ ] `sh tools/wsl/gate.sh` terminou em `ROM OK` (cole a última linha abaixo)
- [ ] Nenhum arquivo do jogo (ISO, `asm/`, assets) nem trecho de assembly no PR
- [ ] Funções que não batem estão em `#ifdef NON_MATCHING` com nota (`nm_wrap.py`)
- [ ] Equivalentes conferidos: retorno usado pelos chamadores, sequência de chamadas, strides de struct, delay slots
- [ ] Funções na ordem de endereço original; helpers `static` dentro de `NON_MATCHING`
- [ ] Não commitei `config/status.csv`
- [ ] Se mexi em header compartilhado: comparei `scoreall.sh` antes/depois

## Saída do gate
```
(cole aqui)
```

## Contagem
<!-- matched / equivalent novos, ex.: +12 matched, +5 equivalent -->
