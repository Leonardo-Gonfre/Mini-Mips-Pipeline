#include "bib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
int bits_imm(const char *b, int ini, int nBits) {
  return sign_ext(separa_bits(b, ini, nBits), nBits);
}
int bits_jump(const char *b) { return separa_bits(b, 4, 12) & 0xFF; }
void inicializa_cpu(CPU *cpu) {
  cpu->pc = cpu->ciclos = cpu->rodando = cpu->i_hist = cpu->stall = 0;
  // Memorias
  memset(cpu->mem_inst->inst, 0, MAX_MEM * sizeof(Instrucao));
  cpu->mem_inst->tamanho = 0;
  memset(cpu->mem_dados->dados, 0, MAX_MEM * sizeof(int));
  cpu->mem_dados->tamanho = 0;
  // Banco de registradores
  memset(cpu->banco->reg, 0, sizeof(cpu->banco->reg));
  cpu->banco->nome[0] = "$r0";
  cpu->banco->nome[1] = "$r1";
  cpu->banco->nome[2] = "$r2";
  cpu->banco->nome[3] = "$r3";
  cpu->banco->nome[4] = "$r4";
  cpu->banco->nome[5] = "$r5";
  cpu->banco->nome[6] = "$r6";
  cpu->banco->nome[7] = "$r7";
  // Registradores de pipeline
  memset(&cpu->if_id, 0, sizeof(Reg_IFID));
  memset(&cpu->id_ex, 0, sizeof(Reg_IDEX));
  memset(&cpu->ex_mem, 0, sizeof(Reg_EXMEM));
  memset(&cpu->mem_wb, 0, sizeof(Reg_MEMWB));
  // Estatisticas
  memset(&cpu->stats, 0, sizeof(Stats));
}

