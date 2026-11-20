#include "parser.h"
#include "diag.h"
#include "gerador.h"
#include "log.h"
#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ASDR + analise semantica + geracao de codigo MEPA (Traducao Dirigida a
 * Sintaxe). A analise sintatica abaixo e a da Fase 1 e ja funciona; os
 * comentarios "PESSOA 1:" / "PESSOA 2:" marcam o que falta acrescentar.
 *
 * Divisao do arquivo:
 *   - do inicio ate bco()  -> PESSOA 1 (programa, declaracoes, sub-rotinas)
 *   - de cmd() ate o fim   -> PESSOA 2 (comandos e expressoes)
 */

#define MAX_IDS_DECL 64 /*identificadores por instrucao de declaracao*/

/*estado interno do parser: nao e exposto a nenhum outro modulo*/
static Lex *lexico = NULL;
static Token atual;       /*token de lookahead (um unico simbolo)*/
static int profundidade;  /*indentacao das mensagens de rastreamento*/
static int viu_main;      /*indica que o procedimento main ja foi analisado*/
static int blocos_sub;    /*contador de blocos start...end da sub-rotina atual*/
static char sub_atual[TOKEN_LEXEMA_MAX + 16]; /*prefixo de escopo, ex.: "proc:main"*/

/*
 * Informacoes da sub-rotina em analise.
 * PESSOA 1 preenche em subrotina(); PESSOA 2 le em cmd_return() e na chamada.
 */
static int idx_sub_atual = -1;         /*indice na TS da sub-rotina atual*/
static TipoSimbolo tipo_retorno_sub;   /*TS_VOID para proc*/
static const char *rotulo_fim_sub;     /*rotulo do RTPR (return desvia para ca)*/
static int analisando_main;            /*1 enquanto analisa proc main*/

/*prototipos dos nao-terminais (a gramatica e mutuamente recursiva)*/
static int gvars(void);
static int lvars(void);
static int decls(CategoriaSimbolo categoria);
static TipoSimbolo tipo(int *tamanho);
static void subrotina(void);
static int param(int idx_sub);
static void bco(void);
static void cmd(void);
static void cmd_echo(void);
static void cmd_get(void);
static void cmd_case(void);
static void cmd_choose(void);
static void cmd_for(void);
static void cmd_while(void);
static void cmd_repeat(void);
static void cmd_return(void);
static void cmd_id(void);
static TipoSimbolo args_chamada(int idx_sub, int linha);
static void indice_vetor(void);
static TipoSimbolo expr(void);
static TipoSimbolo elogc(void);
static TipoSimbolo erlac(void);
static TipoSimbolo earit(void);
static TipoSimbolo earip(void);
static TipoSimbolo fact(void);

/*
 * Apoio semantico e de geracao (PESSOA 2).
 * AINDA_NAO_USADA evita o aviso -Wunused-function enquanto o esqueleto
 * nao chama essas funcoes. Remover a marcacao quando elas forem usadas.
 */
#define AINDA_NAO_USADA __attribute__((unused))
static int busca_declarado(Token id) AINDA_NAO_USADA;
static void exige_tipo(TipoSimbolo esperado, TipoSimbolo obtido, int linha,
                       const char *contexto) AINDA_NAO_USADA;
static void gera_carrega_var(const Simbolo *s) AINDA_NAO_USADA;
static void gera_armazena_var(const Simbolo *s) AINDA_NAO_USADA;

/* ------------------------------------------------------------------ */
/* Apoio: consumo de tokens e rastreamento (pronto desde a Fase 1)     */
/* ------------------------------------------------------------------ */

static void entrar(const char *nao_terminal) {
    /*registra a entrada em um nao-terminal no arquivo de rastreamento*/
    diag_info("%*s> %s", profundidade * 2, "", nao_terminal);
    profundidade++;
}

static void sair(const char *nao_terminal) {
    if (profundidade > 0) profundidade--;
    diag_info("%*s< %s", profundidade * 2, "", nao_terminal);
}

static void avancar(void) {
    /*pede o proximo token ao lex, registra no log e aborta em erro lexico*/
    atual = lex_next(lexico);
    /* PESSOA 1: */
    /* nao registrar o sEOF duas vezes no .tk (pendencia da Fase 1) */
    log_token(atual.linha, lex_nome_categoria(atual.categoria), atual.lexema);
    if (atual.categoria == sERRO) diag_error(atual.linha, "%s", lex_erro(lexico));
}

static int aceitar(CategoriaToken categoria) {
    /*consome o token se ele for da categoria esperada*/
    if (atual.categoria != categoria) return 0;
    avancar();
    return 1;
}

