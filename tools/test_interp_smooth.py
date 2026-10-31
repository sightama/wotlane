#!/usr/bin/env python3
# ABOUTME: Testes do tools/interp_smooth.py com logs sinteticos no formato do wotm_live.inc: imagem com o tempo do jogo atrasado de um
# ABOUTME: valor constante (suave) contra o ultimo quadro do jogo repetido ate o proximo (jogo a 60 numa tela de 144 Hz). Rodar: python tools/test_interp_smooth.py
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import interp_smooth  # noqa: E402

HZ = 144.0


def log(content_fields):
    """Uma linha [interp] por apresentacao (t em s); o tempo do jogo mostrado (fields) vira fA, fB = fA + 1 e o alfa entre eles."""
    lines = []
    for i, ct in enumerate(content_fields):
        fa = math.floor(ct)
        lines.append(f'[interp] t={i / HZ:.6f} g={fa} fA={fa} fB={fa + 1} alpha={ct - fa:.4f}')
    return '\n'.join(lines)


class LagTest(unittest.TestCase):
    def test_interpolated_image_has_constant_lag(self):
        # o que se mostra e o jogo 1,15 field atras do relogio da tela
        cts = [10.0 + i / HZ * 60.0 - 1.15 for i in range(1000)]
        spread, n = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertEqual(n, 900)   # a primeira decima parte fica de fora (relogio assentando)
        self.assertLess(spread, 0.5)

    def test_held_frames_give_a_sawtooth_lag(self):
        # sem interpolacao: o quadro do jogo mais novo (inteiro) fica ate o proximo
        cts = [float(int(i / HZ * 60.0)) for i in range(1000)]
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertGreater(spread, 12.0)   # dente de serra de ~16,7 ms

    def test_every_other_presentation_redrawn_is_still_even(self):
        # interpolacao a 72 redesenhos/s numa tela de 144 Hz: cada imagem fica 2 vblanks, atraso oscila meio periodo da tela
        cts = [10.0 + (i - i % 2) / HZ * 60.0 - 1.15 for i in range(1000)]
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertLess(spread, 8.0)

    def test_a_pause_is_a_step_not_judder(self):
        # o jogo parou 0,3 s no meio (carga): o atraso sobe de uma vez e fica; a imagem continua suave antes e depois
        cts = [30.0 + i / HZ * 60.0 - 1.15 - (18.0 if i >= 500 else 0.0) for i in range(1000)]
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertLess(spread, 0.5)

    def test_live_lines_give_the_blended_game_time(self):
        # o tempo do jogo mostrado e fA + alfa * (fB - fA), e sem a mistura (desligada) a imagem e a de B
        text = '\n'.join([
            '[interp] t=1.0000 g=100 fA=200 fB=201 alpha=0.250 new',
            '[interp] t=1.0070 g=100 fA=200 fB=202 alpha=0.500 (P-A)',
            '[interp] t=1.0140 g=101 fA=201 fB=202 alpha=0.000 (off)',
        ])
        self.assertEqual(interp_smooth.read_presentations(text), [(1.0, 200.25), (1.007, 201.0), (1.014, 202.0)])

    def test_held_images_without_blending_show_the_sawtooth(self):
        held = []
        for i in range(1000):
            newest = int(i / HZ * 60.0)
            held.append(f'[interp] t={i / HZ:.6f} g={newest} fA={newest - 1} fB={newest} alpha=0.000 (off)')
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations('\n'.join(held)))
        self.assertGreater(spread, 12.0)

    def test_other_interp_lines_are_ignored(self):
        text = '[interp] on: 144 Hz screen\n[interp] redraw of 9.00 ms for a period of 6.94 ms: blending suspended'
        self.assertEqual(interp_smooth.read_presentations(text), [])


if __name__ == '__main__':
    unittest.main()
