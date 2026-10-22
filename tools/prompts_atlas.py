"""Monta o atlas dos icones de botao (Kenney "Input Prompts", CC0) que o port desenha no lugar dos icones 3D do jogo.

Uso: python tools/prompts_atlas.py <pasta do pacote extraido, a que tem "Xbox Series/">

Grava native/assets/prompts.png (celulas de 128x128, 16 por linha) e native/wotm_prompts.h (indice da celula de cada botao do
gamepad por estilo, na ordem do GamepadButton do raylib 1..17 mais os analogicos em 18 e 19, e de cada tecla pelo nome de
wotm_settings.hpp, com Esc, F1..F12 e o mouse para a legenda do modo foto). O PNG vai embutido
nos executaveis pelo native/wotm.rc. Rodar de novo so quando mudar a lista abaixo ou o pacote.
"""
import os
import sys

from PIL import Image

CELL = 128
COLS = 16

# GamepadButton do raylib 1..17 (wotm_settings.hpp, kPadButtonNames)
PAD = ["DPAD_UP", "DPAD_RIGHT", "DPAD_DOWN", "DPAD_LEFT", "FACE_UP", "FACE_RIGHT", "FACE_DOWN", "FACE_LEFT",
       "L1", "L2", "R1", "R2", "BACK", "GUIDE", "START", "L3", "R3"]
X, P, S = "Xbox Series", "PlayStation Series", "Nintendo Switch"
STYLES = {
    "Xbox": [X + "/xbox_dpad_up", X + "/xbox_dpad_right", X + "/xbox_dpad_down", X + "/xbox_dpad_left",
             X + "/xbox_button_color_y", X + "/xbox_button_color_b", X + "/xbox_button_color_a", X + "/xbox_button_color_x",
             X + "/xbox_lb", X + "/xbox_lt", X + "/xbox_rb", X + "/xbox_rt",
             X + "/xbox_button_view", X + "/xbox_guide", X + "/xbox_button_menu", X + "/xbox_ls", X + "/xbox_rs"],
    "Ps": [P + "/playstation_dpad_up", P + "/playstation_dpad_right", P + "/playstation_dpad_down", P + "/playstation_dpad_left",
           P + "/playstation_button_color_triangle", P + "/playstation_button_color_circle", P + "/playstation_button_color_cross",
           P + "/playstation_button_color_square",
           P + "/playstation_trigger_l1", P + "/playstation_trigger_l2", P + "/playstation_trigger_r1", P + "/playstation_trigger_r2",
           P + "/playstation3_button_select", None, P + "/playstation3_button_start", P + "/playstation_button_l3",
           P + "/playstation_button_r3"],
    # Nintendo pela posicao: A a direita, B embaixo, X em cima, Y a esquerda
    "Nintendo": [S + "/switch_dpad_up", S + "/switch_dpad_right", S + "/switch_dpad_down", S + "/switch_dpad_left",
                 S + "/switch_button_x", S + "/switch_button_a", S + "/switch_button_b", S + "/switch_button_y",
                 S + "/switch_button_l", S + "/switch_button_zl", S + "/switch_button_r", S + "/switch_button_zr",
                 S + "/switch_button_minus", S + "/switch_button_home", S + "/switch_button_plus", S + "/switch_stick_l_press",
                 S + "/switch_stick_r_press"],
}
# Analogicos (mover), botoes virtuais 18 e 19 depois dos do raylib: o modo foto mostra "analogico esquerdo/direito" na legenda
STYLES["Xbox"] += [X + "/xbox_stick_l", X + "/xbox_stick_r"]
STYLES["Ps"] += [P + "/playstation_stick_l", P + "/playstation_stick_r"]
STYLES["Nintendo"] += [S + "/switch_stick_l", S + "/switch_stick_r"]
K = "Keyboard & Mouse/keyboard_"
KEYS = {"SPACE": "space", "APOSTROPHE": "apostrophe", "COMMA": "comma", "MINUS": "minus", "PERIOD": "period",
        "SLASH": "slash_forward", "SEMICOLON": "semicolon", "EQUAL": "equals", "LBRACKET": "bracket_open",
        "BACKSLASH": "slash_back", "RBRACKET": "bracket_close", "GRAVE": "tilde", "ENTER": "enter", "TAB": "tab",
        "BACKSPACE": "backspace", "INSERT": "insert", "DELETE": "delete", "RIGHT": "arrow_right", "LEFT": "arrow_left",
        "DOWN": "arrow_down", "UP": "arrow_up", "PGUP": "page_up", "PGDN": "page_down", "HOME": "home", "END": "end",
        "CAPSLOCK": "capslock", "KPDECIMAL": "period", "KPDIVIDE": "slash_forward", "KPMULTIPLY": "asterisk",
        "KPMINUS": "minus", "KPPLUS": "numpad_plus", "KPENTER": "numpad_enter", "LSHIFT": "shift", "RSHIFT": "shift",
        "LCTRL": "ctrl", "RCTRL": "ctrl", "LALT": "alt", "RALT": "alt"}
