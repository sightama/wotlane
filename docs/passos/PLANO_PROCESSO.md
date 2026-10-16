PLANO: otimizar o processo de decomp e de port (2026-10-15)
==========================================================

Os blocos marcados com "▎" são a explicação para leigo; o texto logo abaixo de cada um é a versão técnica.

▎ Em palavras simples: existem dois trabalhos acontecendo em paralelo: (a) a decomp, que é reescrever o
▎ código do jogo em C++ legível, função por função; e (b) o port, que é o jogo rodando no PC via uma
▎ tradução automática do executável mais uma camada nossa que desenha a tela. Abaixo está onde o tempo
▎ está sendo gasto sem retorno e o que mudaria isso. A parte 2 (comparação offline) é a prioridade.

Fontes lidas: README, PLANO, ANALYSIS, PROXIMOS_PASSOS, os seis docs/passos/, CONTRIBUTING, o patch do
runtime, os scripts de build/gate e o estado do git.


PARTE 1: ESTRUTURA DO REPO E DO FLUXO
=====================================

1.1 Decomp e port não se conectam (decisão: manter assim por enquanto)
----------------------------------------------------------------------
▎ Em palavras simples: já temos mais de 2.500 funções do jogo reescritas em C++ legível, mas o jogo que
▎ roda no PC não usa nenhuma delas. Ele usa a tradução automática do executável original. Isso foi uma
▎ escolha de propósito: se o port se comporta errado, a culpa é do port, nunca de uma função reescrita.
▎ A proposta aqui NÃO troca a base. É um segundo executável, separado, usado só como cobaia para testar
▎ as funções reescritas jogando. Fica para depois da parte 2.

O patch do runtime não referencia src/, e recomp/nm existe mas nenhum script o usa. A validação das
equivalentes fica no halfcpp (PCSX2) ou no oracle, que só cobre funções folha.

    Estado                              | Funções
    ------------------------------------|---------------------
    matched                             | 1548
    equivalent                          | 1007
    asm                                 | 2544
    equivalentes validadas pelo oracle  | 24 de 60 candidatas

Proposta (adiada): script que gera recomp/nm a partir do ELF nmall e compila um segundo runtime
(build_dev_nm). A base retail continua sendo a referência; o lote.ps1, o vigia, o WOTM_MATCH e o
VU1CHECK viram teste de regressão de todas as equivalentes, inclusive as com VU0 e com chamadas,
sem PCSX2 e sem gen_nm_ld/patch_paddr. É o "fluxo de um mod" do PLANO.md automatizado.

1.2 Código nosso vive dentro de um patch de 4.580 linhas
--------------------------------------------------------
▎ Em palavras simples: boa parte do código que escrevemos para o port (som, desenho ao vivo, comparação
▎ com o VU1, correções de velocidade) não está guardado como arquivos normais no repositório. Está
▎ guardado como um "arquivo de diferenças" aplicado por cima do projeto de terceiros (PS2Recomp). É como
▎ guardar um livro só como lista de correções sobre outro livro: ninguém lê, não dá para ver quem mudou
▎ o quê, e quebra toda vez que o outro livro muda. A proposta é trazer esses arquivos para dentro do
▎ repositório como arquivos comuns e deixar no projeto de terceiros só os pontos de encaixe.

wotm_vu1check.inc (812 linhas), wotm_overrides.cpp (391), wotm_live.inc (387), wotm_snd.inc (250),
wotm_scene.inc e o VU0 nativo (220+) somam mais de 2.600 linhas editadas só no clone externo e
regeneradas por git diff; 32 commits tocaram o patch desde 2026-10-09. Sem blame, revisão ilegível,
git apply quebra quando o fork anda.

Proposta: os arquivos wotm_* vêm para native/runtime/; as costuras do PS2Recomp (CMake com
-DWOTM_NATIVE_DIR, chamadas em ps2_runtime.cpp/main.cpp/EeScheduler) vão para a branch wotm do
submódulo. O CMake passa a listar native/ como dependência e a gambiarra de "tocar ps2_runtime.cpp"
desaparece.

1.3 Endereços e offsets crus em vez de nomes
--------------------------------------------
▎ Em palavras simples: a camada que desenha a tela acessa a memória do jogo por números crus, tipo "o
▎ valor que está no endereço 0x6F8464". Já temos uma lista com o nome de cada endereço (mais de 7 mil)
▎ e a descrição das estruturas em arquivos de cabeçalho. A proposta é gerar automaticamente um arquivo
▎ que troca números por nomes, para decomp e port usarem o mesmo vocabulário.

