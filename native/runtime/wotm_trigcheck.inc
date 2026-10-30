// ABOUTME: WOTM_TRIGCHECK=1: na primeira chamada do sinf o jogo roda o seu sinf/cosf (reducao, Payne-Hanek e nucleos, como o runtime os
// ABOUTME: liga) numa tabela de valores do retail e sai com 0 se todos derem os mesmos bits. Incluido pelo wotm_overrides.cpp.
//
// Os valores esperados sao do sinf/cosf do retail (0x22D5C8/0x22D4C0) rodados no unicorn com o cvt.w.s truncando como o R5900. O mesmo
// resultado em qualquer compilador e CPU e o que deixa Windows e Mac jogarem juntos: o seno e o cosseno do jogo nao podem vir da libm do
// sistema (a da Microsoft e a da Apple diferem no ultimo bit).

namespace
{
    struct TrigGolden
    {
        uint32_t x, sin, cos;
    };
    const TrigGolden kTrigGoldens[] = {
        {0x00000000u, 0x00000000u, 0x3F800000u},
        {0x80000000u, 0x80000000u, 0x3F800000u}, {0x0DA24260u, 0x0DA24260u, 0x3F800000u}, {0x3F000000u, 0x3EF57744u, 0x3F60A940u},
        {0x3F490FDAu, 0x3F3504F3u, 0x3F3504F4u}, {0x3F490FDBu, 0x3F3504F4u, 0x3F3504F3u}, {0x3F800000u, 0x3F576AA5u, 0x3F0A5140u},
        {0x3FC90FDBu, 0x3F800000u, 0xB33BBD2Eu}, {0x4016CBE4u, 0x3F3504F3u, 0xBF3504F3u}, {0x40490FDBu, 0xB3BBBD2Eu, 0xBF800000u},
        {0xC0490FDBu, 0x33BBBD2Eu, 0xBF800000u}, {0x4096CBE4u, 0xBF800000u, 0x324CDE2Eu}, {0x40C90FDBu, 0x343BBD2Eu, 0x3F800000u},
        {0x42C80000u, 0xBF01A12Eu, 0x3F5CC0EEu}, {0x43490000u, 0xBD7D80A2u, 0x3F7F825Du}, {0x43490F5Cu, 0xBAFD4438u, 0x3F7FFFE1u},
        {0x434911ECu, 0x3C045719u, 0x3F7FFDDDu}, {0x437A0000u, 0xBF787486u, 0x3E76C5A3u}, {0xC47A0000u, 0xBF53AE60u, 0x3F0FF813u},
        {0x4640E6B6u, 0xBF344B08u, 0x3F35BE20u}, {0x49742400u, 0xBEB33259u, 0x3F6FCEFCu}, {0x4BE4E1C0u, 0x3F76D13Eu, 0xBE87E65Du},
        {0xCFFD51DAu, 0x3E6EB416u, 0xBF78F287u}, {0x60AD78ECu, 0x3F281569u, 0x3F411723u}, {0x7F61B1E6u, 0x3F5FF9C4u, 0xBEF7F57Cu},
        {0x3F226EF8u, 0x3F17C066u, 0x3F4E2C63u}, {0x3F57E648u, 0x3F3F3387u, 0x3F2A3ABFu}, {0x40A3BEC9u, 0xBF6B5353u, 0x3EC992B0u},
        {0xC0CF40E8u, 0xBE44E5C8u, 0x3F7B3912u}, {0x408360ECu, 0xBF524C07u, 0xBF11FBC6u}, {0x3FA3567Bu, 0x3F74F653u, 0x3E94B8B2u},
        {0xC0554A53u, 0x3E427752u, 0xBF7B575Du}, {0xC04E0A9Cu, 0x3D9F2F07u, 0xBF7F39BCu}, {0xC00FFD9Cu, 0xBF4735CFu, 0xBF20C88Cu},
        {0xBF9CBF51u, 0xBF70CF81u, 0x3EADBD11u}, {0x40D6D7E7u, 0x3ED5BFE5u, 0x3F689FA9u}, {0xC09DA7DAu, 0x3F7A2431u, 0x3E59D173u},
        {0xC0BAD2FFu, 0x3EDC5C92u, 0x3F67137Fu}, {0xC0415E55u, 0xBDF59906u, 0xBF7E270Fu}, {0x401B4C69u, 0x3F27D936u, 0xBF414B79u},
        {0x40BCA73Eu, 0xBEC19997u, 0x3F6CFE41u}, {0xBF52AFD1u, 0xBF3BB213u, 0x3F2E1654u}, {0xBF95C937u, 0xBF6BBB81u, 0x3EC7A973u},
        {0xC01DA4E5u, 0xBF20A729u, 0xBF4750BDu}, {0x40D5B48Du, 0x3EC5123Bu, 0x3F6C46DDu}, {0xBE62E739u, 0xBE610D0Bu, 0x3F79BDAAu},
        {0xBFBFA131u, 0xBF7F4E25u, 0x3D96C7A5u}, {0x40764F24u, 0xBF2648ACu, 0xBF42A476u}, {0xC08D6B14u, 0x3F7515AFu, 0xBE93E97Bu},
        {0x3FCB11B8u, 0x3F7FF7F1u, 0xBC8075FEu}, {0xC02FEA81u, 0xBEC407FAu, 0xBF6C7E36u}, {0x40B689CAu, 0xBF0C0D13u, 0x3F564B22u},
        {0xBFDD72E7u, 0xBF7CC281u, 0xBE226815u}, {0x3FBF7A6Fu, 0x3F7F4864u, 0x3D993210u}, {0x40AD346Bu, 0xBF43C1BBu, 0x3F24F877u},
        {0x42A18315u, 0xBF4C81C4u, 0x3F19FD73u}, {0xC3948691u, 0xBF7C4675u, 0xBE2E0B57u}, {0x4378C848u, 0xBF0FD071u, 0xBF53C950u},
        {0xC33F9D02u, 0xBCC34795u, 0xBF7FED61u}, {0xC30D3DA7u, 0xBE059921u, 0xBF7DCFD7u}, {0x4227465Cu, 0xBF545292u, 0xBF0F057Fu},
        {0xC332D8E4u, 0xBE6303FBu, 0xBF79A135u}, {0xC35E56ABu, 0xBF27BF5Cu, 0xBF4161E8u}, {0x42F42A24u, 0x3ED9FDEBu, 0xBF67A32Du},
        {0xC36F94ECu, 0xBF3B4AF6u, 0x3F2E853Fu}, {0xC13972AAu, 0x3F54047Cu, 0x3F0F7927u}, {0xC380292Fu, 0x3F75E460u, 0x3E8E737Fu},
        {0x437D1C52u, 0x3F7A3F05u, 0xBE57E231u}, {0xC27DBFA0u, 0xBF11A986u, 0x3F528508u}, {0x425563D6u, 0x3D73E43Du, 0xBF7F8BB8u},
        {0x437254DEu, 0xBED4EA65u, 0xBF68D098u}, {0xC31F0CF3u, 0xBF6BC6BAu, 0xBEC7746Bu}, {0xC378A2FDu, 0x3EDF187Fu, 0xBF666B5Bu},
        {0x430EA35Du, 0xBF743C87u, 0xBE996BFEu}, {0x4380A344u, 0xBEA85EF1u, 0x3F71C30Bu}, {0x4ECD523Cu, 0xBF7F5A36u, 0xBD919509u},
        {0x47947B55u, 0x3F2BF779u, 0xBF3DA3EDu}, {0xD358194Fu, 0x3F5679E7u, 0x3F0BC568u}, {0xC5E7C928u, 0xBE1ED14Au, 0xBF7CE6FCu},
        {0xCD4D07F7u, 0xBF7F73F5u, 0xBD85D0B5u}, {0xD2022A34u, 0xBF56F96Fu, 0x3F0B00F2u}, {0xC77D0939u, 0x3F298501u, 0xBF3FD4BAu},
        {0x48700801u, 0x3DB73DF3u, 0x3F7EF926u}, {0x4E1AEE62u, 0xBF7FEC66u, 0x3CC85A1Fu}, {0x450E3E17u, 0x3F7ABAD4u, 0x3E4EB5D6u},
    };

