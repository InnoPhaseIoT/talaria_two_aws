
TOP=../../../../..
TOP := $(abspath $(TOP))
-include $(TOP)/embedded_apps.mak
SDK_DIR ?= $(TOP)
ROOT_DIR := $(SDK_DIR)

# basic rules and variables
include $(ROOT_DIR)/build/rules.mak
include $(TOP)/build.mak

# enable below line to minimize compilation prints
#VERBOSE ?= 0

# rules.mak uses default libs.
# if app needs custom libs, define it here.
# example:
LIBS = -lwifi -lrfdrv -llwip2 -lmbedtls -lsupplicant -ldragonfly -linnos
LIBS += -laws_iot_sdk_t2 -laws_iot_sdk_t2_pal -lcomponents

# to enable fast build using `ccache`, refer to
# variable `FAST` in rules.mak

# by default, `firmware-arm-virt.lds` is used.
# for small size elfs, use the `firmware-arm-ram.lds` by enabling below line
#LDFILE := $(BUILD_DIR)/firmware-arm-ram.lds

all:
	$(MAKE) libcomponents
	$(MAKE) $(create_aws_iot_libs)
	$(MAKE) $(TARGETS)

create_aws_iot_libs= create_lib_folder $(addprefix $(objdir)/,$(aws_iot_libs))

lib_aws_iot_sdk_t2_path=lib/aws_iot_sdk_t2/
lib_aws_iot_sdk_t2_pal_path=lib/aws_iot_sdk_t2_pal/

aws_iot_libs = ${lib_aws_iot_sdk_t2_path}/libaws_iot_sdk_t2.a \
				${lib_aws_iot_sdk_t2_pal_path}/libaws_iot_sdk_t2_pal.a

create_lib_folder:
	mkdir -p $(objdir)/${lib_aws_iot_sdk_t2_path}
	mkdir -p $(objdir)/${lib_aws_iot_sdk_t2_pal_path}
	#echo "==== T2AWS_LOC --> $(T2AWS_LOC) ===="

T2AWS_LOC = $(TOP)/apps/talaria_two_aws

aws_iot_core_include=$(T2AWS_LOC)/aws-iot-device-sdk-embedded-C/include/
aws_iot_core_src=$(T2AWS_LOC)/aws-iot-device-sdk-embedded-C/src/
aws_iot_external_libs=$(T2AWS_LOC)/aws-iot-device-sdk-embedded-C/external_libs/
aws_iot_sdk_t2_pal=$(T2AWS_LOC)/talaria_two_pal/sdk_3.x/

# define custom CFLAGS here.

# Logging level control
#LOG_FLAGS += -DENABLE_IOT_DEBUG
#LOG_FLAGS += -DENABLE_IOT_TRACE
#LOG_FLAGS += -DENABLE_IOT_INFO
LOG_FLAGS += -DENABLE_IOT_WARN
LOG_FLAGS += -DENABLE_IOT_ERROR
CFLAGS += $(LOG_FLAGS)

CFLAGS += -I${aws_iot_core_include} -I${aws_iot_sdk_t2_pal}include \
			-I${aws_iot_external_libs}/jsmn
CPPFLAGS += -D_ENABLE_THREAD_SUPPORT_


#aws iot core code
aws_iot_core = \
	${aws_iot_core_src}aws_iot_shadow_json.o \
	${aws_iot_core_src}aws_iot_shadow.o \
	${aws_iot_core_src}aws_iot_json_utils.o \
	${aws_iot_core_src}aws_iot_jobs_types.o \
	${aws_iot_core_src}aws_iot_jobs_topics.o \
	${aws_iot_core_src}aws_iot_jobs_json.o \
	${aws_iot_core_src}aws_iot_jobs_interface.o \
	${aws_iot_core_src}aws_iot_shadow_records.o \
	${aws_iot_core_src}aws_iot_shadow_actions.o \
	${aws_iot_core_src}aws_iot_mqtt_client_yield.o \
	${aws_iot_core_src}aws_iot_mqtt_client_unsubscribe.o \
	${aws_iot_core_src}aws_iot_mqtt_client_subscribe.o \
	${aws_iot_core_src}aws_iot_mqtt_client_publish.o \
	${aws_iot_core_src}aws_iot_mqtt_client_connect.o \
	${aws_iot_core_src}aws_iot_mqtt_client_common_internal.o \
	${aws_iot_core_src}aws_iot_mqtt_client.o \
	${aws_iot_core_src}aws_iot_json_utils.o  

#aws iot external support libs code
aws_iot_external = \
	${aws_iot_external_libs}/jsmn/jsmn.o

#t2 'platform adaptation layer' implementation code
aws_iot_t2_pal = \
	${aws_iot_sdk_t2_pal}t2_thread.o \
	${aws_iot_sdk_t2_pal}t2_time.o \
	${aws_iot_sdk_t2_pal}t2_network_mbedtls_wrapper.o

libaws_iot_sdk_t2_OBJS := $(addprefix $(objdir)/,${aws_iot_core}) $(addprefix $(objdir)/,${aws_iot_external})
$(objdir)/${lib_aws_iot_sdk_t2_path}/libaws_iot_sdk_t2.a: $(libaws_iot_sdk_t2_OBJS)
$(objdir)/${lib_aws_iot_sdk_t2_path}/libaws_iot_sdk_t2.a: lib_objs = $(libaws_iot_sdk_t2_OBJS)

libaws_iot_sdk_t2_pal_OBJS := $(addprefix $(objdir)/,${aws_iot_t2_pal})
$(objdir)/${lib_aws_iot_sdk_t2_pal_path}/libaws_iot_sdk_t2_pal.a: $(libaws_iot_sdk_t2_pal_OBJS)
$(objdir)/${lib_aws_iot_sdk_t2_pal_path}/libaws_iot_sdk_t2_pal.a: lib_objs = $(libaws_iot_sdk_t2_pal_OBJS)


#------------------------------------------------------Application-Specific Section----------------------------------------------------------#

# Reference -- add the application's .elf targets here
APP := shadow_sample.elf

# Reference -- add the application's code paths here
app_src=$(T2AWS_LOC)/sample_apps/sdk_3.x/shadow_sample/src

# Reference -- add the application's include paths here

# Reference -- include the path of 'aws_iot_config.h' to be used by the application
CFLAGS += -I${app_src}

# Reference -- linker directives
LDFLAGS += -L$(objdir)/${lib_aws_iot_sdk_t2_path}
LDFLAGS += -L$(objdir)/${lib_aws_iot_sdk_t2_pal_path}
LDFLAGS += --no-gc-sections

# Reference -- add the application's source codes here
#shadow_sample app code
SHADOW_SAMPLE_SRC_FILES += ${app_src}/shadow_sample.o
SHADOW_SAMPLE_OBJ_FILES := $(addprefix $(OUTDIR),$(SHADOW_SAMPLE_SRC_FILES:%.c=%.o))
$(OUTDIR)/$(APP)   :  $(SHADOW_SAMPLE_OBJ_FILES)

ALL_APPS := $(APP)
ALL_APPS := $(ALL_APPS) $(ALL_APPS:%.elf=%.elf.strip)
TARGETS  := $(addprefix $(OUTDIR)/,$(ALL_APPS))

#--------------------------------------------------------------------------------------------------------------------------------------------#
clean:
	-rm -rf $(OUTDIR)

include $(BUILD_DIR)/sdk.mak
