#!/usr/bin/env python3
"""Hook PreToolUse (Bash): recusa Python inline em heredoc ou em `python -c` com barra invertida.

Motivo: no Windows, o corpo de um heredoc/`-c` passa por aspas, `\\n`, `\\U` de caminhos (C:\\Users\\...) e vira lixo
(quebra de linha real, erro de escape unicode). Aconteceu varias vezes. Regra: grave o script com a ferramenta Write
(na pasta de rascunho) e rode `python arquivo.py`. Para editar arquivos do repo, use Edit/Write.

Saida: exit 2 + mensagem em stderr = bloqueia a chamada e devolve a mensagem ao modelo.
"""
import json
import re
import sys

try:
    cmd = (json.load(sys.stdin).get("tool_input") or {}).get("command") or ""
except Exception:
    sys.exit(0)  # nao sabe ler a entrada: nao atrapalha

PY = r"\bpython3?(?:\.exe)?\b"
HEREDOC = re.compile(PY + r"[^\n]*<<-?\s*['\"]?\w+")          # python ... <<EOF / <<'EOF' / python - <<EOF
DASH_C_BACKSLASH = re.compile(PY + r"\s+-c\s+[\"'][^\n]*\\")   # python -c "...\..." (barra invertida no corpo)

if HEREDOC.search(cmd) or DASH_C_BACKSLASH.search(cmd):
    sys.stderr.write(
        "Bloqueado: Python inline (heredoc ou `python -c` com barra invertida) corrompe `\\n`, `\\U` e caminhos do Windows. "
        "Grave o script com a ferramenta Write (na pasta de rascunho) e rode `python <arquivo>.py`; "
        "para alterar arquivos do repo use Edit/Write. (hook: .claude/hooks/block_python_heredoc.py)\n"
    )
    sys.exit(2)
sys.exit(0)
