import os
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
MEPA = pathlib.Path(os.environ.get('MEPA', ROOT.parent / 'mepa_vm' / 'mepa.py'))

CASES = [
    ('dimensao_menor', 'proc altera(ref v:int[3])\nstart\n v[3] << 7;\nend\nproc main()\nlocvars\n v is int[2];\nstart\n altera(v);\nend\n', 'dimensao incompativel', None),
    ('dimensao_maior', 'proc altera(ref v:int[2])\nstart\n v[2] << 7;\nend\nproc main()\nlocvars\n v is int[3];\nstart\n altera(v);\nend\n', 'dimensao incompativel', None),
    ('vetor_correto', 'proc altera(ref v:int[2])\nstart\n v[2] << 7;\nend\nproc main()\nlocvars\n v is int[2]; guarda is int;\nstart\n guarda << 99; altera(v); echo(v[2]); echo(guarda);\nend\n', None, '7\n99\n'),
    ('funcao_sem_retorno_else', 'func f(x:int):int\nstart\n case(x>0) return 5; end;\nend\nproc main()\nstart\n echo(f(0));\nend\n', 'return em todos os caminhos', None),
    ('funcao_com_else', 'func f(x:int):int\nstart\n case(x>0) return 5; otherwise return 7; end;\nend\nproc main()\nstart\n echo(f(0)); echo(f(1));\nend\n', None, '7\n5\n'),
    ('funcao_com_retorno_final', 'func f(x:int):int\nstart\n case(x>0) return 5; end; return 7;\nend\nproc main()\nstart\n echo(f(0)); echo(f(1));\nend\n', None, '7\n5\n'),
    ('funcao_while_sem_retorno', 'func f(x:int):int\nstart\n while(x>0) do return x; end;\nend\nproc main()\nstart\n echo(f(0));\nend\n', 'return em todos os caminhos', None),
    ('funcao_repeat_retorno', 'func f():int\nstart\n repeat return 4; until(1=1);\nend\nproc main()\nstart\n echo(f());\nend\n', None, '4\n'),
    ('indice_variavel_valido', 'proc main()\nlocvars\n v is int[2]; i is int; guarda is int;\nstart\n guarda << 99; i << 2; v[i] << 7; echo(v[i]); echo(guarda);\nend\n', None, '7\n99\n'),
    ('indice_variavel_abaixo', 'proc main()\nlocvars\n v is int[2]; i is int; guarda is int;\nstart\n guarda << 99; i << 0; v[i] << 55; echo(guarda);\nend\n', None, ''),
    ('indice_variavel_acima', 'proc main()\nlocvars\n v is int[2]; i is int; guarda is int;\nstart\n guarda << 99; i << 4; v[i] << 55; echo(guarda);\nend\n', None, ''),
]

def main():
    ok_count=0
    with tempfile.TemporaryDirectory() as tmp:
        for name, code, error, expected in CASES:
            src = pathlib.Path(tmp) / (name + '.slac')
            src.write_text(code, encoding='utf8')
            a = subprocess.run([str(ROOT/'complac'), str(src)], capture_output=True, text=True, timeout=5)
            if error:
                ok = a.returncode != 0 and error in a.stderr and not src.with_suffix('.mepa').exists()
                detail = a.stderr.strip()
            elif a.returncode != 0:
                ok = False
                detail = a.stderr.strip()
            else:
                b = subprocess.run(['python3', str(MEPA), '--progfile', str(src.with_suffix('.mepa')), '--silent'], capture_output=True, text=True, timeout=6)
                ok = b.returncode == 0 and b.stdout == expected
                detail = repr(b.stdout) + ' / ' + b.stderr[:150]
            print(('PASS ' if ok else 'FAIL ') + name + ('' if ok else ': '+detail),flush=True)
            ok_count += ok
    print(f'{ok_count}/{len(CASES)} testes passaram')
    raise SystemExit(0 if ok_count==len(CASES) else 1)

if __name__ == '__main__':
    main()
