#!/usr/bin/env python3
"""Bounded integration check on an owned VICE instance, port 6521."""
import argparse,json,os,socket,subprocess,sys,threading,time
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent))
from reload_game import reload_game
from vice_monitor import ViceMonitor,parse_monitor_bytes

def decode(data):
    def word(i):return data[i]|data[i+1]<<8
    return {'signature':bytes(data[:2]).decode(errors='replace'),'state':data[3],
            'ticks':word(4),'updates':word(6),'missed':word(8),'worst_cycles':word(10),
            'renders':word(12),'player':[word(14),word(16)],'camera':[word(18),word(20)],
            'health':data[22],'signals':data[23],'demo':data[24],'last_cycles':word(28),
            'route':data[30],'grounded':data[31],'display_camera':[word(32),word(34)]}

def capture(m,path):
    with m:
        raw=m.command_sequence(['bank cpu','m 0340 0363','m d011 d018'],resume=False)
        data=parse_monitor_bytes(raw['m 0340 0363'],0x340,36)
        state=decode(data)
        if state['state']==1:
            vic=parse_monitor_bytes(raw['m d011 d018'],0xd011,8)
            d018=vic[7]
            x,y=state['display_camera']
            state['fine_mismatches']=int((vic[0]&7)!=7-(y&7))+int((vic[5]&7)!=7-(x&7))
            screen=0x4000+((d018>>4)*1024)
            commands=['bank ram',f'm {screen:04x} {screen+999:04x}','m 8000 9fff','bank cpu']
            reads=m.command_sequence(commands,resume=False)
            actual=parse_monitor_bytes(reads[commands[1]],screen,1000)
            world=parse_monitor_bytes(reads['m 8000 9fff'],0x8000,8192)
            cx,cy=state['display_camera'];cx>>=3;cy>>=3
            expected=[world[(cy+y)*128+cx+x] for y in range(25) for x in range(40)]
            state['screen_mismatches']=sum(a!=b for a,b in zip(actual,expected))
        m.command(f'screenshot "{path.resolve()}" 2',resume=False)
    return state

def main():
    p=argparse.ArgumentParser();p.add_argument('--seconds',type=int,default=22);p.add_argument('--tag',default='first');p.add_argument('--diagnostic',action='store_true');p.add_argument('--trace',action='store_true');p.add_argument('--require-win',action='store_true');a=p.parse_args()
    out=ROOT/'evidence'/a.tag;out.mkdir(parents=True,exist_ok=True)
    with socket.socket() as s:s.bind(('127.0.0.1',6521))
    env={k:v for k,v in os.environ.items() if k not in ('LD_LIBRARY_PATH','GTK_PATH','GTK_EXE_PREFIX','GIO_MODULE_DIR','GTK_IM_MODULE_FILE','GSETTINGS_SCHEMA_DIR','LOCPATH','GDK_PIXBUF_MODULE_FILE','GDK_PIXBUF_MODULEDIR')}
    logfile=out/'vice.log';stop=threading.Event();result={'samples':[],'failure':None}
    with logfile.open('w') as log:
        proc=subprocess.Popen(['x64','-default','-pal','+sound','+warp','+autostart-warp','-speed','100','-remotemonitor','-remotemonitoraddress','ip4://127.0.0.1:6521'],env=env,stdout=log,stderr=log)
        def guard():
            start=time.monotonic()
            while not stop.wait(.1):
                if logfile.stat().st_size>100000 or time.monotonic()-start>a.seconds+40:
                    result['failure']='watchdog';proc.terminate();return
        thread=threading.Thread(target=guard,daemon=True);thread.start()
        try:
            time.sleep(3)
            if not reload_game(ROOT/'deep_signal.prg',port=6521):raise RuntimeError('reload failed')
            m=ViceMonitor(port=6521,timeout=3)
            time.sleep(.5)
            state=capture(m,out/'title.png');result['title']=state;print('TITLE',state,flush=True)
            m.command('> 033c 20')
            if a.trace:m.command('> 033d 01')
            if a.diagnostic:
                time.sleep(.3);m.command('> 033c 20');m.command('> 033f 01')
            started=time.monotonic();next_at=.25 if a.diagnostic else 2
            while time.monotonic()-started<a.seconds:
                time.sleep(.1)
                elapsed=time.monotonic()-started
                if elapsed>=next_at:
                    state=capture(m,out/(f'scan-{int(next_at*1000):05d}.png' if a.diagnostic else f'play-{next_at:02d}.png'));state['wall_seconds']=round(elapsed,2)
                    result['samples'].append(state);print('PLAY',state,flush=True);next_at+=.5 if a.diagnostic else 4
            if a.trace:
                raw=m.command('m 6800 6fff')
                (out/'trace.json').write_text(json.dumps(parse_monitor_bytes(raw,0x6800,2048)))
            result['settings']=m.command_sequence(['resourceget "Speed"','resourceget "AutostartWarp"','warp'])
        except Exception as e:result['failure']=str(e);print('FAILED',repr(e),flush=True)
        finally:
            stop.set();proc.terminate()
            try:proc.wait(timeout=3)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()
            thread.join(timeout=1)
    result['network_errors']=logfile.read_text().count('vice_network_send')
    if any(s.get('screen_mismatches',0) for s in result['samples']):result['failure']='screen/map mismatch'
    if any(s['missed'] for s in result['samples']):result['failure']='missed gameplay frames'
    if any(s.get('fine_mismatches',0) for s in result['samples']):result['failure']='fine scroll mismatch'
    if a.diagnostic:
        result['directions']=sorted({(s['updates']>>6)&7 for s in result['samples'] if s['state']==1})
        if len(result['directions'])!=8:result['failure']='incomplete direction coverage'
    if a.require_win and not any(s['state']==3 and s['signals']==7 for s in result['samples']):result['failure']='demo did not complete mission'
    if result['network_errors']:result['failure']='VICE network errors'
    print('RESULT',result['failure'] or 'PASS',flush=True)
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    return int(result['failure'] is not None)
if __name__=='__main__':raise SystemExit(main())
