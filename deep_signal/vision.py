#!/usr/bin/env python3
"""Local VLM review, recorded with its prompt. No gameplay/FPS claims."""
import argparse,base64,json,urllib.request
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('images',nargs='+',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
prompt='Review these screenshots of an original C64 platform shooter called DEEP SIGNAL. The player is an orange astronaut, drones are red/purple, world platforms cyan/blue, HUD plates at the two top corners. Describe each image, legibility, sprite/terrain alignment, obvious corruption and composition. If any visual detail is uncertain say so. Do not infer frame rate or movement from still images. Be concise.'
payload={'model':'gemma4:e4b-it-qat','stream':False,'think':False,'messages':[{'role':'user','content':prompt,'images':[base64.b64encode(p.read_bytes()).decode() for p in a.images]}],'options':{'temperature':0,'num_predict':800},'keep_alive':0}
req=urllib.request.Request('http://127.0.0.1:11434/api/chat',data=json.dumps(payload).encode(),headers={'Content-Type':'application/json'})
with urllib.request.urlopen(req,timeout=180) as response:r=json.load(response)
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps({'images':[str(p) for p in a.images],'prompt':prompt,'response':r},indent=2)+'\n')
print(r.get('message',{}).get('content','<no response>'),flush=True)
