
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
values: list[float] = []

epoch_start = epoch(np.datetime64('now'))

def new_datapoint(value: float):
    global timestamps
    global values
    global epoch_start
    timestamp = epoch(np.datetime64('now')) - epoch_start
    timestamps.append(timestamp)
    values.append(value)
    print(f"{timestamp}: {value}")

ser = serial.Serial('/dev/ttyACM1', 115200, timeout=1)


def animate(i) -> None:
    global timestamps
    global values

    line = ser.readline().decode("utf-8").replace('\r', '').replace('\n', '')   # read a '\n' terminated line
    if (is_float(line)):
        new_datapoint(float(line))
    else:
        if( len(line) > 1):
            print(line)
    
    ax1.clear()
    ax1.plot(timestamps, values)




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

ani = animation.FuncAnimation(fig, animate, interval=1000)
plt.show()

ser.close()

    
    
