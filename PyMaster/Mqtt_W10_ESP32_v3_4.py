import paho.mqtt.client as mqttc
import time
from tkinter import *
from tkinter import ttk
from time import sleep

sub_topic = "ESP32"  # Messages for Win10 machine
sub_topic_end = "ESP32_END"  # Stop messages for Win10 machine
pub_topic = "W10" #  Messages for ESP32 machine
broker="<your mqtt broker website">"
port="<your mqtt port number>"
username="<your mqtt username>"
password="<your mqtt password"

Connected = False
ibytes = 0
icount = 0
bChunk = False
fo = 0
fSize = 0
bPrint = True


# stop MQTT loop before closing this app
def onWindowExit():
    sleep(1)
    client.loop_stop()    
    root.destroy()


# when the Enter key is pressed (after typing command to slave)
def Enter(*args):
    try:            
        s = inp.get()
        client.publish(pub_topic,s)
        
        text['state'] = 'normal'
        text.insert('end', '\n'+s)
        text['state'] = 'disabled'
        
    except ValueError:
        pass


# setup the GUI
root = Tk()
root.title("Binda IOT Weather Station")
root.protocol('WM_DELETE_WINDOW', onWindowExit)
root.geometry('+20+20')

mainframe = ttk.Frame(root, padding="3 3 12 12")
mainframe.grid(column=0, row=0, sticky=(N, W, E, S))
root.columnconfigure(0, weight=1)
root.rowconfigure(0, weight=1)

inp = StringVar()
inp_entry = ttk.Entry(mainframe, width=7, textvariable=inp)
inp_entry.grid(column=2, row=1, sticky=(W, E))
inp_entry.configure(font = 'TkFixedFont')

text = Text(mainframe, width=100, height=16)
text.configure(font = 'TkFixedFont')
text.insert('1.0', '')
text['state'] = 'disabled'
text.grid(column=2, row=2, sticky='nwes')

ttk.Label(mainframe, text="Type message :").grid(column=1, row=1, sticky=E)
ttk.Label(mainframe, text="Results :").grid(column=1, row=2, sticky=(N, E))

for child in mainframe.winfo_children(): 
    child.grid_configure(padx=5, pady=5)

inp_entry.focus()
root.bind("<Return>", Enter)


# Called when a message has been published to the MQTT broker
def on_publish(mosq, obj, mid):
    s = inp.get()
    inp.set("")
    s2 = pub_topic + ": "+s
    inp.set("")
    
    text['state'] = 'normal'
    text.delete(1.0,'end')
    text.insert(1.0, s2)
    text['state'] = 'disabled'


# Called when connection has been negotiated with MQTT broker
def on_connect(client, userdata, flags, rc):
    if rc==0:
        global Connected
        Connected = True
        client.subscribe(sub_topic)
        client.subscribe(sub_topic_end)
        
        text['state'] = 'normal'
        text.insert('end', 'Connected to broker')
        text.insert('end', '\nSubscribed to topic : '+sub_topic)
        text.insert('end', '\nSubscribed to topic : '+sub_topic_end)
        text['state'] = 'disabled'

    else:
        text['state'] = 'normal'
        text.insert('end', 'Connection failed')
        text['state'] = 'disabled'


# Called when subscribed message is available from the MQTT broker
def on_message(client, userdata, msg):
    global ibytes
    global icount
    global bChunk
    global fo
    global fSize
    global bPrint 

    # Strip b' and ' from the raw message
    message = str(msg.payload)[2:-1]

    if msg.topic==sub_topic_end:
        # This message indicates that all block sub_topic messages have been received
        # Close collected file and reset bChunk to false
        fo.close()
        bChunk = False
        bPrint = False
        
        if fSize==ibytes:
            aTXT = "ALL OK"
        else:
            aTXT = "ERROR IN DOWNLOAD"
            
        if icount==1:
            s2 = " block read  "
        else:
            s2 = " blocks read  "
            
        s = msg.topic+"> "+str(icount)+s2+str(ibytes)+" bytes  "+aTXT

        text['state'] = 'normal'
        text.insert('end', '\n'+s)
        text['state'] = 'disabled'

    if bChunk:
        # Deal with messages that are file chunks by appending them to file
        # and incrementing chunk counter        
        fo.write(msg.payload)
        icount += 1
        
        sp = len(msg.payload);
        ibytes += sp
        s = msg.topic+"> Block "+str(icount)+" : "+str(sp)+" bytes"

        text['state'] = 'normal'
        if icount==1:
            text.insert('end', '\n'+msg.topic+"> .")
        else:            
            text.insert('end', '.')
        text['state'] = 'disabled'
        
    else:
        if bPrint:
            s = msg.topic+": "+message

            text['state'] = 'normal'
            text.insert('end', '\n'+s)
            text['state'] = 'disabled'
            
        else:
            bPrint = True
            
        if message[:3] == "gd ":
            # This message indicates that we will be receiving a file in chunks
            # that we will combine into fileName
            bChunk = True
            icount = 0
            ibytes = 0
            
            # Get the file name and its size in bytes from this message 
            alist = (message[3:]).split(" ")
            fName = alist[0]
            fSize = int(alist[1])
            
            # Open this file in readiness for creating it from chunks
            fo = open("C:\\Users\\roman\\OneDrive\\Python\\"+fName,"wb")
    

# setup MQTT
client = mqttc.Client("Win10 client")
client.username_pw_set(username,password)
client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.connect(broker,port,65535)
client.loop_start()

root.mainloop()    
