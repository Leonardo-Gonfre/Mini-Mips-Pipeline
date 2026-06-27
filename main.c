#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "bib.h"

/* cores */
#define CP_HDR  1
#define CP_SEL  2
#define CP_NRM  3
#define CP_OK   4
#define CP_BUB  5
#define CP_SIG  6
#define CP_FWD  7
#define CP_STL  8
#define CP_FLU  9
#define CP_REG 10
#define CP_FOT 11
#define CP_NXT 12

/* dimensoes */
#define MENU_W 22
#define RGHT_W 30
#define HDR_H   4
#define FOOT_H  2
#define REG_H  12

static WINDOW *whdr, *wmenu, *wctr, *wreg, *wevt, *wfoot;
static int GR, GC, MH, CW, EH;

#define MENU_N 12
static const char *MI[MENU_N]={
  "  1. Carregar .mem  ",
  "  2. Carregar .dat  ",
  "  3. Run            ",
  "  4. Step           ",
  "  5. Back Step      ",
  "  6. Reiniciar      ",
  "  7. Mem. Instrucoes",
  "  8. Mem. Dados     ",
  "  9. Pipeline       ",
  " 10. Estatisticas   ",
  " 11. Salvar .dat    ",
  "  0. Sair           "
};
static const int MA[MENU_N]={1,2,3,4,5,6,7,8,9,10,11,0};
static char gstatus[256]="Carregue um arquivo .mem (opcao 1).";

static void init_colors(void){
  start_color();
  init_pair(CP_HDR, COLOR_WHITE,  COLOR_BLACK);
  init_pair(CP_SEL, COLOR_BLACK,  COLOR_CYAN);
  init_pair(CP_NRM, COLOR_WHITE,  COLOR_BLACK);
  init_pair(CP_OK,  COLOR_GREEN,  COLOR_BLACK);
  init_pair(CP_BUB, COLOR_WHITE,  COLOR_BLACK);
  init_pair(CP_SIG, COLOR_CYAN,   COLOR_BLACK);
  init_pair(CP_FWD, COLOR_YELLOW, COLOR_BLACK);
  init_pair(CP_STL, COLOR_RED,    COLOR_BLACK);
  init_pair(CP_FLU, COLOR_MAGENTA,COLOR_BLACK);
  init_pair(CP_REG, COLOR_CYAN,   COLOR_BLACK);
  init_pair(CP_FOT, COLOR_BLACK,  COLOR_WHITE);
  init_pair(CP_NXT, COLOR_YELLOW, COLOR_BLACK);
}

static void mk_wins(void){
  getmaxyx(stdscr,GR,GC);
  MH=GR-HDR_H-FOOT_H;
  CW=GC-MENU_W-RGHT_W;
  EH=MH-REG_H;
  if(EH<3)EH=3;
  whdr  = newwin(HDR_H, GC,   0,       0);
  wmenu = newwin(MH,    MENU_W, HDR_H,  0);
  wctr  = newwin(MH,    CW,   HDR_H, MENU_W);
  wreg  = newwin(REG_H, RGHT_W, HDR_H, MENU_W+CW);
  wevt  = newwin(EH,    RGHT_W, HDR_H+REG_H, MENU_W+CW);
  wfoot = newwin(FOOT_H,GC,   GR-FOOT_H, 0);
  keypad(wmenu,TRUE);
}

static void del_wins(void){
  if(whdr)  delwin(whdr);
  if(wmenu) delwin(wmenu);
  if(wctr)  delwin(wctr);
  if(wreg)  delwin(wreg);
  if(wevt)  delwin(wevt);
  if(wfoot) delwin(wfoot);
}

