#!/usr/bin/env python3
"""Linux SwitchAsPad receiver. USB is preferred when both transports are active."""
import argparse
import socket
import struct
import threading
import queue
import time
from evdev import UInput, AbsInfo, ecodes as e
REPORT = struct.Struct('<4sIQhhhh')
BUTTONS = {0:e.BTN_EAST, 1:e.BTN_SOUTH, 2:e.BTN_NORTH, 3:e.BTN_WEST,
           4:e.BTN_THUMBL, 5:e.BTN_THUMBR, 6:e.BTN_TL, 7:e.BTN_TR,
           10:e.BTN_START, 11:e.BTN_SELECT}
AXES = [e.ABS_X,e.ABS_Y,e.ABS_RX,e.ABS_RY,e.ABS_Z,e.ABS_RZ,e.ABS_HAT0X,e.ABS_HAT0Y]
def decode(data):
    if len(data)!=REPORT.size: return None
    magic,seq,b,lx,ly,rx,ry=REPORT.unpack(data)
    if magic!=b'SPAD': return None
    down=lambda bit:int(bool(b & (1<<bit)))
    return ({code:down(bit) for bit,code in BUTTONS.items()},
            [lx,max(-32768,min(32767,-ly)),rx,max(-32768,min(32767,-ry)),
             down(8)*255,down(9)*255,down(14)-down(12),down(15)-down(13)])
def usb_worker(events, stop):
    import usb.core
    import usb.util
    while not stop.is_set():
        dev=None
        try:
            dev=usb.core.find(idVendor=0x057e,idProduct=0x3001)
            if dev is None: stop.wait(1); continue
            dev.set_configuration()
            interface=dev.get_active_configuration()[(0,0)]
            ep=next(ep for ep in interface if usb.util.endpoint_direction(ep.bEndpointAddress)==usb.util.ENDPOINT_IN)
            while not stop.is_set():
                try:
                    data=bytes(ep.read(64,timeout=100))
                    if decode(data): events.put(('USB',data))
                except usb.core.USBTimeoutError: pass
        except Exception as exc:
            print('USB:',exc,flush=True); stop.wait(1)
        finally:
            if dev is not None: usb.util.dispose_resources(dev)
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--transport',choices=['auto','wifi','usb'],default='auto')
    args=parser.parse_args()
    caps={e.EV_KEY:list(BUTTONS.values()),e.EV_ABS:[]}
    for axis in AXES:
        low,high=(-1,1) if axis in (e.ABS_HAT0X,e.ABS_HAT0Y) else ((0,255) if axis in (e.ABS_Z,e.ABS_RZ) else (-32768,32767))
        caps[e.EV_ABS].append((axis,AbsInfo(0,low,high,0,2048 if high==32767 else 0,0)))
    events=queue.Queue(); stop=threading.Event()
    sock=None
    if args.transport!='usb':
        sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); sock.bind(('0.0.0.0',26780)); sock.setblocking(False)
    if args.transport!='wifi': threading.Thread(target=usb_worker,args=(events,stop),daemon=True).start()
    last=0; last_usb=0; active=False; source=None; peer=None
    def apply(ui,state):
        keys,axes=state
        for key,value in keys.items(): ui.write(e.EV_KEY,key,value)
        for axis,value in zip(AXES,axes): ui.write(e.EV_ABS,axis,value)
        ui.syn()
    try:
        with UInput(caps,name='SwitchAsPad',vendor=0x1209,product=0x0001,version=1) as ui:
            print('SwitchAsPad ready: UDP 26780 / USB. Ctrl+C to stop.',flush=True)
            while True:
                now=time.monotonic(); batch=[]
                if sock:
                    while True:
                        try:
                            data,addr=sock.recvfrom(256)
                            if decode(data) and (peer is None or peer==addr[0] or now-last>1):
                                peer=addr[0]; batch.append(('Wi-Fi',data))
                                try: sock.sendto(b'SPAK',addr)
                                except OSError: pass
                        except BlockingIOError: break
                while True:
                    try: batch.append(events.get_nowait())
                    except queue.Empty: break
                for transport,data in batch:
                    if transport=='USB': last_usb=now
                    elif now-last_usb<0.3: continue
                    state=decode(data)
                    if state:
                        apply(ui,state); last=now; active=True
                        if source!=transport: print('Connected:',transport,flush=True); source=transport
                if active and now-last>0.3:
                    apply(ui,({key:0 for key in BUTTONS.values()},[0]*8)); active=False; source=None
                    print('Disconnected: controls released.',flush=True)
                time.sleep(0.004)
    except KeyboardInterrupt: pass
    finally:
        stop.set()
        if sock: sock.close()
if __name__=='__main__': main()