// MEMORYs
void carrega_mem(CPU *cpu) {
  char arq[128];
  printf("Arquivo .mem: ");
  limpa_buf();
  if (!fgets(arq, sizeof(arq), stdin))
    return;
  arq[strcspn(arq, "\r\n")] = '\0';

  FILE *p = fopen(arq, "r");
  if (!p) {
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
    fclose(p);
    return;
  }
  cpu->mem_inst->tamanho = 0;

  char buf[32];
  int linha = 0;
  while (fscanf(p, "%31s", buf) == 1) {
    linha++;
    if (buf[0] == '#') {
      while (!feof(p) && fgetc(p) != '\n')
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
  fclose(p);
}
void carrega_dat(CPU *cpu) {
  char arq[128];
  printf("Arquivo .dat: ");
  limpa_buf();
  if (!fgets(arq, sizeof(arq), stdin))
    return;
  arq[strcspn(arq, "\r\n")] = '\0';

  FILE *p = fopen(arq, "r");
  if (!p) {
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
    fclose(p);
    return;
  }
  cpu->mem_dados->tamanho = 0;

  int val;
  while (fscanf(p, "%d", &val) == 1) {
    if (cpu->mem_dados->tamanho >= MAX_MEM) {
      printf("Memoria de dados cheia!\n");
      break;
    }
    cpu->mem_dados->dados[cpu->mem_dados->tamanho++] = val;
  }
  printf("%d valores carregados.\n", cpu->mem_dados->tamanho);
  fclose(p);
}
void salva_dat(CPU *cpu) {
  char arq[128];
  printf("Arquivo de saida (.dat): ");
  limpa_buf();
  if (!fgets(arq, sizeof(arq), stdin))
    return;
  arq[strcspn(arq, "\r\n")] = '\0';

  FILE *f = fopen(arq, "w");
  if (!f) {
    printf("Erro ao criar '%s'.\n", arq);
    return;
  }
  for (int i = 0; i < cpu->mem_dados->tamanho; i++) {
    fprintf(f, "%d\n", cpu->mem_dados->dados[i]);
  }
  fclose(f);
  printf("Salvo em '%s' (%d posicoes).\n", arq, cpu->mem_dados->tamanho);
}

Sinais decoder(Instrucao *inst) {
  Sinais s;
  memset(&s, 0, sizeof(s));
  const char *b = inst->bin;
  inst->opcode = separa_bits(b, 0, 4);

  switch (inst->opcode) {
  case 0: // Tipo R: ADD, SUB, AND, OR
    inst->tipo = TYPE_R;
    inst->rs = separa_bits(b, 4, 3);
    inst->rt = separa_bits(b, 7, 3);
    inst->rd = separa_bits(b, 10, 3);
    inst->funct = separa_bits(b, 13, 3);
    inst->imm = inst->addr = 0;

    s.reg_destino = 1; // rd
    s.ula_fonte = 0;   // registrador
    s.esc_reg = 1;
    switch (inst->funct) {
    case 0:
      s.ctrl_ula = 0;
      break; // ADD
    case 1:
      s.ctrl_ula = 1;
      break; // SUB
    case 2:
      s.ctrl_ula = 2;
      break; // AND
    case 3:
      s.ctrl_ula = 3;
      break; // OR
    default:
      printf("[ID] Funct invalido: %d\n", inst->funct);
      break;
    }
    break;
  case 2: // JUMP
    inst->tipo = TYPE_J;
    inst->addr = bits_jump(b);
    inst->rs = inst->rt = inst->rd = inst->funct = inst->imm = 0;
    s.jump = 1;
    break;
  case 4: // ADDI
    inst->tipo = TYPE_I;
    inst->rs = separa_bits(b, 4, 3);
    inst->rt = separa_bits(b, 7, 3);
    inst->imm = bits_imm(b, 10, 6);
    inst->rd = inst->funct = inst->addr = 0;
    s.ula_fonte = 1; // imediato
    s.esc_reg = 1;
    break;
  case 8: // BEQ
    inst->tipo = TYPE_I;
    inst->rs = separa_bits(b, 4, 3);
    inst->rt = separa_bits(b, 7, 3);
    inst->imm = bits_imm(b, 10, 6);
    inst->rd = inst->funct = inst->addr = 0;
    s.ula_fonte = 0;
    s.branch = 1;
    s.ctrl_ula = 1; // SUB (rs-rt == 0 ?)
    break;
  case 11: // LW
    inst->tipo = TYPE_I;
    inst->rs = separa_bits(b, 4, 3);
    inst->rt = separa_bits(b, 7, 3);
    inst->imm = bits_imm(b, 10, 6);
    inst->rd = inst->funct = inst->addr = 0;
    s.ula_fonte = 1;
    s.mem_para_reg = 1;
    s.esc_reg = 1;
    break;
  case 15: // SW
    inst->tipo = TYPE_I;
    inst->rs = separa_bits(b, 4, 3);
    inst->rt = separa_bits(b, 7, 3);
    inst->imm = bits_imm(b, 10, 6);
    inst->rd = inst->funct = inst->addr = 0;
    s.ula_fonte = 1;
    s.esc_mem = 1;
    break;
  default:
    inst->tipo = TYPE_OTHER;
    break;
  }
  return s;
}
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
    sprintf(buf, "j %d", inst->addr);
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
  case 2: // AND
    r = A & B;
    break;
  case 3: // OR
    r = A | B;
    break;
  default:
    printf("[ULA] Operacao invalida: ctrl=%d\n", ctrl);
    break;
  }
  *zero = (r == 0);
  return r;
}

