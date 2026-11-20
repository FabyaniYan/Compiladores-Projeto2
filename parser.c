/*
 * Modulo parser - ASDR (Analisador Sintatico Descendente Recursivo) da SLAC2.
 *
 * Fase 2: a analise sintatica, a analise semantica e a geracao de codigo MEPA
 * acontecem juntas (Traducao Dirigida a Sintaxe). Cada nao-terminal da EBNF
 * (Apendice A da especificacao) tem uma funcao; enquanto reconhece a
 * estrutura, a funcao confere tipos/declaracoes e emite as instrucoes MEPA
 * pelo modulo gerador.
 *
 * Organizacao do arquivo:
 *   1. estado interno e apoio (tokens, tipos, carga/armazenamento)
 *   2. declaracoes e tipos              (<type>, <decls>, <gvars>, <lvars>)
 *   3. vetores e chamadas               (<vetr>, <call>)
 *   4. expressoes, da maior para a menor precedencia (<fact> ... <expr>)
 *   5. comandos                         (<echo>, <get>, <case>, ... <atr>)
 *   6. blocos, sub-rotinas e programa   (<bco>, <param>, <subs>, <prg>)
 */
#include "parser.h"
#include "symtab.h"
#include "gerador.h"
#include "diag.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#define MAX_IDS 128
#define MAX_ARGS 128
#define STR_CELLS 256
/* ------------------------------------------------------------------ */
/* 1. Estado interno (nao e exposto a nenhum outro modulo)             */
/* ------------------------------------------------------------------ */

static Lex *lexico;
static Token atual;
static int trace_depth, viu_main, blocos_sub;
typedef struct { int conhecido, valor; } Constante;
static Constante constante;

static int eof_registrado; /*evita registrar o sEOF duas vezes no .tk*/
/*
 * globais/locais: posicoes de memoria ja ocupadas (proximo endereco livre)
 * nparam: celulas ocupadas pelos parametros da sub-rotina atual
 * func_idx/dentro_func: controle do return da funcao atual
 */
static int globais, locais, nparam, func_idx, dentro_func;
static char sub_atual[TOKEN_LEXEMA_MAX+16];
/*
 * Assinatura de cada sub-rotina, indexada pelo indice dela na TS:
 * quantidade de parametros, tipo de cada um e se e passado por referencia.
 * idx_param guarda temporariamente os indices dos parametros da sub atual.
 */
static int idx_param[TS_MAX_SIMBOLOS], sig_count[TS_MAX_SIMBOLOS];
static TipoSimbolo sig_tipo[TS_MAX_SIMBOLOS][MAX_ARGS];
static int sig_ref[TS_MAX_SIMBOLOS][MAX_ARGS];
static int sig_dim[TS_MAX_SIMBOLOS][MAX_ARGS];
/*registra entrada/saida de um nao-terminal no .trc (opcao --trace)*/
static void entrar(const char *n) {
    diag_info("%*s> %s",trace_depth*2,"",n);
    trace_depth++;
}
static void sair(const char *n) {
    trace_depth--;
    diag_info("%*s< %s",trace_depth*2,"",n);
}
static void avancar(void) {
    /*pede o proximo token ao lex, registra no .tk e aborta em erro lexico*/
    atual=lex_next(lexico);
    if(atual.categoria!=sEOF || !eof_registrado)
        log_token(atual.linha,lex_nome_categoria(atual.categoria),atual.lexema);
    if(atual.categoria==sEOF)eof_registrado=1;
    if(atual.categoria==sERRO) diag_error(atual.linha,"%s",lex_erro(lexico));
}
/*consome o token se ele for da categoria pedida; devolve 1 se consumiu*/
static int aceitar(CategoriaToken c) {
    if(atual.categoria!=c)return 0;
    avancar();
    return 1;
}
/*exige o token pedido; senao, erro "esperado X, encontrado Y"*/
static Token consumir(CategoriaToken c) {
    Token t=atual;
    if(t.categoria!=c)diag_error(t.linha,"esperado %s, encontrado %s \"%s\"",lex_nome_categoria(c),lex_nome_categoria(t.categoria),t.lexema);
    avancar();
    return t;
}
/*conjunto First(<cmd>): tokens que podem iniciar um comando*/
static int inicia(CategoriaToken c) {
    return c==sECHO||c==sGET||c==sCASE||c==sCHOOSE||c==sFOR||c==sWHILE||c==sREPEAT||c==sRETURN||c==sIDENTIF;
}
/*apoio de tipos: e vetor? / tipo do elemento / celulas de memoria ocupadas*/
static int vetor(TipoSimbolo t) {
    return t==TS_INT_VET||t==TS_LOGIC_VET||t==TS_CHR_VET;
}
static TipoSimbolo base(TipoSimbolo t) {
    return t==TS_INT_VET?TS_INT:t==TS_LOGIC_VET?TS_LOGIC:t==TS_CHR_VET?TS_CHR:t;
}
static int reserva_celulas(int *total, int64_t quantidade, int linha) {
    int inicio = *total;
    if (quantidade > INT_MAX - inicio)
        diag_error(linha, "alocacao excede o limite de celulas MEPA");
    *total += (int)quantidade;
    return inicio;
}
/*DECLARACAO PREVIA: identificador precisa existir na cadeia de escopos visiveis*/
static int resolver(Token id) {
    int k=ts_lookup(id.lexema);
    if(k<0)diag_error(id.linha,"identificador \"%s\" nao declarado",id.lexema);
    return k;
}
/*identificador declarado que tambem precisa ser variavel ou parametro*/
static int variavel(Token id) {
    int k=resolver(id);
    const Simbolo *s=ts_get(k);
    if(s->categoria==TS_FUNC||s->categoria==TS_PROC)diag_error(id.linha,"\"%s\" e sub-rotina, nao variavel",id.lexema);
    return k;
}
/*CHECAGEM DE TIPOS: os dois tipos precisam ser iguais*/
static void igual(TipoSimbolo a, TipoSimbolo b,int linha) {
    if(a!=b)diag_error(linha,"tipos incompativeis: %s e %s",ts_nome_tipo(a),ts_nome_tipo(b));
}
static void exige(TipoSimbolo t,TipoSimbolo want,int linha) {
    igual(t,want,linha);
}
/*converte uma constante inteira, conferindo o limite do int*/
static int inteiro(Token t) {
    char *end;
    long v=strtol(t.lexema,&end,10);
    if(*end||v>INT_MAX||v<INT_MIN)diag_error(t.linha,"constante inteira fora do limite");
    return (int)v;
}
/*
 * Carga e armazenamento de variaveis:
 *   pushaddr: empilha o endereco (CREN; parametro ref ja guarda um endereco -> CRVL)
 *   load:     empilha o valor   (CRVL; parametro ref -> CRVI, carga indireta)
 *   store:    desempilha no destino (ARMZ; parametro ref -> ARMI)
 */
