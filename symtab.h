#ifndef SYMTAB_H
#define SYMTAB_H

#include "lex.h"

/*
 * Modulo symtab - tabela de simbolos com controle de escopo.
 *
 * Fase 2: implementada como TABELA HASH com multiplos niveis de escopo
 * (exigencia do enunciado do Projeto 2).
 *
 * Os escopos espelham a estrutura da SLAC2:
 *   global                  -> secao globvars e nomes de sub-rotinas
 *   proc:main.locals        -> parametros e locvars de uma sub-rotina
 *   proc:main.block#1       -> bloco start...end da sub-rotina
 *
 * A visibilidade segue a cadeia de escopos ativos (do mais interno ao global).
 *
 * CONTRATO ENTRE AS DUAS PESSOAS:
 *   - Pessoa 1 implementa este modulo e PREENCHE os campos do Simbolo
 *     (categoria, tipo, nivel, endereco, tamanho, parametros).
 *   - Pessoa 2 so LE os campos (via ts_lookup + ts_get) para checar tipos
 *     e gerar CRVL/ARMZ/CREN/CHPR.
 *   Se algum campo mudar, as duas precisam combinar antes.
 */

#define TS_MAX_SIMBOLOS 1024
#define TS_MAX_ESCOPOS  256
#define TS_MAX_NOME     512
#define TS_TAM_HASH     211   /*numero de baldes da tabela hash (primo)*/
#define TS_MAX_PARAMS   32    /*parametros por sub-rotina*/
#define TS_TAM_STR      256   /*str: posicao 0 = comprimento + 255 caracteres*/

/*categorias de identificador registradas na TS*/
typedef enum {
    TS_VAR_GLOBAL,
    TS_VAR_LOCAL,
    TS_PARAM,
    TS_PARAM_REF,
    TS_FUNC,
    TS_PROC
} CategoriaSimbolo;

/*
 * Tipos de dado da SLAC2.
 * Os variantes VET representam vetores; TS_STR e o novo tipo string (str).
 * TS_INDEF e usado quando o tipo ainda nao e conhecido ou houve erro.
 */
typedef enum {
    TS_INT, TS_LOGIC, TS_CHR, TS_STR,
    TS_INT_VET, TS_LOGIC_VET, TS_CHR_VET,
    TS_VOID,
    TS_INDEF
} TipoSimbolo;

typedef struct {
    char lexema[TOKEN_LEXEMA_MAX];
    CategoriaSimbolo categoria;
    TipoSimbolo tipo;
    int escopo;     /*indice do escopo em que foi declarado*/
    int extra;      /*tamanho do vetor ou quantidade de parametros (relatorio .ts)*/

    /*--- campos novos da Fase 2 (usados na geracao de codigo MEPA) ---*/
    int nivel;      /*nivel lexico MEPA: 0 = global e main, 1 = demais sub-rotinas*/
    int endereco;   /*deslocamento: variaveis >= 0; parametros negativos (-5, -6...)*/
    int tamanho;    /*posicoes de memoria ocupadas (1, n+1 para vetor, 256 para str)*/
    char rotulo[16];/*rotulo MEPA da sub-rotina (ex.: "L3"); vazio para variaveis*/
    int qtd_param;  /*sub-rotinas: quantidade de parametros*/
    TipoSimbolo tipos_param[TS_MAX_PARAMS]; /*sub-rotinas: tipo de cada parametro*/
    int ref_param[TS_MAX_PARAMS];           /*sub-rotinas: 1 se o parametro e ref*/

    int proximo;    /*encadeamento dentro do mesmo balde da hash; -1 = fim*/
} Simbolo;

/*prepara a tabela e cria o escopo global*/
void ts_init(void);

/*
 * Abre um novo escopo, filho do escopo atual.
 * O nome recebido e o caminho completo que aparecera nos relatorios,
 * por exemplo "fn:SOMA.locals" ou "proc:main.block#1".
 * nivel = nivel lexico MEPA das variaveis declaradas nesse escopo.
 */
void ts_abre_escopo(const char *nome, int nivel);

/*encerra o escopo atual, voltando ao escopo pai*/
void ts_fecha_escopo(void);

/*nome completo do escopo ativo (ex.: "proc:main.locals")*/
const char *ts_escopo_atual(void);

/*nivel lexico MEPA do escopo ativo*/
int ts_nivel_atual(void);

/*
 * Insere um identificador no escopo atual.
 * Declaracao duplicada no mesmo escopo e reportada via diag_error.
 * O endereco e calculado aqui (proxima posicao livre do escopo).
 * Devolve o indice do simbolo inserido.
 */
int ts_insert(const char *lexema, CategoriaSimbolo cat, TipoSimbolo tipo,
              int extra, int linha);

/*busca o identificador na cadeia de escopos visiveis; -1 se nao encontrado*/
int ts_lookup(const char *lexema);

/*busca restrita ao escopo atual; -1 se nao encontrado*/
int ts_lookup_local(const char *lexema);

/*acesso somente leitura a um simbolo pelo indice*/
const Simbolo *ts_get(int indice);

/*ajusta o campo extra de um simbolo ja inserido (usado na contagem de parametros)*/
void ts_set_extra(int indice, int extra);

/*ajusta o tipo de um simbolo ja inserido (tipo de retorno de funcao)*/
void ts_set_tipo(int indice, TipoSimbolo tipo);

/*grava o rotulo MEPA de uma sub-rotina*/
void ts_set_rotulo(int indice, const char *rotulo);

/*acrescenta um parametro (tipo e se e ref) na assinatura da sub-rotina*/
void ts_add_param(int indice_sub, TipoSimbolo tipo, int por_referencia);

/*
 * Depois de lidos todos os parametros, recalcula os enderecos negativos
 * (o ultimo parametro fica em -5, o penultimo em -6, ...).
 */
void ts_ajusta_enderecos_param(int indice_sub);

/*total de posicoes de memoria ocupadas pelas variaveis do escopo atual (para AMEM/DMEM)*/
int ts_memoria_escopo(void);

/*posicoes de memoria ocupadas por um tipo (int = 1, vetor = n+1, str = 256)*/
int ts_tamanho_tipo(TipoSimbolo tipo, int tamanho_vetor);

/*textos usados nos relatorios*/
const char *ts_nome_categoria(CategoriaSimbolo cat);
const char *ts_nome_tipo(TipoSimbolo tipo);

/*percorre a TS na ordem de escopo e de insercao, enviando tudo ao modulo log*/
void ts_dump(void);

#endif
