#include "bib.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  // ALOCACAO DE MEMORIA
  CPU *cpu = (CPU *)calloc(1, sizeof(CPU));
  MemInst *mi = (MemInst *)calloc(1, sizeof(MemInst));
  MemDados *md = (MemDados *)calloc(1, sizeof(MemDados));
  Banco *bnc = (Banco *)calloc(1, sizeof(Banco));
  Snap *hist = (Snap *)calloc(256, sizeof(Snap));
  if (!cpu || !mi || !md || !bnc || !hist) {
    printf("Erro: sem memoria suficiente.\n");
    return 1;
  }
  cpu->mem_inst = mi;
  cpu->mem_dados = md;
  cpu->banco = bnc;
  cpu->hist = hist;
  inicializa_cpu(cpu);

  // MENU
  int menu;
  printf("\n Simulador Mini MIPS 8 bits Pipeline\n");
  do {
    print_menu();
    scanf("%d", &menu);

    switch (menu) {
    case 1:
      carrega_mem(cpu);
      break;
    case 2:
      carrega_dat(cpu);
      break;
    case 3: // Run: executa até o fim
      if (cpu->mem_inst->tamanho == 0) {
        printf("Memoria de intrucoes nao carregada.\n");
        break;
      }
      executa_programa(cpu);
      break;
    case 4: // Step: executa 1 ciclo
      if (cpu->mem_inst->tamanho == 0) {
        printf("Memoria de intrucoes nao carregada.\n");
        break;
      }
      if (!cpu->rodando && pipeline_vazio(cpu) &&
          cpu->pc >= cpu->mem_inst->tamanho) {
        printf("Execucao ja concluida.\n");
        break;
      }
      if (!cpu->rodando && cpu->pc < cpu->mem_inst->tamanho) {
        cpu->rodando = 1;
      }
      executa_ciclo(cpu);
      print_pipeline(cpu);
      break;
    case 5: // Back: desfaz 1 ciclo
      if (cpu->i_hist <= 0) {
        printf("Nenhum estado anterior disponivel.\n");
        break;
      }
      volta_ciclo(cpu);
      print_pipeline(cpu);
      break;
    case 6:
      reinicia(cpu);
      break;
    case 7:  // Imprimir Mem. Instrucoes
      print_mem_inst(cpu);
      break;
    case 8:  // Imprimir Mem. Dados
      print_mem_dat(cpu);
      break;
    case 9:  // Imprimir Mem. Instrucoes + Dados
      print_mem_ambas(cpu);
      break;
    case 10: // Imprimir Registradores
      print_banco(cpu);
      break;
    case 11: // Imprimir Pipeline
      print_pipeline(cpu);
      break;
    case 12: // Estatisticas
      print_stats(cpu);
      break;
    case 13: // Salvar .dat
      salva_dat(cpu);
      break;
    case 0:
      printf("Encerrando simulador.\n");
      break;
    default:
      printf("Opcao invalida.\n");
      break;
    }
  } while (menu != 0);

  // LIBERACAO DE MEMORIA
  if (cpu->mem_inst->inst)
    free(cpu->mem_inst->inst);
  if (cpu->mem_dados->dados)
    free(cpu->mem_dados->dados);
  free(cpu->mem_inst);
  free(cpu->mem_dados);
  free(cpu->banco);
  free(cpu->hist);
  free(cpu);
  return 0;
}