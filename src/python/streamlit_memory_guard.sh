#!/bin/bash

memory_usage() {
    # echo hellllllllllllllllo "$1 aaaaaaaaaaaaaaaaaaaa"
    mem=$(smem | grep "$1" | grep -v grep | tr -s ' '| cut -d\  -f7)
    if [ "$1" = "" ] || [ "$mem" = "" ]; then
        mem=0
    fi
    echo $(( mem / 1024 ))
}

free_memory() {
    awk '/MemFree/ { printf "%.0f \n", $2/1024 }' /proc/meminfo
}

available_memory() {
    awk '/MemAvailable/ { printf "%.0f \n", $2/1024 }' /proc/meminfo
}

total_memory() {
    awk '/MemTotal/ { printf "%.0f \n", $2/1024 }' /proc/meminfo
}

while true; do
    used=$(memory_usage "$(pgrep -x streamlit)")
    # echo used: $used
    if [ $used -gt 1500 ];then
        echo "Streamlit used too much memory, killing process!"
        pkill -9 -x streamlit
    fi
    
    
    available=$(available_memory)
    # echo available: $available
    if [ $available -lt 200 ]; then
        echo "Not enough free memory left, killing process!"
        pkill -9 streamlit
    fi
    
    sleep 1
done
