#include "bib_pipeline.h"

void limpa_buf(void) {
  int c;
  while ((c = getchar()) != '\n' && c != EOF)
    ;
}

int separa_bits(const char *b, int ini, int n) {
  int val = 0;
  for (int i = 0; i < n; i++)
    val = (val << 1) | (b[ini + i] == '1' ? 1 : 0);
  return val;
}

int sign_ext(int val, int nBits) {
  int sign = 1 << (nBits - 1);
  if (val & sign)
    val |= ~((1 << nBits) - 1);
  return val;
}

/* Lê imediato com extensão de sinal */
int bits_imm(const char *b, int ini, int nBits) {
  return sign_ext(separa_bits(b, ini, nBits), nBits);
}

/* Lê endereço de JUMP (12 bits a partir de ini=4, mascarado em 0xFF) */
int bits_jump(const char *b) { return separa_bits(b, 4, 12) & 0xFF; }

int ula(int A, int B, int ctrl, int *ovf, int *zero) {
  int r = 0;
  *ovf = 0;
  switch (ctrl) {
  case 0: // ADD
    r = A + B;
    if ((A > 0 && B > 0 && r < 0) || (A < 0 && B < 0 && r > 0))
      *ovf = 1;
    break;
  case 1: // SUB
    r = A - B;
    if ((A > 0 && B < 0 && r < 0) || (A < 0 && B > 0 && r > 0))
      *ovf = 1;
    break;
  case 2:
    r = A & B;
    break; // AND
  case 3:
    r = A | B;
    break; // OR
  default:
    printf("[ULA] Operacao invalida: ctrl=%d\n", ctrl);
    break;
  }
  *zero = (r == 0);
  return r;
}

Sinais decoder(Instrucao *inst) {}

void disassembla(const Instrucao *inst, char *buf) {
  if (!inst->valida) {
    strcpy(buf, "--- (bolha)");
    return;
  }
  switch (inst->opcode) {
  case 0:
    switch (inst->funct) {
    case 0:
      sprintf(buf, "add  $r%d, $r%d, $r%d", inst->rd, inst->rs, inst->rt);
      break;
    case 1:
      sprintf(buf, "sub  $r%d, $r%d, $r%d", inst->rd, inst->rs, inst->rt);
      break;
    case 2:
      sprintf(buf, "and  $r%d, $r%d, $r%d", inst->rd, inst->rs, inst->rt);
      break;
    case 3:
      sprintf(buf, "or   $r%d, $r%d, $r%d", inst->rd, inst->rs, inst->rt);
      break;
    default:
      sprintf(buf, "??? funct=%d", inst->funct);
      break;
    }
    break;
  case 2:
    sprintf(buf, "j    %d", inst->addr);
    break;
  case 4:
    sprintf(buf, "addi $r%d, $r%d, %d", inst->rt, inst->rs, inst->imm);
    break;
  case 8:
    sprintf(buf, "beq  $r%d, $r%d, %d", inst->rs, inst->rt, inst->imm);
    break;
  case 11:
    sprintf(buf, "lw   $r%d, %d($r%d)", inst->rt, inst->imm, inst->rs);
    break;
  case 15:
    sprintf(buf, "sw   $r%d, %d($r%d)", inst->rt, inst->imm, inst->rs);
    break;
  default:
    sprintf(buf, "data %s", inst->bin);
    break;
  }
}

