#include "symtab.h"
#include "diag.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

/*
 * Organizacao interna (Fase 2):
 *   - os simbolos continuam guardados em um vetor (simbolos[]), na ordem
 *     de insercao; isso mantem ts_get, ts_set_* e ts_dump funcionando;
 *   - a tabela hash (baldes[]) guarda, para cada balde, o indice do
 *     primeiro simbolo; os demais sao encadeados pelo campo "proximo".
 */

typedef struct {
    char nome[TS_MAX_NOME]; /*nome completo do escopo, ex.: "proc:main.locals"*/
    int pai;                /*indice do escopo pai; -1 para o global*/
    int nivel;              /*nivel lexico MEPA*/
    int memoria;            /*posicoes ja ocupadas pelas variaveis deste escopo*/
} Escopo;

static Simbolo simbolos[TS_MAX_SIMBOLOS];
static int total_simbolos = 0;

static Escopo escopos[TS_MAX_ESCOPOS];
static int total_escopos = 0;
static int escopo_ativo = 0;

static int baldes[TS_TAM_HASH]; /*indice do primeiro simbolo de cada balde; -1 = vazio*/

/* ------------------------------------------------------------------ */
/* Funcao de espalhamento                                              */
/* ------------------------------------------------------------------ */

static unsigned int hash(const char *lexema) __attribute__((unused));
static unsigned int hash(const char *lexema) {
    /* PESSOA 1: */
    /* implementar a funcao hash (ex.: djb2: h = h*33 + c) e devolver h % TS_TAM_HASH */
    (void)lexema;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Escopos                                                             */
/* ------------------------------------------------------------------ */

void ts_init(void) {
    total_simbolos = 0;
    total_escopos = 1;
    escopo_ativo = 0;
    snprintf(escopos[0].nome, TS_MAX_NOME, "global");
    escopos[0].pai = -1;
    escopos[0].nivel = 0;
    escopos[0].memoria = 0;

    /* PESSOA 1: */
    /* inicializar todos os baldes[] com -1 (tabela hash vazia) */
    (void)baldes;
}

void ts_abre_escopo(const char *nome, int nivel) {
    int novo;

    if (total_escopos >= TS_MAX_ESCOPOS)
        diag_error(0, "limite de escopos da tabela de simbolos excedido");

    novo = total_escopos++;
    snprintf(escopos[novo].nome, TS_MAX_NOME, "%s", nome);
    escopos[novo].pai = escopo_ativo;
    escopos[novo].nivel = nivel;
    escopos[novo].memoria = 0;
    escopo_ativo = novo;

    /* PESSOA 1: */
    /* conferir se o bloco start...end deve herdar a memoria/nivel do escopo .locals */
}

void ts_fecha_escopo(void) {
    if (escopos[escopo_ativo].pai >= 0) escopo_ativo = escopos[escopo_ativo].pai;
}

const char *ts_escopo_atual(void) { return escopos[escopo_ativo].nome; }

int ts_nivel_atual(void) { return escopos[escopo_ativo].nivel; }

/* ------------------------------------------------------------------ */
/* Insercao e busca                                                    */
/* ------------------------------------------------------------------ */

int ts_lookup_local(const char *lexema) {
    /* PESSOA 1: */
    /* percorrer so o balde hash(lexema), seguindo "proximo", e devolver o indice */
    /* do simbolo com esse lexema que pertenca ao escopo_ativo; -1 se nao achar */
    (void)lexema;
    return -1;
}

int ts_lookup(const char *lexema) {
    /* PESSOA 1: */
    /* para cada escopo da cadeia (escopo_ativo -> pai -> ... -> global), procurar no */
    /* balde hash(lexema) um simbolo daquele escopo; devolver o primeiro achado (o mais interno) */
    /* -1 se nao achar (quem chama decide se e erro "nao declarado") */
    (void)lexema;
    return -1;
}

int ts_insert(const char *lexema, CategoriaSimbolo cat, TipoSimbolo tipo,
              int extra, int linha) {
    /* PESSOA 1: */
    /* 1. UNICIDADE: se ts_lookup_local(lexema) >= 0 -> diag_error "ja declarado no escopo" */
    /* 2. UNICIDADE: sub-rotina com nome igual a variavel global (e vice-versa) -> erro */
    /* 3. preencher simbolos[total_simbolos] (lexema, cat, tipo, escopo, extra, nivel) */
    /* 4. calcular tamanho com ts_tamanho_tipo e endereco = escopos[escopo_ativo].memoria */
    /*    (parametros e sub-rotinas nao ocupam memoria do escopo; somar so para variaveis) */
    /* 5. encadear no inicio do balde: proximo = baldes[h]; baldes[h] = indice */
    /* 6. devolver o indice */
    (void)lexema; (void)cat; (void)tipo; (void)extra; (void)linha;
    return -1;
}

const Simbolo *ts_get(int indice) {
    if (indice < 0 || indice >= total_simbolos) return NULL;
    return &simbolos[indice];
}

void ts_set_extra(int indice, int extra) {
    if (indice >= 0 && indice < total_simbolos) simbolos[indice].extra = extra;
}

void ts_set_tipo(int indice, TipoSimbolo tipo) {
    if (indice >= 0 && indice < total_simbolos) simbolos[indice].tipo = tipo;
}

void ts_set_rotulo(int indice, const char *rotulo) {
    /* PESSOA 1: */
    /* copiar o rotulo para simbolos[indice].rotulo (com snprintf), validando o indice */
    (void)indice; (void)rotulo;
}

/* ------------------------------------------------------------------ */
/* Parametros e memoria                                                */
/* ------------------------------------------------------------------ */

void ts_add_param(int indice_sub, TipoSimbolo tipo, int por_referencia) {
    /* PESSOA 1: */
    /* gravar tipo e ref em tipos_param[qtd_param] / ref_param[qtd_param] e incrementar qtd_param */
    /* (diag_error se passar de TS_MAX_PARAMS) */
    (void)indice_sub; (void)tipo; (void)por_referencia;
}

void ts_ajusta_enderecos_param(int indice_sub) {
    /* PESSOA 1: */
    /* percorrer os parametros do escopo atual na ordem de declaracao e definir: */
    /* endereco do i-esimo = -(qtd_param - i + 4)  ->  ultimo = -5, penultimo = -6 ... */
    /* (funcao: o valor de retorno fica em -(qtd_param + 5)) */
    (void)indice_sub;
}

int ts_memoria_escopo(void) {
    return escopos[escopo_ativo].memoria;
}

int ts_tamanho_tipo(TipoSimbolo tipo, int tamanho_vetor) {
    /* PESSOA 1: */
    /* int/logic/chr = 1; vetor = tamanho_vetor + 1 (posicao 0 guarda o comprimento); */
    /* str = TS_TAM_STR */
    (void)tipo; (void)tamanho_vetor;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Relatorios (ja prontos desde a Fase 1)                              */
/* ------------------------------------------------------------------ */

const char *ts_nome_categoria(CategoriaSimbolo cat) {
    switch (cat) {
        case TS_VAR_GLOBAL: return "varglobal";
        case TS_VAR_LOCAL:  return "varlocal";
        case TS_PARAM:      return "param";
        case TS_PARAM_REF:  return "paramref";
        case TS_FUNC:       return "func";
        case TS_PROC:       return "proc";
        default:            return "?";
    }
}

const char *ts_nome_tipo(TipoSimbolo tipo) {
    switch (tipo) {
        case TS_INT:       return "int";
        case TS_LOGIC:     return "logic";
        case TS_CHR:       return "chr";
        case TS_STR:       return "str";
        case TS_INT_VET:   return "int[]";
        case TS_LOGIC_VET: return "logic[]";
        case TS_CHR_VET:   return "chr[]";
        case TS_VOID:      return "void";
        case TS_INDEF:     return "indef";
        default:           return "?";
    }
}

void ts_dump(void) {
    int e, i;
    if (!log_symtab_ativo()) return;
    /*escopos na ordem de criacao; dentro de cada um, ordem de insercao*/
    for (e = 0; e < total_escopos; e++)
        for (i = 0; i < total_simbolos; i++)
            if (simbolos[i].escopo == e)
                log_simbolo(escopos[e].nome, simbolos[i].lexema,
                            ts_nome_categoria(simbolos[i].categoria),
                            ts_nome_tipo(simbolos[i].tipo),
                            simbolos[i].extra);
}
