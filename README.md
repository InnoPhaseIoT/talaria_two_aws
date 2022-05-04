# AWS IoT Device SDK Embedded C on InnoPhase Talaria TWO Platform

## Table of Contents

- [Introduction](#introduction)
- [Getting Started with the SDK](#get-started)
- [Overview of Sample Applications](#sample-apps)

## Introduction

<a name="introduction"></a>

[AWS IoT Device SDK Embedded-C Release Tag v3.1.5](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v3.1.5) is ported on Talaria TWO Software Development Kit as per the porting guidelines.

Using this, the users can now start developing exciting [ultra-low power IoT solutions on Talaria TWO family of devices](https://innophaseinc.com/talaria-technology-details/), utilizing the power of the AWS IoT Core platform and its services.

More information on aws-iot-device-sdk-embedded-C release tag v3.1.5 can be found here:
[https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v3.1.5](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v3.1.5)

API Documentation and other details specific to AWS IoT Device SDK Embedded C release tag V3.1.5 can be found here:
[http://aws-iot-device-sdk-embedded-c-docs.s3-website-us-east-1.amazonaws.com/index.html](http://aws-iot-device-sdk-embedded-c-docs.s3-website-us-east-1.amazonaws.com/index.html)

## Getting Started with the SDK

<a name="get-started"></a>

### Cloning the 'talaria_two_aws' repository
- Create a new folder in any place and clone the 'talaria_two_aws' repo using below command
``` bash
git clone --recursive https://github.com/InnoPhaseInc/talaria_two_aws.git
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

- On running make, the binaries for the Sample Apps will be created in the path `<sdk_path>/apps/talaria_two_aws/out`.

- All the 'Sample Applications' use a common 'aws iot config file' at the path `<sdk_path>/apps/talaria_two_aws/samples/aws_iot_config.h`.

- Other applications can have their own custom configurations based on their own needs, and the customized 'aws_iot_config.h' file can be included by individual apps at the time of compiling the AWS IoT SDK.

### Programming the Dev-Kits
**Follow Application Note provided with the Talaria TWO SDK at the path `<sdk_path>/apps/iot_aws` for further details on programming certs, keys and executable binaries on Talaria TWO based EVB-A boards and running the Sample Applications / verifying the expected outputs using the Debug Console and AWS Web Console.**

### Folder Structure of the 'talaria_two_aws' repository
The repo `talaria_two_aws` has the below directories/files:

- directory `aws-iot-device-sdk-embedded-C`- Contains the AWS IoT Device SDK Embedded-C Release Tag v3.1.5.
- directory `patches` - Contains patch file `t2_compatibility.patch` for AWS IoT Device SDK V3.1.5 for Talaria TWO compatibility.
- directory `talaria_two_pal`- Its ‘Platform Adaptation Layer’ and contains Talaria TWO Platform specific porting needed to adapt to AWS IoT SDK.
- directory `samples`- Samples provided by the AWS IoT SDK covering Thing Shadow, Jobs and Subscribe/Publish which are ported to Talaria TWO. Changes done for porting the sample Apps are related to APIs used to connect to the network, passing connection params as boot arguments and using rootFS for storing the certs and keys.
- directory `root`: Provides the sample rootFS folder structure to be used while programming the AWS certs and keys to EVB-A for talaria_two_aws Sample Application.
- file `Makefile`- Takes care of making the Sample App executable binaries and aws iot sdk libraries using AWS IoT SDK source files, Sample App source files and `<sdk_path>/apps/talaria_two_aws/samples/aws_iot_config.h`

## Overview of Sample Applications

<a name="sample-apps"></a>

Sample Applications ported to the Talaria TWO Platform can be found in the path `/talaria_two_aws/samples`.
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

Certs and keys are stored in rootFS and read from app specific paths defined in the sample code.

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

Certs and keys are stored in rootFS and read from app specific paths defined in the sample code.

### Jobs Sample
This example takes the parameters from the aws_iot_config.h file and T2 boot arguments and establishes a connection to the AWS IoT MQTT Platform.
It performs several operations to demonstrate the basic capabilities of the AWS IoT Jobs platform.

If all the certs/keys are correct, you should see the list of pending Job Executions printed out by the iot_get_pending_callback_handler.
If there are any existing pending job executions, each will be processed one at a time in the iot_next_job_callback_handler.

After all of the pending jobs have been processed, the program will wait for notifications for new pending jobs and process them one at a time as they come in.

In the main body you can see how each callback is registered for each corresponding Jobs topic.

The application takes in ssid, passphrase, aws host name, aws port and thing name as must provide bootArgs and suspend as optional bootArgs.

Certs and keys are stored in rootFS and read from app specific paths defined in the sample code.

