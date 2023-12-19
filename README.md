# AWS IoT Device SDK Embedded C on InnoPhase Talaria TWO Platform

## Table of Contents

- [Introduction](#introduction)
- [Getting Started with the SDK](#get-started)
- [Overview of Sample Applications](#sample-apps)
- [Releases](#releases)

## Introduction

<a name="introduction"></a>

[AWS IoT Device SDK Embedded-C Release Tag v3.1.5](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v3.1.5) is ported on Talaria TWO Software Development Kit as per the porting guidelines.

Using this, the users can now start developing exciting [ultra-low power IoT solutions on Talaria TWO family of devices](https://innophaseiot.com/talaria-technology-details/), utilizing the power of the AWS IoT Core platform and its services.

More information on aws-iot-device-sdk-embedded-C release tag v3.1.5 can be found here:
[https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v3.1.5](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v3.1.5)

API Documentation and other details specific to AWS IoT Device SDK Embedded C release tag V3.1.5 can be found here:
[http://aws-iot-device-sdk-embedded-c-docs.s3-website-us-east-1.amazonaws.com/index.html](http://aws-iot-device-sdk-embedded-c-docs.s3-website-us-east-1.amazonaws.com/index.html)

## Getting Started with the SDK

<a name="get-started"></a>

### Cloning the 'talaria_two_aws' repository
- Create a new folder in any place and clone the 'talaria_two_aws' repo using below command
``` bash
git clone --recursive https://github.com/InnoPhaseIoT/talaria_two_aws.git
```
> This repo uses [Git Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) for it's dependencies. The option '--recursive' is required to clone git submodule repos needed by 'talaria_two_aws' repo.


### Patching the 'AWS IoT Device SDK Embedded C' for Talaria TWO SDK Compatibility
- After the clone is complete as described in previous step, please move the folder 'talaria_two_aws' to the path `<sdk_path>/apps/`.

- Then go (cd) to the directory `<sdk_path>/apps/talaria_two_aws/aws-iot-device-sdk-embedded-C` and run the below command to patch AWS IoT Device SDK for Talaria TWO compatibility.
``` bash
<sdk_path>/apps/talaria_two_aws/aws-iot-device-sdk-embedded-C$ git apply ../patches/t2_compatibility.patch
```

- **Please note that patching needs to be done only once after the clone is successful.**

### Compiling the AWS IoT SDK and the Sample Apps
- Once the patch is applied successfully, running Make from `<sdk_path>/apps/talaria_two_aws` will compile the 'AWS IoT SDK' along with the Talaria TWO specific 'Platform Adaptation Layer' and 'Sample Applications' ported to Talaria TWO.

- On running make, the binaries for the various Sample Apps will be created in folder 'out' in their individual path, e.g. `<sdk_path>/apps/talaria_two_aws/sample_apps/<platform>/<application_folder>/out`.

- Applications can have their own custom configurations for sdk based on their own needs, and the customized 'aws_iot_config.h' file can be included by individual apps at the time of compiling the AWS IoT SDK.

- All the 'Sample Applications' use their own individually customizable 'aws iot config file' at the path `<sdk_path>/apps/talaria_two_aws/sample_apps/<platform>/<application_folder>/src/aws_iot_config.h`.

### Programming the Dev-Kits
**Follow Application Note provided with the Talaria TWO SDK at the path `<sdk_path>/apps/iot_aws` for further details on programming certs, keys and executable binaries on Talaria TWO based EVB-A boards and running the Sample Applications / verifying the expected outputs using the Debug Console and AWS Web Console.**

### Folder Structure of the 'talaria_two_aws' repository
The repo `talaria_two_aws` has the below directories/files:

- directory `aws-iot-device-sdk-embedded-C`- Contains the AWS IoT Device SDK Embedded-C Release Tag v3.1.5.
- directory `patches` - Contains patch file `t2_compatibility.patch` for AWS IoT Device SDK V3.1.5 for Talaria TWO compatibility.
- directory `talaria_two_pal`- Its ‘Platform Adaptation Layer’ and contains Talaria TWO Platform specific porting needed to adapt to AWS IoT SDK. It contains PAL for 'inno_os' and 'freertos' based SDKs.
- directory `sample_apps`- Samples provided by the AWS IoT SDK covering Thing Shadow, Jobs and Subscribe/Publish which are ported to Talaria TWO. Changes done for porting the sample Apps are related to APIs used to connect to the network, passing connection params as boot arguments and using dataFS for storing the certs and keys. A sensor2cloud-aws app for INP301x EVB's onboard sensors is also available here.
- directory `data`: Provides the sample dataFS folder structure to be used while programming the AWS certs and keys to EVB-A for talaria_two_aws Sample Applications.
- file `Makefile`- Generates the Sample App executable binaries and aws iot sdk libraries, using AWS IoT SDK source files, Sample App source files and `<sdk_path>/apps/talaria_two_aws/sample_apps/<platform>/<application_folder>/src/aws_iot_config.h`.

## Overview of Sample Applications

<a name="sample-apps"></a>

Sample Applications ported to the Talaria TWO Platform can be found in the path `/talaria_two_aws/sample_apps/<platform>`.
A brief overview of these apps is provided in this section.

Follow Application Note provided with the Talaria TWO SDK at the path `<sdk_path>/apps/iot_aws` for further details on programming certs, keys and executable binaries on Talaria TWO based EVB-A boards and running the Sample Applications / verifying the expected outputs using the Debug Console and AWS Web Console.
 
### Subscribe-Publish Sample
This example takes the parameters from the aws_iot_config.h file and T2 boot arguments and establishes a connection to the AWS IoT MQTT Platform.
Then it subscribes and publishes to the topics provided as bootArgs subscribe_topic and publish_topic.

If these topic bootArgs are not provided, then it defaults to 'inno_test/ctrl' as the 'subscribe_topic' and 'inno_test/data' as the 'publish_topic'.

If all the certs/keys are correct, in the T2 Console you should see alternate QoS0 and QoS1 messages being published to 'publish_topic' by the application in a loop.

if 'publishCount' in code is given a non-zero value, then it defines the number of times the publish should happen. With 'publishCount' as 0, it keeps publishing in a loop.

AWS IoT Console->Test page can be used to subscribe to 'inno_test/data' (or T2's 'publish_topic' provided as the bootArg to App) to observe the messages published by the App.

AWS IoT Console->Test page can be used to publish the message to 'inno_test/ctrl'(or T2's 'subscribe_topic' provided as the bootArg to App), and T2 App will receive the
messages and they will be seen on T2 Console.

JSON formatted text as shown below should be used for publishing to T2.
```
 {
 "from": "AWS IoT console"
 "to": "T2"
 "msg": "Hello from AWS IoT console"
 }
```

The application takes in the ssid, passphrase, aws host name, aws port and thing name (as client-id) as must provide bootArgs and publish_topic, subscribe_topic and suspend as optional bootArgs.

Certs and keys are stored in dataFS and read from app specific paths defined in the sample code.

### Shadow Sample

The goal of this sample application is to demonstrate the capabilities of aws iot thing shadow service.
This example takes the parameters from the aws_iot_config.h file and T2 boot arguments and establishes a connection to the AWS IoT Shadow Service.

This device acts as 'Connected Window' and periodically reports (once per 3 seconds) the following parameters to the Thing's Classic Shadow :
 1. temperature of the room (double).( Note - temperature changes are 'simulated'.)
 2. open/close status of the window(bool). (open or close as "windowOpen" true/false.)

The device also 'listens' for a shadow state change for "windowOpen" to act on commands from the cloud.

Two variables from a device's perspective are, double temperature and bool windowOpen.
So, the corresponding 'Shadow Json Document' in the cloud would be --
```
 {
   "reported": {
     "temperature": 32,
     "windowOpen": false
   },
   "desired": {
     "windowOpen": false
   }
 }
```
 
The device opens or closes the window based on json object "windowOpen" data [true/false] received as part of shadow 'delta' callback.
So a jsonStruct_t object 'windowActuator' is created with 'pKey = "windowOpen"' of 'type = SHADOW_JSON_BOOL' and a delta callback 'windowActuate_Callback'.

The App then registers for a 'Delta callback' for 'windowActuator' and receives a callback whenever a state change happens for this object in Thing Shadow.

(For example : Based on temperature reported, a logic running in the aws cloud infra can automatically decide when to open or close the window, and control it
by changing the 'desired' state of "windowOpen". OR a manual input by the end-user using a web app/ phone app can change the 'desired' state of "windowOpen".)

for the Sample App, change in desired section can be done manually as shown below :

Assume the reported and desired states of "windowOpen" are 'false' as shown in above JSON.
From AWS IoT Web Console's Thing Shadow, if the 'desired' section is edited /saved as shown below
```
   "desired": {
     "windowOpen": false
   }
```
then a delta callback will be received by the App, as now there is a difference between desired vs reported.

Received Delta message
```
   "delta": {
     "windowOpen": true
   }
```

This delta message means the desired windowOpen variable has changed to "true".

The application will 'act' on this delta message and publish back windowOpen as true as part of the 'reported' section of the shadow document from the device,
when the next periodic temperature value is reported.
```
   "reported": {
     "temperature": 28,
     "windowOpen": true
   }
```

This 'update reported' message will remove the 'delta' that was created, as now the desired and reported states will match.
If this delta message is not removed, then the AWS IoT Thing Shadow is always going to have a 'delta', and will keep sending delta calllback
anytime an update is applied to the Shadow.

Please Note - Ensure the buffer sizes in aws_iot_config.h are big enough to receive the delta message.
The delta message will also contain the metadata with the timestamps.

The application takes in ssid, passphrase, aws host name, aws port and thing name as must provide bootArgs and suspend as optional bootArgs.

Certs and keys are stored in dataFS and read from app specific paths defined in the sample code.

### Jobs Sample
This example takes the parameters from the aws_iot_config.h file and T2 boot arguments and establishes a connection to the AWS IoT MQTT Platform.
It performs several operations to demonstrate the basic capabilities of the AWS IoT Jobs platform.

If all the certs/keys are correct, you should see the list of pending Job Executions printed out by the iot_get_pending_callback_handler.
If there are any existing pending job executions, each will be processed one at a time in the iot_next_job_callback_handler.

After all of the pending jobs have been processed, the program will wait for notifications for new pending jobs and process them one at a time as they come in.

In the main body you can see how each callback is registered for each corresponding Jobs topic.

The application takes in ssid, passphrase, aws host name, aws port and thing name as must provide bootArgs and suspend as optional bootArgs.

Certs and keys are stored in dataFS and read from app specific paths defined in the sample code.

### sensor2cloud-aws for sensors available onboard in INP301x EVBA
This app is a reference example for sensor2cloud-aws usecase.
This app is similar to 'Shadow Sample' app, uses same boot-args and uses data from sensors available onboard in INP301x EVBA instead of simulated data.
Boot-args are also similar to 'Shadow Sample' with one more boot-arg added, named 'sensor_poll_interval'

Below are the shadow attributes used by this application --

    temperature
    pressure
    humidity
    opticalPower
    sensorPollInterval
    sensorSwitch

Sensor's values are read periodically every 'sensorPollInterval' seconds and sent to AWS IoT Thing Shadow associated with the thing_name passed in boot-arg, if 'sensorSwitch' is ON.
If 'sensorSwitch' is OFF, no values are sent but the app waits for incoming delta callbacks for . 'sensorSwitch' and 'sensorPollInterval'.

On boot, 'sensorSwitch' is forced to be ON ('true') and 'sensorPollInterval' is forced to be whatever value is passed using boot-arg 'sensor_poll_interval' (in seconds).
Later this can be controlled by changing these attributes values in cloud and it takes effect on T2 running via shadow delta callbacks.


## Releases

<a name="releases"></a>

New features and bug fixes will be offered by both the SDKs (Talaria TWO SDK and AWS IoT Device SDK Embedded C).

When a new SDK for Talaria TWO is released, a release from this Repo will be made to support that.

Also, when a new version is ported from AWS IoT Device SDK Embedded C, a release from this Repo will be made to support that.

Releases made from this Repo will be 'tagged-releases' and each release-tag will have the relevant info about respective Talaria TWO SDK version and AWS IoT Device SDK Embedded C version supported by that particular release from this Repo.

### For Example

Tag "v1.0.0_TalariaTWO_SDK_2.4" has the following description --
```
builds with - 'Talaria TWO SDK 2.4'

based on - AWS IoT Device SDK Embedded C - release tag version v.3.1.5
```
and

Tag "v1.1.0_TalariaTWO_SDK_2.5" has the following description --
```
builds with - 'Talaria TWO SDK 2.5'

based on - AWS IoT Device SDK Embedded C - release tag version v.3.1.5
```
The versioning `vx.y.z_TalariaTWO_SDK_m.n.o` follows semantic versioning, vx.y.z. or major.minor.patch.

Supported Talaria TWO SDK version is added with a `_TalariaTWO_SDK_m.n.o` to `vx.y.z`.

A port to a newer version from AWS IoT Device SDK Embedded C will bump the major version `x` and reset the minor version `y` and patch version `z` to 0.

A new TalariaTWO SDK Release support will bump the minor version `y` and reset the patch version `z` to 0, while the major version `x` remains the same.

A critical bug fix will bump the patch version `z` only.

