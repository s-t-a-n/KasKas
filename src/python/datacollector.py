from serial import Serial
from serial import SerialTimeoutException
import time
from datetime import datetime
import numpy as np
import os
import glob
import csv
from typing import Optional
from pathlib import Path
from enum import Enum
import pandas as pd  # read csv, df manipulation
import errno
from multiprocessing.synchronize import Lock as LockType
from multiprocessing import Lock
import argparse
from typing import TextIO

# parser = argparse.ArgumentParser('Start house server')
# parser.add_argument('-v', '--verbose', action='store_true',
#                     help='Print logging output')
# parser.add_argument('-g', '--gui', action='store_true',
#                     help='Show graphical user interface')
# args = parser.parse_args()

#########################################

# ########################################

def is_float(string: str) -> bool:
    return string.replace(".", "").isnumeric()


def epoch(dt64) -> int:
    return dt64.astype("datetime64[s]").astype("int")


epoch_start = epoch(np.datetime64("now"))


""" Find available serial ports """
def find_serial_ports():
    ports = glob.glob("/dev/ttyACM[0-9]*")
    res = []
    for port in ports:
        try:
            s = Serial(port)
            s.close()
            res.append(port)
        except:
            pass
    return res


sampling_interval = 10

# serial
ports = find_serial_ports()
assert len(ports) > 0, "No serial ports available"
ser = Serial(ports[0], 115200, timeout=1.5)
assert ser.is_open, "Serial connection could not be made"


class KasKasAPI:
    """_summary_"""

    _serial: Serial
    _lock: LockType
    _log_file: TextIO

    def __init__(self, serial: Serial, log_fname: Path) -> None:
        self._serial = serial
        self._lock = Lock()
        self._log_file = open(log_fname,mode="a+")

    class OP(Enum):
        FUNCTION_CALL = "!"
        ASSIGNMENT = "="
        ACCESS = "?"
        RESPONSE = "<"

    class Response:
        class Status(Enum):
            OK = 0
            BAD_INPUT = 1
            BAD_RESULT = 2
            TIMEOUT = 3
            BAD_RESPONSE = 4
            UNKNOWN_ERROR = 5

        status: Status
        arguments: Optional[list[str]]

        def __init__(
            self, status: Status, arguments: Optional[list[str]] = None
        ) -> None:
            self.status = status
            self.arguments = arguments

        def __bool__(self) -> bool:
            return self.status == self.Status.OK

    def request(
        self, module: str, function: str, arguments: Optional[list[str]] = None
    ) -> Response:
        with self._lock:
            self._flush()
            argument_substring = ":" + "|".join(arguments) if arguments else ""
            request_line = f"{module}!{function}{argument_substring}\r"
            # print(f"writing request line {request_line}")
            ser.write(bytearray(request_line, "ascii"))
            ser.flush()
            return self._read_response_for(module)

    def _read_response_for(self, module: str, max_skipable_lines: int = 32) -> Response:
        while max_skipable_lines > 0:
            try:
                raw_line = (
                    self._serial.readline()
                    .decode("utf-8")
                    .replace("\r", "")
                    .replace("\n", "")
                )
                # print(f"_read_response_for: {raw_line}")
            except SerialTimeoutException:
                return self.Response(self.Response.Status.TIMEOUT)

            correct_reply_header = module + str(self.OP.RESPONSE.value)
            if raw_line.startswith(correct_reply_header):
                line_reply_header_stripped = raw_line[len(correct_reply_header) :]
                remainder = line_reply_header_stripped.split(":")

                if len(remainder) < 2:
                    print(f"couldnt find return value for reply: {raw_line}")
                    continue

                return_status = int(remainder[0])
                values_line = "".join(remainder[1:])

                values = values_line.split("|")[:-1]
                # print(f"Response: {raw_line} for values_line {values_line}")
                # print(f"found values {values}")
                return self.Response(
                    status=self.Response.Status(return_status), arguments=values
                )
            else:
                self._print_debug_line(raw_line)
                max_skipable_lines -= 1

        print(
            f"Request for {module} failed, couldnt find a respons in {max_skipable_lines} times"
        )
        return self.Response(self.Response.Status.BAD_RESPONSE)

    def _flush(self) -> None:
        # print("flush")
        while self._serial.in_waiting > 0:
            try:
                line = (
                    self._serial.readline()
                    .decode("utf-8")
                    .replace("\r", "")
                    .replace("\n", "")
                )
            except SerialTimeoutException:
                line = self._serial.read_all()
            self._print_debug_line(line + "\n")

    def _print_debug_line(self, line: str) -> None:
        line = line.strip().replace("\r", "").replace("\n", "")
        if len(line) > 0:
            print(f"DBG: {line}")
            self._log_file.write(f"{datetime.now()}: {line}\n");
            self._log_file.flush()


class MetricCollector:
    """_summary_"""

    def __init__(self) -> None:
        pass


def collect_to(metrics: list[str], csv_writer) -> None:
    if len(metrics) > 0 and all([is_float(s) for s in metrics]):
        csv_writer.writerow([datetime.now()] + metrics)
        print(f"{datetime.now()}: {metrics}")
    else:
        print(f"Invalid row: {metrics}")


def do_datacollection(api: KasKasAPI, output_fname: Path, log_fname: Path, sampling_interval: int):
    with open(output_filename, "a+", newline="") as file:

        print("loading csv file")
        writer = csv.writer(file)

        # serial_request("MTC!stopDatadump\r")

        # if not api.request(module="MTC", function="setDatadumpIntervalS", arguments=[f"{sampling_interval}"]):
        #     print("failed to set datadump interval")
        #     return

        # if not api.request(module="MTC", function="startDatadump"):
        #     print("failed to start datadump")
        #     return

        print("setting up datacollection")
        fields_response = api.request(module="MTC", function="getFields")
        if not fields_response or not all(
            [
                str.isalpha(s) and str.isupper(s)
                for s in [s.replace("_", "") for s in fields_response.arguments]
            ]
        ):
            print(f"failed to read fields, got invalid response: {fields_response.arguments}")
            return

        if os.stat(output_filename).st_size == 0:
            # this is the first line in a new file; write headers
            fields = ["TIMESTAMP"] + fields_response.arguments
            print(f"writing fields to new file: {fields}")
            writer.writerow(fields)
        else:
            # this file contains data
            df = pd.read_csv(output_fname, low_memory=True)
            existing_columns = set(df.columns)
            incoming_columns = set(["TIMESTAMP"] + fields_response.arguments)
            if incoming_columns != existing_columns:
                print(f"{output_fname} has columns {existing_columns}")
                print(f"api provides the following fields {incoming_columns}")
                print("the existing and incoming columns do no match")
                return

        print("starting datacollection loop")
        while metrics_response := api.request(module="MTC", function="getMetrics"):
            # print("hello?")
            collect_to(metrics_response.arguments, writer)
            file.flush()
            # print(f"sleeping for  {sampling_interval}s")
            time.sleep(sampling_interval)
    print("datacollection came to a halt")

log_filename = Path("/mnt/USB/kaskas.log")
api = KasKasAPI(ser, log_fname=log_filename)

output_filename = Path("/mnt/USB/kaskas_data.csv")

while True:
    do_datacollection(api, output_filename,log_filename, sampling_interval)
    time.sleep(5)