static void pushaddr(int k) {
    const Simbolo *s=ts_get(k);
    gen_emit("%s %d,%d",s->categoria==TS_PARAM_REF?"CRVL":"CREN",s->nivel,s->endereco);
}
static void load(int k) {
    const Simbolo *s=ts_get(k);
    gen_emit("%s %d,%d",s->categoria==TS_PARAM_REF?"CRVI":"CRVL",s->nivel,s->endereco);
}
static void store(int k) {
    const Simbolo *s=ts_get(k);
    gen_emit("%s %d,%d",s->categoria==TS_PARAM_REF?"ARMI":"ARMZ",s->nivel,s->endereco);
}
/* Unicode UTF-8: a MEPA armazena codigos por caractere, nao por byte. */
/*conta caracteres UTF-8 (a MEPA guarda um codigo por caractere, nao por byte)*/
static int utf8_info(const char *str, int *first) {
    const unsigned char *p=(const unsigned char *)str;
    int count=0;
    *first=0;
    while (*p) {
        unsigned cp; int n;
        if (*p<0x80) {cp=*p; n=1;}
        else if ((*p&0xE0)==0xC0) {cp=*p&0x1F; n=2;}
        else if ((*p&0xF0)==0xE0) {cp=*p&0x0F; n=3;}
        else if ((*p&0xF8)==0xF0) {cp=*p&0x07; n=4;}
        else return -1;
        for(int j=1;j<n;j++) {
            if (!p[j] || (p[j]&0xC0)!=0x80) return -1;
            cp=(cp<<6)|(p[j]&0x3F);
        }
        if((n==2&&cp<0x80)||(n==3&&cp<0x800)||(n==4&&cp<0x10000)||
           (cp>=0xD800&&cp<=0xDFFF)||cp>0x10FFFF) return -1;
        if(count==0) *first=(int)cp;
        count++; p+=n;
    }
    return count;
}
/*emite CRST "texto", escapando aspas, barra e quebras de linha*/
static void emite_str(const char *str) {
    char buf[4096];
    size_t j=0,i,n=strlen(str);
    buf[j++]='"';
    for(i=0;i<n;i++){
        char c=str[i];
        if(c=='"'||c=='\\'){
            buf[j++]='\\';
            buf[j++]=c;
        }
        else if(c=='\n'){
            buf[j++]='\\';
            buf[j++]='n';
        }
        else if(c=='\t'){
            buf[j++]='\\';
            buf[j++]='t';
        }
        else if(c=='\r'){
            buf[j++]='\\';
            buf[j++]='r';
        }
        else buf[j++]=c;
    }
    buf[j++]='"';
    buf[j]=0;
    gen_emit("CRST %s",buf);
}
/* ------------------------------------------------------------------ */
/* 2. Declaracoes e tipos                                              */
/* ------------------------------------------------------------------ */

/* <type> ::= ("sINT" | "sLOGIC" | "sCHR" | "sSTR") ["[" "sCTEINT" "]"] */
static TipoSimbolo tipo(int *tam) {
    TipoSimbolo t;
    entrar("type");
    if(aceitar(sINT))t=TS_INT;
    else if(aceitar(sLOGIC))t=TS_LOGIC;
    else if(aceitar(sCHR))t=TS_CHR;
    else if(aceitar(sSTR))t=TS_STR;
    else{
        diag_error(atual.linha,"tipo esperado: int, logic, chr ou str");
        t=TS_VOID;
    }
    *tam=0;
    if(aceitar(sABRECOL)){
        Token n=consumir(sCTEINT);
        *tam=inteiro(n);
        if(*tam<1)diag_error(n.linha,"tamanho do vetor deve ser positivo");
        consumir(sFECHACOL);
        if(t==TS_STR)diag_error(n.linha,"str nao admite dimensao vetorial");
        t=t==TS_INT?TS_INT_VET:t==TS_LOGIC?TS_LOGIC_VET:TS_CHR_VET;
    }
    sair("type");
    return t;
}
/*
 * <decls> ::= <id> {"," <id>} "sKIND" <type> ";"
 * UNICIDADE: ts_insert recusa nome repetido no mesmo escopo.
 * Cada variavel recebe o proximo endereco livre (vetor ocupa n+1, str 256).
 */
