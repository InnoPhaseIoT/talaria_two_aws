# *****************************************************************************
# Copyright (c) 2014, 2019 IBM Corporation and other Contributors.
#
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
# *****************************************************************************

import getopt
import signal
import time
import sys
import json
import queue

import wiotp.sdk.application.config
import wiotp.sdk.application.client
import wiotp.sdk.exceptions

import numpy
import matplotlib.pyplot as plt
from sensor_processor import SensorProcessor
from sensorJSON import *


plotMap = {}
readingQueue = queue.Queue()

tableRowTemplate = "%-33s%-30s%s"


def mySubscribeCallback(mid, qos):
    if mid == statusMid:
        print("<< Subscription established for status messages at qos %s >> " % qos[0])
    elif mid == eventsMid:
        print("<< Subscription established for event messages at qos %s >> " % qos[0])


def myEventCallback(event):
    # Event looks to contain sensor data
    if JSON_ATTR_READING_PREFIX + "0" in event.data or JSON_ATTR_CALIB in event.data:
        print("%-33s%-30s%s" % (event.timestamp.isoformat(), event.device, event.eventId + ": <" + str(len(event.data)) + " elements>"))
        readingQueue.put(event.data)
    else:
        print("%-33s%-30s%s" % (event.timestamp.isoformat(), event.device, event.eventId + ": " + json.dumps(event.data)))

def myStatusCallback(status):
    if status.action == "Disconnect":
        summaryText = "%s %s (%s)" % (status.action, status.clientAddr, status.reason)
    else:
        summaryText = "%s %s" % (status.action, status.clientAddr)
    print(tableRowTemplate % (status.time.isoformat(), status.device, summaryText))


def interruptHandler(signal, frame):
    client.disconnect()
    sys.exit(0)

def setupPlot():
    ax = plt.subplot(3, 2, 1)
    ax.plot([], [])
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Temperature (C)")
    ax.set_title("Temperature (icp)")
    ax.grid(True)
    plotMap[JSON_ATTR_TEMP_ICP] = ax
    
    ax = plt.subplot(3, 2, 2)
    ax.plot([], [])
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Temperature (C)")
    ax.set_title("Temperature (shtc)")
    ax.grid(True)
    plotMap[JSON_ATTR_TEMP_SHTC] = ax
    
    ax = plt.subplot(3, 2, 3)
    ax.plot([], [])
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Pressure (Pa)")
    ax.set_title("Pressure")
    ax.grid(True)
    plotMap[JSON_ATTR_PRESSURE] = ax
    
    ax = plt.subplot(3, 2, 4)
    ax.plot([], [])
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Humidity (%)")
    ax.set_title("Humidity")
    ax.grid(True)
    plotMap[JSON_ATTR_HUMIDITY] = ax
    
    ax = plt.subplot(3, 2, 5)
    ax.plot([], [])
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Optical Power (nW/cm2)")
    ax.set_title("Optical Power")
    ax.grid(True)
    plotMap[JSON_ATTR_OPT_POW] = ax
    
    plt.tight_layout()

def usage():
    print(
        "simpleApp: Basic application connected to the IBM Internet of Things Cloud service."
        + "\n"
        + "\n"
        + "Options: "
        + "\n"
        + "  -h, --help          Display help information"
        + "\n"
        + "  -o, --organization  Connect to the specified organization"
        + "\n"
        + "  -i, --id            Application identifier (must be unique within the organization)"
        + "\n"
        + "  -k, --key           API key"
        + "\n"
        + "  -t, --token         Authentication token for the API key specified"
        + "\n"
        + "  -c, --config        Load application configuration file (ignore -o, -i, -k, -t options)"
        + "\n"
        + "  -T, --typeId        Restrict subscription to events from devices of the specified type"
        + "\n"
        + "  -I, --deviceId      Restrict subscription to events from devices of the specified id"
        + "\n"
        + "  -E, --event         Restrict subscription to a specific event"
    )


