import re, collections
w={}
for l in open('pg-addr.txt'):
    a,n=l.split(); w[int(a,16)]=int(n)
chains={}; cur=None; lines=open('pg-lines.txt').read().split('\n'); i=0
while i < len(lines):
    l=lines[i]
    if l.startswith('0x'): cur=int(l,16); chains[cur]=[]; i+=1; continue
    if cur is not None and i+1 < len(lines): chains[cur].append((l, lines[i+1])); i+=2; continue
    i+=1
tot=sum(w.values())
def short(f):
    f=re.sub(r'\(.*$','',f); f=re.sub(r'^(void|bool|unsigned int|int|dsp56k::TWord|auto|long long) ','',f); return f.replace('dsp56k::','')[:80]
fn=collections.Counter(); ln=collections.Counter(); top=collections.Counter()
for a,ch in chains.items():
    n=w.get(a,0)
    if not ch: continue
    fn[short(ch[0][0])]+=n
    ln[ch[0][1].split('/')[-1].split(' ')[0]]+=n
    top[short(ch[-1][0])]+=n
print(f'total samples {tot}')
print('--- outermost (real) function'); [print(f'{100*n/tot:5.1f}%  {k}') for k,n in top.most_common(8)]
print('--- innermost inlined function'); [print(f'{100*n/tot:5.1f}%  {k}') for k,n in fn.most_common(28)]
print('--- source lines'); [print(f'{100*n/tot:5.1f}%  {k}') for k,n in ln.most_common(22)]
