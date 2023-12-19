ROOT_LOC=../../../../..
-include ${ROOT_LOC}/embedded_apps.mak
SDK_DIR ?= $(ROOT_LOC)
include $(ROOT_LOC)/build.mak
include $(SDK_DIR)/conf/sdk.mak

targets= $(addprefix $(objdir)/,$(apps))
create_aws_iot_libs= create_lib_folder $(addprefix $(objdir)/,$(aws_iot_libs))
$(targets) : $(COMMON_FILES)

all:
	$(MAKE) libcomponents
	$(MAKE) $(create_aws_iot_libs)
	$(MAKE) $(targets)

lib_aws_iot_sdk_t2_path=lib/aws_iot_sdk_t2/
lib_aws_iot_sdk_t2_pal_path=lib/aws_iot_sdk_t2_pal/

aws_iot_libs = ${lib_aws_iot_sdk_t2_path}/libaws_iot_sdk_t2.a \
				${lib_aws_iot_sdk_t2_pal_path}/libaws_iot_sdk_t2_pal.a

create_lib_folder:
	mkdir -p $(objdir)/${lib_aws_iot_sdk_t2_path}
	mkdir -p $(objdir)/${lib_aws_iot_sdk_t2_pal_path}

T2AWS_LOC = $(realpath $(ROOT_LOC))/apps/talaria_two_aws

aws_iot_core_include=$(T2AWS_LOC)/aws-iot-device-sdk-embedded-C/include/
aws_iot_core_src=$(T2AWS_LOC)/aws-iot-device-sdk-embedded-C/src/
aws_iot_external_libs=$(T2AWS_LOC)/aws-iot-device-sdk-embedded-C/external_libs/
aws_iot_sdk_t2_pal=$(T2AWS_LOC)/talaria_two_pal/inno_os/

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

# Reference -- add the application's .elf and .elf.strip targets here
apps = \
    subscribe_publish_sample.elf \
    subscribe_publish_sample.elf.strip \

# Reference -- add the application's code paths here
app_src=$(T2AWS_LOC)/sample_apps/inno_os/subscribe_publish_sample/src/

# Reference -- add the application's include paths here

# Reference -- include the path of 'aws_iot_config.h' to be used by the application
CFLAGS += -I${app_src}

# Reference -- linker directives
LDFLAGS += -L$(objdir)/${lib_aws_iot_sdk_t2_path}
LDFLAGS += -L$(objdir)/${lib_aws_iot_sdk_t2_pal_path}
LDFLAGS += --no-gc-sections

# Reference -- add the application's source codes here

#subscribe_publish_sample app code
subscribe_publish_sample_obj = \
    ${app_src}subscribe_publish_sample.o

subscribe_publish_sample-virt = yes

# Reference -- add the libraries used by the Application, including aws_iot libraries created
$(objdir)/subscribe_publish_sample.elf:LIBS = \
    -lcomponents -laws_iot_sdk_t2 -laws_iot_sdk_t2_pal -lmbedtls -lwifi -llwip2 -limath -linnobase -ldragonfly
$(objdir)/subscribe_publish_sample.elf: $(addprefix $(objdir)/,${subscribe_publish_sample_obj})

#--------------------------------------------------------------------------------------------------------------------------------------------#

clean:
	rm -rf $(objdir)

-include ${DEPS}