if __name__ == "__main__":
    signal.signal(signal.SIGINT, interruptHandler)

    try:
        opts, args = getopt.getopt(
            sys.argv[1:],
            "h:o:i:k:t:c:T:I:E:",
            ["help", "org=", "id=", "key=", "token=", "config=", "typeId=", "deviceId=", "event="],
        )
    except getopt.GetoptError as err:
        print(str(err))
        usage()
        sys.exit(2)
        
    setupPlot()

    organization = "quickstart"
    appId = "mySampleApp"
    authMethod = None
    authKey = None
    authToken = None
    configFilePath = None
    typeId = "+"
    deviceId = "+"
    event = "+"

    for o, a in opts:
        if o in ("-o", "--organization"):
            organization = a
        elif o in ("-i", "--id"):
            appId = a
        elif o in ("-k", "--key"):
            authMethod = "apikey"
            authKey = a
        elif o in ("-t", "--token"):
            authToken = a
        elif o in ("-c", "--cfg"):
            configFilePath = a
        elif o in ("-T", "--typeId"):
            typeId = a
        elif o in ("-I", "--deviceId"):
            deviceId = a
        elif o in ("-E", "--event"):
            event = a
        elif o in ("-h", "--help"):
            usage()
            sys.exit()
        else:
            assert False, "unhandled option" + o

    client = None
    if configFilePath is not None:
        options = wiotp.sdk.application.config.parseConfigFile(configFilePath)
    else:
        options = {
            "org": organization,
            "id": appId,
            "auth-method": authMethod,
            "auth-key": authKey,
            "auth-token": authToken,
        }
    try:
        client = wiotp.sdk.application.client.ApplicationClient(options)
        # If you want to see more detail about what's going on, set log level to DEBUG
        # import logging
        # client.logger.setLevel(logging.DEBUG)
        client.connect()
    except wiotp.sdk.exceptions.ConfigurationException as e:
        print(str(e))
        sys.exit()
    except wiotp.sdk.exceptions.UnsupportedAuthenticationMethod as e:
        print(str(e))
        sys.exit()
    except wiotp.sdk.exceptions.ConnectionException as e:
        print(str(e))
        sys.exit()

    print("(Press Ctrl+C to disconnect)")

    client.deviceEventCallback = myEventCallback
    client.deviceStatusCallback = myStatusCallback
    client.subscriptionCallback = mySubscribeCallback

    eventsMid = client.subscribeToDeviceEvents(typeId, deviceId, event)
    statusMid = client.subscribeToDeviceStatus(typeId, deviceId)

    print("=============================================================================")
    print(tableRowTemplate % ("Timestamp", "Device", "Event"))
    print("=============================================================================")

    senProcessor = SensorProcessor()

    while True:
        try:
            sensorReadings = readingQueue.get(block=False)
        except queue.Empty:
            pass
        else:
            # Set calibration data first, if present
            if JSON_ATTR_CALIB in sensorReadings:
                senProcessor.set_calibration(sensorReadings[JSON_ATTR_CALIB])
                del sensorReadings[JSON_ATTR_CALIB]
            
            for (readingName, reading) in sensorReadings.items():
                
                processedReading = senProcessor.process_reading(reading)
                
                timestamp = processedReading[JSON_ATTR_TIMESTAMP]
                
                for (measurementName, measurementValue) in processedReading.items():
                    if measurementName in plotMap:
                        ax = plotMap[measurementName]
                        line = ax.get_lines()[0]
                        
                        newX = timestamp/1000000
                        newY = measurementValue
                        
                        if newX not in line.get_xdata():
                            line.set_data(numpy.concatenate((line.get_xdata(), [newX])),
                                          numpy.concatenate((line.get_ydata(), [newY])))
                    
            for ax in plotMap.values():
                ax.relim()
                ax.autoscale_view(True, True, True)
                
            plt.draw()
        
        plt.pause(1)

