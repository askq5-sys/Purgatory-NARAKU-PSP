import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from source_util import original_dir
"""Verify stable prefiltered PSP texel grids without changing map/event logic."""
from pathlib import Path
import json,sys,struct,zipfile
from PIL import Image
import numpy as np
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'));import prepare_world_v060 as w
G=original_dir(r);O=r/'assets';key=bytes.fromhex(json.load(open(G/'data/System.json'))['encryptionKey']);ts=json.load(open(G/'data/Tilesets.json'))
count=0
for mid in(33,34):
 m=json.load(open(G/'data'/f'Map{mid:03}.json'));sets=ts[m['tilesetId']];bitmaps={}
 for i,name in enumerate(sets['tilesetNames']):
  if name:bitmaps[i]=w.load_encrypted_png(w.resolve_named_file(G/'img/tilesets',name),key)
 atlas=Image.frombytes('RGBA',(512,512),(O/f'map{mid:03}_atlas0.rgba8888').read_bytes());ids=sorted(set(m['data'][:m['width']*m['height']*4])-{0})
 for i,tid in enumerate(ids):
  if tid in (355,356,357,363,364,365):continue
  x=i%10*50+1;y=i//10*50+1;actual=atlas.crop((x,y,x+48,y+48));a=np.array(actual)
  assert np.array_equal(a[0::2,0::2],a[1::2,0::2])and np.array_equal(a[0::2,0::2],a[0::2,1::2])and np.array_equal(a[0::2,0::2],a[1::2,1::2])
  tile=w.render_tile(tid,bitmaps).resize((24,24),Image.Resampling.LANCZOS).resize((48,48),Image.Resampling.NEAREST);normalized=Image.new('RGBA',(48,48));normalized.alpha_composite(tile);assert actual.tobytes()==normalized.tobytes();count+=1

print(f'PASS: {count} background tiles retain fixed PSP sampling grids; corpse detail is checked in test_factory_graphics.py')
