import glob, os, sys

def decode(b):
    b = b.strip()
    if not b or b.startswith('#'):
        return None
    if len(b) != 16 or not all(c in '01' for c in b):
        return f'  INVALIDO: {b!r}'
    op = int(b[0:4], 2)
    def imm6(s): raw=int(s,2); return raw if not (raw&32) else raw-64
    if op == 0:
        rs=int(b[4:7],2); rt=int(b[7:10],2); rd=int(b[10:13],2); fn=int(b[13:16],2)
        fn_names={0:'add',1:'sub',2:'and',3:'or'}
        return f'  {b}  ->  {fn_names.get(fn,"??")} $r{rd}, $r{rs}, $r{rt}'
    elif op == 2:
        addr=int(b[4:],2)
        return f'  {b}  ->  j {addr}'
    elif op == 4:
        rs=int(b[4:7],2); rt=int(b[7:10],2); imm=imm6(b[10:16])
        return f'  {b}  ->  addi $r{rt}, $r{rs}, {imm}'
    elif op == 8:
        rs=int(b[4:7],2); rt=int(b[7:10],2); imm=imm6(b[10:16])
        return f'  {b}  ->  beq $r{rs}, $r{rt}, {imm}'
    elif op == 11:
        rs=int(b[4:7],2); rt=int(b[7:10],2); imm=imm6(b[10:16])
        return f'  {b}  ->  lw $r{rt}, {imm}($r{rs})'
    elif op == 15:
        rs=int(b[4:7],2); rt=int(b[7:10],2); imm=imm6(b[10:16])
        return f'  {b}  ->  sw $r{rt}, {imm}($r{rs})'
    else:
        return f'  {b}  ->  OPCODE={op} DESCONHECIDO'

base = os.path.dirname(os.path.abspath(__file__))
for f in sorted(glob.glob(os.path.join(base, '*.mem'))):
    print(f'\n=== {os.path.basename(f)} ===')
    for i, line in enumerate(open(f)):
        line = line.strip()
        if line and not line.startswith('#'):
            r = decode(line)
            if r:
                print(f'[{i+1:02d}] {r}')