static Token consumir(CategoriaToken categoria) {
    /*exige o token esperado; caso contrario reporta esperado x encontrado*/
    Token t = atual;
    if (atual.categoria != categoria)
        diag_error(atual.linha, "esperado %s, encontrado %s \"%s\"",
                   lex_nome_categoria(categoria),
                   lex_nome_categoria(atual.categoria), atual.lexema);
    avancar();
    return t;
}

static int inicia_comando(CategoriaToken c) {
    /*conjunto First(<cmd>)*/
    return c == sECHO || c == sGET || c == sCASE || c == sCHOOSE ||
           c == sFOR || c == sWHILE || c == sREPEAT || c == sRETURN ||
           c == sIDENTIF;
}

static TipoSimbolo tipo_vetor(TipoSimbolo base) {
    if (base == TS_INT) return TS_INT_VET;
    if (base == TS_LOGIC) return TS_LOGIC_VET;
    return TS_CHR_VET;
}

/* ================================================================== */
/*                                                                    */
/*                        PESSOA 1 - INICIO                           */
/*          programa, declaracoes, tipos, sub-rotinas e blocos        */
/*                                                                    */
/* ================================================================== */

/* <prg> ::= [<gvars>] {<subs>} <princ> */
void parse_program(Lex *lex) {
    int qtd_globais = 0;

    lexico = lex;
    profundidade = 0;
    viu_main = 0;
    sub_atual[0] = '\0';

    ts_init();
    avancar(); /*carrega o primeiro token de lookahead*/

    entrar("prg");
    /* PESSOA 1: */
    /* gerar INPP */

    if (atual.categoria == sGLOBVARS) qtd_globais = gvars();
    /* PESSOA 1: */
    /* se qtd_globais > 0, gerar AMEM qtd_globais */
    /* se houver sub-rotinas antes do main, gerar DSVS <rotulo do main> para pular o codigo delas */

    /*sub-rotinas opcionais seguidas, obrigatoriamente, do proc main*/
    while (atual.categoria == sFUNC || atual.categoria == sPROC) {
        if (viu_main)
            diag_error(atual.linha, "nenhuma sub-rotina pode ser declarada depois de main");
        subrotina();
    }

    if (!viu_main)
        diag_error(atual.linha, "programa nao define o procedimento main");

    /* PESSOA 1: */
    /* se qtd_globais > 0, gerar DMEM qtd_globais; depois gerar PARA (o FIM e escrito por ger_fechar) */
    (void)qtd_globais;

    consumir(sEOF);
    sair("prg");
}

/* <gvars> ::= "sGLOBVARS" <decls>{<decls>} ; devolve posicoes de memoria ocupadas */
static int gvars(void) {
    int memoria = 0;

    entrar("gvars");
    consumir(sGLOBVARS);
    if (atual.categoria != sIDENTIF)
        diag_error(atual.linha, "a secao globvars exige ao menos uma declaracao");
    while (atual.categoria == sIDENTIF) memoria += decls(TS_VAR_GLOBAL);
    sair("gvars");
    return memoria;
}

/* <lvars> ::= "sLOCVARS" <decls>{<decls>} ; devolve posicoes de memoria ocupadas */
static int lvars(void) {
    int memoria = 0;

    entrar("lvars");
    consumir(sLOCVARS);
    if (atual.categoria != sIDENTIF)
        diag_error(atual.linha, "a secao locvars exige ao menos uma declaracao");
    while (atual.categoria == sIDENTIF) memoria += decls(TS_VAR_LOCAL);
    sair("lvars");
    return memoria;
}

/* <decls> ::= <id> {"," <id>} "sKIND" <type> ";" ; devolve posicoes de memoria ocupadas */
static int decls(CategoriaSimbolo categoria) {
    char nomes[MAX_IDS_DECL][TOKEN_LEXEMA_MAX];
    int linhas[MAX_IDS_DECL];
    int n = 0, i, tamanho, memoria = 0;
    TipoSimbolo t;

    entrar("decls");
    do {
        Token id = consumir(sIDENTIF);
        if (n >= MAX_IDS_DECL)
            diag_error(id.linha, "declaracao com identificadores demais");
        /* PESSOA 1: */
        /* UNICIDADE: checar se o mesmo nome se repete DENTRO desta lista (ex.: "a, b, a is int;") */
        snprintf(nomes[n], TOKEN_LEXEMA_MAX, "%s", id.lexema);
        linhas[n] = id.linha;
        n++;
    } while (aceitar(sVIRGULA));

    consumir(sKIND);
    t = tipo(&tamanho);
    consumir(sPONTOVIRGULA);

    /*so registramos na TS depois de conhecer o tipo, que vem no fim da regra*/
    for (i = 0; i < n; i++) ts_insert(nomes[i], categoria, t, tamanho, linhas[i]);

    /* PESSOA 1: */
    /* somar em "memoria" o espaco de cada variavel: n * ts_tamanho_tipo(t, tamanho) */
    (void)memoria;

    sair("decls");
    return memoria;
}

