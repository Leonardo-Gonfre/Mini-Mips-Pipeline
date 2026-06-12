#ifndef bib_pipeline_H
#define bib_pipeline_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MEM 256   /* posições na memória de instruções e dados */
#define MAX_REG 8     /* número de registradores ($0 a $r7)        */
#define MAX_HIST 300   /* ciclos máximos que podem ser desfeitos     */

typedef enum { TYPE_R = 0, TYPE_J = 1, TYPE_I = 2, TYPE_OTHER = 3 } TipoInst;

typedef struct {
    TipoInst tipo;
    char bin[17];   /* "0000000000000000\0" */
    int opcode;
    int rs, rt, rd;
    int funct;
    int imm;       /* imediato com extensão de sinal (6 bits → int) */
    int addr;      /* endereço para JUMP (mascarado em 8 bits)      */
    int valida;    /* 1 = instrução real, 0 = bolha                 */
} Instrucao;

typedef struct {
    Instrucao *inst;
    int tamanho;
} MemInst;

typedef struct {
    int *dados;
    int tamanho;
} MemDados;

typedef struct {
    signed char reg[MAX_REG];
    const char *nome[MAX_REG];
} Banco;

typedef struct {
    int reg_destino;  /* 1 = rd  (Tipo R), 0 = rt  (Tipo I)        */
    int ula_fonte;    /* 0 = registrador,  1 = imediato              */
    int mem_para_reg; /* 1 = dado vem da mem (LW), 0 = vem da ULA   */
    int esc_reg;      /* 1 = escreve no banco de registradores       */
    int esc_mem;      /* 1 = escreve na memória de dados (SW)        */
    int branch;       /* 1 = BEQ                                     */
    int jump;         /* 1 = JUMP                                    */
    int ctrl_ula;     /* 0=ADD, 1=SUB, 2=AND, 3=OR                  */
} Sinais;

typedef struct {
    int total;          /* instruções decodificadas (excluindo bolhas) */
    int completas;      /* instruções que passaram pelo WB             */
    int tipo_r, tipo_i, tipo_j;
    int add, sub, and_op, or_op;
    int addi, lw, sw_op, beq, jump;
} Stats;

/* IF/ID — entre Busca e Decodificação */
typedef struct {
    Instrucao inst;
    int pc_mais1; /* PC+1 (próximo PC sequencial) */
    int valida;
} Reg_IFID;

/* ID/EX — entre Decodificação e Execução */
typedef struct {
    Instrucao inst;
    int dado_rs;  /* valor lido de rs no banco               */
    int dado_rt;  /* valor lido de rt no banco               */
    int imm;      /* imediato (já com extensão de sinal)     */
    int pc_mais1;
    int reg_dest; /* índice do registrador destino (rd ou rt)*/
    Sinais s;
    int valida;
} Reg_IDEX;

/* EX/MEM — entre Execução e Acesso à Memória */
typedef struct {
    Instrucao inst;
    int resultado_ula;
    int dado_rt;      /* valor de rt — necessário para SW        */
    int flag_zero;    /* saída zero da ULA (usada por BEQ)       */
    int reg_dest;
    Sinais s;
    int valida;
} Reg_EXMEM;

/* MEM/WB — entre Acesso à Memória e Escrita */
typedef struct {
    Instrucao inst;
    int resultado_mem; /* dado lido da memória (LW)              */
    int resultado_ula;
    int reg_dest;
    Sinais s;
    int valida;
} Reg_MEMWB;

typedef struct {
    int pc;
    int ciclos;
    signed char reg[MAX_REG];
    int dados[MAX_MEM];
    Reg_IFID if_id;
    Reg_IDEX id_ex;
    Reg_EXMEM ex_mem;
    Reg_MEMWB mem_wb;
    Stats stats;
} Snap;

typedef struct {
    int pc;
    int ciclos;
    int rodando; /* 1 = pipeline em execução, 0 = parado/concluído */

    MemInst *mem_inst;
    MemDados *mem_dados;
    Banco *banco;

    /* Registradores de pipeline */
    Reg_IFID if_id;
    Reg_IDEX id_ex;
    Reg_EXMEM ex_mem;
    Reg_MEMWB mem_wb;

    Stats stats;

    /* Histórico para step-back */
    Snap *hist;
    int i_hist;
} CPU;


void limpa_buf(void);
int  separa_bits(const char *b, int ini, int n);
int  sign_ext(int val, int nBits);
int  bits_imm(const char *b, int ini, int nBits);
int  bits_jump(const char *b);

int    ula(int A, int B, int ctrl, int *ovf, int *zero);
Sinais decoder(Instrucao *inst);

void disassembla(const Instrucao *inst, char *buf);
void print_inst(const Instrucao *raw, char *buf);

void carrega_mem(CPU *cpu);
void print_mem_inst(CPU *cpu);
void carrega_dat(CPU *cpu);
void print_mem_dat(CPU *cpu);
void salva_dat(CPU *cpu);
void inicializa_banco(CPU *cpu);
void print_banco(CPU *cpu);
void print_pipeline(CPU *cpu);
void estagio_WB(CPU *cpu);
void estagio_MEM(CPU *cpu);
void estagio_EX(CPU *cpu);
void estagio_ID(CPU *cpu);
void estagio_IF(CPU *cpu);

int  pipeline_vazio(CPU *cpu);
void salva_snap(CPU *cpu);
void executa_ciclo(CPU *cpu);
void volta_ciclo(CPU *cpu);
void executa_programa(CPU *cpu);
void reinicia(CPU *cpu);

void print_stats(CPU *cpu);
void print_header(void);
void print_menu(void);

#endif
