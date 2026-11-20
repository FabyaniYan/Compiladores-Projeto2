# COMPLAC — Compilador SLAC² (Projeto 2)

Projeto da disciplina de Compiladores (Universidade Presbiteriana Mackenzie, 6ª etapa).

Compilador completo em C99 para a linguagem SLAC²: análise léxica, sintática (ASDR)
e semântica, com geração de código para a MEPA por Tradução Dirigida a Sintaxe.

## Integrantes

| Nome | RA |
|---|---:|
| Fabyani Tiva Yan | 10431835 |
| Bruna Amorim Maia | 10431883 |

## Estrutura

```
.
├── Makefile
├── README.md
├── main.c   opt.c/.h   log.c/.h   diag.c/.h
├── lex.c/.h          analisador léxico
├── parser.c/.h       ASDR + semântica + emissão MEPA
├── symtab.c/.h       tabela de símbolos (hash por escopo)
├── gerador.c/.h      instruções MEPA e rótulos
└── testes/
    ├── exemplos/     programas válidos (geram .mepa)
    ├── erros/        programas com erro intencional
    └── *.py          bateria automática (make teste)
```

## Compilação

```bash
make          # gera o binário complac (gcc -Wall -Wextra -std=c99, sem warnings)
make clean    # remove objetos, binário, logs e .mepa
```

## Execução

```bash
./complac <arquivo.slac> [--tokens] [--symtab] [--trace]
python3 mepa.py --progfile <arquivo.mepa> --silent
```

- O compilador sempre gera `<fonte>.mepa`, ao lado do fonte, quando não há erro.
- `--tokens`, `--symtab` e `--trace` geram os logs `.tk`, `.ts` e `.trc` (combináveis, em qualquer ordem).
- Erro: mensagem `<fonte>:<linha>: erro: <mensagem>` no stderr e retorno 1; nenhum `.mepa` é gerado.

## Testes

```bash
make exemplos                               # compila testes/exemplos e testes/erros
make teste                                  # bateria automática (75 casos)
make teste MEPA=/caminho/para/mepa.py       # se o interpretador estiver em outro lugar
```

O interpretador do professor não faz parte do repositório; por padrão o `make teste`
procura em `../mepa_vm/mepa.py`. A bateria compara a saída real do interpretador com a
esperada: chamadas, retorno, recursão, entrada, vetores, referência, strings, `case`,
`choose`, `for`, `while`, `repeat`, operadores e mensagens de erro.
Os testes adicionais conferem dimensões de vetores, limites de índices variáveis e
retornos de funções em diferentes caminhos de execução.

## Verificações semânticas

| Verificação | Exemplo em `testes/erros` |
|---|---|
| Declaração prévia | `nao_declarado`, `nao_declarado_expressao` |
| Sub-rotina inexistente | `rotina_inexistente`, `funcao_inexistente` |
| Unicidade no mesmo escopo | `erro_duplicado`, `dup_lista`, `dup_param_local`, `dup_subrotina`, `dup_global_subrotina` |
| Checagem de tipos | `tipo_atribuicao` |
| Quantidade e tipo de parâmetros | `qtd_argumentos`, `tipo_argumento` |

Sombreamento em escopos diferentes (variável local com o nome de uma global) é permitido.

## Geração de código (MEPA)

- Programa: `INPP`, `AMEM` das globais, `DSVS` para a entrada, sub-rotinas, `CHPR` do `main`, `DMEM`, `PARA`, `FIM`.
- Sub-rotinas: `ENPR 1` / `AMEM` dos locais / corpo / `DMEM` / `RTPR n`. Parâmetros em endereços
  negativos (o último em -5); o retorno de função fica na posição reservada pelo chamador (`AMEM 1`).
- Vetores: `n+1` posições (a posição 0 guarda o tamanho; elementos de 1 a n), acesso com `INDX 1`.
- `str`: 256 células (comprimento + até 255 caracteres), com `CRST`, `CRVM`, `ARVM` e `IMPS`.
- Desvios com rótulos `Ln: NADA`, `DSVF` e `DSVS`, seguindo os gabaritos de `if`/`while` da aula.

## Limitações conhecidas

- Exige ao menos um `return` em cada `func`, mas não verifica todos os caminhos.
- Índice de vetor dado por variável não tem checagem de limite em tempo de execução
  (índice constante fora de 1..n é erro de compilação).
- Não implementado: atribuição de vetor inteiro, comparação entre vetores/strings e `get` de `str`.
- Chamar uma sub-rotina antes de declará-la é erro (declaração prévia).
- No `echo`, inteiros (`IMPR`) e caracteres (`IMPC`) quebram linha; strings (`IMPS`) não.
