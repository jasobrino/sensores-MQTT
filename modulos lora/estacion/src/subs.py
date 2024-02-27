import sys
import json
import paho.mqtt.client as mqtt
from datetime import datetime
import re

def on_connect(client, userdata, flags, rc):
    print('conectado (%s)  - rc: %d' % (client._client_id, rc))
    client.subscribe(topic='sensor/#', qos=0)

def on_message(client, userdata, message):
    now = datetime.now()
    fecha = "{dd:02d}/{MM:02d}/{aa} {HH:02d}:{mm:02d}:{ss:02d}".format(dd=now.day,MM=now.month,aa=now.year,HH=now.hour,mm=now.minute,ss=now.second)
    data = re.sub(r'\bnan\b', 'NaN', str(message.payload.decode("utf-8"))) 
    json_data = json.loads(data) #convierte string a objeto JSON
    json_data['date'] = fecha; #añadirmos la fecha y hora
    #registro = "topic:[{to}] {me} ({da})\n".format(to=message.topic,me=message.payload,da=fecha)
    print(json_data)
    with open("datalog.txt","a") as f:
        f.write(json.dumps(json_data))
        f.write("\n")
        f.close()

def main():
    client = mqtt.Client(client_id='', clean_session=True)
    client = mqtt.Client( clean_session=True)
    client.on_connect = on_connect
    client.on_message = on_message

    # client.connect(host='linux-mqtt', port=1883, keepalive=60)
    client.connect(host='cachonigul.sytes.net', port=1883, keepalive=60)
    client.loop_forever()


if __name__ == '__main__':
    main()

sys.exit(0)