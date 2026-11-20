# Makefile do COMPLAC - compilador para a linguagem SLAC2 (Projeto 2)
# Compilacao completa do projeto com um unico comando: make

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99
ALVO    = complac
FONTES  = main.c opt.c log.c diag.c lex.c symtab.c parser.c gerador.c
OBJETOS = $(FONTES:.c=.o)

# interpretador MEPA do professor (fora do repositorio); pode ser trocado:
#   make teste MEPA=/caminho/para/mepa.py
MEPA ?= ../mepa_vm/mepa.py
export MEPA

all: $(ALVO)

$(ALVO): $(OBJETOS)
	$(CC) $(CFLAGS) -o $@ $(OBJETOS)

# dependencias de cabecalho
main.o:    main.c diag.h gerador.h lex.h log.h opt.h parser.h symtab.h
opt.o:     opt.c opt.h
log.o:     log.c log.h
diag.o:    diag.c diag.h log.h
lex.o:     lex.c lex.h
symtab.o:  symtab.c symtab.h lex.h diag.h log.h
parser.o:  parser.c parser.h lex.h diag.h log.h symtab.h gerador.h
gerador.o: gerador.c gerador.h diag.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# bateria automatica: diagnosticos e execucao do .mepa no interpretador
teste: $(ALVO)
	python3 testes/testes_regressao.py
	python3 testes/testes_mepa.py
	python3 testes/testes_especificacao.py

# compila todos os programas de testes/exemplos e testes/erros
exemplos: $(ALVO)
	@for f in testes/exemplos/*.slac; do ./$(ALVO) $$f; done
	@for f in testes/erros/*.slac; do ./$(ALVO) $$f || true; done

clean:
	rm -f $(OBJETOS) $(ALVO) *.tk *.ts *.trc *.mepa
	rm -f testes/*/*.tk testes/*/*.ts testes/*/*.trc testes/*/*.mepa

.PHONY: all teste exemplos clean
