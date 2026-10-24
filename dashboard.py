"""Tk GUI supervising three C ECU processes; telemetry comes from CAN monitor."""
import json,pathlib,queue,socket,subprocess,sys,threading,tkinter as tk
from tkinter import messagebox
root=pathlib.Path(__file__).resolve().parent
interface=sys.argv[1] if len(sys.argv)>1 else 'vcan0'
exe=root/'build/ecu'
if not exe.exists():sys.exit('Run python3 build.py in Ubuntu first.')
try:
 with socket.socket(socket.PF_CAN,socket.SOCK_RAW,socket.CAN_RAW) as s:s.bind((interface,))
except OSError as e:sys.exit(f'Cannot open {interface}: {e}. See README setup commands.')
app=tk.Tk();app.title('CANLab | Virtual Vehicle Network');app.geometry('1020x720');app.configure(bg='#0f172a')
procs={}; messages=queue.Queue(); closing=False; history=[]
def read_stream(stream,role,is_json=False):
 try:
  for line in stream:
   if is_json:messages.put(('data',json.loads(line)))
   else:messages.put(('error',f'{role}: {line.strip()}'))
 except Exception as e:messages.put(('error',str(e)))
for role in ['monitor','engine','vehicle']:
 p=subprocess.Popen([str(exe),role,interface],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
 procs[role]=p
 threading.Thread(target=read_stream,args=(p.stderr,role),daemon=True).start()
 if role=='monitor':threading.Thread(target=read_stream,args=(p.stdout,role,True),daemon=True).start()
def send(role,cmd):
 try:procs[role].stdin.write(cmd+'\n');procs[role].stdin.flush()
 except (BrokenPipeError,OSError):status.set(f'{role} stopped. Restart the dashboard.')
tk.Label(app,text='CANLAB',font=('Arial',28,'bold'),fg='#38bdf8',bg='#0f172a').pack(pady=(20,2))
tk.Label(app,text=f'Three C ECUs • Linux SocketCAN • {interface} • simulated vehicle data',fg='#cbd5e1',bg='#0f172a').pack()
values=tk.StringVar(value='Waiting for CAN frames...');state=tk.StringVar();status=tk.StringVar(value='Ready')
tk.Label(app,textvariable=values,font=('Arial',19),fg='white',bg='#0f172a').pack(pady=15)
tk.Label(app,textvariable=state,font=('Arial',12),fg='#fbbf24',bg='#0f172a').pack()
canvas=tk.Canvas(app,bg='#172338',height=280,highlightthickness=0);canvas.pack(fill='x',padx=25,pady=15)
for role,controls in [('engine',[('Normal','normal'),('Pause engine','pause'),('Corrupt payload','corrupt'),('Overheat','heat')]),('vehicle',[('Normal','normal'),('Pause vehicle','pause')])]:
 frame=tk.Frame(app,bg='#0f172a');frame.pack(pady=5)
 tk.Label(frame,text=role.upper(),fg='white',bg='#0f172a',width=12).pack(side='left')
 for label,cmd in controls:tk.Button(frame,text=label,command=lambda r=role,c=cmd:send(r,c)).pack(side='left',padx=5)
tk.Label(app,textvariable=status,fg='#94a3b8',bg='#0f172a',wraplength=980).pack(pady=12)
tk.Label(app,text='Stale numbers are last-known values. Pause tests message timeout; corruption tests the application checksum.',fg='#94a3b8',bg='#0f172a').pack()
def draw(d):
 values.set(f"RPM {d['rpm']}    |    Coolant {d['temp']} °C    |    Speed {d['speed']:.1f} km/h")
 state.set(f"Engine: {d['engine']}    Vehicle: {d['vehicle']}    Overheat: {'YES' if d['alarm'] else 'NO'}")
 status.set(f"RX engine={d['engine_rx']} vehicle={d['vehicle_rx']}  |  Rejected={d['invalid']}  |  Sequence events={d['sequence_events']}  |  Elapsed={d['ms']/1000:.1f}s")
 history.append((d['rpm'] if d['engine']=='OK' else None,d['speed'] if d['vehicle']=='OK' else None));del history[:-200]
 canvas.delete('all');w=max(canvas.winfo_width(),200)
 canvas.create_text(15,15,anchor='w',text='RPM (0–8000): cyan   |   Speed (0–240 km/h): green   |   gaps = stale data',fill='white')
 for y in [60,120,180,240]:canvas.create_line(15,y,w-15,y,fill='#334155')
 for channel,scale,color in [(0,8000,'#38bdf8'),(1,240,'#6ee7b7')]:
  for i in range(1,len(history)):
   a,b=history[i-1][channel],history[i][channel]
   if a is not None and b is not None:canvas.create_line(15+(i-1)*(w-30)/199,260-a/scale*220,15+i*(w-30)/199,260-b/scale*220,fill=color,width=2)
def close():
 global closing
 if closing:return
 closing=True
 for p in procs.values():
  if p.poll() is None:p.terminate()
 for p in procs.values():
  try:p.wait(timeout=2)
  except subprocess.TimeoutExpired:p.kill();p.wait()
 app.destroy()
def poll():
 if closing:return
 try:
  for _ in range(100):
   kind,item=messages.get_nowait()
   if kind=='error':messagebox.showerror('CANLab',item);close();return
   draw(item)
 except queue.Empty:pass
 for role,p in procs.items():
  if p.poll() is not None:messagebox.showerror('CANLab',f'{role} exited ({p.returncode}).');close();return
 app.after(50,poll)
app.protocol('WM_DELETE_WINDOW',close);app.after(50,poll);app.mainloop()
