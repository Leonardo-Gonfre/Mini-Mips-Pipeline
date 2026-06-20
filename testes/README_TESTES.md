# Testes de Hazard — Mini-MIPS Pipeline

## Encoding de referência
```
Tipo R: opcode[4] rs[3] rt[3] rd[3] funct[3]   funct: 000=ADD 001=SUB 010=AND 011=OR
Tipo I: opcode[4] rs[3] rt[3] imm[6]            imm: complemento de 2, 6 bits
Tipo J: opcode[4] addr[12]

Opcodes: 0000=R  0010=J  0100=ADDI  1000=BEQ  1011=LW  1111=SW
Regs:    000=$r0  001=$r1  010=$r2  011=$r3  100=$r4  101=$r5  110=$r6  111=$r7
```

---

## 1. hazard_ex_forwarding.mem — EX Forwarding (distância 1)

```
0100 000 001 000101  →  addi $r1, $r0, 5     ; $r1 = 5
0000 001 010 000 000  →  add  $r2, $r1, $r0   ; usa $r1 (EX hazard → forward EX/MEM→ID)
0100 010 011 000010  →  addi $r3, $r2, 2     ; usa $r2 (EX hazard → forward EX/MEM→ID)
```

**Resultados esperados:**
- `$r1 = 5`, `$r2 = 5`, `$r3 = 7`
- Stalls: 0 | Forwards: 2 | Flushes: 0
- Sem stalls pois forwarding resolve o hazard

---

## 2. hazard_mem_forwarding.mem — MEM Forwarding (distância 2)

```
0100 000 001 001001  →  addi $r1, $r0, 9     ; $r1 = 9
0100 000 011 000001  →  addi $r3, $r0, 1     ; $r3 = 1  (instrução "separadora")
0000 001 011 010 000  →  add  $r2, $r1, $r3   ; usa $r1 (MEM hazard → forward MEM/WB→ID)
```

**Resultados esperados:**
- `$r1 = 9`, `$r3 = 1`, `$r2 = 10`
- Stalls: 0 | Forwards: 1 | Flushes: 0

---

## 3. hazard_load_use_stall.mem — Load-Use Hazard (Stall)

Requer: carregar `testes/hazard_load_use.dat` com `mem[0] = 42`

```
1011 000 001 000000  →  lw   $r1, 0($r0)     ; $r1 = mem[0] = 42
0000 001 010 000 000  →  add  $r2, $r1, $r0   ; usa $r1 imediatamente → 1 STALL obrigatório
0100 010 011 000001  →  addi $r3, $r2, 1     ; $r3 = $r2 + 1
```

**Resultados esperados:**
- `$r1 = 42`, `$r2 = 42`, `$r3 = 43`
- Stalls: 1 | Forwards: 1 (MEM forward após o stall) | Flushes: 0

---

## 4. hazard_branch_flush.mem — Branch BEQ Tomado (Flush)

```
0100 000 001 000011  →  addi $r1, $r0, 3     ; $r1 = 3
0100 000 010 000011  →  addi $r2, $r0, 3     ; $r2 = 3
1000 001 010 000010  →  beq  $r1, $r2, 2     ; $r1==$r2 → branch TOMADO, PC=3+2=5
0100 000 011 001100  →  addi $r3, $r0, 12    ; DESCARTADA (flush)
0100 000 100 000111  →  addi $r4, $r0, 7     ; destino do branch, $r4 = 7
```

**Resultados esperados:**
- `$r1 = 3`, `$r2 = 3`, `$r3 = 0` (não executou), `$r4 = 7`
- Stalls: 0 | Forwards: 0 | Flushes: 1

---

## 5. hazard_jump_flush.mem — Jump Flush

```
0100 000 001 000101  →  addi $r1, $r0, 5     ; $r1 = 5
0010 000000000011    →  j 3                  ; salta para addr=3
0100 000 010 001100  →  addi $r2, $r0, 12    ; DESCARTADA (flush)
0100 000 011 001000  →  addi $r3, $r0, 8     ; destino do jump, $r3 = 8
```

**Resultados esperados:**
- `$r1 = 5`, `$r2 = 0` (não executou), `$r3 = 8`
- Stalls: 0 | Forwards: 0 | Flushes: 1

---

## 6. hazard_todos.mem — Teste Combinado

Combina EX forward, MEM forward, load-use stall e branch flush em sequência.

**Resultados esperados:**
- Forwards >= 2 (EX e MEM)
- Stalls >= 1 (load-use)
- Flushes >= 1 (branch)

---

## Como rodar

```
make executa
# Opção 1: Carregar .mem → informe o caminho do arquivo
# Opção 2: Carregar .dat → (só para load-use: testes/hazard_load_use.dat)
# Opção 3: Run (executa tudo)
# Opção 8: Ver registradores
# Opção 10: Ver estatísticas
```
