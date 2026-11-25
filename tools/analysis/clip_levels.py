import sys,wave,numpy as np
w=wave.open(sys.argv[1]);a=np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).reshape(-1,2)/32768
h=4800
print(" t    L dB   R dB  corr  zero%")
for i in range(0,len(a)-h,h):
    b=a[i:i+h];l,r=b[:,0],b[:,1];d=lambda x:20*np.log10(np.sqrt((x**2).mean())+1e-9)
    c=np.corrcoef(l,r)[0,1] if l.std()>1e-6 and r.std()>1e-6 else float('nan')
    print(f"{i/48000:4.1f} {d(l):6.1f} {d(r):6.1f} {c:5.2f} {100*(np.abs(b).max(1)<2/32768).mean():4.0f}",'#'*int(max(0,(d(l)+90)/2)))