static void draw_hdr(CPU *cpu){
  werase(whdr);
  wattron(whdr,COLOR_PAIR(CP_HDR)|A_BOLD);
  int w; getmaxyx(whdr,w,w); (void)w;
  mvwprintw(whdr,0,0,"  MINI-MIPS PIPELINE SIMULATOR");
  wattroff(whdr,COLOR_PAIR(CP_HDR)|A_BOLD);
  wattron(whdr,COLOR_PAIR(CP_HDR));
  float cpi=0;
  if(cpu->stats.completas>0) cpi=(float)cpu->ciclos/cpu->stats.completas;
  mvwprintw(whdr,1,1,"PC:%-3d  Ciclo:%-4d  Completas:%-3d  CPI:%.2f",
    cpu->pc,cpu->ciclos,cpu->stats.completas,cpi);
  mvwprintw(whdr,2,1,"Stalls:%-3d  Forwards:%-3d  Flushes:%-3d  Inst.carregadas:%-3d",
    cpu->stats.stalls,cpu->stats.forwards,cpu->stats.flushes,cpu->mem_inst->tamanho);
  mvwprintw(whdr,3,1,"Status: %s",gstatus);
  wclrtoeol(whdr);
  wattroff(whdr,COLOR_PAIR(CP_HDR));
  wrefresh(whdr);
}

static void draw_menu(int hi){
  werase(wmenu);
  wattron(wmenu,COLOR_PAIR(CP_NRM));
  box(wmenu,0,0);
  mvwprintw(wmenu,0,3," MENU ");
  for(int i=0;i<MENU_N;i++){
    if(i==hi){
      wattron(wmenu,COLOR_PAIR(CP_SEL)|A_BOLD);
      mvwprintw(wmenu,i+1,1,"%-20s",MI[i]);
      wattroff(wmenu,COLOR_PAIR(CP_SEL)|A_BOLD);
    } else {
      wattron(wmenu,COLOR_PAIR(CP_NRM));
      mvwprintw(wmenu,i+1,1,"%-20s",MI[i]);
      wattroff(wmenu,COLOR_PAIR(CP_NRM));
    }
  }
  wattroff(wmenu,COLOR_PAIR(CP_NRM));
  wrefresh(wmenu);
}

static void draw_foot(void){
  werase(wfoot);
  wattron(wfoot,COLOR_PAIR(CP_FOT));
  mvwprintw(wfoot,0,1,"[W/K] Cima  [S/J] Baixo  [ENTER] Selecionar  [Q] Sair");
  wattroff(wfoot,COLOR_PAIR(CP_FOT));
  wrefresh(wfoot);
}