/* <type> ::= ("sINT" | "sLOGIC" | "sCHR" | "sSTR") ["[" "sCTEINT" "]"] */
static TipoSimbolo tipo(int *tamanho) {
    TipoSimbolo base;

    entrar("type");
    if (aceitar(sINT)) base = TS_INT;
    else if (aceitar(sLOGIC)) base = TS_LOGIC;
    else if (aceitar(sCHR)) base = TS_CHR;
    /* PESSOA 1: */
    /* aceitar sSTR -> base = TS_STR (str nao aceita "[n]"; dar erro se vier colchete) */
    else {
        diag_error(atual.linha, "esperado um tipo (int, logic, chr ou str), encontrado %s \"%s\"",
                   lex_nome_categoria(atual.categoria), atual.lexema);
        base = TS_INT; /*inalcancavel: diag_error encerra o programa*/
    }

    *tamanho = 0;
    if (aceitar(sABRECOL)) {
        Token n = consumir(sCTEINT);
        *tamanho = atoi(n.lexema);
        if (*tamanho <= 0)
            diag_error(n.linha, "tamanho de vetor deve ser maior que zero");
        consumir(sFECHACOL);
        base = tipo_vetor(base);
    }
    sair("type");
    return base;
}

/*
 * <subs> ::= (<func> | <proc>)
 * <func> ::= "sFUNC" <id> "(" [<param>] ")" ":" <type> [<lvars>] <bco>
 * <proc> ::= "sPROC" <id> "(" [<param>] ")" [<lvars>] <bco>
 */
static void subrotina(void) {
    int e_funcao = (atual.categoria == sFUNC);
    char escopo[TS_MAX_NOME];
    Token id;
    int idx, qtd_param = 0, tamanho, memoria_locais = 0;

    entrar(e_funcao ? "func" : "proc");
    avancar(); /*consome sFUNC ou sPROC*/
    id = consumir(sIDENTIF);

    /*o nome da sub-rotina pertence ao escopo global*/
    idx = ts_insert(id.lexema, e_funcao ? TS_FUNC : TS_PROC, TS_VOID, 0, id.linha);
    analisando_main = (!e_funcao && strcmp(id.lexema, "main") == 0);

    /* PESSOA 1: */
    /* criar o rotulo da sub-rotina com novo_rotulo(), gravar com ts_set_rotulo(idx, ...) */
    /* e gerar "<rotulo>: NADA" (para o main, usar o rotulo do DSVS gerado em parse_program) */
    /* preencher idx_sub_atual = idx e rotulo_fim_sub = novo_rotulo() */
    idx_sub_atual = idx;
    rotulo_fim_sub = NULL;

    snprintf(sub_atual, sizeof(sub_atual), "%s:%s", e_funcao ? "fn" : "proc", id.lexema);
    blocos_sub = 0;

    /*parametros e variaveis locais compartilham o mesmo escopo*/
    snprintf(escopo, TS_MAX_NOME, "%s.locals", sub_atual);
    /* PESSOA 1: */
    /* nivel MEPA: 0 para o main (locais do main continuam depois das globais), 1 para as demais */
    ts_abre_escopo(escopo, analisando_main ? 0 : 1);

    consumir(sABREPAR);
    if (atual.categoria != sFECHAPAR) qtd_param = param(idx);
    consumir(sFECHAPAR);
    ts_set_extra(idx, qtd_param);
    /* PESSOA 1: */
    /* chamar ts_ajusta_enderecos_param(idx) para os parametros ficarem em -5, -6, ... */

    if (e_funcao) {
        consumir(sDOISPONTOS);
        ts_set_tipo(idx, tipo(&tamanho));
        tipo_retorno_sub = ts_get(idx) ? ts_get(idx)->tipo : TS_INDEF;
    } else {
        tipo_retorno_sub = TS_VOID;
        if (analisando_main) {
            /*main deve ser um procedimento sem parametros*/
            if (qtd_param != 0)
                diag_error(id.linha, "o procedimento main nao pode receber parametros");
            viu_main = 1;
        }
    }

    /* PESSOA 1: */
    /* se NAO for o main: gerar ENPR 1 */

    if (atual.categoria == sLOCVARS) memoria_locais = lvars();
    /* PESSOA 1: */
    /* se memoria_locais > 0, gerar AMEM memoria_locais */

    bco();

    /* PESSOA 1: */
    /* gerar "<rotulo_fim_sub>: NADA" (destino dos return) */
    /* se memoria_locais > 0, gerar DMEM memoria_locais */
    /* se NAO for o main: gerar RTPR qtd_param */
    /* func sem nenhum return -> decidir se e erro semantico */
    (void)memoria_locais;

    ts_fecha_escopo();
    idx_sub_atual = -1;
    sair(e_funcao ? "func" : "proc");
}

