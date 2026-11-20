#!/usr/bin/env python3
"""Compila, executa no interpretador da disciplina e verifica saida e diagnosticos."""
import os, pathlib, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parent.parent  # raiz do projeto (testes/..)
# interpretador MEPA do professor: variavel de ambiente MEPA ou ../mepa_vm/mepa.py
MEPA=pathlib.Path(os.environ.get('MEPA', ROOT.parent/'mepa_vm'/'mepa.py'))
CASES=[
('hello','proc main()\nstart\n echo(1+2*3);\nend\n','7\n'),
('globals','globvars\n g is int;\nproc main()\nstart\n g << 42; echo(g);\nend\n','42\n'),
('while','proc main()\nlocvars\n i is int;\nstart\n i << 1; while(i <= 3) do echo(i); i << i+1; end;\nend\n','1\n2\n3\n'),
('repeat','proc main()\nlocvars\n i is int;\nstart\n i << 0; repeat i << i+1; echo(i); until(i=3);\nend\n','1\n2\n3\n'),
('case','proc main()\nstart\n case(2>1) echo(8); otherwise echo(99); end;\nend\n','8\n'),
('choose','proc main()\nstart\n choose(3) match 1,3 -> echo(7); match 2 -> echo(9); others -> echo(4); end;\nend\n','7\n'),
('choose_else','proc main()\nstart\n choose(9) match 1,3 -> echo(7); others -> echo(4); end;\nend\n','4\n'),
('for','proc main()\nlocvars\n i is int;\nstart\n for i from 1 to 3 do echo(i); end;\nend\n','1\n2\n3\n'),
('array','proc main()\nlocvars\n v is int[3]; i is int;\nstart\n i << 1; v[i] << 77; echo(v[1]);\nend\n','77\n'),
('procedure','proc dobro(x:int)\nstart\n echo(x*2);\nend\nproc main()\nstart\n dobro(12);\nend\n','24\n'),
('function','func sq(x:int):int\nstart\n return x*x;\nend\nproc main()\nstart\n echo(sq(9));\nend\n','81\n'),
('ref','proc incr(ref x:int)\nstart\n x << x+1;\nend\nproc main()\nlocvars\n n is int;\nstart\n n << 5; incr(n); echo(n);\nend\n','6\n'),
('string','proc main()\nlocvars\n msg is str;\nstart\n msg << "Hello"; echo(msg);\nend\n','Hello'),
('string_literal','proc main()\nstart\n echo("Ola");\nend\n','Ola'),
('negatives','proc main()\nstart\n echo(-3 + 5);\nend\n','2\n'),
('for_negative','proc main()\nlocvars\n i is int;\nstart\n for i from 3 to 1 by -1 do echo(i); end;\nend\n','3\n2\n1\n'),
('char','proc main()\nlocvars\n c is chr;\nstart\n c << \'Z\'; echo(c);\nend\n','Z'),
('escape','proc main()\nstart\n echo("Hello\\nWorld");\nend\n','Hello\nWorld'),
('multiple_strings','proc main()\nlocvars\n a,b is str;\nstart\n a << "ABC"; b << "xyz"; echo(a); echo(b);\nend\n','ABCxyz'),
('nested_calls','func add(a:int,b:int):int\nstart\n return a+b;\nend\nproc main()\nstart\n echo(add(add(1,2),4));\nend\n','7\n'),
('array_reference','proc set(ref v:int[3])\nstart\n v[1] << 42;\nend\nproc main()\nlocvars\n v is int[3];\nstart\n set(v); echo(v[1]);\nend\n','42\n'),
('nested_choose','proc main()\nstart\n choose(1) match 1 -> choose(2) match 2 -> echo(4); end; end;\nend\n','4\n'),
('shadow_globals','globvars\n a is int;\nproc upd()\nstart\n a << 11;\nend\nproc main()\nlocvars\n a is int;\nstart\n a << 6; upd(); echo(a);\nend\n','6\n'),
('negated_comparison','proc main()\nstart\n case(~(1>2)) echo(1); end;\nend\n','1\n'),
('logic_expression','proc main()\nstart\n case((1<2)&(3>2)) echo(5); end;\nend\n','5\n'),
('string_value_param','proc rename(x:str)\nstart\n x << "new"; echo(x);\nend\nproc main()\nlocvars\n a is str;\nstart\n a << "old"; rename(a); echo(a);\nend\n','newold'),
('string_ref_param','proc rename(ref x:str)\nstart\n x << "new";\nend\nproc main()\nlocvars\n a is str;\nstart\n a << "old"; rename(a); echo(a);\nend\n','new'),
('string_value_literal','proc show(s:str)\nstart\n echo(s);\nend\nproc main()\nstart\n show("ok");\nend\n','ok'),
('string_return','func hi():str\nstart\n return "ola";\nend\nproc main()\nstart\n echo(hi());\nend\n','ola'),
('string_nested','func hi(x:str):str\nstart\n return x;\nend\nproc main()\nstart\n echo(hi("ola"));\nend\n','ola'),
('recursion','func fat(n:int):int\nstart\n case(n<=1) return 1; end; return n*fat(n-1);\nend\nproc main()\nstart\n echo(fat(5));\nend\n','120\n'),
('read_input','proc main()\nlocvars\n x is int;\nstart\n get(x); echo(x+1);\nend\n','24\n'),
('by_two','proc main()\nlocvars\n i is int;\nstart\n for i from 1 to 5 by 2 do echo(i); end;\nend\n','1\n3\n5\n'),

]
ERRORS=[
('wrong_type','proc main()\nlocvars\n x is int;\nstart\n x << "abc";\nend\n','tipos incompativeis'),
('bad_condition','proc main()\nstart\n while(1) do echo(9); end;\nend\n','tipos incompativeis'),
('bad_call','proc test(x:int)\nstart\nend\nproc main()\nstart\n test("hi");\nend\n','tipos incompativeis'),
('undeclared','proc main()\nstart\n y << 2;\nend\n','nao declarado'),
('function_no_return','func teste():int\nstart\n echo(2);\nend\nproc main()\nstart\nend\n','exige return'),
('bad_ref','proc alterar(ref x:int)\nstart\nend\nproc main()\nstart\n alterar(1);\nend\n','por referencia'),
('duplicate_param_local','proc sub(x:int)\nlocvars\n x is int;\nstart\nend\nproc main()\nstart\nend\n','ja declarado'),
('duplicate','proc main()\nlocvars\n x is int; x is int;\nstart\nend\n','ja declarado'),
]