static void decls(CategoriaSimbolo cat) {
    Token ids[MAX_IDS];
    int n=0,i,cap;
    TipoSimbolo t;
    entrar("decls");
    do{
        if(n==MAX_IDS)diag_error(atual.linha,"identificadores demais na declaracao");
        ids[n++]=consumir(sIDENTIF);
    }
    while(aceitar(sVIRGULA));
    consumir(sKIND);
    t=tipo(&cap);
    consumir(sPONTOVIRGULA);
    for(i=0;i<n;i++){
        int idx=ts_insert(ids[i].lexema,cat,t,cap,ids[i].linha);
        int *total = cat == TS_VAR_GLOBAL ? &globais : &locais;
        int64_t celulas = t == TS_STR ? STR_CELLS : vetor(t) ? (int64_t)cap + 1 : 1;
        int slot = reserva_celulas(total, celulas, ids[i].linha);
        ts_set_addr(idx,slot,cat==TS_VAR_GLOBAL?0:1);
    }
    sair("decls");
}
/* <gvars> ::= "sGLOBVARS" <decls>{<decls>} */
static void gvars(void) {
    entrar("gvars");
    consumir(sGLOBVARS);
    if(atual.categoria!=sIDENTIF)diag_error(atual.linha,"globvars exige declaracoes");
    while(atual.categoria==sIDENTIF)decls(TS_VAR_GLOBAL);
    sair("gvars");
}
/* <lvars> ::= "sLOCVARS" <decls>{<decls>} */
static void lvars(void) {
    entrar("lvars");
    consumir(sLOCVARS);
    if(atual.categoria!=sIDENTIF)diag_error(atual.linha,"locvars exige declaracoes");
    while(atual.categoria==sIDENTIF)decls(TS_VAR_LOCAL);
    sair("lvars");
}
static TipoSimbolo expr(void);
/* ------------------------------------------------------------------ */
/* 3. Vetores e chamadas                                               */
/* ------------------------------------------------------------------ */

/*
 * <vetr> ::= <id> "[" ("sCTEINT" | <id>) "]"   (a partir do colchete)
 * Verifica o indice antes de calcular o endereco do elemento.
 */
static TipoSimbolo indice(int limite,int vet) {
    Token t;
    entrar("vetr");
    consumir(sABRECOL);
    if(atual.categoria==sCTEINT){
        t=consumir(sCTEINT);
        int value=inteiro(t);
        if(value<1 || value>limite)diag_error(t.linha,"indice %d fora do intervalo 1..%d",value,limite);
        pushaddr(vet);
        gen_emit("CRCT %d",value);
    }
    else if(atual.categoria==sIDENTIF){
        int k,ok;
        t=consumir(sIDENTIF);
        k=variavel(t);
        exige(ts_get(k)->tipo,TS_INT,t.linha);
        load(k);
        gen_emit("CRCT 1");
        gen_emit("CMME");
        ok=gen_new_label();
        gen_emit("DSVF L%d",ok);
        gen_emit("PARA");
        gen_label(ok);
        load(k);
        gen_emit("CRCT %d",limite);
        gen_emit("CMMA");
        ok=gen_new_label();
        gen_emit("DSVF L%d",ok);
        gen_emit("PARA");
        gen_label(ok);
        pushaddr(vet);
        load(k);
    }
    else diag_error(atual.linha,"indice de vetor deve ser inteiro ou identificador");
    consumir(sFECHACOL);
    gen_emit("INDX 1");
    sair("vetr");
    return TS_INT;
}

/*
 * Um argumento de chamada. VALIDACAO DE PARAMETROS: o tipo precisa bater
 * com a assinatura; parametro ref exige variavel e empilha o endereco.
 */
static TipoSimbolo argumento(int idx,int p) {
    TipoSimbolo t;
    Token id;
    int linha=atual.linha;
    const Simbolo *s;
    if(p>=MAX_ARGS)diag_error(linha,"argumentos demais");
    if(sig_ref[idx][p]){
        if(atual.categoria!=sIDENTIF)diag_error(linha,"argumento %d exige variavel por referencia",p+1);
        id=consumir(sIDENTIF);
        int k=variavel(id);
        s=ts_get(k);
        t=s->tipo;
        if(atual.categoria==sABRECOL){
            if(!vetor(t))diag_error(id.linha,"\"%s\" nao e vetor",id.lexema);
            indice(s->extra,k);
            t=base(t);
        }
        else pushaddr(k);
        igual(sig_tipo[idx][p],t,linha);
        if(vetor(t) && sig_dim[idx][p]!=s->extra)
            diag_error(id.linha,"dimensao incompativel: esperado %d, recebido %d",sig_dim[idx][p],s->extra);
        return t;
    }
    t=expr();
    igual(sig_tipo[idx][p],t,linha);
    return t;
}
/*
 * <call> ::= <id> "(" [<expr> {"," <expr>}] ")"
 * Confere se o nome e sub-rotina e a quantidade de argumentos.
 * Funcao: AMEM antes dos argumentos (espaco do retorno); CHPR no fim.
 */