/* <param> ::= ["sREF"] <id> ":" <type> {"," ["sREF"] <id> ":" <type>} */
static int param(int idx_sub) {
    int n = 0, tamanho;
    TipoSimbolo t;

    entrar("param");
    do {
        int por_referencia = aceitar(sREF);
        Token id = consumir(sIDENTIF);
        consumir(sDOISPONTOS);
        t = tipo(&tamanho);
        /*vetores sao sempre passados por referencia, conforme a especificacao*/
        if (t == TS_INT_VET || t == TS_LOGIC_VET || t == TS_CHR_VET) por_referencia = 1;
        /* PESSOA 1: */
        /* str tambem deve ser passada por referencia? conferir na especificacao */
        ts_insert(id.lexema, por_referencia ? TS_PARAM_REF : TS_PARAM, t, tamanho, id.linha);
        /* PESSOA 1: */
        /* registrar o parametro na assinatura: ts_add_param(idx_sub, t, por_referencia) */
        (void)idx_sub;
        n++;
    } while (aceitar(sVIRGULA));
    sair("param");
    return n;
}

/* <bco> ::= "sSTART" {<cmd> ";"} "sEND" */
static void bco(void) {
    char escopo[TS_MAX_NOME];

    entrar("bco");
    blocos_sub++;
    snprintf(escopo, TS_MAX_NOME, "%s.block#%d", sub_atual, blocos_sub);
    /* PESSOA 1: */
    /* o bloco usa o mesmo nivel MEPA da sub-rotina (ts_nivel_atual()) */
    ts_abre_escopo(escopo, ts_nivel_atual());

    consumir(sSTART);
    while (atual.categoria != sEND) {
        if (!inicia_comando(atual.categoria))
            diag_error(atual.linha, "esperado sEND ou um comando, encontrado %s \"%s\"",
                       lex_nome_categoria(atual.categoria), atual.lexema);
        cmd();
        consumir(sPONTOVIRGULA);
    }
    consumir(sEND);

    ts_fecha_escopo();
    sair("bco");
}

/* ================================================================== */
/*                                                                    */
/*                         PESSOA 1 - FIM                             */
/*  -------------------------- CORTE --------------------------       */
/*                        PESSOA 2 - INICIO                           */
/*                     comandos e expressoes                          */
/*                                                                    */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/* Apoio semantico e de geracao                                        */
/* ------------------------------------------------------------------ */

/*DECLARACAO PREVIA: devolve o indice do identificador na TS ou aborta*/
static int busca_declarado(Token id) {
    /* PESSOA 2: */
    /* idx = ts_lookup(id.lexema); se idx < 0 -> diag_error "identificador nao declarado" */
    /* devolver idx */
    (void)id;
    return -1;
}

/*CHECAGEM DE TIPOS: aborta se o tipo obtido nao for o esperado*/
static void exige_tipo(TipoSimbolo esperado, TipoSimbolo obtido, int linha, const char *contexto) {
    /* PESSOA 2: */
    /* se obtido != esperado -> diag_error com contexto (ex.: "condicao do while deve ser logic") */
    (void)esperado; (void)obtido; (void)linha; (void)contexto;
}

/*empilha o valor de uma variavel simples*/
static void gera_carrega_var(const Simbolo *s) {
    /* PESSOA 2: */
    /* variavel/param por valor: CRVL nivel,endereco */
    /* param ref: CRVI nivel,endereco (carga indireta) */
    (void)s;
}

/*desempilha o topo para uma variavel simples*/
static void gera_armazena_var(const Simbolo *s) {
    /* PESSOA 2: */
    /* variavel/param por valor: ARMZ nivel,endereco */
    /* param ref: ARMI nivel,endereco (armazenamento indireto) */
    (void)s;
}

/*
 * <cmd> := <echo> | <get> | <case> | <chse> | <for> | <whle> |
 *          <rept> | <call> | <ret> | <atr>
 */
static void cmd(void) {
    entrar("cmd");
    switch (atual.categoria) {
        case sECHO:    cmd_echo();   break;
        case sGET:     cmd_get();    break;
        case sCASE:    cmd_case();   break;
        case sCHOOSE:  cmd_choose(); break;
        case sFOR:     cmd_for();    break;
        case sWHILE:   cmd_while();  break;
        case sREPEAT:  cmd_repeat(); break;
        case sRETURN:  cmd_return(); break;
        case sIDENTIF: cmd_id();     break;
        default:
            diag_error(atual.linha, "esperado um comando, encontrado %s \"%s\"",
                       lex_nome_categoria(atual.categoria), atual.lexema);
    }
    sair("cmd");
}