    PS2Runtime::RecompiledFunction g_origSinfForCheck = nullptr;

    // Funcao float de um argumento do jogo ($f12 -> $f0) chamada de dentro de um gancho; o contexto volta como estava.
    uint32_t trigGuestCall(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, PS2Runtime::RecompiledFunction f, uint32_t fn, uint32_t xBits)
    {
        const R5900Context saved = *ctx;
        std::memcpy(&ctx->f[12], &xBits, sizeof xBits);
        ctx->r[31] = _mm_set_epi64x(0, 0);
        ctx->pc = fn;
        f(rdram, ctx, rt);
        uint32_t out;
        std::memcpy(&out, &ctx->f[0], sizeof out);
        *ctx = saved;
        return out;
    }

    void sinfTrigCheck(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        const PS2Runtime::RecompiledFunction cosf = runtime->lookupFunction(wotm::fn::cosf);
        int bad = 0;
        for (const TrigGolden &g : kTrigGoldens)
        {
            const uint32_t s = trigGuestCall(rdram, ctx, runtime, g_origSinfForCheck, wotm::fn::sinf, g.x);
            const uint32_t c = trigGuestCall(rdram, ctx, runtime, cosf, wotm::fn::cosf, g.x);
            if (s != g.sin || c != g.cos)
            {
                ++bad;
                std::fprintf(stderr, "[trigcheck] x=%08X: sin %08X (retail %08X), cos %08X (retail %08X)\n", g.x, s, g.sin, c, g.cos);
            }
        }
        const int total = int(sizeof kTrigGoldens / sizeof kTrigGoldens[0]);
        std::fprintf(stderr, "[trigcheck] %d de %d valores iguais ao retail\n", total - bad, total);
        std::fflush(stderr);
        std::exit(bad == 0 ? 0 : 1);
    }
}

void wotmTrigCheckInstall(PS2Runtime &runtime)
{
    if (!std::getenv("WOTM_TRIGCHECK") || !(g_origSinfForCheck = runtime.lookupFunction(wotm::fn::sinf)))
        return;
    runtime.registerFunction(wotm::fn::sinf, &sinfTrigCheck);
    std::fprintf(stderr, "[trigcheck] seno e cosseno do jogo contra o retail na primeira chamada do sinf (WOTM_TRIGCHECK)\n");
}
