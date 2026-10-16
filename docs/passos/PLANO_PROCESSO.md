# Plano de processo (2026-10-15): o que foi proposto e o estado de cada item

Os blocos marcados com "▎" são a explicação para leigo; são o único texto do repo escrito para quem não programa. O detalhe técnico de cada item está no documento apontado na tabela, não aqui.

▎ Em palavras simples: existem dois trabalhos acontecendo em paralelo: (a) a decomp, que é reescrever o
▎ código do jogo em C++ legível, função por função; e (b) o port, que é o jogo rodando no PC via uma
▎ tradução automática do executável mais uma camada nossa que desenha a tela. Em 2026-10-15 listamos
▎ onde o tempo estava sendo gasto sem retorno e o que mudaria isso. Em 2026-10-16 quase tudo estava feito
▎ e a direção passou a ser: primeiro terminar o port (HUD, tela larga, acabamento gráfico), depois a decomp.

## Estado (2026-10-16)
| Item | Proposta | Estado | Detalhe |
|---|---|---|---|
| 2.1/2.2 | Comparação VU1 x nativo offline: instantâneo (`.vu1snap`) + replay de mesa com tabela e imagens | feito | `VU1CHECK.md`, "Como funciona"; `NATIVO.md`, "Instantâneo e checagem sem jogo" |
| 2.3 | Parar de esperar o evento: gatilho por condição (`WOTM_VU1CHECK_WHEN`), rajada, teto de disparos, provocar com `WOTM_MATCH`/`PS2X_PAD` | feito (o hook que força o estado do monstro saiu do plano: pad + gatilho bastam) | `NATIVO.md`, tabela de variáveis |
| 2, peça 3 | Corpus de regressão | feito: `tools/corpus.ps1 -Capture` / `-Check -Baseline` | `VU1CHECK.md`, "Corpus de regressão" |
| 2, extra | Inventário do que falta no port (só um lado desenha, por dono; diff por pixel com dono; HUD por id; funciona nos menus) | feito | `VU1CHECK.md`, "Inventário do port" |
| 1.2 | Código do port como arquivos normais no repo, não como patch | feito: `native/runtime/`; o patch só tem costuras | `NATIVO.md`, "Onde mora o código do runtime" |
| 1.3 | Endereços por nome | feito: `tools/native_syms.py` gera `native/wotm_syms.h`; sobram ~28 offsets de struct e ~40 sem símbolo | `NATIVO.md`, "Endereços por nome" |
| 1.4 | Verificação por TU num comando; rascunhos do m2c em lote; limpar worktrees | feito: `tools/verify_tu.py`, `tools/m2c_batch.sh` | `FERRAMENTAS.md` |
| 1.5 | Docs: estado atual separado do diário; tabela de variáveis gerada do código; apagar variáveis e scripts mortos | feito em 2026-10-16: 7 variáveis e 14 scripts apagados; tabela conferida por `tools/wotm_env.py`; diários encolhidos | `NATIVO.md`, "Diagnósticos" |
| 1.1 | Runtime cobaia: o mesmo runtime rodando o ELF com as equivalentes, para testar as funções reescritas jogando | **incompleto e parado** (boota e chega à luta; quebra numa chamada indireta; correção escrita, não testada) | `NATIVO.md`, "Runtime cobaia" |

## As propostas, em palavras simples

### 1.1 Decomp e port não se conectam
▎ Já temos mais de 2.500 funções do jogo reescritas em C++ legível, mas o jogo que roda no PC não usa
▎ nenhuma delas. Ele usa a tradução automática do executável original. Isso foi uma escolha de propósito:
▎ se o port se comporta errado, a culpa é do port, nunca de uma função reescrita. A proposta NÃO troca a
▎ base. É um segundo executável, separado, usado só como cobaia para testar as funções reescritas jogando.
▎ Chegou a abrir e entrar na luta, mas ainda quebra; ficou parado porque o port vem primeiro.

### 1.2 Código nosso vivia dentro de um patch
▎ Boa parte do código que escrevemos para o port (som, desenho ao vivo, comparação com o VU1, correções de
▎ velocidade) estava guardado como um "arquivo de diferenças" aplicado por cima do projeto de terceiros
▎ (PS2Recomp). É como guardar um livro só como lista de correções sobre outro livro: ninguém lê, não dá
▎ para ver quem mudou o quê, e quebra toda vez que o outro livro muda. Agora esses arquivos são arquivos
▎ comuns do repositório e no projeto de terceiros ficaram só os pontos de encaixe.

### 1.3 Endereços e offsets crus em vez de nomes
▎ A camada que desenha a tela acessava a memória do jogo por números crus, tipo "o valor que está no
▎ endereço 0x6F8464". Já tínhamos uma lista com o nome de cada endereço (mais de 7 mil). Agora um arquivo
▎ gerado troca números por nomes, para decomp e port usarem o mesmo vocabulário.

### 1.4 Fluxo por função e por lote
▎ Para dar uma função como "pronta", a pessoa rodava sete ferramentas na mão. Agora um comando só roda
▎ todas e mostra uma tabela. Também dá para gerar os rascunhos automáticos de todas as funções da fila de
▎ uma vez. O que trava o trabalho em paralelo é mexer no mesmo arquivo de definições compartilhadas, então
▎ isso se faz em série.

### 1.5 Docs e diagnósticos
▎ A documentação misturava "como as coisas são hoje" com "o diário de como chegamos aqui". O port tinha 72
▎ "interruptores" de teste, vários que a própria documentação dizia que não serviam mais; cada um é código
▎ a mais para manter. Os mortos foram apagados, a tabela dos que existem é conferida contra o código por um
▎ script, e os diários foram encolhidos (o histórico fica no git).

### 2 Comparação VU1 x nativo offline (era a prioridade)
▎ Antes, a comparação acontecia dentro do jogo rodando. Para saber se a nossa tela estava igual à do PS2,
▎ o jogo precisava estar numa luta, chegar no momento interessante, e aí executar o "hardware falso" do PS2
▎ para ter a referência. Cada investigação pagava três esperas: a luta chegar lá, o evento acontecer, e a
▎ comparação rodar. E a cada ajuste nosso, tudo de novo. Isso custou minutos a horas por investigação.
▎
▎ Agora o jogo tira uma "foto completa" da memória nos momentos interessantes (ou quando se pede, com uma
▎ tecla, ou quando um evento nasce, como uma explosão). Uma ferramenta de mesa abre a foto, calcula a imagem
▎ de referência do PS2 e a nossa, e mostra as duas lado a lado com as diferenças, em segundos, quantas vezes
▎ se quiser, sem abrir o jogo. Um conjunto de fotos nomeadas (menus, lutas, destroços, arco elétrico) é
▎ rodado inteiro depois de cada mudança e diz o que piorou. E a mesma ferramenta produz a lista do que ainda
▎ falta no port (o que o PS2 desenha e nós não, e vice-versa), em vez de alguém lembrar.
