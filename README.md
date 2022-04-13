# talaria_two_aws


## Introduction

[AWS IoT Device SDK Embedded-C Release Tag v3.1.5](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v3.1.5) is ported on Talaria TWO Software Development Kit as per the porting guidelines.

Using this, the users can now start developing exciting [ultra-low power IoT solutions on Talaria TWO family of devices](https://innophaseinc.com/talaria-technology-details/), utilizing the power of the AWS IoT Core platform and it's services.

Sample Application codes covering Thing Shadow, Jobs and Subscribe/Publish are provided.

## Getting Started

- Create a new folder in any place and clone the 'talaria_two_aws' repo using below command
    ```
    git clone --recursive git@github.com:InnoPhaseInc/talaria_two_aws.git
    ```
> This repo uses [Git Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) for it's dependencies. The option '--recursive' is required to clone git submodule repos needed by 'talaria_two_aws' repo.


- Once the clone is complete, move the folder 'talaria_two_aws' to the path `<sdk_path>/apps/`.

- Then go to the directory 'talaria_two_aws' and run the below script to patch for t2 compatibility. This needs to be done only once, after the clone is successful.
``` bash
<sdk_path>/apps/talaria_two_aws$ sh apply_t2_compatibility_patch.sh
```

- Once the above command is run successfully, running Make from here will create binaries in the path 'talaria_two_aws/out'

- Follow Application Note with `<sdk_path>/apps/iot_aws` for more details on running the Sample Applications.