static TipoSimbolo chamada(Token id,int expressao) {
    int k=resolver(id),n=0;
    const Simbolo *s=ts_get(k);
    int qtd;
    entrar("call");
    if(s->categoria!=TS_FUNC&&s->categoria!=TS_PROC)diag_error(id.linha,"\"%s\" nao e sub-rotina",id.lexema);
    if(expressao && s->categoria!=TS_FUNC)diag_error(id.linha,"procedimento \"%s\" nao retorna valor",id.lexema);
    if(!s->label)diag_error(id.linha,"sub-rotina \"%s\" sem entrada MEPA",id.lexema);
    qtd=sig_count[k];
    if(s->categoria==TS_FUNC)gen_emit("AMEM %d",s->tipo==TS_STR?STR_CELLS:1);
    consumir(sABREPAR);
    if(atual.categoria!=sFECHAPAR){
        do{
            if(n>=qtd)diag_error(id.linha,"sub-rotina \"%s\" exige %d argumento(s), recebeu mais",id.lexema,qtd);
            argumento(k,n++);
        }
        while(aceitar(sVIRGULA));
    }
    consumir(sFECHAPAR);
    if(n!=qtd)diag_error(id.linha,"sub-rotina \"%s\" exige %d argumento(s), recebeu %d",id.lexema,qtd,n);
    gen_emit("CHPR L%d,1",s->label);
    if(!expressao && s->categoria==TS_FUNC)gen_emit("DMEM %d",s->tipo==TS_STR?STR_CELLS:1);
    sair("call");
    return s->tipo;
}
/* ------------------------------------------------------------------ */
/* 4. Expressoes (cada funcao devolve o tipo da subexpressao)          */
/* ------------------------------------------------------------------ */

/*
 * <fact> ::= <elem> | "sNEG" <fact> | "sSUBRAT" <fact> | "(" <expr> ")"
 * <elem> ::= <litl> | <id> | <vetr> | <call>
 * ~ exige logic (NEGA); - unario exige int (INVR).
 */
/* Avalia constantes para a analise de retorno sem alterar o codigo emitido. */
static void calcula_constante(CategoriaToken op, Constante esquerda) {
    int64_t a = esquerda.valor, b = constante.valor, valor;
    int conhecida = esquerda.conhecido && constante.conhecido;
    constante = (Constante){0, 0};
    if (!conhecida) return;

    switch (op) {
        case sSOMA: valor = a + b; break;
        case sSUBRAT: valor = a - b; break;
        case sMULT: valor = a * b; break;
        /* A divisao fica a cargo da VM, inclusive erros e arredondamento. */
        case sMAIOR: valor = a > b; break;
        case sMAIORIGUAL: valor = a >= b; break;
        case sMENOR: valor = a < b; break;
        case sMENORIGUAL: valor = a <= b; break;
        case sIGUAL: valor = a == b; break;
        case sDIFERENTE: valor = a != b; break;
        case sAND: valor = a && b; break;
        case sOR: valor = a || b; break;
        default: return;
    }
    if (valor >= INT_MIN && valor <= INT_MAX)
        constante = (Constante){1, (int)valor};
}

static TipoSimbolo fact(void) {
    TipoSimbolo t=TS_VOID;
    Token id;
    int ln=atual.linha;
    entrar("fact");
    constante = (Constante){0, 0};
    if(aceitar(sNEG)){
        t=fact();
        exige(t,TS_LOGIC,ln);
        gen_emit("NEGA");
        if (constante.conhecido) constante.valor = !constante.valor;
    }
    else if(aceitar(sSUBRAT)){
        t=fact();
        exige(t,TS_INT,ln);
        gen_emit("INVR");
        Constante zero = {1, 0};
        calcula_constante(sSUBRAT, zero);
    }
    else if(aceitar(sABREPAR)){
        t=expr();
        consumir(sFECHAPAR);
    }
    else if(atual.categoria==sCTEINT){
        id=consumir(sCTEINT);
        int valor = inteiro(id);
        gen_emit("CRCT %d",valor);
        constante = (Constante){1, valor};
        t=TS_INT;
    }
    else if(atual.categoria==sCTECHAR){
        id=consumir(sCTECHAR);
        int cp, count=utf8_info(id.lexema,&cp);
        if(count!=1)diag_error(id.linha,"literal de caractere invalido");
        gen_emit("CRCT %d",cp);
        constante = (Constante){1, cp};
        t=TS_CHR;
    }
    else if(atual.categoria==sSTRING){
        int len,cp;
        id=consumir(sSTRING);
        len=utf8_info(id.lexema,&cp);
        if(len<0)diag_error(id.linha,"string contem UTF-8 invalido");
        if(len>=STR_CELLS)diag_error(id.linha,"string excede %d caracteres",STR_CELLS-1);
        emite_str(id.lexema);
        if(len+1<STR_CELLS)gen_emit("AMEM %d",STR_CELLS-len-1);
        t=TS_STR;
    }
    else if(atual.categoria==sIDENTIF){
        int k;
        id=consumir(sIDENTIF);
        if(atual.categoria==sABREPAR)t=chamada(id,1);
        else{
            k=variavel(id);
            t=ts_get(k)->tipo;
            if(atual.categoria==sABRECOL){
                if(!vetor(t))diag_error(id.linha,"\"%s\" nao e vetor",id.lexema);
                indice(ts_get(k)->extra,k);
                gen_emit("CONT");
                t=base(t);
            }
            else if(t==TS_STR){
                pushaddr(k);
                gen_emit("CRVM %d",STR_CELLS);
            }
            else if(vetor(t))diag_error(id.linha,"vetor exige indice na expressao");
            else load(k);
        }
        constante = (Constante){0, 0};
    }
    else diag_error(atual.linha,"operando invalido %s \"%s\"",lex_nome_categoria(atual.categoria),atual.lexema);
    sair("fact");
    return t;
}
/* <earip> ::= <fact> {("*" | "//") <fact>}      int x int -> int */
static TipoSimbolo earip(void){
    TipoSimbolo t,r;
    entrar("earip");
    t=fact();
    while(atual.categoria==sMULT||atual.categoria==sDIV){
        CategoriaToken op=atual.categoria;
        int ln=atual.linha;
        avancar();
        Constante esquerda = constante;
        r=fact();
        exige(t,TS_INT,ln);
        exige(r,TS_INT,ln);
        gen_emit(op==sMULT?"MULT":"DIVI");
        calcula_constante(op, esquerda);
        t=TS_INT;
    }
    sair("earip");
    return t;
}
/* <earit> ::= <earip> {("+" | "-") <earip>}    int x int -> int */
static TipoSimbolo earit(void){
    TipoSimbolo t,r;
    entrar("earit");
    t=earip();
    while(atual.categoria==sSOMA||atual.categoria==sSUBRAT){
        CategoriaToken op=atual.categoria;
        int ln=atual.linha;
        avancar();
        Constante esquerda = constante;
        r=earip();
        exige(t,TS_INT,ln);
        exige(r,TS_INT,ln);
        gen_emit(op==sSOMA?"SOMA":"SUBT");
        calcula_constante(op, esquerda);
        t=TS_INT;
    }
    sair("earit");
    return t;
}
/* Comparacoes ordenadas precedem igualdade e diferenca (manual, p. 7). */
static TipoSimbolo comparacao(void){
    TipoSimbolo t = earit();
    while(atual.categoria==sMAIOR||atual.categoria==sMAIORIGUAL||
          atual.categoria==sMENOR||atual.categoria==sMENORIGUAL){
        CategoriaToken op = atual.categoria;
        int ln = atual.linha;
        Constante esquerda = constante;
        avancar();
        TipoSimbolo r = earit();
        igual(t,r,ln);
        if(t!=TS_INT&&t!=TS_CHR)diag_error(ln,"comparacao ordenada exige int ou chr");
        gen_emit("%s",op==sMAIOR?"CMMA":op==sMAIORIGUAL?"CMAG":op==sMENOR?"CMME":"CMEG");
        calcula_constante(op, esquerda);
        t=TS_LOGIC;
    }
    return t;
}

