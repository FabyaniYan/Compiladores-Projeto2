#!/usr/bin/env python3
"""Regressoes iniciais do projeto 2. Execute: python3 testes_regressao.py"""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent  # raiz do projeto (testes/..)
CASES = [
 ('valido', '''proc main()\nlocvars\n x is int;\nstart\n x << 1;\n echo(x);\nend\n''', True, ''),
 ('nao_declarado_atribuicao', '''proc main()\nstart\n y << 2;\nend\n''', False, 'nao declarado'),
 ('nao_declarado_expressao', '''proc main()\nlocvars\n x is int;\nstart\n x << y + 1;\nend\n''', False, 'nao declarado'),
 ('rotina_inexistente', '''proc main()\nstart\n naoExiste();\nend\n''', False, 'nao declarado'),
 ('duplicado_local', '''proc main()\nlocvars\n x is int;\n x is int;\nstart\nend\n''', False, 'ja declarado'),
 ('duplicado_global', '''globvars\n x is int;\n x is chr;\nproc main()\nstart\nend\n''', False, 'ja declarado'),
 ('global_valido', '''globvars\n g is int;\nproc main()\nstart\n g << 1;\nend\n''', True, ''),
 ('sombreamento', '''globvars\n g is int;\nproc main()\nlocvars\n g is int;\nstart\n g << 1;\nend\n''', True, ''),
 ('chamada_variavel', '''proc main()\nlocvars\n x is int;\nstart\n x();\nend\n''', False, 'nao e sub-rotina'),
 ('qtd_argumentos', '''proc teste(x:int)\nstart\nend\nproc main()\nstart\n teste();\nend\n''', False, 'exige 1 argumento'),
 ('parametro_valido', '''proc teste(x:int)\nstart\n echo(x);\nend\nproc main()\nstart\n teste(1);\nend\n''', True, ''),
 ('indice_nao_declarado', '''proc main()\nlocvars\n v is int[3];\nstart\n v[i] << 2;\nend\n''', False, 'nao declarado'),
]

def run():
    subprocess.run(['make', '-s', '-C', str(ROOT)], check=True)
    passed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, src, valid, message in CASES:
            source = pathlib.Path(tmp) / (name + '.slac')
            source.write_text(src, encoding='utf-8')
            proc = subprocess.run([str(ROOT / 'complac'), str(source)], text=True, capture_output=True)
            ok = (proc.returncode == 0) == valid and message in proc.stderr
            print(('OK  ' if ok else 'FAIL'), name, (proc.stderr.strip() if not ok else ''))
            passed += int(ok)
    print(f'{passed}/{len(CASES)} testes passaram')
    raise SystemExit(0 if passed == len(CASES) else 1)

if __name__ == '__main__':
    run()
