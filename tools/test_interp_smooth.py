#!/usr/bin/env python3
# ABOUTME: Testes do tools/interp_smooth.py com logs sinteticos: imagem com o tempo do jogo atrasado de um valor constante (suave)
# ABOUTME: contra o ultimo quadro do jogo repetido ate o proximo (jogo a 60 numa tela de 144 Hz). Rodar: python tools/test_interp_smooth.py
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import interp_smooth  # noqa: E402

HZ = 144.0


def log(content_fields):
    """Uma linha por apresentacao (t em s; ct = tempo do jogo mostrado, em fields)."""
    return '\n'.join(f'[interp] t={i / HZ:.6f} a=0.500 r=1 ct={ct:.4f} cam=0.000,0.000,0.000' for i, ct in enumerate(content_fields))


class LagTest(unittest.TestCase):
    def test_interpolated_image_has_constant_lag(self):
        # o que se mostra e o jogo 1,15 field atras do relogio da tela
        cts = [i / HZ * 60.0 - 1.15 for i in range(1000)]
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
        cts = []
        for i in range(1000):
            k = i - i % 2
            cts.append(k / HZ * 60.0 - 1.15)
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertLess(spread, 8.0)

    def test_a_pause_is_a_step_not_judder(self):
        # o jogo parou 0,3 s no meio (autoteste, carga): o atraso sobe de uma vez e fica; a imagem continua suave antes e depois
        cts = [i / HZ * 60.0 - 1.15 - (18.0 if i >= 500 else 0.0) for i in range(1000)]
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(log(cts)))
        self.assertLess(spread, 0.5)

    def test_vblank_grid_from_presentation_index(self):
        # com n= e hz= o instante e o vblank (n / hz), nao o relogio da CPU (que oscila com a fila do driver)
        text = '\n'.join(f'[interp] t={i / HZ + (0.004 if i % 3 else 0.0):.6f} a=0.500 r=1 ct={i / HZ * 60.0 - 1.15:.4f} n={i} hz=144 '
                         'cam=0.000,0.000,0.000' for i in range(1000))
        spread, _ = interp_smooth.lag_spread_ms(interp_smooth.read_presentations(text))
        self.assertLess(spread, 0.5)

    def test_old_lines_without_content_time_are_ignored(self):
        text = '[interp] t=1.0 a=1.000 r=1 cam=1.000,2.000,3.000'
        self.assertEqual(interp_smooth.read_presentations(text), [])


if __name__ == '__main__':
    unittest.main()
