import paho.mqtt.client as mqttc
import time
from tkinter import *
from tkinter import ttk
from time import sleep

sub_topic = "ESP32"  # Messages for Win10 machine
sub_topic_end = "ESP32_END"  # Stop messages for Win10 machine
pub_topic = "W10" #  Messages for ESP32 machine

broker="<your mqtt broker website"
port=66666 #<your mqtt port number>
username="<your mqtt username>"
password="<your mqtt password>"

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
    global broker
    global port
    global username
    global password
    
    try:            
        s = inp.get()
        c = s.split()
        if c[0]!='Xmqtt':
            client.publish(pub_topic,s)
            text['state'] = 'normal'
            text.insert('end', '\n'+s)
            text['state'] = 'disabled'
            
        else: # read in mqtt config for this master py program
            # must have 4 parameters 
            if len(c)==5:
                try:
                    # setup proposed login parameters
                    broker=c[1]
                    try:
                        port=int(c[2],10)
                    except ValueError:
                        port=66666
                    username=c[3]
                    password=c[4]

                    # display proposed login parameters
                    text['state'] = 'normal'
                    text.delete(1.0,'end')
                    text.insert('end', 'broker='+broker)
                    text.insert('end', '\nport='+str(port))
                    text.insert('end', '\nusername='+username)
                    text.insert('end', '\npassword='+password+'\n')
                    text['state'] = 'disabled'

                    # disconnect nicely before reconnecting
                    client.loop_stop()
                    client.disconnect()

                    # (re)start MQTT loop with new parameters
                    client.username_pw_set(username,password)
                    client.on_connect = on_connect
                    client.on_message = on_message
                    client.on_publish = on_publish
                    client.connect(broker,port,65535)
                    client.loop_start()

                    # save configuration to text file
                    d = "C:\\Users\\roman\\OneDrive\\Python\\MQTTconfig_W10.txt"
                    f = open(d, 'wt')
                    for i in range(1,5):
                        f.write(c[i]+'\n')
                    f.close()                                 
                    
                except:
                    text['state'] = 'normal'
                    text.delete(1.0,'end')
                    text.insert('end', '*** MQTT CONNECTION FAILED ***')
                    text['state'] = 'disabled'
                
            else:
                text['state'] = 'normal'
                text.delete(1.0,'end')
                text.insert('end', '*** PARSING ERROR FOR MASTER MQTT CONFIGURATION ***')
                text['state'] = 'disabled'
        
    except ValueError:
        text['state'] = 'normal'
        text.delete(1.0,'end')
        text.insert('end', '*** VALUE ERROR ***')
        text['state'] = 'disabled'


# download mqtt configuration parameters from MQTTconfig_W10.txt file
# this enables easy change and prevents exposing login details with
# this python source being located on a public GitHub directory.
d = "C:\\Users\\roman\\OneDrive\\Python\\MQTTconfig_W10.txt"
with open(d) as f:
    c = f.readlines()
broker=c[0].rstrip()
port=int(c[1],10)
username=c[2].rstrip()
password=c[3].rstrip()


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
        text.insert('end', '*** CONNECTION FAILED ***\n')
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
    

# start MQTT loop
client = mqttc.Client("Win10 client")
client.username_pw_set(username,password)
client.on_connect = on_connect
client.on_message = on_message
client.on_publish = on_publish
client.connect(broker,port,65535)
client.loop_start()

# tkinter GUI loop
root.mainloop()    