static TipoSimbolo erlac(void){
    entrar("erlac");
    TipoSimbolo t = comparacao();
    while(atual.categoria==sIGUAL||atual.categoria==sDIFERENTE){
        CategoriaToken op = atual.categoria;
        int ln = atual.linha;
        Constante esquerda = constante;
        avancar();
        TipoSimbolo r = comparacao();
        igual(t,r,ln);
        if(vetor(t)||t==TS_STR)diag_error(ln,"comparacao de vetores/strings nao suportada na MEPA");
        gen_emit("%s",op==sIGUAL?"CMIG":"CMDG");
        calcula_constante(op, esquerda);
        t=TS_LOGIC;
    }
    sair("erlac");
    return t;
}
/* <elogc> ::= <erlac> {"&" <erlac>}       logic x logic -> logic */
static TipoSimbolo elogc(void){
    TipoSimbolo t,r;
    entrar("elogc");
    t=erlac();
    while(aceitar(sAND)){
        int ln=atual.linha;
        Constante esquerda = constante;
        r=erlac();
        exige(t,TS_LOGIC,ln);
        exige(r,TS_LOGIC,ln);
        gen_emit("CONJ");
        calcula_constante(sAND, esquerda);
        t=TS_LOGIC;
    }
    sair("elogc");
    return t;
}
/* <expr> ::= <elogc> {"|" <elogc>}        logic x logic -> logic */
static TipoSimbolo expr(void){
    TipoSimbolo t,r;
    entrar("expr");
    t=elogc();
    while(aceitar(sOR)){
        int ln=atual.linha;
        Constante esquerda = constante;
        r=elogc();
        exige(t,TS_LOGIC,ln);
        exige(r,TS_LOGIC,ln);
        gen_emit("DISJ");
        calcula_constante(sOR, esquerda);
        t=TS_LOGIC;
    }
    sair("expr");
    return t;
}
static int cmd(void);
/* ------------------------------------------------------------------ */
/* 5. Comandos                                                         */
/* ------------------------------------------------------------------ */

/*sequencia {<cmd> ";"}*/
static int cmds(void){
    int garante=0;
    while(inicia(atual.categoria)){
        int r=cmd();
        if(r)garante=1;
        consumir(sPONTOVIRGULA);
    }
    return garante;
}
/* <echo> ::= "sECHO" "(" <elem> {"," <elem>} ")"   int/logic: IMPR, chr: IMPC, str: IMPS */
static void echo(void){
    TipoSimbolo t;
    entrar("echo");
    consumir(sECHO);
    consumir(sABREPAR);
    do{
        t=expr();
        if(t==TS_STR)gen_emit("IMPS %d",STR_CELLS);
        else if(t==TS_CHR)gen_emit("IMPC");
        else gen_emit("IMPR");
    }
    while(aceitar(sVIRGULA));
    consumir(sFECHAPAR);
    sair("echo");
}
/* <get> ::= "sGET" "(" (<id> | <vetr>) ")"   LEIT + ARMZ (ou ARVM 1 no vetor) */
static void get(void){
    Token id;
    int k,ind=0;
    TipoSimbolo t;
    entrar("get");
    consumir(sGET);
    consumir(sABREPAR);
    id=consumir(sIDENTIF);
    k=variavel(id);
    t=ts_get(k)->tipo;
    if(atual.categoria==sABRECOL){
        if(!vetor(t))diag_error(id.linha,"identificador nao e vetor");
        indice(ts_get(k)->extra,k);
        ind=1;
        t=base(t);
    }
    else if(vetor(t)||t==TS_STR)diag_error(id.linha,"get exige variavel escalar ou elemento de vetor");
    consumir(sFECHAPAR);
    gen_emit("LEIT");
    if(ind)gen_emit("ARVM 1");
    else store(k);
    sair("get");
}
/*
 * <case> ::= "sCASE" "(" <expr> ")" {<cmd> ";"} ["sOTHERWISE" {<cmd> ";"}] "sEND"
 * Gabarito do if: expr / DSVF Lsenao / cmds / DSVS Lfim / Lsenao: cmds / Lfim:
 */