symbol_addrs.txt tem 7.284 símbolos, mas native/ usa cerca de 130 endereços absolutos e
wotm_native.hpp tem 164 acessos +0x... Os layouts já estão em include/game/game.h e em
include/hieri_types.h (gerado do mdebug). Proposta: gerador de native/wotm_syms.h com constexpr por
símbolo, e um Ram::at<T>() sobre os headers existentes.

1.4 Fluxo por função e por lote
-------------------------------
▎ Em palavras simples: para dar uma função como "pronta", a pessoa roda sete ferramentas na mão. A
▎ proposta é um comando só que roda todas e mostra uma tabela. Também dá para gerar os rascunhos
▎ automáticos de todas as funções da fila de uma vez. As pastas de trabalho paralelo já integradas
▎ podem ser apagadas; o que trava o paralelismo é mexer no mesmo arquivo de definições compartilhadas.

- tools/wsl/verify_tu.sh <TU>: roda scoreall, difftest_all, nm_calls, nm_retvals e nm_audit e imprime
  uma tabela por função. O CLAUDE.md encolhe.
- Rascunhos do m2c em lote para toda a fila de boot_coverage_asm.csv em build/m2c/ (fora do repo).
- Worktrees: 9 das 11 branches wt/ estão merged e as pastas continuam no disco; limpar. Regra de lote:
  um agente por TU, headers compartilhados (109 commits em include/game desde o dia 9) só em série.

1.5 Docs e diagnósticos
-----------------------
▎ Em palavras simples: a documentação mistura "como as coisas são hoje" com "o diário de como chegamos
▎ aqui". O port tem 72 "interruptores" de teste, vários que a própria documentação diz que não servem
▎ mais; cada um é código a mais para manter. Há scripts de rascunho largados na pasta de ferramentas.

- docs/passos é o que mais muda no repo; NATIVO.md mistura estado atual com diário. Separar "estado
  atual por área" (curto) do histórico que o git já guarda.
- 72 variáveis WOTM_* no código. Apagar as variantes descartadas (SKIN2BONE, ADCMODE, FUDGEOLD,
  OLDCLOCK, PALNOSWAP...) e gerar a tabela de variáveis a partir do código.
- tools/wsl tem 11 scripts _*.sh de rascunho e alguns .py pontuais. Mover para fora ou apagar.


PARTE 2: COMPARAÇÃO VU1 x NATIVO OFFLINE (PRIORIDADE)
=====================================================

2.1 Por que o VU1CHECK é lento
------------------------------
▎ Em palavras simples: hoje a comparação acontece dentro do jogo rodando. Para saber se a nossa tela
▎ está igual à do PS2, o jogo precisa estar numa luta, chegar no momento interessante, e aí executar o
▎ "hardware falso" do PS2 para ter a referência. Cada investigação paga três esperas: a luta chegar lá,
▎ o evento acontecer, e a comparação rodar. E a cada ajuste nosso, tudo de novo. Isso custou minutos a
▎ horas por investigação em partidas em lote com IAs.

Três coisas acopladas: a referência (o que o VU1 mandaria ao GS) só é produzida ao vivo; a cena
nativa só é coletada ao vivo; e o evento depende da sorte da IA. Nenhuma precisa ser assim. A
referência é uma função pura de um estado: RAM naquele instante, memória do VU1 (microcódigo e
dados), registradores do VIF1 e do GS, e o endereço de início da cadeia. Tudo isso cabe num arquivo.

2.2 Proposta: instantâneos + ferramenta de mesa
-----------------------------------------------
▎ Em palavras simples: o jogo só tira uma "foto completa" da memória nos momentos interessantes.
▎ Depois, uma ferramenta de mesa parecida com o viewer abre a foto, calcula a imagem de referência do
▎ PS2 e a nossa, e mostra as duas lado a lado com as diferenças. Leva segundos e dá para repetir
▎ quantas vezes quiser depois de cada ajuste, sem abrir o jogo.

Três peças, em ordem de valor:

1. Instantâneo completo em vez de checagem. O WOTM_VU1CHECK passa a gravar um pacote: ram, vram,
   memória do VU1 (micro e dados), registradores do VIF1 e do GS, e o endereço de início de cada
   cadeia do quadro. O runtime já grava ram_<q>.bin/vram_<q>.bin e já sabe onde cada cadeia começa;
   é estender o dump existente. A cena precisa ser coletada no início de cada cadeia (NATIVO.md), então
   grava-se um instantâneo por cadeia, ou um por quadro mais a diferença.