// ESTAGIOS
void estagio_WB(CPU *cpu) {
  Reg_MEMWB *r = &cpu->mem_wb;
  if (!r->valida)
    return;

  // Escreve no banco de registradores
  if (r->s.esc_reg && r->reg_dest > 0 && r->reg_dest < MAX_REG) {
    int val = r->s.mem_para_reg ? r->resultado_mem : r->resultado_ula;
    cpu->banco->reg[r->reg_dest] = (signed char)(val & 0xFF);
  }

  if (r->inst.valida)
    cpu->stats.completas++;
}
void estagio_MEM(CPU *cpu) {
  Reg_EXMEM *ex = &cpu->ex_mem;
  Reg_MEMWB *wb = &cpu->mem_wb;

  // Propaga dados para MEM/WB
  wb->inst = ex->inst;
  wb->s = ex->s;
  wb->resultado_ula = ex->resultado_ula;
  wb->reg_dest = ex->reg_dest;
  wb->resultado_mem = 0;
  wb->valida = ex->valida;
  if (!ex->valida)
    return;

  // LW: leitura da memória de dados
  if (ex->s.mem_para_reg) {
    int addr = ex->resultado_ula;
    if (addr >= 0 && addr < MAX_MEM && cpu->mem_dados->dados) {
      wb->resultado_mem = cpu->mem_dados->dados[addr];
    } else {
      printf("[MEM] Endereco invalido (LW): %d\n", addr);
    }
  }

  // SW: escrita na memória de dados
  if (ex->s.esc_mem) {
    int addr = ex->resultado_ula;
    if (addr >= 0 && addr < MAX_MEM && cpu->mem_dados->dados) {
      cpu->mem_dados->dados[addr] = (int)(signed char)(ex->dado_rt & 0xFF);
    } else {
      printf("[MEM] Endereco invalido (SW): %d\n", addr);
    }
  }
}
void estagio_EX(CPU *cpu) {
  Reg_IDEX *id = &cpu->id_ex;
  Reg_EXMEM *ex = &cpu->ex_mem;

  // Propaga para EX/MEM
  ex->inst = id->inst;
  ex->s = id->s;
  ex->dado_rt = id->dado_rt;
  ex->resultado_ula = ex->flag_zero = ex->reg_dest = 0;
  ex->valida = id->valida;
  if (!id->valida)
    return;

  // ULA (dado_rs e dado_rt ja contem valores com forwarding)
  int A = id->dado_rs;
  int B = id->s.ula_fonte ? id->imm : id->dado_rt;
  int ovf;
  ex->resultado_ula = ula(A, B, id->s.ctrl_ula, &ovf, &ex->flag_zero);

  /* Registrador destino: rd (Tipo R) ou rt (Tipo I) */
  ex->reg_dest = id->s.reg_destino ? id->inst.rd : id->inst.rt;

  // Resolucao de Branch/Jump. Penalidade: 1 ciclo (instrucao em IF/ID e
  // descartada)
  if (id->s.jump) {
    cpu->pc = id->inst.addr;
    cpu->if_id.valida = 0;
    memset(&cpu->if_id.inst, 0, sizeof(Instrucao));
    cpu->stats.flushes++;
  } else if (id->s.branch && ex->flag_zero) {
    cpu->pc = id->pc_mais1 + id->imm;
    cpu->if_id.valida = 0;
    memset(&cpu->if_id.inst, 0, sizeof(Instrucao));
    cpu->stats.flushes++;
  }
}
void estagio_ID(CPU *cpu) {
  Reg_IFID *f = &cpu->if_id;
  Reg_IDEX *id = &cpu->id_ex;

  // Se há stall (load-use hazard): insere bolha no ID/EX e congela IF/ID
  if (cpu->stall) {
    memset(id, 0, sizeof(Reg_IDEX));
    id->valida = 0;
    cpu->stats.stalls++;
    return;
  }

  // Propaga instrucao para ID/EX
  id->inst = f->inst;
  id->pc_mais1 = f->pc_mais1;
  id->dado_rs = id->dado_rt = id->imm = id->reg_dest = 0;
  id->valida = f->valida;
  if (!f->valida)
    return;

  id->s = decoder(&id->inst);

  // Leitura do banco de registradores
  id->dado_rs = (int)cpu->banco->reg[id->inst.rs];
  id->dado_rt = (int)cpu->banco->reg[id->inst.rt];
  id->imm = id->inst.imm;
  id->reg_dest = id->s.reg_destino ? id->inst.rd : id->inst.rt;

  // Estatisticas
  switch (id->inst.opcode) {
  case 0: // Nao conta NOP (add $r0,$r0,$r0)
    if (id->inst.rs || id->inst.rt || id->inst.rd || id->inst.funct) {
      cpu->stats.tipo_r++;
      cpu->stats.total++;
      switch (id->inst.funct) {
      case 0:
        cpu->stats.add++;
        break;
      case 1:
        cpu->stats.sub++;
        break;
      case 2:
        cpu->stats.and_op++;
        break;
      case 3:
        cpu->stats.or_op++;
        break;
      }
    }
    break;
  case 2:
    cpu->stats.tipo_j++;
    cpu->stats.jump++;
    cpu->stats.total++;
    break;
  case 4:
    cpu->stats.tipo_i++;
    cpu->stats.addi++;
    cpu->stats.total++;
    break;
  case 8:
    cpu->stats.tipo_i++;
    cpu->stats.beq++;
    cpu->stats.total++;
    break;
  case 11:
    cpu->stats.tipo_i++;
    cpu->stats.lw++;
    cpu->stats.total++;
    break;
  case 15:
    cpu->stats.tipo_i++;
    cpu->stats.sw_op++;
    cpu->stats.total++;
    break;
  default:
    break;
  }
}
void estagio_IF(CPU *cpu) {
  Reg_IFID *f = &cpu->if_id;

  // Se stall ativo, congela IF (nao busca nova instrucao, mantem IF/ID)
  if (cpu->stall) {
    return; // PC nao avancou, IF/ID mantem instrucao anterior
  }

  if (cpu->pc < cpu->mem_inst->tamanho) {
    f->inst = cpu->mem_inst->inst[cpu->pc];
    f->inst.valida = 1;
    f->pc_mais1 = cpu->pc + 1;
    f->valida = 1;
    cpu->pc++;
  } else {
    // Fim das instrucoes: insere bolha para zerar o pipeline
    f->valida = 0;
    memset(&f->inst, 0, sizeof(Instrucao));
    f->pc_mais1 = cpu->pc;
  }
}