static int case_cmd(void){
    int end,else_l;
    TipoSimbolo t;
    entrar("case");
    consumir(sCASE);
    consumir(sABREPAR);
    t=expr();
    exige(t,TS_LOGIC,atual.linha);
    consumir(sFECHAPAR);
    else_l=gen_new_label();
    end=gen_new_label();
    gen_emit("DSVF L%d",else_l);
    int then_ret=cmds(),else_ret=0;
    gen_emit("DSVS L%d",end);
    gen_label(else_l);
    if(aceitar(sOTHERWISE))else_ret=cmds();
    consumir(sEND);
    gen_label(end);
    sair("case");
    return then_ret && else_ret;
}
/*
 * <chse> ::= "sCHOOSE" "(" <expr> ")" <mlst> "sEND"
 * O valor da expressao fica numa posicao temporaria (AMEM 1) e e comparado
 * com cada constante do match (CMIG); o primeiro que casar executa e sai.
 */
static int choose_cmd(void){
    int slot,end,found,match_l;
    Token v;
    TipoSimbolo t;
    entrar("chse");
    consumir(sCHOOSE);
    consumir(sABREPAR);
    slot=reserva_celulas(&locais,1,atual.linha);
    gen_emit("AMEM 1");
    t=expr();
    exige(t,TS_INT,atual.linha);
    consumir(sFECHAPAR);
    gen_emit("ARMZ 1,%d",slot);
    if(atual.categoria!=sMATCH)diag_error(atual.linha,"choose exige match");
    end=gen_new_label();
    int all_ret=1,has_others=0;
    while(atual.categoria==sMATCH){
        found=gen_new_label();
        match_l=gen_new_label();
        entrar("mtch");
        consumir(sMATCH);
        do{
            int neg=aceitar(sSUBRAT);
            v=consumir(sCTEINT);
            gen_emit("CRVL 1,%d",slot);
            gen_emit("CRCT %d",neg?-inteiro(v):inteiro(v));
            gen_emit("CMIG");
            gen_emit("DSVF L%d",match_l);
            gen_emit("DSVS L%d",found);
            gen_label(match_l);
            match_l=gen_new_label();
        }
        while(aceitar(sVIRGULA));
        gen_emit("DSVS L%d",match_l);
        gen_label(found);
        consumir(sIMPLIC);
        if(!cmd())all_ret=0;
        consumir(sPONTOVIRGULA);
        gen_emit("DSVS L%d",end);
        gen_label(match_l);
        sair("mtch");
    }
    if(aceitar(sOTHERS)){
        has_others=1;
        entrar("othr");
        consumir(sIMPLIC);
        if(!cmd())all_ret=0;
        consumir(sPONTOVIRGULA);
        sair("othr");
    }
    consumir(sEND);
    gen_label(end);
    gen_emit("DMEM 1");
    locais--;
    sair("chse");
    return has_others && all_ret;
}
/*limite do for: identificador int ou constante (com sinal)*/
static void valor_for(void){
    Token id;
    int k,neg=aceitar(sSUBRAT);
    if(atual.categoria==sIDENTIF){
        if(neg)diag_error(atual.linha,"negativo de variavel nao permitido neste contexto");
        id=consumir(sIDENTIF);
        k=variavel(id);
        exige(ts_get(k)->tipo,TS_INT,id.linha);
        load(k);
    }
    else{
        id=consumir(sCTEINT);
        gen_emit("CRCT %d",neg?-inteiro(id):inteiro(id));
    }
}
/*
 * <for> ::= "sFOR" <id> "sFROM" (<id>|"sCTEINT") "sTO" (<id>|"sCTEINT")
 *           ["sBY" "sCTEINT"] "sDO" {<cmd> ";"} "sEND"
 * O limite e calculado uma vez e guardado num temporario.
 * Passo positivo testa CMEG (<=); passo negativo testa CMAG (>=).
 */
static void for_cmd(void){
    Token id,passo;
    int k,step=1,loop,end;
    entrar("for");
    consumir(sFOR);
    id=consumir(sIDENTIF);
    k=variavel(id);
    exige(ts_get(k)->tipo,TS_INT,id.linha);
    consumir(sFROM);
    valor_for();
    store(k);
    consumir(sTO);
    int limite=reserva_celulas(&locais,1,atual.linha);
    gen_emit("AMEM 1");
    valor_for();
    gen_emit("ARMZ 1,%d",limite);
    if(aceitar(sBY)){
        int neg=aceitar(sSUBRAT);
        passo=consumir(sCTEINT);
        step=inteiro(passo)*(neg?-1:1);
        if(step==0)diag_error(passo.linha,"passo do for nao pode ser zero");
    }
    consumir(sDO);
    loop=gen_new_label();
    end=gen_new_label();
    gen_label(loop);
    load(k);
    gen_emit("CRVL 1,%d",limite);
    gen_emit("%s",step>0?"CMEG":"CMAG");
    gen_emit("DSVF L%d",end);
    cmds();
    load(k);
    gen_emit("CRCT %d",step);
    gen_emit("SOMA");
    store(k);
    gen_emit("DSVS L%d",loop);
    consumir(sEND);
    gen_label(end);
    gen_emit("DMEM 1");
    locais--;
    sair("for");
}
/*
 * <whle> ::= "sWHILE" "(" <expr> ")" "sDO" {<cmd> ";"} "sEND"
 * Gabarito: Linicio: expr / DSVF Lfim / cmds / DSVS Linicio / Lfim:
 */