void carrega_mem(CPU *cpu) {
  char arq[128];
  printf("Arquivo .mem: ");
  limpa_buf();
  if (!fgets(arq, sizeof(arq), stdin))
    return;
  arq[strcspn(arq, "\r\n")] = '\0';

  FILE *f = fopen(arq, "r");
  if (!f) {
    printf("Erro: nao foi possivel abrir '%s'.\n", arq);
    return;
  }

  if (cpu->mem_inst->inst) {
    free(cpu->mem_inst->inst);
    cpu->mem_inst->inst = NULL;
  }
  cpu->mem_inst->inst = (Instrucao *)calloc(MAX_MEM, sizeof(Instrucao));

  if (!cpu->mem_inst->inst) {
    printf("Sem memoria.\n");
    fclose(f);
    return;
  }
  cpu->mem_inst->tamanho = 0;

  char buf[32];
  int linha = 0;
  while (fscanf(f, "%31s", buf) == 1) {
    linha++;
    if (buf[0] == '#') {
      while (!feof(f) && fgetc(f) != '\n')
        ;
      continue;
    }
    int len = (int)strlen(buf);
    if (len != 16) {
      printf("[Linha %d] Ignorada: comprimento %d != 16\n", linha, len);
      continue;
    }
    int ok = 1;
    for (int i = 0; i < 16 && ok; i++)
      if (buf[i] != '0' && buf[i] != '1')
        ok = 0;
    if (!ok) {
      printf("[Linha %d] Ignorada: caracter invalido\n", linha);
      continue;
    }
    if (cpu->mem_inst->tamanho >= MAX_MEM) {
      printf("Memoria cheia!\n");
      break;
    }

    int idx = cpu->mem_inst->tamanho;
    strncpy(cpu->mem_inst->inst[idx].bin, buf, 16);
    cpu->mem_inst->inst[idx].bin[16] = '\0';
    cpu->mem_inst->inst[idx].valida = 1;
    cpu->mem_inst->tamanho++;
  }
  printf("%d instrucoes carregadas.\n", cpu->mem_inst->tamanho);
  fclose(f);
}

void print_mem_inst(CPU *cpu) {
  if (!cpu->mem_inst->inst || cpu->mem_inst->tamanho == 0) {
    printf("Memoria de instrucoes vazia.\n");
    return;
  }
  char asm_buf[64];
  printf("\n+------+------------------+----------------------------------------"
         "--+\n");
  printf("| Addr | Binario          | Assembly                                 "
         "|\n");
  printf("+------+------------------+------------------------------------------"
         "+\n");
  for (int i = 0; i < cpu->mem_inst->tamanho; i++) {
    print_inst(&cpu->mem_inst->inst[i], asm_buf);
    printf("| %4d | %-16s | %-40s |\n", i, cpu->mem_inst->inst[i].bin, asm_buf);
  }
  printf("+------+------------------+------------------------------------------"
         "+\n");
  printf("Total: %d instrucoes\n", cpu->mem_inst->tamanho);
}

// memoria de dados

void carrega_dat(CPU *cpu) {
  char arq[128];
  printf("Arquivo .dat: ");
  limpa_buf();
  if (!fgets(arq, sizeof(arq), stdin))
    return;
  arq[strcspn(arq, "\r\n")] = '\0';

  FILE *f = fopen(arq, "r");
  if (!f) {
    printf("Erro: nao foi possivel abrir '%s'.\n", arq);
    return;
  }

  if (cpu->mem_dados->dados) {
    free(cpu->mem_dados->dados);
    cpu->mem_dados->dados = NULL;
  }
  cpu->mem_dados->dados = (int *)calloc(MAX_MEM, sizeof(int));

  if (!cpu->mem_dados->dados) {
    printf("Sem memoria.\n");
    fclose(f);
    return;
  }
  cpu->mem_dados->tamanho = 0;

  int val;
  while (fscanf(f, "%d", &val) == 1) {
    if (cpu->mem_dados->tamanho >= MAX_MEM) {
      printf("Memoria de dados cheia!\n");
      break;
    }
    cpu->mem_dados->dados[cpu->mem_dados->tamanho++] = val;
  }
  printf("%d valores carregados.\n", cpu->mem_dados->tamanho);
  fclose(f);
}

void print_mem_dat(CPU *cpu) {
  if (!cpu->mem_dados->dados || cpu->mem_dados->tamanho == 0) {
    printf("Memoria de dados nao carregada.\n");
    return;
  }
  printf("\n+------+--------+\n");
  printf("| Addr |  Valor |\n");
  printf("+------+--------+\n");
  for (int i = 0; i < cpu->mem_dados->tamanho; i++)
    printf("| %4d | %6d |\n", i, cpu->mem_dados->dados[i]);
  printf("+------+--------+\n");
  printf("Total: %d posicoes\n", cpu->mem_dados->tamanho);
}

