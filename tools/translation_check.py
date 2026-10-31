#!/usr/bin/env python3
# ABOUTME: Proves a translation commit only changed comments and string literals: compares each changed file with comments and
# ABOUTME: literals stripped, lists the literals that changed, and flags changed text that a log matcher (Expect, -match) still looks for.
"""Usage:
  python tools/translation_check.py [--base REV] [--head REV] [--strings] [--extra DIR]... [paths...]

Compares every file that differs between --base (default HEAD) and --head (default: the working tree), or only the given paths.
For each file: "same code" when the file with comments and string literals stripped is token-for-token identical, otherwise the
first differing line on each side. A changed literal must keep its printf conversions (in order), escapes and format fields
({0}, $var); "PLACEHOLDERS CHANGED" otherwise. Exit status 1 if any file fails either check.

  --strings  also print every changed string literal (old -> new), for review
  --extra    another folder whose scripts match log text (e.g. the coord repo's scripts/); searched together with native/ and tools/

Matcher check (always on): a changed literal is flagged when a string literal anywhere in native/, tools/, the root .bat files or
--extra still contains a fixed piece of its old text that the new text lacks (a PowerShell -Expect regex, a grep pattern), or, for
a pattern in a script, when the text it used to match still exists in a literal elsewhere. Each flag needs a look: change the
matcher in the same commit, or leave the string alone.

Languages: C/C++/C# (.c .cpp .h .hpp .inc .cs .rc), Python, PowerShell, sh, batch (.bat .cmd, where echo text counts as a
literal), CMake. Other files are reported as "not checked".
"""
import argparse
import io
import re
import subprocess
import sys
import tokenize
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANGS = {'.c': 'c', '.cc': 'c', '.cpp': 'c', '.h': 'c', '.hpp': 'c', '.inc': 'c', '.cs': 'c', '.rc': 'c', '.py': 'py',
         '.ps1': 'ps1', '.sh': 'sh', '.bat': 'bat', '.cmd': 'bat', '.cmake': 'cmake'}
SCRIPT_LANGS = ('py', 'ps1', 'sh', 'bat', 'cmake')
CODE_TOKEN = re.compile(r'\w+|[^\w\s]')


def lang_of(path):
    p = Path(path)
    if p.name == 'CMakeLists.txt':
        return 'cmake'
    if p.parent.name == 'githooks' and not p.suffix:
        return 'sh'
    return LANGS.get(p.suffix.lower())


class Out:
    """Collects code text and string literals in order; adjacent literals merge into one (so "a" "b" == "ab" for pairing)."""

    def __init__(self):
        self.segs = []

    def code(self, text):
        if text:
            if self.segs and self.segs[-1][0] == 'c':
                self.segs[-1][1] += text
            else:
                self.segs.append(['c', text])

    def string(self, text):
        if self.segs and self.segs[-1][0] == 's':
            self.segs[-1][1] += text
        elif len(self.segs) >= 2 and self.segs[-1][0] == 'c' and not self.segs[-1][1].strip() and self.segs[-2][0] == 's':
            self.segs.pop()
            self.segs[-1][1] += ' ' + text
        else:
            self.segs.append(['s', text])

    def result(self):
        """(code tokens with one '<STR>' per literal, [(line, literal)])"""
        tokens, strings, line = [], [], 1
        for kind, text in self.segs:
            if kind == 'c':
                tokens += [(t.group(), line + text.count('\n', 0, t.start())) for t in CODE_TOKEN.finditer(text)]
            else:
                tokens.append(('<STR>', line))
                strings.append((line, text))
            line += text.count('\n')
        return tokens, strings


def scan_c(text, out, i=0, stop=None):
    """C, C++, C# and .rc. Returns the index just past `stop` (a closing brace of a C# interpolation hole) or the end."""
    n = len(text)
    start = i
    while i < n:
        ch = text[i]
        if stop and ch == stop:
            out.code(text[start:i])
            return i
        if text.startswith('//', i):
            out.code(text[start:i])
            j = text.find('\n', i)
            i = start = n if j < 0 else j
        elif text.startswith('/*', i):
            out.code(text[start:i])
            j = text.find('*/', i + 2)
            end = n if j < 0 else j + 2
            out.code('\n' * text.count('\n', i, end) or ' ')
            i = start = end
        elif ch == 'R' and re.match(r'R"([^(\s]*)\(', text[i:]) and not (i and (text[i - 1].isalnum() or text[i - 1] == '_')):
            out.code(text[start:i])
            delim = re.match(r'R"([^(\s]*)\(', text[i:]).group(1)
            j = text.find(')' + delim + '"', i)
            end = n if j < 0 else j + len(delim) + 2
            out.string(text[i:end])
            i = start = end
        elif ch == '"' or (ch in '@$' and text[i + 1:i + 2] in ('"', '@', '$') and '"' in text[i + 1:i + 3]):
            out.code(text[start:i])
            i = start = scan_c_string(text, out, i)
        elif ch == "'":
            j = i + 1
            while j < n and text[j] != "'" and text[j] != '\n':
                j += 2 if text[j] == '\\' else 1
            i = j + 1
        else:
            i += 1
    out.code(text[start:i])
    return i