static int while_cmd(void){
    int begin,end;
    TipoSimbolo t;
    entrar("whle");
    consumir(sWHILE);
    consumir(sABREPAR);
    begin=gen_new_label();
    end=gen_new_label();
    gen_label(begin);
    t=expr();
    int sempre_verdadeira = constante.conhecido && constante.valor;
    exige(t,TS_LOGIC,atual.linha);
    consumir(sFECHAPAR);
    consumir(sDO);
    gen_emit("DSVF L%d",end);
    int corpo_retorna = cmds();
    consumir(sEND);
    gen_emit("DSVS L%d",begin);
    gen_label(end);
    sair("whle");
    return sempre_verdadeira && corpo_retorna;
}
/*
 * <rept> ::= "sREPEAT" {<cmd> ";"} "sUNTIL" "(" <expr> ")"
 * Repete enquanto a condicao for falsa: Linicio: cmds / expr / DSVF Linicio
 */
static int repeat_cmd(void){
    int begin;
    TipoSimbolo t;
    entrar("rept");
    consumir(sREPEAT);
    begin=gen_new_label();
    gen_label(begin);
    int guaranteed=cmds();
    consumir(sUNTIL);
    consumir(sABREPAR);
    t=expr();
    exige(t,TS_LOGIC,atual.linha);
    consumir(sFECHAPAR);
    gen_emit("DSVF L%d",begin);
    sair("rept");
    return guaranteed;
}
/*
 * <ret> ::= "sRETURN" <elem>
 * So dentro de func; o tipo precisa ser o do retorno. Grava o valor na
 * posicao reservada pelo chamador (-(nparam+5)) e encerra com RTPR.
 */
static void return_cmd(void){
    TipoSimbolo t;
    int ln=atual.linha;
    entrar("ret");
    consumir(sRETURN);
    if(!dentro_func)diag_error(ln,"return somente permitido em funcao");
    TipoSimbolo ret=ts_get(func_idx)->tipo;
    if(ret==TS_STR)gen_emit("CREN 1,%d",-(nparam+4+STR_CELLS));
    t=expr();
    igual(ret,t,ln);
    if(ret==TS_STR)gen_emit("ARVM %d",STR_CELLS);
    else gen_emit("ARMZ 1,%d",-(nparam+5));
    if(locais)gen_emit("DMEM %d",locais);
    gen_emit("RTPR %d",nparam);
    sair("ret");
}
/*
 * Comando iniciado por identificador:
 *   <call> ::= <id> "(" ... ")"            (proximo token e "(")
 *   <atr>  ::= (<id> | <vetr>) "sATRIB" <elem>
 * Na atribuicao, o tipo da expressao precisa ser o da variavel.
 */
static void id_cmd(void){
    Token id;
    int k,ind=0;
    TipoSimbolo t,r;
    entrar("atr");
    id=consumir(sIDENTIF);
    if(atual.categoria==sABREPAR){
        chamada(id,0);
        sair("atr");
        return;
    }
    k=variavel(id);
    t=ts_get(k)->tipo;
    if(atual.categoria==sABRECOL){
        if(!vetor(t))diag_error(id.linha,"\"%s\" nao e vetor",id.lexema);
        indice(ts_get(k)->extra,k);
        t=base(t);
        ind=1;
    }
    else if(vetor(t))diag_error(id.linha,"atribuicao de vetor completo nao suportada");
    else if(t==TS_STR)pushaddr(k);
    consumir(sATRIB);
    r=expr();
    igual(t,r,id.linha);
    if(ind)gen_emit("ARVM 1");
    else if(t==TS_STR)gen_emit("ARVM %d",STR_CELLS);
    else store(k);
    sair("atr");
}
/* <cmd> ::= <echo>|<get>|<case>|<chse>|<for>|<whle>|<rept>|<call>|<ret>|<atr> */
static int cmd(void){
    int guaranteed=0;
    entrar("cmd");
    switch(atual.categoria){
        case sECHO:echo();
        break;
        case sGET:get();
        break;
        case sCASE:guaranteed=case_cmd();
        break;
        case sCHOOSE:guaranteed=choose_cmd();
        break;
        case sFOR:for_cmd();
        break;
        case sWHILE:guaranteed=while_cmd();
        break;
        case sREPEAT:guaranteed=repeat_cmd();
        break;
        case sRETURN:return_cmd();guaranteed=1;
        break;
        case sIDENTIF:id_cmd();
        break;
        default:diag_error(atual.linha,"comando invalido");
    }
    sair("cmd");
    return guaranteed;
}
/* ------------------------------------------------------------------ */
/* 6. Blocos, sub-rotinas e programa                                   */
/* ------------------------------------------------------------------ */

/* <bco> ::= "sSTART" {<cmd> ";"} "sEND"   (abre o escopo .block#N) */
static int bco(void){
    char scope[TS_MAX_NOME];
    entrar("bco");
    blocos_sub++;
    snprintf(scope,sizeof(scope),"%s.block#%d",sub_atual,blocos_sub);
    ts_abre_escopo(scope);
    consumir(sSTART);
    int guaranteed=cmds();
    consumir(sEND);
    ts_fecha_escopo();
    sair("bco");
    return guaranteed;
}
/*
 * <param> ::= ["sREF"] <id> ":" <type> {"," ["sREF"] <id> ":" <type>}
 * Vetores sao sempre por referencia. Guarda a assinatura da sub-rotina.
 */
