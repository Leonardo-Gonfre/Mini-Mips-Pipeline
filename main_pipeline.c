#include "bib_pipeline.h"
int main(void) {

  CPU *cpu = (CPU *)calloc(1, sizeof(CPU));
  MemInst *mi = (MemInst *)calloc(1, sizeof(MemInst));
  MemDados *md = (MemDados *)calloc(1, sizeof(MemDados));
  Banco *bnc = (Banco *)calloc(1, sizeof(Banco));
  Snap *hist = (Snap *)calloc(MAX_HIST, sizeof(Snap));

  if (!cpu || !mi || !md || !bnc || !hist) {
    printf("Erro: sem memoria suficiente.\n");
    return 1;
  }

  cpu->mem_inst = mi;
  cpu->mem_dados = md;
  cpu->banco = bnc;
  cpu->hist = hist;
  inicializa_banco(cpu);

  print_header();

  int op;
  do {
    print_menu();
    if (scanf("%d", &op) != 1) {
      limpa_buf();
      op = -1;
      continue;
    }

    switch (op) {
    case 1:
      carrega_mem(cpu);
      break;
    case 2:
      carrega_dat(cpu);
      break;

    case 3:
      print_mem_inst(cpu);
      print_mem_dat(cpu);
      break;

    case 4:
      print_banco(cpu);
      break;
    case 5:
      print_pipeline(cpu);
      break;

    case 6: /* Step: executa 1 ciclo */
      if (!cpu->mem_inst->inst || cpu->mem_inst->tamanho == 0) {
        printf("Carregue instrucoes primeiro (opcao 1).\n");
        break;
      }
      if (!cpu->rodando && pipeline_vazio(cpu) &&
          cpu->pc >= cpu->mem_inst->tamanho) {
        printf("Execucao ja concluida. Use opcao 9 para reiniciar.\n");
        break;
      }
      if (!cpu->rodando && cpu->pc < cpu->mem_inst->tamanho)
        cpu->rodando = 1;
      executa_ciclo(cpu);
      print_pipeline(cpu);
      break;

    case 7: /* Back: desfaz 1 ciclo */
      volta_ciclo(cpu);
      print_pipeline(cpu);
      break;

    case 8: /* Run: executa até o fim */
      executa_programa(cpu);
      print_banco(cpu);
      print_stats(cpu);
      break;

    case 9:
      reinicia(cpu);
      break;
    case 10:
      // salva_dat(cpu);
      break;
    case 11:
      print_stats(cpu);
      break;
    case 0:
      printf("Encerrando simulador.\n");
      break;
    default:
      printf("Opcao invalida.\n");
      break;
    }
  } while (op != 0);

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