static void draw_stage(WINDOW *w,int *row,const char *name,
  const Instrucao *inst,const Sinais *s,int is_bubble,
  int show_ula,int ula_res,int ula_zero,
  int show_dest,int reg_dest,int show_mem,int mem_res,int stall_flag,
  int stage_type)
{
  int mw; int dummy; getmaxyx(w,dummy,mw); (void)dummy;
  int bw=mw-2;
  /* borda superior */
  wattron(w,COLOR_PAIR(CP_SIG));
  mvwprintw(w,*row,1,"+-- %-8s",name);
  for(int c=11;c<bw;c++) waddch(w,'-');
  waddch(w,'+');
  wattroff(w,COLOR_PAIR(CP_SIG));
  (*row)++;
  if(*row>=MH-1)return;

  char ibuf[64];
  if(is_bubble){
    /* mostra bolha e sinais zerados */
    wattron(w,COLOR_PAIR(CP_BUB)|A_DIM);
    if(stall_flag)
      mvwprintw(w,*row,2,"| (STALL - bolha inserida)%*s|",bw-27,"");
    else
      mvwprintw(w,*row,2,"| (bolha)%*s|",bw-10,"");
    wattroff(w,COLOR_PAIR(CP_BUB)|A_DIM);
    (*row)++;
    if(*row>=MH-1) return;

    /* Exibe sinais zerados para bolha (filtrados por estagio) */
    wattron(w,COLOR_PAIR(CP_SIG));
    if(stage_type <= 1){
      /* IF/ID e ID/EX: todos os sinais */
      mvwprintw(w,*row,2,"| Sinais: RegDst=%-1d UlaFte=%-1d EscReg=%-1d EscMem=%-1d%*s|",
        0,0,0,0,bw-42,"");
      (*row)++;
      if(*row<MH-1){
        mvwprintw(w,*row,2,"|         Branch=%-1d  Jump=%-1d   MemToReg=%-1d  CtrlULA=%-1d%*s|",
          0,0,0,0,bw-48,"");
        (*row)++;
      }
    } else if(stage_type == 2){
      /* EX/MEM: sinais MEM+WB (EX ja consumidos) */
      mvwprintw(w,*row,2,"| Sinais: EscReg=%-1d EscMem=%-1d MemToReg=%-1d%*s|",
        0,0,0,bw-34,"");
      (*row)++;
    } else {
      /* MEM/WB: so sinais WB */
      mvwprintw(w,*row,2,"| Sinais: EscReg=%-1d MemToReg=%-1d%*s|",
        0,0,bw-25,"");
      (*row)++;
    }
    wattroff(w,COLOR_PAIR(CP_SIG));

    if(show_ula && *row<MH-1){
      wattron(w,COLOR_PAIR(CP_FWD));
      mvwprintw(w,*row,2,"| ULA: resultado=%-6d  Zero=%-1d%*s|",0,0,bw-30,"");
      wattroff(w,COLOR_PAIR(CP_FWD));
      (*row)++;
    }
    if(show_dest && *row<MH-1){
      wattron(w,COLOR_PAIR(CP_REG));
      if(show_mem)
        mvwprintw(w,*row,2,"| Dest:$r%-1d  MemToReg:%-1d  result_mem=%-6d%*s|",
          0,0,0,bw-42,"");
      else
        mvwprintw(w,*row,2,"| Destino: $r%-1d  EscReg=%-1d  MemToReg=%-1d%*s|",
          0,0,0,bw-36,"");
      wattroff(w,COLOR_PAIR(CP_REG));
      (*row)++;
    }
  } else if (!inst->valida) {
    wattron(w,COLOR_PAIR(CP_BUB)|A_DIM);
    mvwprintw(w,*row,2,"| %*s|",bw-10,"");
    wattroff(w,COLOR_PAIR(CP_BUB)|A_DIM);
  } else {
    Instrucao tmp = *inst;
    decoder(&tmp);
    disassembla(&tmp,(char*)ibuf);
    wattron(w,COLOR_PAIR(CP_OK)|A_BOLD);
    mvwprintw(w,*row,2,"| Inst: %-*s|",bw-10,ibuf);
    wattroff(w,COLOR_PAIR(CP_OK)|A_BOLD);
  }
  (*row)++;
  if(*row>=MH-1)return;

  if(!is_bubble && inst->valida && s){
    wattron(w,COLOR_PAIR(CP_SIG));
    static const char *cn[]={"ADD","SUB","AND","OR"};
    if(stage_type <= 1){
      /* IF/ID e ID/EX: todos os sinais */
      mvwprintw(w,*row,2,"| Sinais: RegDst=%-1d UlaFte=%-1d EscReg=%-1d EscMem=%-1d%*s|",
        s->reg_destino,s->ula_fonte,s->esc_reg,s->esc_mem,bw-42,"");
      (*row)++;
      if(*row<MH-1){
        mvwprintw(w,*row,2,"|         Branch=%-1d  Jump=%-1d   MemToReg=%-1d  CtrlULA=%s%*s|",
          s->branch,s->jump,s->mem_para_reg,
          (s->ctrl_ula<4?cn[s->ctrl_ula]:"?"),bw-48,"");
        (*row)++;
      }
    } else if(stage_type == 2){
      /* EX/MEM: sinais MEM+WB (EX ja consumidos) */
      mvwprintw(w,*row,2,"| Sinais: EscReg=%-1d EscMem=%-1d MemToReg=%-1d%*s|",
        s->esc_reg,s->esc_mem,s->mem_para_reg,bw-34,"");
      (*row)++;
    } else {
      /* MEM/WB: so sinais WB */
      mvwprintw(w,*row,2,"| Sinais: EscReg=%-1d MemToReg=%-1d%*s|",
        s->esc_reg,s->mem_para_reg,bw-25,"");
      (*row)++;
    }
    wattroff(w,COLOR_PAIR(CP_SIG));
    if(show_ula && *row<MH-1){
      wattron(w,COLOR_PAIR(CP_FWD));
      mvwprintw(w,*row,2,"| ULA: resultado=%-6d  Zero=%-1d%*s|",ula_res,ula_zero,bw-30,"");
      wattroff(w,COLOR_PAIR(CP_FWD));
      (*row)++;
    }
    if(show_dest && *row<MH-1){
      wattron(w,COLOR_PAIR(CP_REG));
      if(show_mem)
        mvwprintw(w,*row,2,"| Dest:$r%-1d  MemToReg:%-1d  result_mem=%-6d%*s|",
          reg_dest,s->mem_para_reg,mem_res,bw-42,"");
      else
        mvwprintw(w,*row,2,"| Destino: $r%-1d  EscReg=%-1d  MemToReg=%-1d%*s|",
          reg_dest,s->esc_reg,s->mem_para_reg,bw-36,"");
      wattroff(w,COLOR_PAIR(CP_REG));
      (*row)++;
    }
  }
  if(*row<MH-1){
    wattron(w,COLOR_PAIR(CP_SIG));
    mvwprintw(w,*row,1,"+");
    for(int c=2;c<bw+1;c++) waddch(w,'-');
    waddch(w,'+');
    wattroff(w,COLOR_PAIR(CP_SIG));
    (*row)++;
  }
}