2. vu1cmp: a comparação como programa de mesa. Executável ao lado do viewer que linka a biblioteca do
   runtime (o build.bat já usa o raylib de lá) e roda os interpretadores de VIF1/VU1 e o GS em
   software sobre o instantâneo. Saída em três formas: a tabela numérica do VU1CHECK de hoje, a
   imagem de referência rasterizada pelo GS em software, e a imagem nativa desenhada pelo viewer, com
   diff por pixel. O código de desmontar XGKICK em triângulos e casar com o nativo já existe em
   wotm_vu1check.inc; só muda de lugar. Aceita filtro por polyPkt ou por fx[i] (como WOTM_ONLYNODE).

3. Corpus de regressão. Pasta fora do repo com 20 a 30 instantâneos nomeados pelo que mostram: arco
   do Robo-47, explosão, chuva de destroços, prédio fantasma, METRO com 4 monstros. Cada mudança na
   camada nativa roda vu1cmp sobre o corpus inteiro em segundos e diz o que piorou. Hoje isso custa um
   lote de minutos a horas, sem garantia de rever o mesmo quadro.

2.3 Como parar de esperar o evento acontecer
--------------------------------------------
▎ Em palavras simples: mesmo com a foto, alguém precisa tirá-la no momento certo. Em vez de deixar as
▎ IAs lutarem até acontecer, dá para pedir ao jogo que avise quando o evento ocorrer, ou provocar o
▎ evento de propósito.

- Gatilho por condição, não por relógio: WOTM_SNAP_WHEN=fx:7 grava o instantâneo no primeiro quadro
  em que um sistema de partículas do tipo 7 tem partículas vivas, e sai. O vigia já faz isso para
  congelamento.
- Provocar o evento: WOTM_MATCH pula o menu e PS2X_PAD dá roteiro de botões. Com o jogador sob
  roteiro em vez de sob a IA, o golpe é apertado no segundo 5, não sorteado no minuto 12. Para o que
  o pad não alcança, um hook que força o estado do monstro (como já se faz com m_playerNum no AIVSAI).
- Recortar o que entra: para a maioria das pistas abertas do VU1CHECK.md, uma cadeia de um objeto
  basta.


ORDEM DE EXECUÇÃO
=================
▎ Em palavras simples: primeiro a comparação offline (parte 2), porque é o que mais consome horas
▎ hoje. Depois trazer o código do port para dentro do repositório como arquivos normais. Depois trocar
▎ números por nomes e criar o comando único de verificação. A cobaia com as funções reescritas (1.1)
▎ fica por último.

1. [FEITO 2026-10-15] Parte 2, peça 1 (instantâneo completo) e peça 2 só com a tabela numérica,
   reaproveitando o wotm_vu1check.inc. WOTM_VU1CHECK_SNAP=<prefixo> grava o .vu1snap de cada quadro
   amostrado; tools\vu1cmp.ps1 <arquivo ou pasta> refaz a checagem em ~1 s por instantâneo, sem jogo,
   com saída idêntica à ao vivo (conferido em 2 quadros). Peça 3 (corpus) já funciona passando uma
   pasta ao vu1cmp.ps1; falta só escolher e nomear os instantâneos.
   [FEITO 2026-10-15, 2a rodada] 2.3 gatilho e rajada (WOTM_VU1CHECK_WHEN=fx:<tipo>,frame:<a>-<b>;
   WOTM_VU1CHECK_BURST=<k>), sessão num arquivo só com deltas entre quadros (~7 MB por quadro em vez
   de 40), teto WOTM_VU1CHECK_SNAP_MAX, e -Frame no vu1cmp.ps1. Conferido: 447/447 linhas iguais numa
   sessão de 21 quadros, 5 s de replay. Falta de 2.3 só o hook que força o estado do monstro.
   [FEITO 2026-10-15, 3a rodada] gatilho por borda (dispara quando o evento nasce, não enquanto
   existe), WOTM_VU1CHECK_SHOTS=<n>, condição preset:<fxId>; e a peça 2 com imagem: o --vu1cmp grava
   <arquivo>_<g_frame>_{ref,nat,diff,trio}.png (ref = GS em software do runtime alimentado pelos
   mesmos pacotes, aproximação do PS2; nat = nativo; diff; trio lado a lado). Para isso o
   instantâneo passou a guardar os pacotes do canal GIF e os registradores do GS, e ao vivo os
   uploads de textura vão ao GS (PS2X_GS_UPLOADS ligado sozinho). Verdade de pixel continua sendo
   exportar os pacotes para o renderizador por software do PCSX2 (passo futuro).
   [FEITO 2026-10-16] Inventário (dia 1 do plano decidido com o dono): a checagem lista o que só um
   lado desenha, por dono (linhas "inv"): primitivos do PATH2/PATH3 (HUD, menus, fade) e órfãos do
   VU1 de um lado, objetos nativos sem triângulo do VU1 (inclusive céu/ícones, com alfa máximo) do
   outro. Funciona em qualquer tela (qualquer chamador de hier(0,0)); gatilho mem:<end>[=<val>] e
   tecla F8. Costura nova no fork: g_wotmPath2Src (origem na RAM do DIRECT). Testado: 10 telas do
   menu numa sessão, replay offline igual ao ao vivo; primeiro achado em VU1CHECK.md.
