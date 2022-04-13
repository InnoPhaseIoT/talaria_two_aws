
#!/bin/bash

PATCH_FILE_1_PATH=patches/t2_compatibility.patch
PATCH_TARGET_1_PATH=aws-iot-device-sdk-embedded-C

ROOT_PATH="$PWD"
echo $ROOT_PATH

echo "...patching $ROOT_PATH/$PATCH_TARGET_1_PATH"
cd $ROOT_PATH/$PATCH_TARGET_1_PATH
git apply --whitespace=nowarn $ROOT_PATH/$PATCH_FILE_1_PATH

echo "...patching completed"