def scan_c_string(text, out, i):
    """A "..." literal, C# @"..." / $"..." / $@"..."; C# interpolation holes {...} are code."""
    n = len(text)
    prefix = ''
    while text[i] in '@$':
        prefix += text[i]
        i += 1
    verbatim, interp = '@' in prefix, '$' in prefix
    piece = prefix + '"'
    i += 1
    while i < n:
        ch = text[i]
        if ch == '\\' and not verbatim:
            piece += text[i:i + 2]
            i += 2
        elif ch == '"' and verbatim and text[i + 1:i + 2] == '"':
            piece += '""'
            i += 2
        elif ch == '"':
            out.string(piece + '"')
            return i + 1
        elif ch == '\n' and not verbatim:
            break
        elif interp and ch == '{' and text[i + 1:i + 2] == '{':
            piece += '{{'
            i += 2
        elif interp and ch == '{':
            out.string(piece + '{')
            out.code('{')
            i = scan_c(text, out, i + 1, '}')
            out.code('}')
            piece = '}'
            i += 1
        else:
            piece += ch
            i += 1
    out.string(piece)
    return i


def scan_py(text, out):
    toks = tokenize.generate_tokens(io.StringIO(text).readline)
    last_row = 1
    for tok in toks:
        kind = tok.type
        gap = '\n' * (tok.start[0] - last_row)
        last_row = tok.end[0]
        if kind in (tokenize.COMMENT, tokenize.NL, tokenize.ENDMARKER):
            out.code(gap)
        elif kind == tokenize.STRING or tokenize.tok_name.get(kind) == 'FSTRING_MIDDLE':
            out.code(gap)
            out.string(tok.string)
        elif kind == tokenize.NEWLINE:
            out.code(gap + ' NEWLINE ')
        elif kind == tokenize.INDENT:
            out.code(gap + ' INDENT ')
        elif kind == tokenize.DEDENT:
            out.code(gap + ' DEDENT ')
        else:
            out.code(gap + ' ' + tok.string + ' ')


def at_word_start(text, i, extra):
    return i == 0 or text[i - 1].isspace() or text[i - 1] in extra


def scan_sh(text, out, i=0, stop=None, cmake=False):
    """sh (and CMake): # comments, '...' and "..." literals, $(...) inside "..." is code, heredoc bodies are literals."""
    n = len(text)
    start = i
    depth = 0
    heredocs = []
    while i < n:
        ch = text[i]
        if stop and ch == '(':
            depth += 1
        elif stop and ch == ')':
            if depth == 0:
                out.code(text[start:i])
                return i
            depth -= 1
        if ch == '#' and (cmake or at_word_start(text, i, ';&|(')):
            out.code(text[start:i])
            j = text.find('\n', i)
            i = start = n if j < 0 else j
        elif ch == "'" and not cmake:
            out.code(text[start:i])
            j = text.find("'", i + 1)
            end = n if j < 0 else j + 1
            out.string(text[i:end])
            i = start = end
        elif ch == '"':
            out.code(text[start:i])
            i = start = scan_sh_dquote(text, out, i, cmake)
        elif ch == '\\':
            i += 2
        elif text.startswith('<<', i) and not cmake:
            m = re.match(r'<<-?\s*([\'"]?)(\w+)\1', text[i:])
            if m:
                heredocs.append(m.group(2))
                i += m.end()
            else:
                i += 2
        elif ch == '\n' and heredocs:
            out.code(text[start:i + 1])
            i += 1
            for delim in heredocs:
                m = re.compile(r'^\t*' + re.escape(delim) + r'[ \t]*$', re.M).search(text, i)
                end = n if not m else m.start()
                out.string(text[i:end])
                i = end
            heredocs = []
            start = i
        else:
            i += 1
    out.code(text[start:i])
    return i