for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
    KEYS[c] = c.lower()
for d in "0123456789":
    KEYS[d] = d
    KEYS["KP" + d] = d
# Fora do mapa de teclas do jogo (so na legenda do modo foto): Esc, F1..F12 e o mouse
KEYS["ESC"] = "escape"
for n in range(1, 13):
    KEYS["F%d" % n] = "f%d" % n
M = "Keyboard & Mouse/"
MOUSE = {"MOUSE_LEFT": M + "mouse_left", "MOUSE_RIGHT": M + "mouse_right", "MOUSE_SCROLL": M + "mouse_scroll",
         "MOUSE_MOVE": M + "mouse_move"}


def main():
    src = sys.argv[1]
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    files, index = [], {}

    def cell(rel):
        if rel is None:
            return -1
        if rel not in index:
            base, name = rel.rsplit("/", 1)
            path = os.path.join(src, base, "Double", name + ".png")
            if not os.path.exists(path):
                sys.exit("falta " + path)
            index[rel] = len(files)
            files.append(path)
        return index[rel]

    styles = {k: [cell(r) for r in v] for k, v in STYLES.items()}
    keys = sorted([(name, cell(K + f)) for name, f in KEYS.items()] + [(name, cell(f)) for name, f in MOUSE.items()])
    rows = (len(files) + COLS - 1) // COLS
    atlas = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
    for i, path in enumerate(files):
        im = Image.open(path).convert("RGBA")
        atlas.paste(im, ((i % COLS) * CELL, (i // COLS) * CELL))
    os.makedirs(os.path.join(repo, "native", "assets"), exist_ok=True)
    atlas.save(os.path.join(repo, "native", "assets", "prompts.png"), optimize=True)

    out = ["// Gerado por tools/prompts_atlas.py (nao editar): celulas do native/assets/prompts.png, icones do Kenney \"Input Prompts\" (CC0).",
           "#pragma once", "", "namespace wotm::prompts", "{",
           f"inline constexpr int kCell = {CELL}, kCols = {COLS}, kRows = {rows};",
           "// GamepadButton do raylib (indice 0 sem icone) -> celula; -1 = sem icone no estilo. 18 e 19: analogico esquerdo e direito",
           f"inline constexpr int kPadCells = {len(PAD) + 3};"]
    for k, v in styles.items():
        out.append(f"inline constexpr int k{k}[kPadCells] = {{-1, " + ", ".join(str(c) for c in v) + "};")
    out.append("struct KeyCell { const char *name; int cell; };")
    out.append("// Tecla (nome do wotm_settings.hpp) -> celula")
    out.append("inline constexpr KeyCell kKeys[] = {")
    for n, c in keys:
        out.append(f'    {{"{n}", {c}}},')
    out += ["};", "} // namespace wotm::prompts", ""]
    with open(os.path.join(repo, "native", "wotm_prompts.h"), "w", newline="\n") as f:
        f.write("\n".join(out))
    print(f"{len(files)} icones, atlas {COLS * CELL}x{rows * CELL}")


main()
