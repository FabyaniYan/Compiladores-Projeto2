# Makefile do COMPLAC - compilador para a linguagem SLAC2 (Fase 2)
# Compilacao completa do projeto com um unico comando: make

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99
ALVO    = complac
FONTES  = main.c opt.c log.c diag.c lex.c symtab.c gerador.c parser.c
OBJETOS = $(FONTES:.c=.o)

# interpretador MEPA fornecido pelo professor (ajustar o caminho se precisar)
MEPA    = python3 ../mepa_vm/mepa.py --progfile

all: $(ALVO)

$(ALVO): $(OBJETOS)
	$(CC) $(CFLAGS) -o $@ $(OBJETOS)

# dependencias de cabecalho
main.o:    main.c diag.h gerador.h lex.h log.h opt.h parser.h symtab.h
opt.o:     opt.c opt.h
log.o:     log.c log.h
diag.o:    diag.c diag.h gerador.h log.h
lex.o:     lex.c lex.h
symtab.o:  symtab.c symtab.h lex.h diag.h log.h
gerador.o: gerador.c gerador.h
parser.o:  parser.c parser.h lex.h diag.h gerador.h log.h symtab.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# compila os programas de teste (gera os .mepa)
teste: $(ALVO)
	./$(ALVO) testes/exemplo1.slac --tokens --symtab --trace
	./$(ALVO) testes/exemplo2.slac --tokens --symtab --trace
	./$(ALVO) testes/exemplo3.slac --tokens --symtab --trace
	./$(ALVO) testes/enunciado_fig1.slac
	-./$(ALVO) testes/erro_sintatico.slac
	-./$(ALVO) testes/erro_lexico.slac
	-./$(ALVO) testes/erro_duplicado.slac
	# PESSOA 1: acrescentar aqui os testes de erro semantico de declaracao/escopo/parametros
	# PESSOA 2: acrescentar aqui os testes de erro semantico de tipos e de nao declarado

# executa um .mepa no interpretador: make roda ARQ=testes/exemplo1.mepa
roda:
	$(MEPA) $(ARQ)

clean:
	rm -f $(OBJETOS) $(ALVO) testes/*.tk testes/*.ts testes/*.trc testes/*.mepa

.PHONY: all teste roda clean
