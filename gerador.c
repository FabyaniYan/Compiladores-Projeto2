#include "gerador.h"

#include <stdio.h>
#include <string.h>

#define GER_MAX_CAMINHO 512
#define GER_MAX_ROTULOS 4096
#define GER_TAM_ROTULO  16

static FILE *arq_mepa = NULL;
static char caminho_mepa[GER_MAX_CAMINHO];
static int contador_rotulos = 0;
static char rotulos[GER_MAX_ROTULOS][GER_TAM_ROTULO]; /*guarda os rotulos ja criados*/

int ger_abrir(const char *arquivo_fonte) {
    /* PESSOA 1: */
    /* montar caminho_mepa trocando a extensao do fonte por .mepa */
    /* (mesma logica de troca_extensao do log.c) e abrir com fopen(..., "w") */
    /* zerar contador_rotulos; devolver 1 se abriu, 0 se falhou */
    (void)arquivo_fonte;
    (void)caminho_mepa;
    return 1;
}

void gera_instr_mepa(const char *rotulo, const char *mnemonico,
                     const char *parametro1, const char *parametro2) {
    /* PESSOA 1: */
    /* se arq_mepa == NULL, nao faz nada */
    /* com rotulo: escrever "L1:" alinhado na coluna 0; sem rotulo: recuar com espacos */
    /* escrever o mnemonico; se houver parametro1, " p1"; se houver parametro2, ",p2" SEM espaco */
    /* terminar com '\n' */
    (void)rotulo; (void)mnemonico; (void)parametro1; (void)parametro2;
}

void gera_instr_int(const char *rotulo, const char *mnemonico, int qtd_params,
                    int parametro1, int parametro2) {
    /* PESSOA 1: */
    /* chamar gera_instr_mepa usando int2str nos parametros (0, 1 ou 2 parametros) */
    (void)rotulo; (void)mnemonico; (void)qtd_params; (void)parametro1; (void)parametro2;
}

const char *novo_rotulo(void) {
    /* PESSOA 1: */
    /* incrementar contador_rotulos e escrever "L<n>" em rotulos[contador_rotulos] */
    /* devolver rotulos[contador_rotulos] (cada rotulo tem buffer proprio) */
    (void)rotulos;
    return "L0";
}

const char *int2str(int valor) {
    /* PESSOA 1: */
    /* usar um buffer rotativo (ex.: static char buf[4][16] e um indice que gira) */
    /* para permitir gera_instr_mepa(NULL, "CRVL", int2str(0), int2str(3)) */
    (void)valor;
    return "0";
}

void ger_fechar(void) {
    /* PESSOA 1: */
    /* escrever a linha "FIM", fechar arq_mepa e zerar o ponteiro */
    (void)arq_mepa; (void)contador_rotulos;
}

void ger_descartar(void) {
    /* PESSOA 1: */
    /* fechar arq_mepa (se aberto) e apagar o arquivo com remove(caminho_mepa) */
}
