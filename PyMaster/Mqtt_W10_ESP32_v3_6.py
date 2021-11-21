import paho.mqtt.client as mqttc
import time
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
bStop = False


# when the Enter key is pressed (after typing command to slave)
def Enter():
    global broker
    global port
    global username
    global password
    
    try:
        # read and split input string list c
#        s = input('ENTER MESSAGE : ')
        s = 'gd SensorData.csv'
        c = s.split()

        if c[0]!='Xmqtt': # 'send' string list c to slave
            client.publish(pub_topic,s)
            print(s)
            
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
                    print('broker='+broker)
                    print('\nport='+str(port))
                    print('\nusername='+username)
                    print('\npassword='+password+'\n')

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
                    print('*** MQTT CONNECTION FAILED ***')
                
            else:
                print('*** PARSING ERROR FOR MASTER MQTT CONFIGURATION ***')
        
    except ValueError:
        print('*** VALUE ERROR ***')


# download mqtt configuration parameters from MQTTconfig_W10.txt file
# this enables easy change and prevents exposing login details with
# this python source being located on a public GitHub directory.
d = "C:\\Users\\roman\\OneDrive\\Python\\MQTTconfig_W10.txt"
with open(d) as f:
    c = f.readlines()
f.close()                                 
broker=c[0].rstrip()
port=int(c[1],10)
username=c[2].rstrip()
password=c[3].rstrip()


# Called when a message has been published to the MQTT broker
def on_publish(mosq, obj, mid):
    s = " <= entered"
    print(s)


# Called when connection has been negotiated with MQTT broker
def on_connect(client, userdata, flags, rc):
    if rc==0:
        global Connected
        Connected = True
        client.subscribe(sub_topic)
        client.subscribe(sub_topic_end)
        
        print('Connected to broker')
        print('Subscribed to topic : '+sub_topic)
        print('Subscribed to topic : '+sub_topic_end)

    else:
        print('*** CONNECTION FAILED ***\n')


# Called when subscribed message is available from the MQTT broker
def on_message(client, userdata, msg):
    global ibytes
    global icount
    global bChunk
    global fo
    global fSize
    global bPrint
    global bStop

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
            bStop = True
        else:
            aTXT = "ERROR IN DOWNLOAD"
            
        if icount==1:
            s2 = " block read  "
        else:
            s2 = " blocks read  "
            
        s = "  "+str(icount)+s2+str(ibytes)+" bytes  "+aTXT

        print(s)

    if bChunk:
        # Deal with messages that are file chunks by appending them to file
        # and incrementing chunk counter        
        fo.write(msg.payload)
        icount += 1
        
        sp = len(msg.payload);
        ibytes += sp
        s = "  Block "+str(icount)+" : "+str(sp)+" bytes"

        if icount==1:
            print(".")
        else:            
            print('.')
        
    else:
        if bPrint:
            print(message)
            
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

while Connected != True:
    time.sleep(1.0)


Enter()
time.sleep(1.0)
while (bStop==False):
    print("FIN")            
    time.sleep(1.0)
client.loop_stop()

# Open this file and write text 'finished' to indicate finished download.
f = open("C:\\Users\\roman\\OneDrive\\Python\\Finished.txt","w")
f.write('finished')
f.close()                                 
print("FINISHED")