static void draw_pipeline(CPU *cpu){
  werase(wctr);
  box(wctr,0,0);
  mvwprintw(wctr,0,3," PIPELINE - Ciclo %d ",cpu->ciclos);
  int row=1;

  /* proxima instrucao */
  wattron(wctr,COLOR_PAIR(CP_NXT)|A_BOLD);
  if(cpu->pc<cpu->mem_inst->tamanho){
    Instrucao tmp=cpu->mem_inst->inst[cpu->pc];
    decoder(&tmp);
    char nb[64]; disassembla(&tmp,nb);
    mvwprintw(wctr,row,2,"Proxima (PC=%d): %s",cpu->pc,nb);
  } else {
    mvwprintw(wctr,row,2,"Proxima (PC=%d): (fim do programa)",cpu->pc);
  }
  wattroff(wctr,COLOR_PAIR(CP_NXT)|A_BOLD);
  row+=2;

  /* IF/ID */
  {
    Sinais ifid_s;
    memset(&ifid_s, 0, sizeof(Sinais));
    if(!cpu->if_id.bolha && cpu->if_id.inst.valida){
      Instrucao tmp_ifid = cpu->if_id.inst;
      ifid_s = decoder(&tmp_ifid);
    }
    draw_stage(wctr,&row,"IF/ID",&cpu->if_id.inst,&ifid_s,
      cpu->if_id.bolha,0,0,0,0,0,0,0,cpu->stall,0);
  }

  /* ID/EX */
  draw_stage(wctr,&row,"ID/EX",&cpu->id_ex.inst,&cpu->id_ex.s,
    cpu->id_ex.bolha,0,0,0,0,0,0,0,0,1);

  /* EX/MEM */
  draw_stage(wctr,&row,"EX/MEM",&cpu->ex_mem.inst,&cpu->ex_mem.s,
    cpu->ex_mem.bolha,
    1,cpu->ex_mem.resultado_ula,cpu->ex_mem.flag_zero,0,0,0,0,0,2);

  /* MEM/WB */
  draw_stage(wctr,&row,"MEM/WB",&cpu->mem_wb.inst,&cpu->mem_wb.s,
    cpu->mem_wb.bolha,0,0,0,
    1,cpu->mem_wb.reg_dest,1,cpu->mem_wb.resultado_mem,0,3);

  /* WB concluido */
  if(row<MH-2 && cpu->wb_last_valida){
    wattron(wctr,COLOR_PAIR(CP_OK));
    char wb[64]; disassembla(&cpu->wb_last,wb);
    mvwprintw(wctr,row,2,"Concluida: %s",wb);
    wattroff(wctr,COLOR_PAIR(CP_OK));
  }
  wrefresh(wctr);
}

