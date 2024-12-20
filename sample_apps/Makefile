
PLATFORM_TYPE := $(shell cat ../../../build.mak | grep "SDK_THREE_DOT_X")
#$(info PLATFORM_TYPE=$(PLATFORM_TYPE))

all:
ifeq ($(PLATFORM_TYPE),BUILD_TYPE_FLAG = -DSDK_THREE_DOT_X)
	echo "$(PLATFORM_TYPE) building sample apps for sdk_3.x..."
	cd sdk_3.x/                              && $(MAKE) all
else
	echo "$(PLATFORM_TYPE) building sample apps for sdk_2.x..."
	cd sdk_2.x/                               && $(MAKE) all
endif

clean:
ifeq ($(PLATFORM_TYPE),BUILD_TYPE_FLAG = -DSDK_THREE_DOT_X)
	echo "$(PLATFORM_TYPE) cleaning sample apps for sdk_3.x..."
	cd sdk_3.x/                              && $(MAKE) clean
else
	echo "$(PLATFORM_TYPE) cleaning sample apps for sdk_2.x..."
	cd sdk_2.x/                               && $(MAKE) clean
endif

