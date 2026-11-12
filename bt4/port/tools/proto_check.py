# Finds functions declared in more than one place with a parameter that is a pointer in one declaration and an
# integer in another (harmless on the PS2, where both are 32 bits; on a 64-bit build the caller passes 32 bits and
# the function reads 64). Also return types.
import re,sys,glob,collections
files=glob.glob('src/**/*.c',recursive=True)+glob.glob('include/**/*.h',recursive=True)+glob.glob('port/src/**/*.c',recursive=True)+glob.glob('port/src/**/*.h',recursive=True)
decl=collections.defaultdict(list)
rx=re.compile(r'(?:^|\n)\s*(?:extern\s+)?((?:const\s+|unsigned\s+|signed\s+|struct\s+|union\s+|enum\s+)*[A-Za-z_]\w*(?:\s*\*+)?)\s*\b([A-Za-z_]\w*)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*(;|\{)')
def kind(t):
    t=t.strip()
    if t in('void',''): return 'void'
    if '...' in t: return 'var'
    if '*' in t or '[' in t: return 'ptr'
    b=re.sub(r'\b(const|volatile|register)\b','',t).strip()
    b=re.sub(r'\s+\w+$','',b) if re.search(r'\s\w+$',b) and not re.fullmatch(r'(unsigned|signed|long|short|struct|union|enum)\s+\w+',b) else b
    b=b.strip()
    if b in('f32','float','double','f64'): return 'flt'
    if b in('s64','u64','long','unsigned long','long long','unsigned long long','size_t','uintptr_t','intptr_t','ssize_t'): return 'i64'
    if re.match(r'(struct|union)\b',b) or (b[:1].isupper() and not b.isupper()): return 'agg:'+b
    return 'i32'
for f in files:
    try: s=open(f,errors='replace').read()
    except OSError: continue
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s)
    for m in rx.finditer(s):
        ret,name,params=m.group(1),m.group(2),m.group(3)
        if name in('if','while','for','switch','return','sizeof') or ret.strip() in('return','else','case','goto','typedef'): continue
        ps=[p for p in re.split(r',(?![^()]*\))',params)] if params.strip() else []
        decl[name].append((f,kind(ret),tuple(kind(p) for p in ps)))
bad=[]
for n,ds in decl.items():
    if len(ds)<2: continue
    sigs=set((d[1],d[2]) for d in ds)
    if len(sigs)<2: continue
    # compare position by position among declarations with the same count
    byc=collections.defaultdict(list)
    for d in ds: byc[len(d[2])].append(d)
    for c,g in byc.items():
        for i in range(c):
            ks=set(d[2][i] for d in g)
            if 'ptr' in ks and ('i32' in ks):
                bad.append((n,'parameter %d'%(i+1),[(d[0],d[2][i]) for d in g]))
        rk=set(d[1] for d in g)
        if 'ptr' in rk and 'i32' in rk:
            bad.append((n,'return value',[(d[0],d[1]) for d in g]))
print(len(bad),'functions with a pointer / 32-bit integer disagreement')
for n,w,g in sorted(bad)[:400]:
    i=[f for f,k in g if k=='i32']; p=[f for f,k in g if k=='ptr']
    print('%s\t%s\tint in: %s\tpointer in: %s'%(n,w,', '.join(sorted(set(i)))[:150],', '.join(sorted(set(p)))[:110]))
