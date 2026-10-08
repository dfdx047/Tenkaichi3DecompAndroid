#!/usr/bin/env python3
"""Cross-file ABI check of the Android build (BT3_KEEP=1 keeps the .ll files): a function declared in one file with
other argument types than its definition. On AArch64 a 16-byte aligned struct passed by value takes an even register
pair, a 4-byte aligned one does not, so such a mismatch moves every later argument (the beam-clash crash). Prints
SER lines for the ones that matter."""
import re,glob,sys
D=re.compile(r'^define [^@]*@("?[\w.$]+"?)\((.*)\)\s*(?:local_unnamed_addr\s*)?(?:#\d+\s*)?(?:!dbg.*)?(?:\{|$)')
C=re.compile(r'^declare [^@]*@("?[\w.$]+"?)\((.*)\)')
def norm(args):
    # keep only types: drop attributes and names
    out=[];depth=0;cur=''
    for ch in args:
        if ch in '([{<': depth+=1
        if ch in ')]}>': depth-=1
        if ch==',' and depth==0: out.append(cur.strip()); cur=''
        else: cur+=ch
    if cur.strip(): out.append(cur.strip())
    res=[]
    for a in out:
        a=re.sub(r'%[\w.$"-]+$','',a).strip()
        a=re.sub(r'\b(noundef|nonnull|readonly|writeonly|noalias|nocapture|signext|zeroext|inreg|returned|nofree|immarg|captures\(\w+\)|align \d+|dereferenceable(_or_null)?\(\d+\)|alignstack\(\d+\))\b','',a)
        a=re.sub(r'\s+',' ',a).strip()
        res.append(a)
    return res
defs={};decls={}
for f in glob.glob('port/build/android/obj/*.ll'):
    for line in open(f,encoding='latin-1'):
        m=D.match(line)
        if m: defs[m.group(1)]=(norm(m.group(2)),f); continue
        m=C.match(line)
        if m: decls.setdefault(m.group(1),[]).append((norm(m.group(2)),f))
n=0
for name,lst in decls.items():
    if name not in defs: continue
    d,df=defs[name]
    for a,f in lst:
        if '...' in a or '...' in d: continue
        if a!=d:
            n+=1
            print(name, df.split('/')[-1], d, '| decl in', f.split('/')[-1], a)
print(n,'mismatches',file=sys.stderr)

def cls(t):
    t=t.replace('captures(none)','').strip()
    if t.startswith('ptr'): return 'ptr' if 'addrspace' not in t else t
    m=re.fullmatch(r'i(\d+)',t)
    if m: return 'w' if int(m.group(1))<=32 else 'x' if int(m.group(1))==64 else t
    if t in('float',): return 'w'
    if t=='double': return 'x'
    return t
bad=[]
for name,lst in decls.items():
    if name not in defs: continue
    d,df=defs[name]
    if '...' in d: continue
    dc=[cls(t) for t in d]
    for a,f in lst:
        if '...' in a: continue
        ac=[cls(t) for t in a]
        if ac==dc: continue
        # caller narrower/wider ints into callee 'w' is fine; callee 'x' from caller 'w' is not
        ok = len(ac)==len(dc) and all(x==y or (y=='w' and x in('w','x')) or (x=='ptr' and y=='ptr') for x,y in zip(ac,dc))
        if not ok: bad.append((name,df.split('/')[-1],d,f.split('/')[-1],a))
for b in bad: print('BAD',*b)
print(len(bad),'dangerous',file=sys.stderr)

def k(t):
    t=re.sub(r'captures\(\w+\)|initializes\([^)]*\)\)?','',t).strip()
    if t.startswith('ptr') or re.fullmatch(r'i\d+',t) and int(t[1:])<=64 or t in('float','double'): return 'r'
    return t
print('=== serious ===')
seen=set()
for name,lst in decls.items():
    if name not in defs: continue
    d,df=defs[name]
    if '...' in d: continue
    dk=[k(t) for t in d]
    for a,f in lst:
        if '...' in a: continue
        ak=[k(t) for t in a]
        short=len(ak)<len(dk)
        agg=any(x!=y for x,y in zip(ak,dk))
        if short or agg:
            key=(name,tuple(ak))
            if key in seen: continue
            seen.add(key)
            print('SER',name,df.split('/')[-1],d,'<-',f.split('/')[-1],a)