def scan_sh_dquote(text, out, i, cmake):
    n = len(text)
    piece = '"'
    i += 1
    while i < n:
        ch = text[i]
        if ch == '\\':
            piece += text[i:i + 2]
            i += 2
        elif ch == '"':
            out.string(piece + '"')
            return i + 1
        elif text.startswith('$(', i) and not cmake:
            out.string(piece + '$(')
            out.code('$(')
            i = scan_sh(text, out, i + 2, ')')
            out.code(')')
            piece = ')'
            i += 1
        else:
            piece += ch
            i += 1
    out.string(piece)
    return i


def scan_ps1(text, out, i=0, stop=None):
    """PowerShell: # and <# #> comments, '...', "..." (its $(...) is code) and @"..."@ / @'...'@ here-strings."""
    n = len(text)
    start = i
    depth = 0
    while i < n:
        ch = text[i]
        if stop and ch == '(':
            depth += 1
        elif stop and ch == ')':
            if depth == 0:
                out.code(text[start:i])
                return i
            depth -= 1
        if text.startswith('<#', i):
            out.code(text[start:i])
            j = text.find('#>', i + 2)
            end = n if j < 0 else j + 2
            out.code('\n' * text.count('\n', i, end) or ' ')
            i = start = end
        elif ch == '#' and at_word_start(text, i, ';(){|'):
            out.code(text[start:i])
            j = text.find('\n', i)
            i = start = n if j < 0 else j
        elif text.startswith('@"', i) or text.startswith("@'", i):
            out.code(text[start:i])
            close = '\n' + text[i + 1] + '@'
            j = text.find(close, i + 2)
            end = n if j < 0 else j + 3
            out.string(text[i:end])
            i = start = end
        elif ch == "'":
            out.code(text[start:i])
            j = i + 1
            while j < n and not (text[j] == "'" and text[j + 1:j + 2] != "'"):
                j += 2 if text[j] == "'" else 1
            out.string(text[i:j + 1])
            i = start = j + 1
        elif ch == '"':
            out.code(text[start:i])
            i = start = scan_ps1_dquote(text, out, i)
        elif ch == '`':
            i += 2
        else:
            i += 1
    out.code(text[start:i])
    return i


def scan_ps1_dquote(text, out, i):
    n = len(text)
    piece = '"'
    i += 1
    while i < n:
        ch = text[i]
        if ch == '`':
            piece += text[i:i + 2]
            i += 2
        elif ch == '"' and text[i + 1:i + 2] == '"':
            piece += '""'
            i += 2
        elif ch == '"':
            out.string(piece + '"')
            return i + 1
        elif text.startswith('$(', i):
            out.string(piece + '$(')
            out.code('$(')
            i = scan_ps1(text, out, i + 2, ')')
            out.code(')')
            piece = ')'
            i += 1
        else:
            piece += ch
            i += 1
    out.string(piece)
    return i


BAT_ECHO = re.compile(r'(^|[\s(&|@])echo(?=[\s.:])(?![ \t]+(off|on)\s*$)', re.I)


def scan_bat(text, out):
    """Batch: rem/:: lines are comments; the text after echo and "..." are literals."""
    for line in text.splitlines(keepends=True):
        body = line.strip().lstrip('@').lower()
        if body.startswith('::') or body == 'rem' or body.startswith('rem ') or body.startswith('rem\t'):
            out.code('\n' if line.endswith('\n') else '')
            continue
        i = 0
        while i < len(line):
            m = BAT_ECHO.search(line, i)
            q = line.find('"', i)
            if m and (q < 0 or m.end() <= q):
                out.code(line[i:m.end()])
                j = m.end()
                while j < len(line) and line[j] not in '&|<>)\r\n':
                    j += 1
                if line[m.end():j].strip():
                    out.string(line[m.end():j])
                i = j
            elif q >= 0:
                out.code(line[i:q])
                j = line.find('"', q + 1)
                end = len(line.rstrip('\r\n')) if j < 0 else j + 1
                out.string(line[q:end])
                i = end
            else:
                out.code(line[i:])
                i = len(line)


def split(text, lang):
    out = Out()
    if lang == 'c':
        scan_c(text, out)
    elif lang == 'py':
        scan_py(text, out)
    elif lang == 'ps1':
        scan_ps1(text, out)
    elif lang == 'sh':
        scan_sh(text, out)
    elif lang == 'cmake':
        scan_sh(text, out, cmake=True)
    elif lang == 'bat':
        scan_bat(text, out)
    else:
        raise ValueError(lang)
    return out.result()


