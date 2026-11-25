import sys,wave,numpy as np
w=wave.open(sys.argv[1]);a=np.abs(np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).reshape(-1,2)).max(1)/32768
h=240;e=[a[i:i+h].max()>10**(-70/20) for i in range(0,len(a)-h,h)]
on=None;last=0
for k,v in enumerate(e+[False]*20):
    if v:
        if on is None:on=k
        last=k
    elif on is not None and k-last>10:
        print(f"{on*.005:.3f} -> {last*.005:.3f}");on=None