void inicializa_banco(CPU *cpu) {
  memset(cpu->banco->reg, 0, sizeof(cpu->banco->reg));
  cpu->banco->nome[0] = "$0";
  cpu->banco->nome[1] = "$r1";
  cpu->banco->nome[2] = "$r2";
  cpu->banco->nome[3] = "$r3";
  cpu->banco->nome[4] = "$r4";
  cpu->banco->nome[5] = "$r5";
  cpu->banco->nome[6] = "$r6";
  cpu->banco->nome[7] = "$r7";
}

void print_banco(CPU *cpu) {
  printf("\n+------+--------+\n");
  printf("| Reg  |  Valor |\n");
  printf("+------+--------+\n");
  for (int i = 0; i < MAX_REG; i++)
    printf("| %4s | %6d |\n", cpu->banco->nome[i], (int)cpu->banco->reg[i]);
  printf("+------+--------+\n");
}

void print_pipeline(CPU *cpu) {}
void estagio_WB(CPU *cpu) {}
void estagio_MEM(CPU *cpu) {}
void estagio_EX(CPU *cpu) {}
void estagio_ID(CPU *cpu) {}
void estagio_IF(CPU *cpu) {}

void print_inst(const Instrucao *raw, char *buf) { disassembla(raw, buf); }

int pipeline_vazio(CPU *cpu) {
  return (cpu->if_id.valida + cpu->id_ex.valida + cpu->ex_mem.valida +
          cpu->mem_wb.valida) == 0;
}
void salva_snap(CPU *cpu) {

  if (cpu->i_hist >= MAX_HIST)
    return; /* histórico cheio, não salva */

  Snap *s = &cpu->hist[cpu->i_hist++]; /* pega próxima posição livre */

  s->pc = cpu->pc;
  s->ciclos = cpu->ciclos;

  /* Copia banco de registradores */
  memcpy(s->reg, cpu->banco->reg, MAX_REG * sizeof(signed char));

  /* Copia memória de dados (se carregada) */
  if (cpu->mem_dados->dados)
    memcpy(s->dados, cpu->mem_dados->dados, MAX_MEM * sizeof(int));

  /* Copia os 4 registradores de pipeline */
  s->if_id = cpu->if_id;
  s->id_ex = cpu->id_ex;
  s->ex_mem = cpu->ex_mem;
  s->mem_wb = cpu->mem_wb;

  /* Copia estatísticas */
  s->stats = cpu->stats;
}

void executa_ciclo(CPU *cpu) {

  salva_snap(cpu); /* salva estado ANTES de modificar */

  /* Ordem reversa — preserva valores do ciclo anterior */
  estagio_WB(cpu);
  estagio_MEM(cpu);
  estagio_EX(cpu);
  estagio_ID(cpu);
  estagio_IF(cpu);

  cpu->ciclos++;

  /* Detecta fim: sem mais instruções para buscar E pipeline drenado */
  if (cpu->pc >= cpu->mem_inst->tamanho && pipeline_vazio(cpu))
    cpu->rodando = 0;
}

void volta_ciclo(CPU *cpu) {

  if (cpu->i_hist <= 0) {
    printf("Nenhum estado anterior disponivel.\n");
    return;
  }

  Snap *s = &cpu->hist[--cpu->i_hist]; /* volta uma posição no histórico */

  cpu->pc = s->pc;
  cpu->ciclos = s->ciclos;

  /* Restaura banco de registradores */
  memcpy(cpu->banco->reg, s->reg, MAX_REG * sizeof(signed char));

  /* Restaura memória de dados (se carregada) */
  if (cpu->mem_dados->dados)
    memcpy(cpu->mem_dados->dados, s->dados, MAX_MEM * sizeof(int));

  /* Restaura os 4 registradores de pipeline */
  cpu->if_id = s->if_id;
  cpu->id_ex = s->id_ex;
  cpu->ex_mem = s->ex_mem;
  cpu->mem_wb = s->mem_wb;

  /* Restaura estatísticas */
  cpu->stats = s->stats;

  cpu->rodando = 1;
  printf("Ciclo desfeito. PC=%d | Ciclo=%d\n", cpu->pc, cpu->ciclos);
}

