import re,sys,collections
L=re.compile(r'^ *([0-9.]+) t(\w+) (.*)$')
for fn in sys.argv[1:]:
    print("=====",fn.split('/')[-1])
    live={};cb2v={};done=[];stops=[]
    for line in open(fn,errors='replace'):
        m=L.match(line)
        if not m:continue
        t=float(m[1]);s=m[3]
        m2=re.match(r'voice (\w+) Create fmt (\w+) ch (\d) rate (\d+) bits (\d+) flags \d+ cb (\w+)',s)
        if m2:
            v=dict(v=m2[1],fmt=m2[2],ch=m2[3],rate=m2[4],cb=m2[6],create=t,start=[],stop=[],bs=[],be=[],se=[],sub=[],na=0,namax=0,nap=set(),destroy=None)
            live[m2[1]]=v;cb2v[m2[6]]=v;continue
        m2=re.match(r'voice (\w+) (Submit|Start|Stop|Flush|Destroy|NOT ADVANCING)(.*)',s)
        if m2 and m2[1] in live:
            v=live[m2[1]];k=m2[2]
            if k=='Submit':v['sub'].append((t,int(re.search(r'bytes (\d+)',s)[1])))
            elif k=='Start':v['start'].append(t)
            elif k=='Stop':v['stop'].append(t);stops.append(t)
            elif k=='NOT ADVANCING':
                v['na']+=1;a=re.search(r'ADVANCING (\d+) ms, (\d+) buffers queued, played (\d+)',s);v['namax']=max(v['namax'],int(a[1]));v['nap'].add(int(a[3]))
            elif k=='Destroy':v['destroy']=t;done.append(v);live.pop(m2[1])
            continue
        m2=re.match(r'cb (\w+) (BufferStart|BufferEnd|StreamEnd)',s)
        if m2 and m2[1] in cb2v:cb2v[m2[1]][{'BufferStart':'bs','BufferEnd':'be','StreamEnd':'se'}[m2[2]]].append(t)
    A=done+list(live.values())
    G=collections.defaultdict(list)
    for v in A:G[(v['fmt'],v['ch'],v['rate'])].append(v)
    print("fmt ch rate      n started  1+starts multi  noBufferStart  bs-delay med/max ms  naVoices")
    for k,g in sorted(G.items()):
        st=[v for v in g if v['start']];no=[v for v in st if not v['bs']]
        d=sorted(v['bs'][0]-v['start'][0] for v in st if v['bs'])
        print(k,len(g),len(st),sum(len(v['start'])>1 for v in st),len(no),f"{d[len(d)//2]:.0f}/{d[-1]:.0f}" if d else '-',sum(v['na']>0 for v in g))
    print("-- long-lag or stuck voices (start -> first BufferStart > 300 ms, or none)")
    n=0
    for v in sorted(A,key=lambda v:v['create']):
        if not v['start']:continue
        lag=v['bs'][0]-v['start'][0] if v['bs'] else None
        if lag is None or lag>300 or v['fmt']=='0161':
            n+=1
            if n>70:break
            print(f"{v['create']:9.0f} {v['fmt']} {v['rate']} sub {[b for _,b in v['sub']][:3]} start {[round(x) for x in v['start']][:4]} stop {[round(x) for x in v['stop']][:4]} bufStart {[round(x) for x in v['bs']][:2]} streamEnd {[round(x) for x in v['se']][:1]} destroy {v['destroy'] and round(v['destroy'])} NA {v['na']}/{v['namax']}ms played {sorted(v['nap'])[:2]}")
    # bursts of Start calls = pause resumes
    allst=sorted((x,v['fmt']) for v in A for x in v['start'])
    b=collections.Counter(int(x//50) for x,_ in allst)
    print("-- 50ms windows with >=4 Starts:",[(k*50,c,dict(collections.Counter(f for x,f in allst if int(x//50)==k))) for k,c in sorted(b.items()) if c>=4][:25])
    c=collections.Counter(int(x//50) for x in stops)
    print("-- 50ms windows with >=4 Stops:",[(k*50,n) for k,n in sorted(c.items()) if n>=4][:25])
