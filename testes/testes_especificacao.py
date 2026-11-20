#!/usr/bin/env python3
"""Casos adicionais construidos a partir do Manual SLAC2 (2026) e do Projeto 2."""
import os
from pathlib import Path
import subprocess, tempfile
ROOT=Path(__file__).resolve().parent.parent  # raiz do projeto (testes/..)
# interpretador MEPA do professor: variavel de ambiente MEPA ou ../mepa_vm/mepa.py
MEPA=Path(os.environ.get('MEPA', ROOT.parent/'mepa_vm'/'mepa.py'))
cases=[
 ('vetor_ultimo_indice','proc main()\nlocvars\n v is int[3]; sentinela is int;\nstart\n sentinela << 999; v[3] << 42; echo(v[3]); echo(sentinela);\nend\n','42\n999\n'),
 ('vetor_global_ultimo_indice','globvars\n v is int[2]; guarda is int;\nproc main()\nstart\n guarda << 7; v[2] << 12; echo(v[2]); echo(guarda);\nend\n','12\n7\n'),
 ('vetor_primeiro_indice','proc main()\nlocvars\n v is int[2];\nstart\n v[1] << 2; v[2] << 3; echo(v[1]+v[2]);\nend\n','5\n'),
 ('vetor_ref_ultimo_indice','proc set(ref a:int[2])\nstart\n a[2] << 81;\nend\nproc main()\nlocvars\n a is int[2]; z is int;\nstart\n z << 17; set(a); echo(a[2]); echo(z);\nend\n','81\n17\n'),
 ('string_255','proc main()\nlocvars\n s is str;\nstart\n s << "'+('A'*255)+'"; echo(s);\nend\n','A'*255),
 ('unicode_string','proc main()\nstart\n echo("Olá mundo! Ação e coração");\nend\n','Olá mundo! Ação e coração'),
 ('unicode_chr',"proc main()\nstart\n echo('á');\nend\n",'á'),
 ('unicode_255','proc main()\nstart\n echo("'+('á'*255)+'");\nend\n','á'*255),
]
errors=[
 ('vetor_zero','proc main()\nlocvars\n v is int[3];\nstart\n v[0] << 1;\nend\n','fora do intervalo'),
 ('vetor_alem','proc main()\nlocvars\n v is int[3];\nstart\n v[4] << 1;\nend\n','fora do intervalo'),
 ('string_256','proc main()\nstart\n echo("'+('A'*256)+'");\nend\n','excede 255'),
]
def main():
    subprocess.run(['make','-s','-C',str(ROOT)],check=True)
    passed=0
    with tempfile.TemporaryDirectory() as temp:
        for name,source,expected in cases:
            f=Path(temp)/(name+'.slac'); f.write_text(source,encoding='utf-8')
            a=subprocess.run([str(ROOT/'complac'),str(f)],capture_output=True,text=True)
            if a.returncode:
                ok=False;detail=a.stderr
            else:
                b=subprocess.run(['python3',str(MEPA),'--progfile',str(f.with_suffix('.mepa')),'--silent'],capture_output=True,text=True,timeout=15)
                ok=b.returncode==0 and b.stdout==expected
                detail=f'{b.stdout[:100]!r} stderr={b.stderr[:200]!r}'
            print(('PASS ' if ok else 'FAIL ')+name+(' '+detail if not ok else ''));passed+=ok
        for name,source,needle in errors:
            f=Path(temp)/(name+'.slac'); f.write_text(source)
            a=subprocess.run([str(ROOT/'complac'),str(f)],capture_output=True,text=True)
            ok=a.returncode!=0 and needle in a.stderr
            print(('PASS ' if ok else 'FAIL ')+name+(' '+a.stderr if not ok else ''));passed+=ok
    print(f'{passed}/{len(cases)+len(errors)} testes da especificacao passaram')
    raise SystemExit(0 if passed==len(cases)+len(errors) else 1)
if __name__=='__main__': main()
