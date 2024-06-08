
import serial
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib import style
import pandas as pd
from datetime import datetime
import numpy as np


def is_float(string: str) -> bool:
    return string.replace(".", "").isnumeric()




style.use('fivethirtyeight')

fig = plt.figure()
ax1 = fig.add_subplot(1,1,1)


def epoch(dt64) -> int:
    return dt64.astype('datetime64[s]').astype('int')


timestamps: list[np.datetime64] = []
collector: list[list[float]] = []

epoch_start = epoch(np.datetime64('now'))

def new_datapoint(value: float, idx: int ):
    global timestamps
    global collector
    global epoch_start
    
    while len(collector) < idx + 1:
        collector.append([])
    
    if(idx == 0):
        timestamp = epoch(np.datetime64('now')) - epoch_start
        timestamps.append(timestamp)

    collector[idx].append(value)
    
import glob
import serial

def available_serial_ports():
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

ports = available_serial_ports()
assert len(ports) > 0, "No serial ports available"
ser = serial.Serial(ports[0], 115200, timeout=1)


def animate(i) -> None:
    global timestamps
    global collector

    line = ser.readline().decode("utf-8").replace('\r', '').replace('\n', '')   # read a '\n' terminated line
    values = line.split('|')
    if (len(values) > 0 and is_float(values[0])):
        ax1.clear()
        for idx, v in enumerate(values):
            new_datapoint(float(v), idx)
            ax1.plot(timestamps, collector[idx])
        print(f"{timestamps[-1]}: {line}")
        
    else:
        if( len(line) > 1):
            print(line)
    





# time.sleep(1)
# new_datapoint(1);
# time.sleep(2)
# new_datapoint(2);
# time.sleep(3)
# new_datapoint( 3);
# time.sleep(4)
# new_datapoint( 4);
# time.sleep(5)
# new_datapoint( 5);

ani = animation.FuncAnimation(fig, animate, interval=100)
plt.show()

ser.close()

    
    
