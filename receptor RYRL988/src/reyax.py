import serial, serial.tools.list_ports as list_ports,time
import sys

def printf(format, *args):
    sys.stdout.write(format % args)

# captura el nombre del puerto ftdi
ports = list_ports.comports()
port = ports[0].device
print(f'port: {port} length:{len(ports)}')
reyax = serial.Serial(port=port, baudrate=115200)
counter = 1

def send_message(mesg):
    reyax.write(f'{mesg}\r\n'.encode("utf-8"))
    # time.sleep(0.1)
    resp = reyax.readline().decode('utf-8')
    print('respuesta:', resp)
    if(resp.find("OK") < 0): 
        print(f"error en send_message: {resp=}")
        return False
    return True

if __name__ == '__main__':
    cad100 = '1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890'
    data = f'AT+SEND=101,{len(cad100)},{cad100}'
    try:
        print("saliendo del modo sleep")
        reyax.write('AT+MODE=0\r\n'.encode('utf-8'))
        reyax.readline().decode("utf-8")
        # establecemos el pin de encriptacion
        if(send_message('AT+CPIN=4B5FF2A9')):
            print("establecido codigo encriptacion")
        # start = time.time();
        # if(send_message(data)):
        #     print("mensaje payload 100 enviado")
        #     print(f'tiempo: {time.time() - start}')
        while True:
            time.sleep(2)
            cadena = format("AT+SEND=101,6,%06d\r\n" % counter)
            # print("enviado: %s" % cadena)
            sys.stdout.write("enviado: ")
            sys.stdout.write(cadena)
            reyax.write(cadena.encode('utf-8'))
            counter += 1
        # pasamos a modo sleep
        # print("activando sleep")
        # reyax.write('AT+MODE=1\r\n'.encode('utf-8'))
        # while True:
        #     message = reyax.readline().decode('utf-8')
        #     message = message.replace('\n','').replace('\r','')
        #     print(f"message:[{message}]")

    except Exception as err:
        print(f"ocurrio una excepcion {err=}, {type(err)=}")
        reyax.close()
        raise