static void param(int idx){
    int n=0,sz;
    TipoSimbolo t;
    Token id;
    entrar("param");
    do{
        int ref=aceitar(sREF);
        id=consumir(sIDENTIF);
        consumir(sDOISPONTOS);
        t=tipo(&sz);
        if(vetor(t))ref=1;
        if(n>=MAX_ARGS)diag_error(id.linha,"parametros demais");
        sig_tipo[idx][n]=t;
        sig_ref[idx][n]=ref;
        sig_dim[idx][n]=sz;
        int p=ts_insert(id.lexema,ref?TS_PARAM_REF:TS_PARAM,t,sz,id.linha);
        idx_param[n]=p;
        n++;
    }
    while(aceitar(sVIRGULA));
    sig_count[idx]=n;
    sair("param");
}
/*grava o tamanho na posicao 0 de cada vetor declarado (CRCT n / ARMZ)*/
static void inicializa_vetores(CategoriaSimbolo cat){
    for(int k=0;k<TS_MAX_SIMBOLOS;k++){
        const Simbolo *s=ts_get(k);
        if(!s)break;
        if(s->categoria==cat&&vetor(s->tipo)&&
           (cat==TS_VAR_GLOBAL || ts_lookup_local(s->lexema)==k)){
            gen_emit("CRCT %d",s->extra);
            gen_emit("ARMZ %d,%d",s->nivel,s->endereco);
        }
    }
}
/*
 * <func> ::= "sFUNC" <id> "(" [<param>] ")" ":" <type> [<lvars>] <bco>
 * <proc> ::= "sPROC" <id> "(" [<param>] ")" [<lvars>] <bco>
 * MEPA: DSVS pula o corpo; Lsub: ENPR 1 / AMEM locais / corpo / DMEM / RTPR n.
 * Parametros ficam em enderecos negativos: o ultimo em -5, o anterior em -6...
 */
static void subrotina(void){
    Token id;
    int isfunc=atual.categoria==sFUNC,idx,sz,label,skip,i;
    char scope[TS_MAX_NOME];
    entrar(isfunc?"func":"proc");
    avancar();
    id=consumir(sIDENTIF);
    if(isfunc&&strcmp(id.lexema,"main")==0)diag_error(id.linha,"main deve ser procedimento");
    idx=ts_insert(id.lexema,isfunc?TS_FUNC:TS_PROC,TS_VOID,0,id.linha);
    label=gen_new_label();
    ts_set_label(idx,label);
    skip=gen_new_label();
    gen_emit("DSVS L%d",skip);
    gen_label(label);
    gen_emit("ENPR 1");
    snprintf(sub_atual,sizeof(sub_atual),"%s:%s",isfunc?"fn":"proc",id.lexema);
    blocos_sub=0;
    snprintf(scope,sizeof(scope),"%s.locals",sub_atual);
    ts_abre_escopo(scope);
    consumir(sABREPAR);
    if(atual.categoria!=sFECHAPAR)param(idx);
    consumir(sFECHAPAR);
    nparam=0;
    for(i=0;i<sig_count[idx];i++)nparam+=(sig_tipo[idx][i]==TS_STR&&!sig_ref[idx][i])?STR_CELLS:1;
    ts_set_extra(idx,sig_count[idx]);
    int offset=-(nparam+4);
    for(i=0;i<sig_count[idx];i++){
        ts_set_addr(idx_param[i],offset,1);
        offset+=(sig_tipo[idx][i]==TS_STR&&!sig_ref[idx][i])?STR_CELLS:1;
    }
    if(isfunc){
        consumir(sDOISPONTOS);
        TipoSimbolo ret=tipo(&sz);
        if(vetor(ret))diag_error(id.linha,"retorno de vetor ainda nao suportado");
        ts_set_tipo(idx,ret);
    }
    else if(strcmp(id.lexema,"main")==0){
        if(nparam)diag_error(id.linha,"main nao pode receber parametros");
        viu_main=1;
    }
    func_idx=idx;
    dentro_func=isfunc;
    locais=0;
    if(atual.categoria==sLOCVARS)lvars();
    if(locais)gen_emit("AMEM %d",locais);
    inicializa_vetores(TS_VAR_LOCAL);
    int retorno_garantido=bco();
    if(isfunc && !retorno_garantido) diag_error(id.linha,"funcao \"%s\" exige return em todos os caminhos",id.lexema);
    if(locais)gen_emit("DMEM %d",locais);
    gen_emit("RTPR %d",nparam);
    ts_fecha_escopo();
    gen_label(skip);
    sair(isfunc?"func":"proc");
}
/*
 * <prg> ::= [<gvars>] {<subs>} <princ>
 * MEPA: INPP / AMEM globais / DSVS Lentrada / sub-rotinas /
 *       Lentrada: CHPR main / DMEM globais / PARA   (FIM e escrito por gen_save)
 */
void parse_program(Lex *lex){
    lexico=lex;
    eof_registrado=0;
    trace_depth=0;
    viu_main=0;
    globais=0;
    sub_atual[0]=0;
    memset(sig_count,0,sizeof(sig_count));
    ts_init();
    avancar();
    entrar("prg");
    gen_emit("INPP");
    if(atual.categoria==sGLOBVARS)gvars();
    if(globais)gen_emit("AMEM %d",globais);
    inicializa_vetores(TS_VAR_GLOBAL);
    int entry=gen_new_label();
    gen_emit("DSVS L%d",entry);
    int main_label=0;
    while(atual.categoria==sFUNC||atual.categoria==sPROC){
        if(viu_main)diag_error(atual.linha,"nenhuma sub-rotina depois de main");
        int was=viu_main;
        subrotina();
        if(!was&&viu_main){
            int idx=ts_lookup("main");
            main_label=ts_get(idx)->label;
        }
    }
    if(!viu_main)diag_error(atual.linha,"programa nao define proc main");
    consumir(sEOF);
    gen_label(entry);
    gen_emit("CHPR L%d,0",main_label);
    if(globais)gen_emit("DMEM %d",globais);
    gen_emit("PARA");
    sair("prg");
}