void executa_programa(CPU *cpu) {

  if (!cpu->mem_inst->inst || cpu->mem_inst->tamanho == 0) {
    printf("Nenhuma instrucao carregada. Use opcao 1 primeiro.\n");
    return;
  }
  cpu->rodando = 1;

  /* Limite de segurança: evita loop infinito */
  int lim = cpu->mem_inst->tamanho * 2 + 10;
  while (cpu->rodando && lim-- > 0)
    executa_ciclo(cpu);

  printf("\nExecucao concluida em %d ciclos (%d instrucoes completas).\n",
         cpu->ciclos, cpu->stats.completas);
}

void reinicia(CPU *cpu) {
  cpu->pc = 0;
  cpu->ciclos = 0;
  cpu->rodando = 0;
  cpu->i_hist = 0;

  /* Zera os 4 registradores de pipeline (tudo vira bolha) */
  memset(&cpu->if_id, 0, sizeof(Reg_IFID));
  memset(&cpu->id_ex, 0, sizeof(Reg_IDEX));
  memset(&cpu->ex_mem, 0, sizeof(Reg_EXMEM));
  memset(&cpu->mem_wb, 0, sizeof(Reg_MEMWB));

  /* Zera as estatísticas */
  memset(&cpu->stats, 0, sizeof(Stats));

  /* Reinicia banco de registradores ($0 a $r7 = 0) */
  inicializa_banco(cpu);

  printf("Pipeline reiniciado. PC=0, registradores zerados.\n");
}

void salva_dat(CPU *cpu) {}

void print_stats(CPU *cpu) {
  printf("\n======== ESTATISTICAS ========\n");
  printf("Ciclos totais         : %d\n", cpu->ciclos);
  printf("Instrucoes decodif.   : %d\n", cpu->stats.total);
  printf("Instrucoes completas  : %d\n", cpu->stats.completas);

  if (cpu->stats.completas > 0 && cpu->ciclos > 0)
    printf("CPI (ciclos/completas): %.2f\n",
           (float)cpu->ciclos / (float)cpu->stats.completas);

  printf("\nTipo R: %d\n", cpu->stats.tipo_r);
  printf("  ADD=%d | SUB=%d | AND=%d | OR=%d\n", cpu->stats.add, cpu->stats.sub,
         cpu->stats.and_op, cpu->stats.or_op);
  printf("Tipo I: %d\n", cpu->stats.tipo_i);
  printf("  ADDI=%d | LW=%d | SW=%d | BEQ=%d\n", cpu->stats.addi, cpu->stats.lw,
         cpu->stats.sw_op, cpu->stats.beq);
  printf("Tipo J: %d  (JUMP=%d)\n", cpu->stats.tipo_j, cpu->stats.jump);
}

void print_header(void) {
  printf("\n================================================\n");
  printf("  Mini MIPS 8 bits — Simulador Pipeline (5E)   \n");
  printf("  IF -> ID -> EX -> MEM -> WB                  \n");
  printf("  Sem tratamento de hazards de dados            \n");
  printf("================================================\n");
}

void print_menu(void) {
  printf("\n+------- MENU -------+\n");
  printf("| 1. Carregar .mem     |\n");
  printf("| 2. Carregar .dat     |\n");
  printf("| 3. Ver memorias      |\n");
  printf("| 4. Ver registradores |\n");
  printf("| 5. Ver pipeline      |\n");
  printf("| 6. Step (1 clk)      |\n");
  printf("| 7. Back (desfaz)     |\n");
  printf("| 8. Run (tudo)        |\n");
  printf("| 9. Reiniciar         |\n");
  printf("|10. Salvar .dat       |\n");
  printf("|11. Estatisticas      |\n");
  printf("| 0. Sair              |\n");
  printf("+----------------------+\n");
  printf("Opcao: ");
}