/* <echo> ::= "sECHO" "(" <elem> {"," <elem>} ")" */
static void cmd_echo(void) {
    TipoSimbolo t;

    entrar("echo");
    consumir(sECHO);
    consumir(sABREPAR);
    do {
        /* PESSOA 2: */
        /* string literal: CRST "texto" + IMPS (tamanho+1) */
        t = expr();
        /* PESSOA 2: */
        /* depois do expr: int/logic -> IMPR ; chr -> IMPC ; variavel str -> CREN + CRVM 256 + IMPS 256 */
        (void)t;
    } while (aceitar(sVIRGULA));
    consumir(sFECHAPAR);
    /* PESSOA 2: */
    /* decidir se o echo pula linha no final (testar no interpretador) */
    sair("echo");
}

/* <get> ::= "sGET" "(" (<id> | <vetr>) ")" */
static void cmd_get(void) {
    Token id;

    entrar("get");
    consumir(sGET);
    consumir(sABREPAR);
    id = consumir(sIDENTIF);
    /* PESSOA 2: */
    /* DECLARACAO PREVIA: busca_declarado(id); so pode ser variavel ou parametro (nao sub-rotina) */
    (void)id;
    if (atual.categoria == sABRECOL) indice_vetor();
    /* PESSOA 2: */
    /* variavel simples: LEIT + gera_armazena_var ; elemento de vetor: CREN + indice + INDX 1 + LEIT + ARVM 1 */
    consumir(sFECHAPAR);
    sair("get");
}

/*
 * <case> ::= "sCASE" "(" <expr> ")" {<cmd> ";"}
 *            ["sOTHERWISE" {<cmd> ";"}] "sEND"
 */
static void cmd_case(void) {
    TipoSimbolo t;

    entrar("case");
    consumir(sCASE);
    consumir(sABREPAR);
    t = expr();
    /* PESSOA 2: */
    /* exige_tipo(TS_LOGIC, t, ...) */
    /* criar rotulos L_senao e L_fim; gerar DSVF L_senao (gabarito do if da aula A10) */
    (void)t;
    consumir(sFECHAPAR);

    while (inicia_comando(atual.categoria)) { cmd(); consumir(sPONTOVIRGULA); }
    /* PESSOA 2: */
    /* gerar DSVS L_fim e "L_senao: NADA" */

    if (aceitar(sOTHERWISE))
        while (inicia_comando(atual.categoria)) { cmd(); consumir(sPONTOVIRGULA); }

    consumir(sEND);
    /* PESSOA 2: */
    /* gerar "L_fim: NADA" */
    sair("case");
}

/*
 * <chse>  ::= "sCHOOSE" "(" <expr> ")" <mlst> "sEND"
 * <mlst>  ::= <mtch> {<mtch>} [<othr>]
 * <mtch>  ::= "sMATCH" <mvlrs> "sIMPLIC" <cmd> ";"
 * <mvlrs> ::= "sCTEINT" {"," "sCTEINT"}
 * <othr>  ::= "sOTHERS" "sIMPLIC" <cmd> ";"
 */
static void cmd_choose(void) {
    TipoSimbolo t;

    entrar("chse");
    consumir(sCHOOSE);
    consumir(sABREPAR);
    t = expr();
    /* PESSOA 2: */
    /* exige_tipo(TS_INT, t, ...) */
    /* guardar o valor da expressao numa posicao temporaria (AMEM 1 + ARMZ) para comparar */
    /* com cada valor do match; criar o rotulo L_fim do choose */
    (void)t;
    consumir(sFECHAPAR);

    if (atual.categoria != sMATCH)
        diag_error(atual.linha, "o comando choose exige ao menos uma clausula match");

    while (atual.categoria == sMATCH) {
        entrar("mtch");
        consumir(sMATCH);
        do {
            aceitar(sSUBRAT); /*admite valores negativos nas alternativas*/
            consumir(sCTEINT);
            /* PESSOA 2: */
            /* para cada valor: carregar temporario, CRCT valor, CMIG; juntar varios valores com DISJ */
        } while (aceitar(sVIRGULA));
        consumir(sIMPLIC);
        /* PESSOA 2: */
        /* DSVF L_proximo_match */
        cmd();
        /* PESSOA 2: */
        /* DSVS L_fim e "L_proximo_match: NADA" */
        consumir(sPONTOVIRGULA);
        sair("mtch");
    }

    if (aceitar(sOTHERS)) {
        entrar("othr");
        consumir(sIMPLIC);
        cmd();
        consumir(sPONTOVIRGULA);
        sair("othr");
    }

    consumir(sEND);
    /* PESSOA 2: */
    /* "L_fim: NADA" e DMEM 1 do temporario */
    sair("chse");
}