static void draw_regs(CPU *cpu){
  werase(wreg);
  box(wreg,0,0);
  mvwprintw(wreg,0,3," REGISTRADORES ");
  wattron(wreg,COLOR_PAIR(CP_REG)|A_BOLD);
  for(int i=0;i<MAX_REG;i++)
    mvwprintw(wreg,i+1,2,"%-4s = %d",cpu->banco->nome[i],(int)cpu->banco->reg[i]);
  wattroff(wreg,COLOR_PAIR(CP_REG)|A_BOLD);
  wrefresh(wreg);
}

static void draw_events(CPU *cpu){
  werase(wevt);
  box(wevt,0,0);
  mvwprintw(wevt,0,3," HAZARDS ");
  int row=1;
  int pw=RGHT_W-4; /* largura util do painel */
  char *p=cpu->cycle_log;
  while(*p && row<EH-1){
    /* le ate nova linha */
    char line[512]; int i=0;
    while(*p && *p!='\n' && i<511) line[i++]=*p++;
    if(*p=='\n') p++;
    line[i]='\0';
    if(i==0) continue;
    /* cor baseada no conteudo */
    int attr=COLOR_PAIR(CP_NRM);
    if(strstr(line,"FORWARD"))  attr=COLOR_PAIR(CP_FWD)|A_BOLD;
    else if(strstr(line,"STALL")) attr=COLOR_PAIR(CP_STL)|A_BOLD;
    else if(strstr(line,"FLUSH")) attr=COLOR_PAIR(CP_FLU)|A_BOLD;
    /* quebra linha em chunks de pw chars */
    int pos=0, len=(int)strlen(line);
    while(pos<len && row<EH-1){
      wattron(wevt,attr);
      mvwprintw(wevt,row,2,"%-*.*s",pw,pw,line+pos);
      wattroff(wevt,attr);
      pos+=pw; row++;
    }
  }
  if(row==1){
    wattron(wevt,COLOR_PAIR(CP_BUB)|A_DIM);
    mvwprintw(wevt,1,2,"(sem eventos)");
    wattroff(wevt,COLOR_PAIR(CP_BUB)|A_DIM);
  }
  wrefresh(wevt);
}

static void input_dlg(const char *prompt, char *out, int maxlen){
  int h=5,w=60,y=(GR-h)/2,x=(GC-w)/2;
  WINDOW *dw=newwin(h,w,y,x);
  box(dw,0,0);
  mvwprintw(dw,0,2," Entrada ");
  mvwprintw(dw,1,2,"%s",prompt);
  echo(); curs_set(1);
  mvwgetnstr(dw,2,2,out,maxlen-1);
  noecho(); curs_set(0);
  delwin(dw);
  touchwin(stdscr); refresh();
}

static void msg_dlg(const char *msg){
  int h=5,w=62,y=(GR-h)/2,x=(GC-w)/2;
  WINDOW *dw=newwin(h,w,y,x);
  box(dw,0,0);
  mvwprintw(dw,0,2," Mensagem ");
  mvwprintw(dw,2,2,"%.58s",msg);
  mvwprintw(dw,3,2,"[Pressione qualquer tecla]");
  wrefresh(dw);
  wgetch(dw);
  delwin(dw);
  touchwin(stdscr); refresh();
}

