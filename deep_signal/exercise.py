#!/usr/bin/env python3
"""Owned, bounded VICE smoke: diagnostic controls, injected fall, restart, GIF demo.
Requires ffmpeg in addition to the ordinary build/check tools.
"""
import json,os,socket,subprocess,tempfile,threading,time
from pathlib import Path
from check import ROOT,ViceMonitor,parse_monitor_bytes,reload_game,capture

def main():
    out=ROOT/'evidence'/'controls';out.mkdir(parents=True,exist_ok=True)
    labels={line.split()[2]:int(line.split()[1],16) for line in (ROOT/'deep_signal.lbl').read_text().splitlines() if line.startswith('al ')}
    env={k:v for k,v in os.environ.items() if k not in ('LD_LIBRARY_PATH','GTK_PATH','GTK_EXE_PREFIX','GIO_MODULE_DIR','GTK_IM_MODULE_FILE','GSETTINGS_SCHEMA_DIR','LOCPATH','GDK_PIXBUF_MODULE_FILE','GDK_PIXBUF_MODULEDIR')}
    with socket.socket() as s:s.bind(('127.0.0.1',6521))
    result={};stop=threading.Event();logfile=out/'vice.log'
    with logfile.open('w') as log:
        proc=subprocess.Popen(['x64','-default','-pal','+sound','+warp','+autostart-warp','-speed','100','-remotemonitor','-remotemonitoraddress','ip4://127.0.0.1:6521'],env=env,stdout=log,stderr=log)
        def guard():
            start=time.monotonic()
            while not stop.wait(.1):
                if time.monotonic()-start>90 or logfile.stat().st_size>100000:proc.terminate();return
        thread=threading.Thread(target=guard,daemon=True);thread.start()
        try:
            time.sleep(3)
            assert reload_game(ROOT/'deep_signal.prg',port=6521)
            m=ViceMonitor(port=6521,timeout=3)
            def key(name,seconds=.12):
                value={'space':16,'d':8,'a':4,'w':1,'r':64}[name]
                m.command(f'> 033e {value:02x}')
                try:time.sleep(seconds)
                finally:m.command('> 033e 00')
            def snap(name):
                time.sleep(.06);s=capture(m,out/(name+'.png'));result[name]=s;print(name,s,flush=True);return s
            key('space');time.sleep(.6);a=snap('start');assert a['state']==1 and a['demo']==0
            key('d',.5);b=snap('right');assert b['player'][0]>a['player'][0]+8
            key('a',.3);c=snap('left');assert c['player'][0]<b['player'][0]-4
            m.command('> 033e 08');time.sleep(.4);m.command('> 033e 00')
            held=snap('held-right');assert held['player'][0]>c['player'][0]+20
            key('w',.05);d=snap('jump');assert d['player'][1]<c['player'][1]-10
            key('space',.05)
            addr=labels['._shot_life'];life=parse_monitor_bytes(m.command(f'm {addr:04x} {addr:04x}'),addr,1)[0]
            result['shot_life']=life;assert life>0
            # Controlled injection beyond bottom boundary tests real hurt/death logic.
            def put(name,value,size=1):
                addr=labels['._'+name];return f'> {addr:04x} '+ ' '.join(f'{(value>>(8*i))&255:02x}' for i in range(size))
            m.command_sequence([put('health',1),put('invulnerable',0),put('py',490,2),put('pyq',1960,2),put('vy',0)])
            time.sleep(.2);dead=snap('death');assert dead['state']==2 and dead['health']==0
            key('space');time.sleep(.3);again=snap('retry');assert again['state']==1 and again['health']==5 and again['signals']==0
            key('r');time.sleep(.3);restart=snap('restart');assert restart['state']==1 and restart['player'][0]==64
            result['input_method']='diagnostic hold byte; GUI keyboard automation inconclusive, not certified'
            assert reload_game(ROOT/'deep_signal.prg',port=6521)
            time.sleep(.3)
            with tempfile.TemporaryDirectory(prefix='deep-signal-frames-') as tmp:
                frames=Path(tmp);started=time.monotonic()
                for i in range(104):
                    if i==8:m.command('> 033c 20')
                    m.command(f'screenshot "{frames/f"{i:04d}.png"}" 2')
                    time.sleep(max(0,started+(i+1)/8-time.monotonic()))
                result['recording_final']=snap('recording-final')
                assert result['recording_final']['state']==3
                subprocess.run(['ffmpeg','-v','error','-y','-framerate','8','-i',str(frames/'%04d.png'),'-filter_complex','[0:v]split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=none','-loop','0',str(ROOT/'deep_signal.gif')],check=True,timeout=30)
            result['gif']='104 screenshots at 8 Hz wall-clock sampling; monitor pauses affect sampling, not a frame-rate benchmark'
            result['pass']=True
        except Exception as e:result['failure']=repr(e);raise
        finally:
            stop.set();proc.terminate()
            try:proc.wait(timeout=3)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()
            thread.join(timeout=1)
            result['network_errors']=logfile.read_text().count('vice_network_send')
            (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    assert result['network_errors']==0
if __name__=='__main__':main()
