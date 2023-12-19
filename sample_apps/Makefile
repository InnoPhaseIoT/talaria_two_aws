
PLATFORM_TYPE := $(shell cat ../../../build.mak | grep "INP_FREERTOS_SDK")
#$(info PLATFORM_TYPE=$(PLATFORM_TYPE))

all:
ifeq ($(PLATFORM_TYPE),BUILD_TYPE_FLAG = -DINP_FREERTOS_SDK)
	echo "$(PLATFORM_TYPE) building freertos sample apps..."
	cd freertos/                              && $(MAKE) all
else
	echo "$(PLATFORM_TYPE) building inno_os sample apps..."
	cd inno_os/                               && $(MAKE) all
endif

clean:
ifeq ($(PLATFORM_TYPE),BUILD_TYPE_FLAG = -DINP_FREERTOS_SDK)
	echo "$(PLATFORM_TYPE) cleaning freertos sample apps..."
	cd freertos/                              && $(MAKE) clean
else
	echo "$(PLATFORM_TYPE) cleaning inno_os sample apps..."
	cd inno_os/                               && $(MAKE) clean
endif

