# COMPLAC - Compilador para SLAC² (Fase 2)

Projeto da disciplina de Compiladores (Mackenzie, 6ª etapa).

Esta entrega corresponde à **Fase 2**: análise semântica e geração de código
para a MEPA, feitas durante a análise sintática (Tradução Dirigida a Sintaxe).

## Integrantes

| Nome | RA |
|---|---:|
| Fabyani Tiva Yan | 10431835 |
| Bruna Amorim Maia | 10431883 |

## Compilação

```bash
make          # gera o binario complac (-Wall -Wextra -std=c99)
make clean    # remove objetos, binario, logs e .mepa dos testes
```

## Execução

```bash
./complac <arquivo.slac> [--tokens] [--symtab] [--trace]
```

Sempre gera `<fonte>.mepa` ao lado do fonte. As opções geram os mesmos logs da Fase 1
(`.tk`, `.ts`, `.trc`).

Para executar o código gerado no interpretador fornecido:

```bash
python3 mepa.py --progfile testes/enunciado_fig1.mepa
```

<!-- PESSOA 1: -->
<!-- conferir se os codigos de retorno (0 sucesso / 1 erro) e o formato da mensagem de erro continuam iguais -->

## Módulos

| Módulo | Responsabilidade | Interface pública |
|---|---|---|
| `main` | Orquestra a execução | — |
| `opt` | Linha de comando | `opts_parse`, `opts_get` |
| `lex` | Analisador léxico | `lex_next` |
| `parser` | ASDR + semântica + emissão MEPA | `parse_program` |
| `symtab` | Tabela de símbolos (hash com níveis de escopo) | `ts_insert`, `ts_lookup` |
| `gerador` | Escrita do `.mepa` e rótulos | `gera_instr_mepa`, `novo_rotulo` |
| `diag` | Mensagens de erro e rastreamento | `diag_error`, `diag_info` |
| `log` | Arquivos `.tk`, `.ts`, `.trc` | `log_token`, `log_simbolo`, `log_trace` |

## Verificações semânticas

<!-- PESSOA 1: -->
<!-- listar o que foi implementado em unicidade e escopo (casos de duplicata cobertos) -->

<!-- PESSOA 2: -->
<!-- listar declaracao previa, checagem de tipos (regras por operador) e validacao de parametros -->

## Geração de código

<!-- PESSOA 1: -->
<!-- explicar layout de memoria: globais e locais do main no nivel 0, sub-rotinas no nivel 1, -->
<!-- parametros em -5, -6..., retorno de func, vetor com n+1 posicoes, str com 256 -->

<!-- PESSOA 2: -->
<!-- mostrar o gabarito de cada comando (case, choose, for, while, repeat, echo, get) -->

## Particularidades

<!-- AS DUAS: -->
<!-- decisoes que nao estao claras na especificacao (ex.: echo pula linha?, str por referencia?) -->