static void draw_mem_inst(CPU *cpu){
  werase(wctr); box(wctr,0,0);
  mvwprintw(wctr,0,3," MEM. INSTRUCOES ");
  int row=1,mw;int dummy;getmaxyx(wctr,dummy,mw);(void)dummy;
  wattron(wctr,COLOR_PAIR(CP_SIG));
  mvwprintw(wctr,row++,1,"Addr  Binary            Assembly");
  mvwprintw(wctr,row++,1,"----  ----------------  ----------------------------------");
  wattroff(wctr,COLOR_PAIR(CP_SIG));
  for(int i=0;i<cpu->mem_inst->tamanho && row<MH-1;i++){
    char ab[64]; print_inst(&cpu->mem_inst->inst[i],ab);
    int at=COLOR_PAIR(CP_NRM);
    if(i==cpu->pc) at=COLOR_PAIR(CP_NXT)|A_BOLD;
    wattron(wctr,at);
    mvwprintw(wctr,row++,1,"%4d  %-16s  %-*s",i,
      cpu->mem_inst->inst[i].bin,mw-26,ab);
    wattroff(wctr,at);
  }
  mvwprintw(wctr,row,1,"Total: %d instrucoes  (PC atual=%d)",
    cpu->mem_inst->tamanho,cpu->pc);
  wrefresh(wctr);
}

static void draw_mem_dat(CPU *cpu){
  werase(wctr); box(wctr,0,0);
  mvwprintw(wctr,0,3," MEM. DADOS ");
  int row=1;
  int mw,dummy; getmaxyx(wctr,dummy,mw); (void)dummy;
  int usable = mw - 2;
  int colw = 16; // largura de cada coluna de dados com separador
  int max_cols = usable / colw;
  if(max_cols < 1) max_cols = 1;
  int rows = MH - 4; // duas linhas de cabeçalho + linha de total
  if(rows < 1) rows = 1;
  int cols = (MAX_MEM + rows - 1) / rows;
  if(cols > max_cols) cols = max_cols;
  rows = (MAX_MEM + cols - 1) / cols;

  // Cabeçalhos repetidos por coluna
  wattron(wctr,COLOR_PAIR(CP_SIG));
  int x = 1;
  for(int c=0; c<cols; c++){
    mvwprintw(wctr,row, x, "Addr  Valor  ");
    x += colw;
    if(c < cols-1 && x < mw-1) mvwprintw(wctr,row,x-1,"|");
  }
  row++;
  x = 1;
  for(int c=0; c<cols; c++){
    mvwprintw(wctr,row, x, "----  -------");
    x += colw;
    if(c < cols-1 && x < mw-1) mvwprintw(wctr,row,x-1,"|");
  }
  row++;
  wattroff(wctr,COLOR_PAIR(CP_SIG));

  for(int r=0; r<rows && row<MH-1; r++){
    int x = 1;
    for(int c=0; c<cols; c++){
      int idx = c*rows + r;
      if(idx >= MAX_MEM) break;
      wattron(wctr,COLOR_PAIR(CP_NRM));
      mvwprintw(wctr,row,x,"%4d: %6d",idx,cpu->mem_dados->dados[idx]);
      wattroff(wctr,COLOR_PAIR(CP_NRM));
      x += colw;
      if(c < cols-1 && x < mw-1){
        mvwprintw(wctr,row,x-1,"|");
      }
    }
    row++;
  }
  mvwprintw(wctr,row,1,"Total: %d posicoes  Colunas: %d  Linhas: %d",MAX_MEM,cols,rows);
  wrefresh(wctr);
}

