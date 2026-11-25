import re,sys,collections
L=re.compile(r'^ *([0-9.]+) t(\w+) (.*)$')
for fn in sys.argv[1:]:
    print("=====",fn.split('/')[-1]);live={};cb={};A=[]
    for line in open(fn,errors='replace'):
        m=L.match(line)
        if not m:continue
        t=float(m[1]);s=m[3]
        q=re.match(r'voice (\w+) Create fmt (\w+) ch (\d) rate (\d+) .* cb (\w+)',s)
        if q:
            v=dict(id=q[1],fmt=q[2],ch=q[3],rate=q[4],t=t,ev=[]);live[q[1]]=v;cb[q[5]]=v;A.append(v);continue
        q=re.match(r'voice (\w+) (Submit bytes (\d+) flags (\d+)|Start|Stop|Flush|Destroy|NOT ADVANCING (\d+) ms, (\d+) buffers queued, played (\d+))',s)
        if q and q[1] in live:
            v=live[q[1]];k=q[2].split()[0]
            v['ev'].append((t,{'Submit':f"S{q[3]}f{q[4]}",'NOT':f"NA{q[5]}ms q{q[6]} p{q[7]}"}.get(k,k)))
            if k=='Destroy':live.pop(q[1])
            continue
        q=re.match(r'cb (\w+) (\w+)',s)
        if q and q[1] in cb:cb[q[1]]['ev'].append((t,{'BufferStart':'bs','BufferEnd':'be','StreamEnd':'se'}.get(q[2],q[2])))
    # pauses: >=5 Stop in 30ms followed by >=5 Start in 30 ms where voices are not destroyed
    res=[];
    allr=collections.defaultdict(list)
    for v in A:
        st=[t for t,e in v['ev'] if e=='Stop'];de=[t for t,e in v['ev'] if e=='Destroy']
        for x in st:
            if not de or de[0]-x>50:allr['stop'].append(x)
        ss=[t for t,e in v['ev'] if e=='Start']
        for a,b in zip(st,[y for y in ss]):pass
        for y in ss:
            if any(x<y for x in st):allr['restart'].append(y)
    grp=lambda l:[(k,len(list(g))) for k,g in __import__('itertools').groupby(sorted(round(x/60) for x in l))]
    print("PAUSE  (stops not followed by destroy):",[(k*60,n) for k,n in grp(allr['stop']) if n>=3])
    print("RESUME (restart after stop):           ",[(k*60,n) for k,n in grp(allr['restart']) if n>=3])
    print("-- streamed voices (first Submit flags 0)")
    for v in A:
        sub=[e for _,e in v['ev'] if e.startswith('S') and 'f' in e]
        if sub and sub[0].endswith('f0'):
            print(f"{v['t']:8.0f} {v['id']} fmt{v['fmt']} ch{v['ch']} {v['rate']}: "+' '.join(f"{e}@{t:.0f}" for t,e in v['ev'])[:420])
    print("-- non-streamed voices whose first Start came >150 ms after creation, or never")
    n=0
    for v in A:
        sub=[e for _,e in v['ev'] if e.startswith('S') and 'f' in e]
        if sub and sub[0].endswith('f0'):continue
        s0=[t for t,e in v['ev'] if e=='Start']
        if not s0 or s0[0]-v['t']>150:
            n+=1
            if n<=45:print(f"{v['t']:8.0f} {v['id']} fmt{v['fmt']} ch{v['ch']} {v['rate']}: "+' '.join(f"{e}@{t:.0f}" for t,e in v['ev'])[:230])
    print("total",n)
