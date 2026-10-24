"""Live integration test. Run alone on an otherwise idle vcan interface."""
import json,pathlib,queue,socket,subprocess,sys,threading,time
root=pathlib.Path(__file__).resolve().parents[1];iface=sys.argv[1] if len(sys.argv)>1 else 'vcan0'
if not iface.startswith('vcan'):sys.exit('Use a dedicated vcan interface.')
try:
 with socket.socket(socket.PF_CAN,socket.SOCK_RAW,socket.CAN_RAW) as s:s.bind((iface,))
except OSError as e:sys.exit(f'Live test unavailable: {e}. Set up vcan first; no tests were run.')
procs=[];q=queue.Queue()
def start(role):
 p=subprocess.Popen([str(root/'build/ecu'),role,iface],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=None,text=True,bufsize=1);procs.append(p);return p
checks=0
def wait_for(predicate,label,timeout=4):
 global checks
 limit=time.monotonic()+timeout
 while time.monotonic()<limit:
  try:d=q.get(timeout=.2)
  except queue.Empty:continue
  if predicate(d):checks+=1;print('PASS:',label);return d
 raise AssertionError(label)
def send(p,cmd):
 while True:
  try:q.get_nowait()
  except queue.Empty:break
 p.stdin.write(cmd+'\n');p.stdin.flush()
try:
 m=start('monitor')
 def read():
  for line in m.stdout:q.put(json.loads(line))
 threading.Thread(target=read,daemon=True).start()
 wait_for(lambda d:d['engine']=='WAITING' and d['vehicle']=='WAITING','initial waiting state')
 e=start('engine');v=start('vehicle')
 wait_for(lambda d:d['engine']=='OK' and d['vehicle']=='OK','both publishers received')
 send(e,'pause');wait_for(lambda d:d['engine']=='TIMEOUT' and d['vehicle']=='OK','engine timeout isolated')
 send(e,'normal');wait_for(lambda d:d['engine']=='OK','engine recovery')
 send(e,'corrupt');wait_for(lambda d:d['invalid']>=3 and d['engine']=='TIMEOUT','corrupt frames rejected and do not refresh age')
 send(e,'normal');wait_for(lambda d:d['engine']=='OK','valid-frame recovery')
 send(e,'heat');wait_for(lambda d:d['alarm'],'overheat alarm')
 send(e,'normal');wait_for(lambda d:not d['alarm'] and d['temp']==85,'alarm clears')
 send(v,'pause');wait_for(lambda d:d['vehicle']=='TIMEOUT' and d['engine']=='OK','vehicle timeout isolated')
 send(v,'normal');wait_for(lambda d:d['vehicle']=='OK','vehicle recovery')
 print(f'{checks} live integration checks passed.')
finally:
 for p in procs:
  if p.poll() is None:p.terminate()
 for p in procs:
  try:p.wait(timeout=2)
  except subprocess.TimeoutExpired:p.kill();p.wait()
