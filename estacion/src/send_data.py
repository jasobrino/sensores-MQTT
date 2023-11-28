import sys
import json
import paho.mqtt.client as mqtt
import re
import time

def on_connect(client, userdata, flags, rc):
    print('conectado (%s)  - rc: %d' % (client._client_id, rc))
    client.subscribe(topic='sensor/#', qos=0)

def main():
    #client = mqtt.Client(client_id='', clean_session=True)
    client = mqtt.Client( clean_session=True)
    client.on_connect = on_connect

    client.connect(host='barebone', port=1883, keepalive=60)
    
    datalog = open("datalog.txt")
    while True:
        linea = datalog.readline()
        if not linea:
            break
        json_data = {}
        #data = re.sub(r'\bnan\b', 'NaN', str(linea.decode("utf-8"))) 
        json_linea = json.loads(linea) #convierte string a objeto JSON
        for key, val in json_linea.items():
            json_data[key.upper()] = val
        out = "\"{}\",{},{},{},{},{},{},{}".format(json_data.get("ID"), json_data.get("TEMP"), json_data.get("HUMEDAD"),
                json_data.get("PRESION"), json_data.get("ECO2"), json_data.get("TVOC"), json_data.get("VBAT"), json_data.get("RSSI"))
        print(out)
        client.publish('sensor', out)
        time.sleep(10)
        
    datalog.close()
    print("fin de datalog")

if __name__ == '__main__':
    main()

sys.exit(0)