// EXECUTE
void forwarding_unit(CPU *cpu) {
  Reg_IDEX *id = &cpu->id_ex;
  if (!id->valida)
    return;

  Reg_EXMEM *ex = &cpu->ex_mem;
  Reg_MEMWB *wb = &cpu->mem_wb;

  // Forward para dado_rs (operando A)
  // EX hazard: instrucao anterior escreve no reg que ID precisa
  if (ex->valida && ex->s.esc_reg && ex->reg_dest != 0 &&
      ex->reg_dest == id->inst.rs) {
    id->dado_rs = ex->resultado_ula;
    cpu->stats.forwards++;
  }
  // MEM hazard: instrucao 2 ciclos atras escreve no reg
  else if (wb->valida && wb->s.esc_reg && wb->reg_dest != 0 &&
           wb->reg_dest == id->inst.rs) {
    int val = wb->s.mem_para_reg ? wb->resultado_mem : wb->resultado_ula;
    id->dado_rs = val;
    cpu->stats.forwards++;
  }

  // Forward para dado_rt (operando B)
  if (ex->valida && ex->s.esc_reg && ex->reg_dest != 0 &&
      ex->reg_dest == id->inst.rt) {
    id->dado_rt = ex->resultado_ula;
    cpu->stats.forwards++;
  } else if (wb->valida && wb->s.esc_reg && wb->reg_dest != 0 &&
             wb->reg_dest == id->inst.rt) {
    int val = wb->s.mem_para_reg ? wb->resultado_mem : wb->resultado_ula;
    id->dado_rt = val;
    cpu->stats.forwards++;
  }
}
void detecta_hazard(CPU *cpu) {
  cpu->stall = 0;
  Reg_IDEX *id = &cpu->id_ex;
  Reg_IFID *f = &cpu->if_id;

  if (!id->valida || !f->valida)
    return;

  // Verifica se ID/EX é um LW (mem_para_reg == 1)
  if (id->s.mem_para_reg) {
    int dest_lw = id->s.reg_destino ? id->inst.rd : id->inst.rt;
    if (dest_lw == 0)
      return; // $r0 nunca causa hazard

    // Decodifica instrucao em IF/ID para saber quais regs ela usa
    Instrucao tmp = f->inst;
    Sinais s_next = decoder(&tmp);

    // Verifica se a proxima instrucao usa rs ou rt do LW
    int usa_rs = 1; // todas as instrucoes usam rs
    int usa_rt = (!s_next.ula_fonte && !s_next.jump); // R-type e BEQ usam rt

    if ((usa_rs && tmp.rs == dest_lw) || (usa_rt && tmp.rt == dest_lw)) {
      cpu->stall = 1;
    }
  }
}

int pipeline_vazio(CPU *cpu) {
  return !cpu->if_id.valida && !cpu->id_ex.valida && !cpu->ex_mem.valida &&
         !cpu->mem_wb.valida;
}

void salva_snap(CPU *cpu) {
  // OBS PILHA
  if (cpu->i_hist >= 256)
    return;
  Snap *s = &cpu->hist[cpu->i_hist++];
  s->pc = cpu->pc;
  s->ciclos = cpu->ciclos;
  memcpy(s->reg, cpu->banco->reg, MAX_REG * sizeof(signed char));
  if (cpu->mem_dados->dados) {
    memcpy(s->dados, cpu->mem_dados->dados, MAX_MEM * sizeof(int));
  }
  s->if_id = cpu->if_id;
  s->id_ex = cpu->id_ex;
  s->ex_mem = cpu->ex_mem;
  s->mem_wb = cpu->mem_wb;
  s->stats = cpu->stats;
}