/*valor usado nos limites do for: um identificador ou uma constante inteira*/
static void valor_for(void) {
    if (atual.categoria == sIDENTIF) {
        /* PESSOA 2: */
        /* busca_declarado, exigir tipo int e gerar a carga do valor (gera_carrega_var) */
        avancar();
        return;
    }
    aceitar(sSUBRAT);
    consumir(sCTEINT);
    /* PESSOA 2: */
    /* gerar CRCT do valor (com sinal, se veio o "-") */
}

/*
 * <for> ::= "sFOR" <id> "sFROM" (<id>|"sCTEINT") "sTO" (<id>|"sCTEINT")
 *           ["sBY" "sCTEINT"] "sDO" {<cmd> ";"} "sEND"
 */
static void cmd_for(void) {
    Token var;

    entrar("for");
    consumir(sFOR);
    var = consumir(sIDENTIF);
    /* PESSOA 2: */
    /* busca_declarado(var) e exigir variavel int */
    (void)var;
    consumir(sFROM);
    valor_for();
    /* PESSOA 2: */
    /* armazenar o valor inicial na variavel de controle (ARMZ) */
    /* "L_inicio: NADA"; carregar a variavel de controle */
    consumir(sTO);
    valor_for();
    /* PESSOA 2: */
    /* comparar: CMEG se o passo for positivo, CMAG se for negativo; DSVF L_fim */
    if (aceitar(sBY)) { aceitar(sSUBRAT); consumir(sCTEINT); }
    /* PESSOA 2: */
    /* guardar o passo (padrao 1) para usar depois do corpo */
    consumir(sDO);
    while (inicia_comando(atual.categoria)) { cmd(); consumir(sPONTOVIRGULA); }
    consumir(sEND);
    /* PESSOA 2: */
    /* incremento: carrega variavel, CRCT passo, SOMA, ARMZ; DSVS L_inicio; "L_fim: NADA" */
    /* ATENCAO: o "to" nao pode ser lido antes de L_inicio se for regerar a cada volta; */
    /* escrever o gabarito no papel antes de codificar (exercicio 1 da aula A10) */
    sair("for");
}

/* <whle> ::= "sWHILE" "(" <expr> ")" "sDO" {<cmd> ";"} "sEND" */
static void cmd_while(void) {
    TipoSimbolo t;

    entrar("whle");
    consumir(sWHILE);
    /* PESSOA 2: */
    /* criar L_inicio e L_fim; gerar "L_inicio: NADA" */
    consumir(sABREPAR);
    t = expr();
    /* PESSOA 2: */
    /* exige_tipo(TS_LOGIC, t, ...); gerar DSVF L_fim */
    (void)t;
    consumir(sFECHAPAR);
    consumir(sDO);
    while (inicia_comando(atual.categoria)) { cmd(); consumir(sPONTOVIRGULA); }
    consumir(sEND);
    /* PESSOA 2: */
    /* gerar DSVS L_inicio e "L_fim: NADA" */
    sair("whle");
}

/* <rept> ::= "sREPEAT" {<cmd> ";"} "sUNTIL" "(" <expr> ")" */
static void cmd_repeat(void) {
    TipoSimbolo t;

    entrar("rept");
    consumir(sREPEAT);
    /* PESSOA 2: */
    /* criar L_inicio e gerar "L_inicio: NADA" */
    while (inicia_comando(atual.categoria)) { cmd(); consumir(sPONTOVIRGULA); }
    consumir(sUNTIL);
    consumir(sABREPAR);
    t = expr();
    /* PESSOA 2: */
    /* exige_tipo(TS_LOGIC, t, ...); repete ENQUANTO a condicao for falsa: DSVF L_inicio */
    (void)t;
    consumir(sFECHAPAR);
    sair("rept");
}

/* <ret> ::= "sRETURN" <elem> */
static void cmd_return(void) {
    TipoSimbolo t;
    int linha = atual.linha;

    entrar("ret");
    consumir(sRETURN);
    t = expr();
    /* PESSOA 2: */
    /* return so e valido dentro de func (tipo_retorno_sub != TS_VOID) */
    /* exige_tipo(tipo_retorno_sub, t, linha, "retorno da funcao") */
    /* gerar ARMZ 1,-(qtd_param+5) (posicao do valor de retorno) e DSVS rotulo_fim_sub */
    (void)t; (void)linha;
    sair("ret");
}

