#ifndef bib_H
#define bib_H

#define MAX_MEM 256
#define MAX_REG 8

// STRUCTs / ENUMs
typedef enum { TYPE_R, TYPE_J, TYPE_I, TYPE_OTHER } TipoInst;

typedef struct {
  int reg_destino;  // 1 = rd  (Tipo R), 0 = rt  (Tipo I)
  int ula_fonte;    // 1 = imediato, 0 = registrador
  int mem_para_reg; // 1 = dado vem da mem(LW), 0 = vem da ULA
  int esc_reg;      // 1 = escreve no banco de registradores
  int esc_mem;      // 1 = escreve na memória de dados(SW)
  int branch;       // 1 = BEQ
  int jump;         // 1 = JUMP
  int ctrl_ula;     // 0=ADD, 1=SUB, 2=AND, 3=OR
} Sinais;

typedef struct {
  TipoInst tipo;
  char bin[17];
  int opcode, rs, rt, rd, funct, imm, addr;
  int valida; // 1 = instrução real, 0 = bolha
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

// REGISTRADORES DE PIPELINE
typedef struct {
  Instrucao inst;
  int pc_mais1, valida, bolha;
} Reg_IFID;
typedef struct {
  Instrucao inst;
  int dado_rs, dado_rt, imm, reg_dest, pc_mais1, valida, bolha;
  Sinais s;
} Reg_IDEX;
typedef struct {
  Instrucao inst;
  int resultado_ula, dado_rt, reg_dest, flag_zero, valida, bolha;
  Sinais s;
} Reg_EXMEM;
typedef struct {
  Instrucao inst;
  int reg_dest, resultado_mem, resultado_ula, valida, bolha;
  Sinais s;
} Reg_MEMWB;

typedef struct {
  int total;     // instruções decodificadas (excluindo bolhas)
  int completas; // instruções que passaram pelo WB
  int tipo_r, tipo_i, tipo_j;
  int add, sub, and_op, or_op, addi, lw, sw_op, beq, jump;
  int stalls;   // bolhas inseridas por load-use hazard
  int forwards; // forwarding aplicados (EX e MEM)
  int flushes;  // instruções descartadas por branch/jump
} Stats;

typedef struct {
  int pc, ciclos, dados[MAX_MEM];
  signed char reg[MAX_REG];
  Reg_IFID if_id;
  Reg_IDEX id_ex;
  Reg_EXMEM ex_mem;
  Reg_MEMWB mem_wb;
  Stats stats;
} Snap;
typedef struct {
  int pc, ciclos, i_hist;
  int rodando; // 1 = pipeline em execução, 0 = parado/concluído
  int stall;   // 1 = stall ativo (load-use hazard), congela IF e IF/ID
  MemInst *mem_inst;
  MemDados *mem_dados;
  Banco *banco;
  Reg_IFID if_id;
  Reg_IDEX id_ex;
  Reg_EXMEM ex_mem;
  Reg_MEMWB mem_wb;
  Stats stats;
  Snap *hist;
  Instrucao wb_last;      // instrução que saiu do WB neste ciclo
  int wb_last_valida;     // 1 se wb_last é real
  char cycle_log[1024];  // log de eventos do ciclo (hazards/forwards)
  int cycle_log_len;     // tamanho atual do log
} CPU;

// PROTOTIPOS
void limpa_buf(void);
int separa_bits(const char *b, int ini, int n);
int sign_ext(int val, int nBits);
int bits_imm(const char *b, int ini, int nBits);
int bits_jump(const char *b);
void inicializa_cpu(CPU *cpu);
void carrega_mem(CPU *cpu);
void carrega_dat(CPU *cpu);
void salva_dat(CPU *cpu);
int ula(int A, int B, int ctrl, int *ovf, int *zero);
Sinais decoder(Instrucao *inst);
void estagio_WB(CPU *cpu);
void estagio_MEM(CPU *cpu);
void estagio_EX(CPU *cpu);
void estagio_ID(CPU *cpu);
void estagio_IF(CPU *cpu);
void detecta_hazard(CPU *cpu);
void forwarding_unit(CPU *cpu);
int pipeline_vazio(CPU *cpu);
void salva_snap(CPU *cpu);
void executa_ciclo(CPU *cpu);
void volta_ciclo(CPU *cpu);
void executa_programa(CPU *cpu);
void reinicia(CPU *cpu);
void disassembla(const Instrucao *inst, char *buf);
void print_menu(void);
void print_inst(const Instrucao *raw, char *buf);
void print_mem_inst(CPU *cpu);
void print_mem_dat(CPU *cpu);
void print_banco(CPU *cpu);
void print_mem_ambas(CPU *cpu);
void print_pipeline(CPU *cpu);
void print_stats(CPU *cpu);

#endif