def compare(old_text, new_text, lang):
    """(first difference as (old line, new line) or None, [(new line, old literal, new literal)] for changed literals)"""
    old_tokens, old_strings = split(old_text, lang)
    new_tokens, new_strings = split(new_text, lang)
    for k, (a, b) in enumerate(zip(old_tokens, new_tokens)):
        if a[0] != b[0]:
            return (a[1], b[1]), []
    if len(old_tokens) != len(new_tokens):
        k = min(len(old_tokens), len(new_tokens))
        last = lambda toks: toks[k][1] if k < len(toks) else (toks[-1][1] if toks else 1)
        return (last(old_tokens), last(new_tokens)), []
    changed = [(nl, o, s) for (ol, o), (nl, s) in zip(old_strings, new_strings) if o != s]
    return None, changed


PRINTF = re.compile(r'%(?:%|[-+ #0]*(?:\*|\d+)?(?:\.(?:\*|\d+))?(?:hh|h|ll|l|L|z|j|t|I64|I32)?[diouxXeEfFgGaAcspn]|\(\w+\)[-+ #0]*\d*(?:\.\d+)?[diouxXeEfFgGcrs])')
ESCAPE = re.compile(r'\\[ntr0abfv]|`[ntr0]')
FIELD = re.compile(r"\{[^{}\s]*\}|\$\{?[A-Za-z_]\w*(?::\w+)?\}?|\$\(")


def placeholders(literal, lang):
    """What a translation must keep: printf conversions in order (C, Python), and as a multiset the escape sequences and
    format/interpolation fields ({0}, {name}, $var)."""
    ordered = PRINTF.findall(literal) if lang in ('c', 'py', 'sh') else []
    return ordered, sorted(ESCAPE.findall(literal) + FIELD.findall(literal))


def fixed_pieces(pattern, min_len=5):
    """The fixed text runs of a regex (or plain string), lowercased: '\\[sync\\] seed \\d+' -> ['[sync] seed']."""
    pieces, cur, i, n = [], '', 0, len(pattern)
    while i < n:
        ch = pattern[i]
        if ch == '\\' and i + 1 < n:
            nxt = pattern[i + 1]
            if nxt.isalnum():
                pieces.append(cur)
                cur = ''
            else:
                cur += nxt
            i += 2
        elif ch in '[{':
            pieces.append(cur)
            cur = ''
            close = ']' if ch == '[' else '}'
            j = pattern.find(close, i + 1)
            i = n if j < 0 else j + 1
        elif ch in '.*+?()|^$`%':
            pieces.append(cur)
            cur = ''
            i += 1
        else:
            cur += ch
            i += 1
    pieces.append(cur)
    return [p.strip().lower() for p in pieces if len(p.strip()) >= min_len and re.search(r'[A-Za-z]', p)]


def literal_body(raw):
    """A literal without its quotes/prefixes, close enough for substring searches."""
    s = raw
    s = re.sub(r'^[rRbBuUfF@$]*("""|\'\'\'|"|\')', '', s)
    s = re.sub(r'("""|\'\'\'|"|\')$', '', s)
    return s


def corpus_files(extra_dirs):
    files = []
    for root in [REPO / 'native', REPO / 'tools'] + [Path(d) for d in extra_dirs]:
        files += [p for p in root.rglob('*') if p.is_file() and lang_of(p) and 'build' not in p.relative_to(root).parts[:-1]]
    files += [p for p in REPO.glob('*.bat')]
    own = ('translation_check.py', 'test_translation_check.py')   # their sample Portuguese is not a matcher
    return [p for p in files if p.name not in own]


def corpus_entry(path, lang, body):
    """One corpus literal, with what matcher_flags needs computed once."""
    is_matcher = lang in SCRIPT_LANGS or Path(path).suffix == '.cs'
    return (str(Path(path).resolve()), rel(path), lang, body.lower(), fixed_pieces(body) if is_matcher else [])


def load_corpus(extra_dirs):
    """corpus_entry for every literal in the current tree's code and scripts."""
    corpus = []
    for p in corpus_files(extra_dirs):
        try:
            _, strings = split(p.read_text(encoding='utf-8', errors='replace'), lang_of(p))
        except (tokenize.TokenError, SyntaxError, IndentationError):
            continue
        corpus += [corpus_entry(p, lang_of(p), literal_body(s)) for _, s in strings]
    return corpus


