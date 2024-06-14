
import serial
import time
from datetime import datetime
import numpy as np
import os 


def is_float(string: str) -> bool:
    return string.replace(".", "").isnumeric()

def epoch(dt64) -> int:
    return dt64.astype('datetime64[s]').astype('int')


# timestamps: list[np.datetime64] = []
# collector: list[list[float]] = []

epoch_start = epoch(np.datetime64('now'))

# def new_datapoint(value: float, idx: int ):
#     global timestamps
#     global collector
#     global epoch_start
    
#     while len(collector) < idx + 1:
#         collector.append([])
    
#     if(idx == 0):
#         timestamp = epoch(np.datetime64('now')) - epoch_start
#         timestamps.append(timestamp)

#     collector[idx].append(value)
    
import glob
import serial

""" Find available serial ports"""
def find_serial_ports():
    ports = glob.glob('/dev/ttyACM[0-9]*')

    res = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            res.append(port)
        except:
            pass
    return res

ports = find_serial_ports()
assert len(ports) > 0, "No serial ports available"
ser = serial.Serial(ports[0], 115200, timeout=3)
assert ser.is_open, "Serial connection could not be made"

def collect_to(csv_writer) -> None:
    line = ser.readline().decode("ascii").replace('\r', '').replace('\n', '')   # read a '\n' terminated line
    values = line.split('|')[:-1]
    if (len(values) > 0 and is_float(values[0])):
        csv_writer.writerow([datetime.now()] + values);
        print(f"{datetime.now()}: {line}")
        
    else:
        if( len(line) > 1):
            print(line)
    

import csv

fname = 'kaskas_data.csv'

with open(fname, 'a+', newline='') as file:
    writer = csv.writer(file)
    # ser.write(bytearray("MTC!stopDatadump\n",'ascii'))
    ser.write(bytearray("MTC!startDatadump\n",'ascii'))
    time.sleep(1)
    line = ser.readline().decode("utf-8").replace('\r', '').replace('\n', '')
    field_names = line.split('|')[:-1]
    field_names = ["TIMESTAMP"] + field_names
    print(field_names)
    if os.stat(fname).st_size == 0: # only write headers when empty
        writer.writerow(field_names)
    
    while True:
        collect_to(writer)
        file.flush()





ser.close()

    
    