/*
 * Comando iniciado por identificador. Um unico token de lookahead
 * basta para distinguir os dois casos:
 *   <call> ::= <id> "(" [<expr> {"," <expr>}] ")"
 *   <atr>  ::= (<id> | <vetr>) "sATRIB" <elem>
 */
static void cmd_id(void) {
    Token id = consumir(sIDENTIF);
    TipoSimbolo t;

    /* PESSOA 2: */
    /* DECLARACAO PREVIA: idx = busca_declarado(id) (pega "chamada a sub-rotina inexistente") */

    if (atual.categoria == sABREPAR) {
        entrar("call");
        /* PESSOA 2: */
        /* conferir que idx e uma sub-rotina (TS_PROC ou TS_FUNC) e passar idx para args_chamada */
        t = args_chamada(-1, id.linha);
        /* PESSOA 2: */
        /* se for func usada como comando, descartar o retorno (DMEM 1) */
        (void)t;
        sair("call");
        return;
    }
    entrar("atr");
    /* PESSOA 2: */
    /* conferir que idx e variavel ou parametro (atribuir a sub-rotina e erro) */
    /* vetor: gerar CREN do vetor antes do indice */
    if (atual.categoria == sABRECOL) indice_vetor();
    consumir(sATRIB);
    t = expr();
    /* PESSOA 2: */
    /* CHECAGEM DE TIPOS: tipo da variavel (ou do elemento do vetor) == t (str aceita string literal) */
    /* variavel simples: gera_armazena_var ; elemento de vetor: ARVM 1 */
    (void)t;
    sair("atr");
}

/*
 * Lista de argumentos de uma chamada, a partir do parentese de abertura.
 * Devolve o tipo de retorno da sub-rotina (TS_VOID para proc).
 */
static TipoSimbolo args_chamada(int idx_sub, int linha) {
    int n = 0;
    TipoSimbolo t;

    /* PESSOA 2: */
    /* se for func: AMEM 1 ANTES dos argumentos (espaco do valor de retorno) */
    consumir(sABREPAR);
    if (atual.categoria != sFECHAPAR)
        do {
            /* PESSOA 2: */
            /* parametro ref (ou vetor): o argumento tem que ser variavel -> gerar CREN em vez do valor */
            t = expr();
            /* PESSOA 2: */
            /* VALIDACAO DE PARAMETROS: comparar t com ts_get(idx_sub)->tipos_param[n] */
            (void)t;
            n++;
        } while (aceitar(sVIRGULA));
    consumir(sFECHAPAR);
    /* PESSOA 2: */
    /* VALIDACAO DE PARAMETROS: n == ts_get(idx_sub)->qtd_param, senao diag_error */
    /* gerar CHPR rotulo_da_sub,1 */
    /* devolver o tipo da sub-rotina (ts_get(idx_sub)->tipo) */
    (void)idx_sub; (void)linha; (void)n;
    return TS_INDEF;
}

/* <vetr> ::= <id> "[" ("sCTEINT" | <id>) "]" (a partir do colchete) */
static void indice_vetor(void) {
    consumir(sABRECOL);
    /* PESSOA 2: */
    /* indice constante: CRCT valor ; indice identificador: busca_declarado + exigir int + carregar */
    /* depois: INDX 1 (o endereco do elemento fica no topo da pilha) */
    if (atual.categoria == sCTEINT || atual.categoria == sIDENTIF) avancar();
    else
        diag_error(atual.linha, "esperado sCTEINT ou sIDENTIF como indice, encontrado %s \"%s\"",
                   lex_nome_categoria(atual.categoria), atual.lexema);
    consumir(sFECHACOL);
}

/* ------------------------------------------------------------------ */
/* Hierarquia de expressoes (da menor para a maior precedencia)        */
/* Cada funcao devolve o TIPO da subexpressao (checagem de tipos)      */
/* ------------------------------------------------------------------ */

/* <expr> ::= <elogc> {"sOR" <elogc>} */
static TipoSimbolo expr(void) {
    TipoSimbolo t;

    entrar("expr");
    t = elogc();
    while (aceitar(sOR)) {
        TipoSimbolo t2 = elogc();
        /* PESSOA 2: */
        /* os dois lados tem que ser logic; gerar DISJ; resultado logic */
        (void)t2;
    }
    sair("expr");
    return t;
}

/* <elogc> ::= <erlac> {"sAND" <erlac>} */
static TipoSimbolo elogc(void) {
    TipoSimbolo t;

    entrar("elogc");
    t = erlac();
    while (aceitar(sAND)) {
        TipoSimbolo t2 = erlac();
        /* PESSOA 2: */
        /* os dois lados tem que ser logic; gerar CONJ; resultado logic */
        (void)t2;
    }
    sair("elogc");
    return t;
}

