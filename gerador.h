#ifndef GERADOR_H
#define GERADOR_H

/*
 * Modulo gerador - emissao formatada das instrucoes MEPA e gerenciamento
 * de rotulos (labels). O parser chama estas funcoes durante a analise
 * (Traducao Dirigida a Sintaxe); nenhum outro modulo escreve no .mepa.
 *
 * Saida: <fonte>.mepa, ao lado do arquivo fonte.
 *
 * Formato de cada linha:
 *        CRVL 0,1          (instrucao sem rotulo, recuada)
 *   L1:  NADA              (instrucao com rotulo)
 * IMPORTANTE: o interpretador NAO aceita espaco entre os dois parametros
 * ("CRVL 0,1" e nao "CRVL 0, 1").
 */

/*cria o arquivo <fonte>.mepa; devolve 1 em sucesso*/
int ger_abrir(const char *arquivo_fonte);

/*
 * Emite uma instrucao. Qualquer argumento pode ser NULL:
 *   gera_instr_mepa(NULL, "INPP", NULL, NULL);   ->      INPP
 *   gera_instr_mepa(NULL, "CRVL", "0", "1");     ->      CRVL 0,1
 *   gera_instr_mepa("L1", "NADA", NULL, NULL);   -> L1:  NADA
 */
void gera_instr_mepa(const char *rotulo, const char *mnemonico,
                     const char *parametro1, const char *parametro2);

/*atalho para instrucoes com parametros inteiros (CRCT 5, CRVL 0,1, AMEM 3...)*/
void gera_instr_int(const char *rotulo, const char *mnemonico, int qtd_params,
                    int parametro1, int parametro2);

/*devolve um rotulo novo e unico: "L1", "L2", ... (string propria, nao precisa liberar)*/
const char *novo_rotulo(void);

/*converte inteiro em texto (buffer rotativo, pode ser usado 2x na mesma chamada)*/
const char *int2str(int valor);

/*escreve FIM e fecha o arquivo (encerramento normal)*/
void ger_fechar(void);

/*fecha e APAGA o .mepa incompleto (chamado quando ha erro de compilacao)*/
void ger_descartar(void);

#endif