void executa_ciclo(CPU *cpu) {
  salva_snap(cpu);

  // Detecta hazards ANTES de executar os estagios
  detecta_hazard(cpu);

  // (Ordem de execucao: WB -> MEM -> EX -> ID -> IF)
  estagio_WB(cpu);
  estagio_MEM(cpu);

  // Aplica forwarding antes de EX usar os dados
  forwarding_unit(cpu);

  estagio_EX(cpu);
  estagio_ID(cpu);
  estagio_IF(cpu);
  cpu->ciclos++;

  // Verifica termino: sem instrucoes para buscar E pipeline vazio
  if (cpu->pc >= cpu->mem_inst->tamanho && pipeline_vazio(cpu)) {
    cpu->rodando = 0;
  }
}
void executa_programa(CPU *cpu) {
  cpu->rodando = 1;
  while (cpu->rodando) {
    executa_ciclo(cpu);
  }
  printf("\nExecucao concluida em %d ciclos (%d instrucoes completas).\n",
         cpu->ciclos, cpu->stats.completas);
}
void volta_ciclo(CPU *cpu) {
  // OBS PILHA
  Snap *s = &cpu->hist[--cpu->i_hist];
  cpu->pc = s->pc;
  cpu->ciclos = s->ciclos;
  memcpy(cpu->banco->reg, s->reg, MAX_REG * sizeof(signed char));
  if (cpu->mem_dados->dados) {
    memcpy(cpu->mem_dados->dados, s->dados, MAX_MEM * sizeof(int));
  }
  cpu->if_id = s->if_id;
  cpu->id_ex = s->id_ex;
  cpu->ex_mem = s->ex_mem;
  cpu->mem_wb = s->mem_wb;
  cpu->stats = s->stats;
  cpu->rodando = 1;
  printf("Ciclo desfeito. PC=%d | Ciclo=%d\n", cpu->pc, cpu->ciclos);
}
void reinicia(CPU *cpu) {
  cpu->pc = cpu->ciclos = cpu->rodando = cpu->i_hist = cpu->stall = 0;
  memset(&cpu->if_id, 0, sizeof(Reg_IFID));
  memset(&cpu->id_ex, 0, sizeof(Reg_IDEX));
  memset(&cpu->ex_mem, 0, sizeof(Reg_EXMEM));
  memset(&cpu->mem_wb, 0, sizeof(Reg_MEMWB));
  memset(cpu->banco->reg, 0, sizeof(cpu->banco->reg));
  memset(&cpu->stats, 0, sizeof(Stats));
  memset(cpu->mem_dados->dados, 0, MAX_MEM * sizeof(int));
  cpu->mem_dados->tamanho = 0;
  memset(cpu->mem_inst->inst, 0, MAX_MEM * sizeof(Instrucao));
  cpu->mem_inst->tamanho = 0;
  printf("Pipeline e memorias reinicializados.\n");
}

