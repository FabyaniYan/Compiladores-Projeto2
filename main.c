/*
 * COMPLAC - compilador para a linguagem SLAC2
 * Fase 2: analise lexica, sintatica e semantica + geracao de codigo MEPA.
 *
 * Este modulo apenas orquestra a execucao: interpreta a linha de comando,
 * abre o arquivo fonte, inicializa os demais modulos, dispara a analise e
 * encerra tudo de forma ordenada. Nao realiza analise diretamente.
 */

#include "diag.h"
#include "gerador.h"
#include "lex.h"
#include "log.h"
#include "opt.h"
#include "parser.h"
#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    const Opcoes *opcoes;
    FILE *fonte;
    Lex lex;

    /*1. opcoes de execucao*/
    if (!opts_parse(argc, argv)) {
        opts_uso(argv[0]);
        return EXIT_FAILURE;
    }
    opcoes = opts_get();

    /*2. arquivo fonte*/
    fonte = fopen(opcoes->arquivo_fonte, "r");
    if (fonte == NULL) {
        fprintf(stderr, "nao foi possivel abrir \"%s\"\n", opcoes->arquivo_fonte);
        return EXIT_FAILURE;
    }

    /*3. inicializacao dos modulos de apoio*/
    diag_init(opcoes->arquivo_fonte);
    if (!log_abrir(opcoes->arquivo_fonte, opcoes->gerar_tokens,
                   opcoes->gerar_symtab, opcoes->gerar_trace)) {
        fprintf(stderr, "nao foi possivel criar os arquivos de log\n");
        fclose(fonte);
        return EXIT_FAILURE;
    }

    /*arquivo de saida <fonte>.mepa (sempre gerado, nao depende de opcao)*/
    if (!ger_abrir(opcoes->arquivo_fonte)) {
        fprintf(stderr, "nao foi possivel criar o arquivo .mepa\n");
        log_fechar();
        fclose(fonte);
        return EXIT_FAILURE;
    }

    if (!lex_init(&lex, fonte)) {
        fprintf(stderr, "falha ao inicializar o analisador lexico\n");
        ger_descartar();
        log_fechar();
        fclose(fonte);
        return EXIT_FAILURE;
    }

    /*4. analise + geracao de codigo: o parser consome os tokens do lex sob demanda
         e, enquanto reconhece cada estrutura, chama o gerador (SDT)*/
    parse_program(&lex);

    /*5. relatorios e encerramento ordenado*/
    ts_dump();
    ger_fechar();
    log_fechar();
    fclose(fonte);

    diag_ok("compilacao concluida com sucesso: %s", opcoes->arquivo_fonte);
    return EXIT_SUCCESS;
}
