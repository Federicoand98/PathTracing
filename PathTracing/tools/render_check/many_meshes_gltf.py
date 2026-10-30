#!/usr/bin/env python3
"""Scena glTF sintetica con N mesh. Uso: many_meshes_gltf.py <N> <percorso-senza-estensione>"""
import json,struct,sys,base64
n=int(sys.argv[1]); out=sys.argv[2]
# cubo non indicizzato: 36 vertici con normali
P=[];N=[]
for ax in range(3):
  for s in (-1,1):
    u=[0,0,0];v=[0,0,0];w=[0,0,0];w[ax]=s;u[(ax+1)%3]=1;v[(ax+2)%3]=1
    if s<0:u,v=v,u
    c=lambda a,b:[w[i]+a*u[i]+b*v[i] for i in range(3)]
    for a,b in [(-1,-1),(1,-1),(1,1),(-1,-1),(1,1),(-1,1)]:
      P.append(c(a,b));N.append(w)
buf=b''.join(struct.pack('fff',*p) for p in P)+b''.join(struct.pack('fff',*p) for p in N)
open(out+'.bin','wb').write(buf)
side=round(n**(1/3)); nodes=[]
for x in range(side):
  for y in range(side):
    for z in range(side):
      s=4.0/side
      nodes.append({"mesh":0,"translation":[(x-side/2)*s,(y-side/2)*s,(z-side/2)*s],"scale":[s*.3]*3})
g={"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":list(range(len(nodes)))}],"nodes":nodes,
 "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"material":0}]}],
 "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[.8,.5,.3,1],"roughnessFactor":.6}}],
 "buffers":[{"uri":out.split('/')[-1]+'.bin',"byteLength":len(buf)}],
 "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":432},{"buffer":0,"byteOffset":432,"byteLength":432}],
 "accessors":[{"bufferView":0,"componentType":5126,"count":36,"type":"VEC3","min":[-1,-1,-1],"max":[1,1,1]},{"bufferView":1,"componentType":5126,"count":36,"type":"VEC3"}]}
json.dump(g,open(out+'.gltf','w')); print(len(nodes),'mesh')
