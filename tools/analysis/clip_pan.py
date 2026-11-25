import sys,wave,numpy as np
w=wave.open(sys.argv[1]);a=np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).reshape(-1,2)/32768
h=2400
# optional ranges in seconds after the file: 0-0.3 5.8-6.9
for t0,t1 in [tuple(map(float,x.split('-'))) for x in sys.argv[2:]] or [(0,len(a)/48000)]:
    print(f"-- {t0}-{t1}")
    for i in range(int(t0*48000),int(t1*48000)-h,h):
        b=a[i:i+h];l,r=b.T;rl,rr=(l**2).mean()**.5,(r**2).mean()**.5
        if max(rl,rr)<1e-3:continue
        # residual after removing the best single L=k*R pan: high means several pan positions at once
        k=(l@r)/(r@r);res=((l-k*r)**2).mean()**.5
        print(f"{i/48000:.2f} L-R {20*np.log10(rl/rr):+5.1f} dB  second-source residual {20*np.log10(res/rl+1e-9):6.1f} dB  peak {np.abs(b).max():.2f}")