static void draw_stats(CPU *cpu){
  werase(wctr); box(wctr,0,0);
  wattron(wctr,COLOR_PAIR(CP_NRM));
  mvwprintw(wctr,0,3," ESTATISTICAS ");
  wattroff(wctr,COLOR_PAIR(CP_NRM));
  int r=1;
  float cpi=cpu->stats.completas>0?(float)cpu->ciclos/cpu->stats.completas:0;
  wattron(wctr,COLOR_PAIR(CP_OK)|A_BOLD);
  mvwprintw(wctr,r++,2,"Ciclos totais:       %d",cpu->ciclos);
  mvwprintw(wctr,r++,2,"Inst. buscadas:      %d",cpu->stats.buscadas);
  mvwprintw(wctr,r++,2,"Inst. decodificadas: %d",cpu->stats.total);
  mvwprintw(wctr,r++,2,"Inst. completas:     %d",cpu->stats.completas);
  mvwprintw(wctr,r++,2,"CPI:                 %.2f",cpi);
  r++;
  wattroff(wctr,COLOR_PAIR(CP_OK)|A_BOLD);
  wattron(wctr,COLOR_PAIR(CP_STL)|A_BOLD);
  mvwprintw(wctr,r++,2,"--- Hazards ---");
  mvwprintw(wctr,r++,2,"Stalls (load-use):   %d",cpu->stats.stalls);
  mvwprintw(wctr,r++,2,"Forwards:            %d",cpu->stats.forwards);
  mvwprintw(wctr,r++,2,"Flushes (br/jump):   %d",cpu->stats.flushes);
  r++;
  wattroff(wctr,COLOR_PAIR(CP_STL)|A_BOLD);
  wattron(wctr,COLOR_PAIR(CP_SIG));
  mvwprintw(wctr,r++,2,"--- Instrucoes ---");
  mvwprintw(wctr,r++,2,"Tipo R: %d  ADD=%d SUB=%d AND=%d OR=%d",
    cpu->stats.tipo_r,cpu->stats.add,cpu->stats.sub,cpu->stats.and_op,cpu->stats.or_op);
  mvwprintw(wctr,r++,2,"Tipo I: %d  ADDI=%d LW=%d SW=%d BEQ=%d",
    cpu->stats.tipo_i,cpu->stats.addi,cpu->stats.lw,cpu->stats.sw_op,cpu->stats.beq);
  mvwprintw(wctr,r++,2,"Tipo J: %d  JUMP=%d",cpu->stats.tipo_j,cpu->stats.jump);
  wattroff(wctr,COLOR_PAIR(CP_SIG));
  wrefresh(wctr);
}

static void redraw(CPU *cpu,int hi,int mode){
  draw_hdr(cpu);
  draw_menu(hi);
  draw_foot();
  draw_regs(cpu);
  draw_events(cpu);
  switch(mode){
    case 0: draw_pipeline(cpu); break;
    case 1: draw_mem_inst(cpu); break;
    case 2: draw_mem_dat(cpu);  break;
    case 3: draw_stats(cpu);    break;
  }
  doupdate();
}

