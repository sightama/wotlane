# Goliath Prime travado ao ser empalado (2026-10-23)

Sintoma: Goliath (tipo 13, 0x1A0, ASSBOSS1) como jogador 1 contra CPU; ao ser "empalado" fica parado para sempre.

## 1. Caminho do impale

O impale não é golpe de monstro: é o **dardo** (`StateJavelin`, anim 0x36), o arremesso de um pickup rígido do tipo 7 (poste,
antena, viga). Qualquer monstro (a CPU também) que segure um tipo 7 pode lançá-lo.

1. `Weapon::DetonateWeapon` (0x1E3xxx, caso `m_type` 0x17): o pickup tipo 7 bate num interativo `m_kind == 1` (monstro) →
   `impaled = 1` **antes** da chamada, depois `Monster::attachPickupImpaler(vítima, m_pickup, &dir)` (jal em 0x1E3BB0). Como
   `impaled` já é 1, o `DetonateWeapon` não mata o pickup; em seguida `Interactives::dispatchHit(&dir, m_dyn.damage, dono, vítima)`
   aplica o dano normal e `playWeaponExplosion(..., impaled)`. É o único chamador de `attachPickupImpaler`.
2. `Monster::attachPickupImpaler` (0x161C90, 0x1AC): se `m_unk49` (+0x49) for 0, a vida do pickup (+0x48) for <= 0, o pickup não
   tiver `cs` (+0xC) ou a vítima já tiver um impaler válido (+0x68A8/+0x68AC), chama `LevelPickups::killPickup(it, 1)`
   (0x182D20) e volta (caminho "recusa" do próprio jogo). Senão: `detachPickupImpaler(true)`, `dropPickup`, solta agarrão
   (`StateGrappled::breakFree` via +0x68B4), grava o iterador em `m_impaler` (+0x68A8) ou `m_reverseImpaler` (+0x68AC) pelo
   lado do golpe, `LevelPickups::impalePickup`, `hdRemoveCsFromGrid` e **`enterNewState(this + 0x106E0)`** (`StateImpaled`).
3. `Monster::enterNewState` (`Monster.cpp:1824`) **não consulta `transitionOK`**: entra direto.

## 2. Por que trava

- `StateImpaled::transitionOK` (0x154168) existe e exige `m_anims[0x8E/0x8F/0x90].ci != 0` (empalado, luta, arrancar), mas
  o `attachPickupImpaler` pula a checagem. O ASSBOSS1 não tem 142 (0x8E) nem 144 (0x90) (`goliath_codigo.md`, seção 3).
- `StateImpaled::transitionInto` (0x1541A0) zera `m_attacksEnabled`; `StateImpaled::update` (0x1542E0) só sai quando
  `anim == 0x90` e `animationGetCurrentPercent(m_anims[0x90]) >= 0.99`. Com `ci == 0`, `animationGetCurrentPercent` devolve
  **-1.0**, então mesmo depois de apertar até `hold <= 0` o estado nunca termina. Sem a 0x8E/0x8F também não há pose de luta.
  Só uma preempção externa o tiraria (o `handlePreemption` 0x154678 religaria `m_attacksEnabled` e soltaria o impaler).
- Atacante: nada nele espera o resultado (o `StateJavelin` termina pela própria anim); não trava.
- O gatilho de fase `doImpaleVolume` (0x16DE68) é outro caminho e já confere `StateStunned::transitionOK` antes de entrar; não
  é o problema.

Não há checagem por tipo no caminho (nada como o `StateGrapple::transitionFeasible` que barra 0x1A0/0x1C0).

## 3. Gancho proposto (native/runtime/wotm_custom.inc)

`registerFunction(wotm::fn::attachPickupImpaler__7MonsterGQ2t10LinkedList1ZP6Pickup8IteratorP8_fvector, &attachImpaler)`,
guardando o original com `lookupFunction`, e `g_killPickup = lookupFunction(wotm::fn::killPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori)`.

```
void attachImpaler(rdram, ctx, runtime) {
    const uint32_t m = GPR_U32(ctx, 4);            // vítima; a1 = PickupIter (uma palavra), a2 = &dir
    auto noAnim = [&](uint32_t id) { return READ32(m + 0x1CF0 + id * 0x10) == 0; };   // m_anims[id].ci
    if (ctx->pc == kAttach && g_killPickup && (noAnim(0x8E) || noAnim(0x8F) || noAnim(0x90))) {
        // killPickup(it, 1), como o ramo de recusa do retail (escrita de GPR como no resto do runtime)
        ctx->r[4] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(GPR_U32(ctx, 5))));
        ctx->r[5] = _mm_set_epi64x(0, 1);
        ctx->pc = kKillPickup;                       // salto de cauda (mesmo padrão do loadMechBossSoundBank)
        g_killPickup(rdram, ctx, runtime);
        return;
    }
    g_origAttach(rdram, ctx, runtime);
}
```

- Condição genérica (espelha o `StateImpaled::transitionOK`), não por tipo: cobre ASSBOSS0/1 e futuros monstros custom sem as
  animações. Monstros comuns têm as três e seguem o retail.
- Efeito na vítima: não entra em `StateImpaled`, não larga o que segura, não perde o agarrão; recebe o `dispatchHit` normal do
  `DetonateWeapon` (dano `m_dyn.damage`, pelo `takeHit` já tratado no `wotm_custom.inc`) = golpe comum de objeto arremessado.
- Efeito no pickup/atacante: o pickup some (como o `killPickup` que o `DetonateWeapon` faria sem impale); o lançador ainda ganha a
  stamina (`creditStamina`, que roda depois). Único resíduo: `playWeaponExplosion` recebe `impaled = 1` (som de impacto de
  empalar), cosmético.
- Conferir no jogo: CPU com poste lança dardo no Goliath; o log do `[custom]` deve mostrar a recusa e o Goliath segue jogável.