def matcher_flags(path, lang, old, new, corpus):
    """Reasons this literal change may break a matcher elsewhere."""
    flags = set()
    me = str(Path(path).resolve())
    old_l, new_l = literal_body(old).lower(), literal_body(new).lower()
    old_pieces = [p for p in fixed_pieces(literal_body(old), 6) if p not in new_l] if lang in SCRIPT_LANGS else []
    for where, name, _, body, pieces in corpus:
        if where == me:
            continue
        for piece in pieces:
            if piece in old_l and piece not in new_l:
                flags.add('%s still looks for "%s"' % (name, piece))
        for piece in old_pieces:
            if piece in body:
                flags.add('%s still has "%s" (the old text matched it)' % (name, piece))
    return sorted(flags)


def rel(p):
    try:
        return str(Path(p).resolve().relative_to(REPO))
    except ValueError:
        return str(p)


def git(*args):
    return subprocess.run(['git', '-C', str(REPO)] + list(args), capture_output=True, text=True, encoding='utf-8',
                          errors='replace', check=True).stdout


def read_file(f):
    return f.read_text(encoding='utf-8', errors='replace') if f.is_file() else None


def read_at(rev, path):
    if rev is None:
        return read_file(REPO / path)
    r = subprocess.run(['git', '-C', str(REPO), 'show', '%s:%s' % (rev, path)], capture_output=True)
    return r.stdout.decode('utf-8', errors='replace') if r.returncode == 0 else None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog='\n'.join(__doc__.splitlines()[3:]))
    ap.add_argument('paths', nargs='*')
    ap.add_argument('--base', default='HEAD')
    ap.add_argument('--head', default=None, help='a revision (default: the working tree)')
    ap.add_argument('--strings', action='store_true')
    ap.add_argument('--extra', action='append', default=[])
    ap.add_argument('--before', help='with --after: compare DIR/<path> to the --after folder instead of git revisions, for '
                                     'files outside this repo such as the PS2Recomp clone (paths are relative to both)')
    ap.add_argument('--after')
    args = ap.parse_args(argv)

    if bool(args.before) != bool(args.after) or (args.before and not args.paths):
        ap.error('--before and --after go together, with the paths to compare')
    if args.before:
        paths = args.paths
        read_old = lambda p: read_file(Path(args.before) / p)
        read_new = lambda p: read_file(Path(args.after) / p)
    else:
        if args.paths:
            paths = [rel(Path(p)) for p in args.paths]
        else:
            rng = [args.base] + ([args.head] if args.head else [])
            paths = [p for p in git('diff', '--name-only', *rng).splitlines() if p]
        read_old = lambda p: read_at(args.base, p)
        read_new = lambda p: read_at(args.head, p)
    if args.head:
        corpus_note = 'matcher check uses the working tree, not --head'
        print(corpus_note)
    corpus = load_corpus(args.extra)
    bad = 0
    nchanged = 0
    for path in paths:
        lang = lang_of(path)
        old, new = read_old(path), read_new(path)
        if lang is None or old is None or new is None:
            print('%-60s not checked (%s)' % (path, 'no language' if lang is None else 'added or removed'))
            continue
        try:
            diff, changed = compare(old, new, lang)
        except (tokenize.TokenError, SyntaxError, IndentationError) as e:
            print('%-60s could not tokenize: %s' % (path, e))
            bad += 1
            continue
        if diff:
            print('%-60s CODE CHANGED: first difference at old line %d, new line %d' % (path, diff[0], diff[1]))
            bad += 1
            continue
        nchanged += len(changed)
        broken = [(line, o, s) for line, o, s in changed if placeholders(o, lang) != placeholders(s, lang)]
        print('%-60s %s, %d literals changed' % (path, 'PLACEHOLDERS CHANGED' if broken else 'same code', len(changed)))
        bad += 1 if broken else 0
        for line, o, s in changed:
            flags = matcher_flags(REPO / path, lang, o, s, corpus)
            if (line, o, s) in broken:
                flags.insert(0, 'PLACEHOLDERS: %s -> %s' % (placeholders(o, lang), placeholders(s, lang)))
            if args.strings or flags:
                print('  %d: %s\n     -> %s' % (line, o, s))
            for f in flags:
                print('     %s' % (f if f.startswith('PLACEHOLDERS') else 'MATCHER: ' + f))
    print('%d files, %d failing, %d literals changed' % (len(paths), bad, nchanged))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
