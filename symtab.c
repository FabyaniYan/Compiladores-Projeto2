#include "symtab.h"
#include "diag.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TS_HASH_BUCKETS 257

/*
 * Tabela hash: um conjunto de baldes por escopo. Cada balde guarda o indice
 * do primeiro simbolo; os demais sao encadeados por proximo_hash.
 * Os simbolos continuam no vetor simbolos[], na ordem de insercao (relatorio .ts).
 */
static int buckets[TS_MAX_ESCOPOS][TS_HASH_BUCKETS];
static int proximo_hash[TS_MAX_SIMBOLOS];

/*funcao hash FNV-1a*/
static unsigned hash_nome(const char *nome) {
    unsigned long h = 2166136261u;
    for (; *nome; nome++) { h ^= (unsigned char)*nome; h *= 16777619u; }
    return (unsigned)(h % TS_HASH_BUCKETS);
}

typedef struct {
    char nome[TS_MAX_NOME]; /*nome completo do escopo, ex.: "proc:main.locals"*/
    int pai;                /*indice do escopo pai; -1 para o global*/
} Escopo;

static Simbolo simbolos[TS_MAX_SIMBOLOS];
static int total_simbolos = 0;

static Escopo escopos[TS_MAX_ESCOPOS];
static int total_escopos = 0;
static int escopo_ativo = 0;

/*procura o nome so dentro de um escopo, percorrendo um unico balde*/
static int buscar_no_escopo(const char *nome, int escopo) {
    int i = buckets[escopo][hash_nome(nome)];
    while (i >= 0) {
        if (strcmp(simbolos[i].lexema, nome) == 0) return i;
        i = proximo_hash[i];
    }
    return -1;
}


void ts_init(void) {
    total_simbolos = 0;
    total_escopos = 1;
    escopo_ativo = 0;
    snprintf(escopos[0].nome, TS_MAX_NOME, "global");
    escopos[0].pai = -1;
    for (int e = 0; e < TS_MAX_ESCOPOS; e++)
        for (int b = 0; b < TS_HASH_BUCKETS; b++) buckets[e][b] = -1;
}

void ts_abre_escopo(const char *nome) {
    int novo;

    if (total_escopos >= TS_MAX_ESCOPOS)
        diag_error(0, "limite de escopos da tabela de simbolos excedido");

    novo = total_escopos++;
    /*o nome ja chega pronto (ex.: "proc:main.block#1"); aqui so guardamos a cadeia*/
    snprintf(escopos[novo].nome, TS_MAX_NOME, "%s", nome);
    escopos[novo].pai = escopo_ativo;
    escopo_ativo = novo;
}

void ts_fecha_escopo(void) {
    if (escopos[escopo_ativo].pai >= 0) escopo_ativo = escopos[escopo_ativo].pai;
}

const char *ts_escopo_atual(void) { return escopos[escopo_ativo].nome; }

int ts_lookup_local(const char *lexema) {
    return buscar_no_escopo(lexema, escopo_ativo);
}

int ts_lookup(const char *lexema) {
    for (int e = escopo_ativo; e >= 0; e = escopos[e].pai) {
        int i = buscar_no_escopo(lexema, e);
        if (i >= 0) return i;
    }
    return -1;
}

int ts_insert(const char *lexema, CategoriaSimbolo cat, TipoSimbolo tipo,
              int extra, int linha) {
    int i;

    /*a linguagem nao admite dois identificadores iguais no mesmo escopo*/
    if (ts_lookup_local(lexema) >= 0)
        diag_error(linha, "identificador \"%s\" ja declarado no escopo %s",
                   lexema, escopos[escopo_ativo].nome);

    if (total_simbolos >= TS_MAX_SIMBOLOS)
        diag_error(linha, "limite de simbolos da tabela excedido");

    i = total_simbolos++;
    snprintf(simbolos[i].lexema, TOKEN_LEXEMA_MAX, "%s", lexema);
    simbolos[i].categoria = cat;
    simbolos[i].tipo = tipo;
    simbolos[i].escopo = escopo_ativo;
    simbolos[i].extra = extra;
    simbolos[i].endereco = 0;
    simbolos[i].nivel = 0;
    simbolos[i].label = 0;

    /*encadeia no inicio do balde do escopo atual*/
    unsigned h = hash_nome(lexema);
    proximo_hash[i] = buckets[escopo_ativo][h];
    buckets[escopo_ativo][h] = i;
    return i;
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
        case TS_INT_VET:   return "int[]";
        case TS_LOGIC_VET: return "logic[]";
        case TS_CHR_VET:   return "chr[]";
        case TS_VOID:      return "void";
        case TS_STR:       return "str";
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

void ts_set_addr(int i, int addr, int nivel) {
    if (i >= 0 && i < total_simbolos) {
        simbolos[i].endereco = addr;
        simbolos[i].nivel = nivel;
    }
}

void ts_set_label(int i, int label) {
    if (i >= 0 && i < total_simbolos) simbolos[i].label = label;
}