def main():
    subprocess.run(['make','-s','-C',str(ROOT)],check=True)
    count=0
    with tempfile.TemporaryDirectory() as directory:
        for name,source,expected in CASES:
            path=pathlib.Path(directory)/(name+'.slac');path.write_text(source)
            a=subprocess.run([str(ROOT/'complac'),str(path)],capture_output=True,text=True)
            if a.returncode==0:
                b=subprocess.run(['python3',str(MEPA),'--progfile',str(path.with_suffix('.mepa')),'--silent'],capture_output=True,text=True,input='23\n' if name=='read_input' else None,timeout=10)
                ok=b.returncode==0 and b.stdout==expected
                info=f'output={b.stdout!r} expected={expected!r} stderr={b.stderr[:220]!r}'
            else:ok=False;info=a.stderr
            print(('PASS ' if ok else 'FAIL ')+name+((' '+info) if not ok else ''));count+=ok
        for name,source,needle in ERRORS:
            path=pathlib.Path(directory)/(name+'.slac');path.write_text(source)
            a=subprocess.run([str(ROOT/'complac'),str(path)],capture_output=True,text=True)
            ok=a.returncode!=0 and needle in a.stderr and not path.with_suffix('.mepa').exists()
            print(('PASS ' if ok else 'FAIL ')+name+((' '+a.stderr) if not ok else ''));count+=ok
    print(f'{count}/{len(CASES)+len(ERRORS)} passed')
    raise SystemExit(0 if count==len(CASES)+len(ERRORS) else 1)
if __name__=='__main__':main()
