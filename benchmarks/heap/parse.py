import re,sys
name=None; objs=0
for l in open(sys.argv[1] if len(sys.argv)>1 else 'out.txt'):
    l=l.rstrip('\n')
    if l.startswith('BEGIN '):
        name=l[6:]; objs=0
    elif l.startswith('END '):
        m=re.search(r'n=(\d+) pages=(\d+),(\d+)',l); n=int(m.group(1)); pa=int(m.group(2)); pb=int(m.group(3))
        print(f"{name:45s} n={n:5d}  objects/op={objs/(3*n):8.2f}   bytes/op={pb*65536/n:10.0f} (windows {pa},{pb} pages)")
        name=None
    elif l.startswith('[sgcl] mem') and name:
        m=re.search(r'objects created:\s*(\d+)',l)
        if m: objs+=int(m.group(1))