2. Parte 2, peça 2: diff por pixel com atribuição (qual primitivo pintou cada pixel que difere) e a
   imagem de referência nos menus (sai preta: buffer/Z do GS em software); peça 3 (corpus, com um
   instantâneo por tela de menu e os da luta).
3. [FEITO 2026-10-15] 1.2: wotm_live/scene/snd/vu1check.inc, wotm_overrides.cpp e wotm_vu0_native.cpp/.h
   vivem em native/runtime/ no repo; o CMake do fork (-DWOTM_NATIVE_DIR obrigatório) os inclui e compila;
   o patch caiu de ~5.900 para ~2.500 linhas e só tem costuras. O .gen.inc do VU0 vai para <build>\wotm_gen.
   O hook de forçar estado do monstro saiu do plano (pad + WOTM_MATCH + gatilho bastam).
4. [FEITO 2026-10-15] 1.3: tools/native_syms.py gera native/wotm_syms.h (wotm::sym / wotm::fn, 7.282
   símbolos) e trocou 145 endereços crus de native/ por nome (só no código); sobram 28 offsets de
   struct e ~40 sem símbolo.
   [FEITO 2026-10-15] 1.4: tools/verify_tu.py (pontuação + difftest + chamadas + layout + retornos numa
   tabela por função; --no-nm em segundos, com ELF ~4,5 min), tools/m2c_batch.sh (rascunhos da fila
   inteira em build/m2c/), 9 worktrees integradas removidas (ficaram cameras2 e weapon2, com commits
   à frente). Regra de lote (agente por TU, header só em série) já está no CLAUDE.md pelo fluxo.
5. [FEITO 2026-10-15, parcial] 1.5: 7 variáveis WOTM_* de "comportamento antigo" removidas do código
   (SKIN2BONE, FUDGEOLD, PALNOSWAP, ADCMODE, ADCSHIFT, NOCTRL, NODESTR), 14 scripts de rascunho
   apagados de tools/, "Onde estamos" do PROXIMOS_PASSOS atualizado (estava com números de 10 dias
   atrás). Não feito: separar estado atual de diário em RUNTIME.md/ESPINHA.md/TESTE_PCSX2.md (mexer
   nisso sem necessidade concreta é custo sem retorno; fazer quando um desses docs atrapalhar).
6. [EM ANDAMENTO 2026-10-15] 1.1: cadeia pronta: sh tools/wsl/recomp_nm.sh (ELF nmall + PS2Recomp ->
   recomp/nm), build_runtime.bat nm (build_dev_nm com -DPS2X_RUNNER_DIR), run_live.bat/lote.ps1
   -Build build_dev_nm. config/nm_all_exclude.txt lista o que não linka (hoje game/screen inteiro).
   Achados: Grapple.cpp declarava mathfRotMatrixRPH com [3] (símbolo inexistente); Monster precisa
   de -G0; o analisador toma 3 MB de .data por código (sub_0040C7FC, 90 MB de C++): o script poda.
   O cobaia compila e abre, mas o boot salta para lixo: o gen_nm_ld.py deslocou a TU do CTODLinkList
   em 0x798 bytes e a tabela .ctors (dados) ainda aponta o endereço retail. Também falta um mapa
   retail -> nmall para os hooks do runtime (as equivalentes que cresceram vivem no .nm_extra).
   BLOQUEADO no lado da decomp (gen_nm_ld.py); detalhes em NATIVO.md, "Runtime cobaia".
   [INCOMPLETO 2026-10-16] Bloqueio resolvido: o cobaia completo (105 TUs) boota, toca os filmes e chega à luta. A hipótese da
   .ctors estava errada (o runtime carregava o ELF retail). Corrigidos também: hooks por endereço retail (wotm_syms_nm.h), ~90 nop
   do analisador do PS2Recomp, endereços crus dentro de peças deslocadas (gen_nm_ld.py relativo ao símbolo), .ninja_log e falha
   de compilação escondida no build_nm.sh, helpers desligados pelo nm_select.py. Nenhum era bug de equivalente.
   FALTA: (a) testar o nm_fix_data.py (tabela _6Camera$UPDATE_FUNK em .short crus: quebra na luta, Cameras::Update -> 0x11F630);
   (b) uma partida inteira sem quebra; (c) lote retail x cobaia com a mesma semente; (d) levar o bisect.ps1 (rascunho) para tools/.
