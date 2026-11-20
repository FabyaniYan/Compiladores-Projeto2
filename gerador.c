#include "gerador.h"
#include "diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 100000
#define MAX_PATH_MEPA 2048

static char *lines[MAX_LINES]; /*instrucoes ja emitidas, na ordem*/
static int count;              /*quantidade de instrucoes*/
static int labels;             /*ultimo rotulo criado*/

/*monta <fonte-sem-extensao>.mepa em path*/
static void caminho_mepa(char *path, const char *source) {
    const char *slash = strrchr(source, '/');
    const char *dot = strrchr(source, '.');
    size_t len = strlen(source);

    if (dot && (!slash || dot > slash)) len = (size_t)(dot - source);
    if (len + 6 >= MAX_PATH_MEPA) diag_error(0, "caminho de saida muito longo");
    memcpy(path, source, len);
    strcpy(path + len, ".mepa");
}

void gen_reset(void) {
    count = 0;
    labels = 0;
}

void gen_emit(const char *fmt, ...) {
    char buf[2048];
    va_list args;
    size_t n;

    if (count >= MAX_LINES) diag_error(0, "limite de instrucoes MEPA excedido");

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    n = strlen(buf) + 1;
    lines[count] = malloc(n);
    if (!lines[count]) diag_error(0, "memoria insuficiente no gerador");
    memcpy(lines[count], buf, n);
    count++;
}

int gen_new_label(void) { return ++labels; }

void gen_label(int n) { gen_emit("L%d: NADA", n); }

void gen_invalidate(const char *source) {
    char path[MAX_PATH_MEPA];
    caminho_mepa(path, source);
    /*se o arquivo nao existir, remove falha e tudo bem; erros de acesso aparecem em gen_save*/
    remove(path);
}

void gen_save(const char *source) {
    char path[MAX_PATH_MEPA];
    FILE *f;
    int i;

    caminho_mepa(path, source);
    f = fopen(path, "w");
    if (!f) diag_error(0, "nao foi possivel criar o arquivo .mepa");

    for (i = 0; i < count; i++)
        if (fprintf(f, "%s\n", lines[i]) < 0) {
            fclose(f);
            remove(path);
            diag_error(0, "falha ao gravar MEPA");
        }

    if (fprintf(f, "FIM\n") < 0 || fclose(f) != 0) {
        remove(path);
        diag_error(0, "falha ao finalizar MEPA");
    }

    /*libera a memoria das instrucoes*/
    for (i = 0; i < count; i++) {
        free(lines[i]);
        lines[i] = NULL;
    }
    count = 0;
}