int main(void){
  CPU      *cpu  = (CPU *)     calloc(1,sizeof(CPU));
  MemInst  *mi   = (MemInst *) calloc(1,sizeof(MemInst));
  MemDados *md   = (MemDados *)calloc(1,sizeof(MemDados));
  Banco    *bnc  = (Banco *)   calloc(1,sizeof(Banco));
  Snap     *hist = (Snap *)    calloc(256,sizeof(Snap));
  if(!cpu||!mi||!md||!bnc||!hist){fputs("sem memoria\n",stderr);return 1;}
  cpu->mem_inst=mi; cpu->mem_dados=md; cpu->banco=bnc; cpu->hist=hist;
  inicializa_cpu(cpu);
  cpu->quiet = 1;

  initscr(); cbreak(); noecho(); curs_set(0);
  keypad(stdscr,TRUE);
  init_colors();
  mk_wins();

  int hi=0, mode=0, running=1;
  redraw(cpu,hi,mode);

  while(running){
    int ch=wgetch(wmenu);
    switch(ch){
      case KEY_UP: case 'w': case 'k':
        if(hi>0){ hi--; } break;
      case KEY_DOWN: case 's': case 'j':
        if(hi<MENU_N-1){ hi++; } break;
      case '\n': case KEY_ENTER:{
        int ac=MA[hi];
        char fn[256]="";
        int r;
        switch(ac){
          case 1:
            reinicia(cpu);
            input_dlg("Arquivo .mem:",fn,256);
            r=carrega_mem_filename(cpu,fn);
            if(r<0) snprintf(gstatus,256,"ERRO: arquivo '%.80s' nao encontrado.",fn);
            else    snprintf(gstatus,256,"%d instrucoes carregadas de '%.80s'.",r,fn);
            mode=0; break;
          case 2:
            input_dlg("Arquivo .dat:",fn,256);
            r=carrega_dat_filename(cpu,fn);
            if(r<0) snprintf(gstatus,256,"ERRO: arquivo '%.80s' nao encontrado.",fn);
            else    snprintf(gstatus,256,"%d valores carregados de '%.80s'.",r,fn);
            mode=0; break;
          case 3:
            if(cpu->mem_inst->tamanho==0){
              msg_dlg("Carregue um arquivo .mem primeiro (opcao 1).");
            } else {
              cpu->rodando=1;
              snprintf(gstatus,256,"Executando...");
              redraw(cpu,hi,mode);
              executa_programa(cpu);
              snprintf(gstatus,256,"Concluido: %d ciclos, %d inst. completas.",
                cpu->ciclos,cpu->stats.completas);
            }
            mode=0; break;
          case 4:
            if(cpu->mem_inst->tamanho==0){
              msg_dlg("Carregue um arquivo .mem primeiro (opcao 1).");
            } else {
              if(!cpu->rodando && pipeline_vazio(cpu) &&
                 cpu->pc>=cpu->mem_inst->tamanho){
                msg_dlg("Execucao ja concluida.");
              } else {
                if(!cpu->rodando && cpu->pc<cpu->mem_inst->tamanho)
                  cpu->rodando=1;
                executa_ciclo(cpu);
                snprintf(gstatus,256,"Step: ciclo %d concluido. PC=%d",
                  cpu->ciclos,cpu->pc);
              }
            }
            mode=0; break;
          case 5:
            if(cpu->i_hist<=0){
              msg_dlg("Nenhum estado anterior disponivel.");
            } else {
              volta_ciclo(cpu);
              snprintf(gstatus,256,"Back-Step: voltou para ciclo %d. PC=%d",
                cpu->ciclos,cpu->pc);
            }
            mode=0; break;
          case 6:
            reinicia(cpu);
            snprintf(gstatus,256,"Pipeline e memorias reinicializados.");
            mode=0; break;
          case 7:  mode=1; snprintf(gstatus,256,"Memoria de instrucoes."); break;
          case 8:  mode=2; snprintf(gstatus,256,"Memoria de dados."); break;
          case 9: mode=0; snprintf(gstatus,256,"Visualizacao do pipeline."); break;
          case 10: mode=3; snprintf(gstatus,256,"Estatisticas."); break;
          case 11:
            input_dlg("Salvar .dat como:",fn,256);
            r=salva_dat_filename(cpu,fn);
            if(r<0) snprintf(gstatus,256,"ERRO ao salvar '%.80s'.",fn);
            else    snprintf(gstatus,256,"Salvo '%.80s' (%d posicoes).",fn,r);
            break;
          case 0: running=0; break;
        }
        break;
      }
      case 'q': case 'Q': running=0; break;
      case KEY_RESIZE:
        del_wins(); mk_wins(); break;
    }
    if(running) redraw(cpu,hi,mode);
  }

  del_wins();
  endwin();
  if(cpu->mem_inst->inst) free(cpu->mem_inst->inst);
  if(cpu->mem_dados->dados) free(cpu->mem_dados->dados);
  free(mi); free(md); free(bnc); free(hist); free(cpu);
  return 0;
}
