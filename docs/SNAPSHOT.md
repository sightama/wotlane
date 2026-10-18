# Situação do port (atualizado em 2026-10-18)

Só o que está em aberto. A lista técnica completa (com fotos e medições) é a do `passos/VU1CHECK.md`, e ela manda: o que sai de lá sai daqui também.

## 1. Corrigido, falta o dono conferir ao vivo
- Céu do menu (drive-in) com área preta: quinas do telão, logo abaixo da faixa de cima
- Céu da AIRPORT escuro (vale olhar também as fases do Dragon)
- Sombra do corpo inteiro do Kineticlops (CENTRAL); conferir que a dos outros continua igual
- Rastro de poeira de prédio destruído (cor e U da textura)
- Linhas (`FX_LINE`): corda do mangual do Ultra-V e rastros de linha; cor das partículas
- Projéteis do Plantboss (3-MILE): 5 em leque, ~1,2 s, cada um solta 5 teleguiados (causa era o `rand()`, já corrigido)
- Explosão na frente do Assboss (CANYON2): conferir no **PCSX2** se o corpo escurece por cima do fogo

## 2. Em aberto, sem pressa
- Ponta da corrente do Ultra-V com 2 a 3 px de diferença (só mexer se aparecer na tela)

## 3. Coisas ainda não implementadas

### No port
- Tela dividida (2 jogadores na mesma tela), incluindo a faixa preta que separa as vistas
- Iluminação: é a próxima área da lista do port. Hoje só a luz direcional está implementada; não achei luz pontual no código nativo
- Áudio dos filmes: segue o relógio do PC, não o do jogo
- Renderização nativa completa: hoje o VU1/GS emulado ainda serve de referência; destino é desenhar direto na fronteira da engine

### Na decomp
- Carregadores e câmera em C++ legível (`Shell::Load*`, `MonsterParse`, câmera): é o que vai permitir modar, e ainda não foi feito
- Runtime cobaia: parado, quebrado
