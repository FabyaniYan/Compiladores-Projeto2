#ifndef GERADOR_H
#define GERADOR_H

/*
 * Modulo gerador - emissao das instrucoes MEPA e controle de rotulos.
 *
 * As instrucoes sao acumuladas em memoria durante a analise e so viram
 * o arquivo <fonte>.mepa quando a compilacao termina sem erro. Assim um
 * programa com erro nunca deixa um .mepa incompleto para tras.
 *
 * Formato: parametros separados por virgula SEM espaco (ex.: "CRVL 0,1"),
 * exigencia do interpretador MEPA usado na disciplina.
 */

/*zera a lista de instrucoes e o contador de rotulos*/
void gen_reset(void);

/*apaga um <fonte>.mepa antigo, antes de comecar a compilar*/
void gen_invalidate(const char *source);

/*acrescenta uma instrucao, no estilo printf (ex.: gen_emit("CRVL %d,%d", n, e))*/
void gen_emit(const char *fmt, ...);

/*emite "Ln: NADA" (destino de desvios)*/
void gen_label(int n);

/*reserva um novo numero de rotulo: 1, 2, 3...*/
int gen_new_label(void);

/*grava todas as instrucoes em <fonte>.mepa, seguidas de FIM*/
void gen_save(const char *source);

#endif