/* <erlac> ::= <earit> {<oprel> <earit>} */
static TipoSimbolo erlac(void) {
    TipoSimbolo t;

    entrar("erlac");
    t = earit();
    while (atual.categoria == sMAIOR || atual.categoria == sMAIORIGUAL ||
           atual.categoria == sMENOR || atual.categoria == sMENORIGUAL ||
           atual.categoria == sIGUAL || atual.categoria == sDIFERENTE) {
        CategoriaToken op = atual.categoria;
        TipoSimbolo t2;
        avancar();
        t2 = earit();
        /* PESSOA 2: */
        /* tipos dos dois lados iguais; <,<=,>,>= so para int/chr; resultado logic */
        /* gerar CMMA, CMAG, CMME, CMEG, CMIG ou CMDG conforme op */
        (void)op; (void)t2;
    }
    sair("erlac");
    return t;
}

/* <earit> ::= <earip> {<opari> <earip>} */
static TipoSimbolo earit(void) {
    TipoSimbolo t;

    entrar("earit");
    t = earip();
    while (atual.categoria == sSOMA || atual.categoria == sSUBRAT) {
        CategoriaToken op = atual.categoria;
        TipoSimbolo t2;
        avancar();
        t2 = earip();
        /* PESSOA 2: */
        /* os dois lados tem que ser int; gerar SOMA ou SUBT; resultado int */
        (void)op; (void)t2;
    }
    sair("earit");
    return t;
}

/* <earip> ::= <fact> {<oparp> <fact>} */
static TipoSimbolo earip(void) {
    TipoSimbolo t;

    entrar("earip");
    t = fact();
    while (atual.categoria == sMULT || atual.categoria == sDIV) {
        CategoriaToken op = atual.categoria;
        TipoSimbolo t2;
        avancar();
        t2 = fact();
        /* PESSOA 2: */
        /* os dois lados tem que ser int; gerar MULT ou DIVI; resultado int */
        (void)op; (void)t2;
    }
    sair("earip");
    return t;
}

/*
 * <fact> ::= <elem> | "sNEG" <fact> | "sSUBRAT" <fact> | "(" <expr> ")"
 *
 * <elem> engloba literais, identificadores, elementos de vetor, chamadas de
 * sub-rotina e expressoes entre parenteses. Os operadores unarios sao
 * associativos a direita, o que a recursao abaixo reproduz naturalmente.
 */
static TipoSimbolo fact(void) {
    TipoSimbolo t = TS_INDEF;
    Token tk;

    entrar("fact");
    switch (atual.categoria) {
        case sNEG:
            avancar();
            t = fact();
            /* PESSOA 2: */
            /* exigir logic; gerar NEGA */
            break;
        case sSUBRAT:
            avancar();
            t = fact();
            /* PESSOA 2: */
            /* exigir int; gerar INVR */
            break;
        case sABREPAR:
            avancar();
            t = expr();
            consumir(sFECHAPAR);
            break;
        case sCTEINT:
            tk = atual;
            avancar();
            /* PESSOA 2: */
            /* gerar CRCT valor; t = TS_INT */
            (void)tk;
            break;
        case sCTECHAR:
            tk = atual;
            avancar();
            /* PESSOA 2: */
            /* gerar CRCT <codigo ASCII do caractere>; t = TS_CHR */
            (void)tk;
            break;
        case sSTRING:
            tk = atual;
            avancar();
            /* PESSOA 2: */
            /* gerar CRST "texto"; t = TS_STR */
            (void)tk;
            break;
        case sIDENTIF:
            tk = atual;
            avancar();
            /* PESSOA 2: */
            /* DECLARACAO PREVIA: idx = busca_declarado(tk) */
            if (atual.categoria == sABREPAR) {
                /* PESSOA 2: */
                /* <call>: tem que ser func (proc nao devolve valor); t = tipo de retorno */
                t = args_chamada(-1, tk.linha);
            } else if (atual.categoria == sABRECOL) {
                /* PESSOA 2: */
                /* <vetr>: CREN do vetor, depois indice, depois CRVM 1; t = tipo base do vetor */
                indice_vetor();
            } else {
                /* PESSOA 2: */
                /* variavel simples: gera_carrega_var; t = tipo da variavel */
            }
            break;
        default:
            diag_error(atual.linha, "esperado um operando, encontrado %s \"%s\"",
                       lex_nome_categoria(atual.categoria), atual.lexema);
    }
    sair("fact");
    return t;
}

/* ================================================================== */
/*                         PESSOA 2 - FIM                             */
/* ================================================================== */