// PRINTs
void print_menu(void) {
  printf("\n+----------- MENU ------------+\n"
         "| 1. Carregar .mem            |\n"
         "| 2. Carregar .dat            |\n"
         "| 3. Executar programa (Run)  |\n"
         "| 4. Executar um ciclo (Step) |\n"
         "| 5. Voltar ciclo (Back Step) |\n"
         "| 6. Reiniciar                |\n"
         "| 7. Imprimir memorias        |\n"
         "| 8. Imprimir registradores   |\n"
         "| 9. Imprimir pipeline        |\n"
         "|10. Estatisticas             |\n"
         "|11. Salvar .dat              |\n"
         "| 0. Sair                     |\n"
         "+-----------------------------+\n"
         "Opcao: ");
}
void print_inst(const Instrucao *raw, char *buf) {
  if (!raw->valida) {
    strcpy(buf, "--- (bolha)");
    return;
  }
  Instrucao tmp = *raw;
  decoder(&tmp);
  disassembla(&tmp, buf);
}
void print_mem_inst(CPU *cpu) {
  char asm_buf[64];
  printf("\n+------+------------------+----------------------------------------"
         "--+\n");
  printf("| Addr | Binario          | Assembly                                 "
         "|\n");
  printf("+------+------------------+------------------------------------------"
         "+\n");
  for (int i = 0; i < MAX_MEM; i++) {
    print_inst(&cpu->mem_inst->inst[i], asm_buf);
    printf("| %4d | %-16s | %-40s |\n", i, cpu->mem_inst->inst[i].bin, asm_buf);
  }
  printf("+------+------------------+------------------------------------------"
         "+\n");
  printf("Total: %d instrucoes\n", cpu->mem_inst->tamanho);
}
void print_mem_dat(CPU *cpu) {
  printf("\n+------+--------+\n");
  printf("| Addr |  Valor |\n");
  printf("+------+--------+\n");
  for (int i = 0; i < MAX_MEM; i++) {
    printf("| %4d | %6d |\n", i, cpu->mem_dados->dados[i]);
  }
  printf("+------+--------+\n");
  printf("Total: %d posicoes\n", cpu->mem_dados->tamanho);
}
void print_banco(CPU *cpu) {
  printf("\n+------+--------+\n");
  printf("| Reg  |  Valor |\n");
  printf("+------+--------+\n");
  for (int i = 0; i < MAX_REG; i++)
    printf("| %4s | %6d |\n", cpu->banco->nome[i], (int)cpu->banco->reg[i]);
  printf("+------+--------+\n");
}
void print_pipeline(CPU *cpu) {
  char b0[64], b1[64], b2[64], b3[64];

  /* IF/ID: instrução ainda não decodificada — decodifica cópia para display */
  print_inst(&cpu->if_id.inst, b0);
  if (!cpu->if_id.valida) {
    strcpy(b0, "--- (bolha)");
  }

  /* ID/EX já decodificadas */
  disassembla(&cpu->id_ex.inst, b1);
  if (!cpu->id_ex.valida)
    strcpy(b1, "--- (bolha)");
  disassembla(&cpu->ex_mem.inst, b2);
  if (!cpu->ex_mem.valida)
    strcpy(b2, "--- (bolha)");
  disassembla(&cpu->mem_wb.inst, b3);
  if (!cpu->mem_wb.valida)
    strcpy(b3, "--- (bolha)");

  printf(
      "\n+-------+-----------------------------------------------------------+"
      "\n"
      "| PIPELINE — Ciclo %-4d  |  PC = %-3d  |  Completas: %-4d           |\n"
      "+--------+-----------------------------------------------------------+\n"
      "| Estado | Instrucao                                                |\n"
      "+--------+-----------------------------------------------------------+\n"
      "|   IF   | %-57s|\n"
      "|   ID   | %-57s|\n"
      "|   EX   | %-57s|\n"
      "|   MEM  | %-57s|\n"
      "|   WB   | %-57s|\n" // WB processa o que está em MEM/WB
      "+--------+-----------------------------------------------------------+"
      "\n",
      cpu->ciclos, cpu->pc, cpu->stats.completas, b0, b1, b2, b3, b3);
}
void print_stats(CPU *cpu) {
  printf("\n======== ESTATISTICAS ========\n"
         "Ciclos totais: %d\n"
         "Instrucoes decodificadas: %d\n"
         "Instrucoes completas: %d\n",
         cpu->ciclos, cpu->stats.total, cpu->stats.completas);
  if (cpu->stats.completas > 0 && cpu->ciclos > 0) {
    printf("CPI: %.2f\n", (float)cpu->ciclos / (float)cpu->stats.completas);
  }
  printf("\n--- Hazards ---\n"
         "Stalls (load-use):  %d\n"
         "Forwards (EX/MEM):  %d\n"
         "Flushes (branch/jump): %d\n",
         cpu->stats.stalls, cpu->stats.forwards, cpu->stats.flushes);
  printf("\nTipo R: %d\n"
         "  ADD = %d | SUB = %d | AND = %d | OR = %d\n"
         "Tipo I: %d\n"
         "  ADDI = %d | LW = %d | SW = %d | BEQ = %d\n"
         "Tipo J: %d  (JUMP=%d)\n",
         cpu->stats.tipo_r, cpu->stats.add, cpu->stats.sub, cpu->stats.and_op,
         cpu->stats.or_op, cpu->stats.tipo_i, cpu->stats.addi, cpu->stats.lw,
         cpu->stats.sw_op, cpu->stats.beq, cpu->stats.tipo_j, cpu->stats.jump);
}