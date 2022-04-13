ROOT_LOC=../..
-include ${ROOT_LOC}/embedded_apps.mak
SDK_DIR ?= $(ROOT_LOC)
include $(ROOT_LOC)/build.mak
include $(SDK_DIR)/conf/sdk.mak

lib_aws_iot_sdk_t2_path=lib/aws_iot_sdk_t2/
lib_aws_iot_sdk_t2_pal_path=lib/aws_iot_sdk_t2_pal/

aws_iot_core_include=aws-iot-device-sdk-embedded-C/include/
aws_iot_core_src=aws-iot-device-sdk-embedded-C/src/
aws_iot_external_libs=aws-iot-device-sdk-embedded-C/external_libs/
aws_iot_sdk_t2_pal=talaria_two_pal/

# Logging level control
#LOG_FLAGS += -DENABLE_IOT_DEBUG
#LOG_FLAGS += -DENABLE_IOT_TRACE
#LOG_FLAGS += -DENABLE_IOT_INFO
LOG_FLAGS += -DENABLE_IOT_WARN
LOG_FLAGS += -DENABLE_IOT_ERROR
CFLAGS += $(LOG_FLAGS)

CPPFLAGS += -I${aws_iot_core_include} -I${aws_iot_sdk_t2_pal}include -Isamples
CPPFLAGS += -D_ENABLE_THREAD_SUPPORT_
LDFLAGS += -L$(objdir)/${lib_aws_iot_sdk_t2_path}
LDFLAGS += -L$(objdir)/${lib_aws_iot_sdk_t2_pal_path}
LDFLAGS += --no-gc-sections

apps = subscribe_publish_sample.elf jobs_sample.elf shadow_sample.elf subscribe_publish_sample.elf.strip \
	   jobs_sample.elf.strip shadow_sample.elf.strip

targets=$(addprefix $(objdir)/,$(apps))
all: libcomponents $(objdir)/${lib_aws_iot_sdk_t2_path}/libaws_iot_sdk_t2.a \
        $(objdir)/${lib_aws_iot_sdk_t2_pal_path}/libaws_iot_sdk_t2_pal.a  $(targets)
$(targets) : $(COMMON_FILES)

#aws iot code
aws_iot_core =  ${aws_iot_core_src}aws_iot_shadow_json.o \
                ${aws_iot_core_src}aws_iot_shadow.o \
                ${aws_iot_core_src}aws_iot_json_utils.o \
                ${aws_iot_core_src}aws_iot_jobs_types.o \
                ${aws_iot_core_src}aws_iot_jobs_topics.o \
                ${aws_iot_core_src}aws_iot_jobs_json.o\
                ${aws_iot_core_src}aws_iot_jobs_interface.o \
                ${aws_iot_core_src}aws_iot_shadow_records.o\
                ${aws_iot_core_src}aws_iot_shadow_actions.o \
                ${aws_iot_core_src}aws_iot_mqtt_client_yield.o \
                ${aws_iot_core_src}aws_iot_mqtt_client_unsubscribe.o \
                ${aws_iot_core_src}aws_iot_mqtt_client_subscribe.o \
                ${aws_iot_core_src}aws_iot_mqtt_client_publish.o  \
                ${aws_iot_core_src}aws_iot_mqtt_client_connect.o \
                ${aws_iot_core_src}aws_iot_mqtt_client_common_internal.o   \
                ${aws_iot_core_src}aws_iot_mqtt_client.o\
                ${aws_iot_core_src}aws_iot_json_utils.o   

#aws iot external support libs code
aws_iot_external =  ${aws_iot_external_libs}/jsmn/jsmn.o
CPPFLAGS +=-I${aws_iot_external_libs}/jsmn/

#t2 platform adaptation layer implementation code
aws_iot_t2_pal =    ${aws_iot_sdk_t2_pal}t2_thread.o \
                    ${aws_iot_sdk_t2_pal}t2_time.o \
                    ${aws_iot_sdk_t2_pal}t2_network_mbedtls_wrapper.o

$(objdir)/${lib_aws_iot_sdk_t2_path}/libaws_iot_sdk_t2.a: $(addprefix $(objdir)/,${aws_iot_core}) \
	$(addprefix $(objdir)/,${aws_iot_external})
	mkdir -p $(objdir)/${lib_aws_iot_sdk_t2_path}
	${CROSS_COMPILE}ar rcs $@ $^

$(objdir)/${lib_aws_iot_sdk_t2_pal_path}/libaws_iot_sdk_t2_pal.a: $(addprefix $(objdir)/,${aws_iot_t2_pal})
	mkdir -p $(objdir)/${lib_aws_iot_sdk_t2_pal_path}
	${CROSS_COMPILE}ar rcs $@ $^

subscribe_publish_sample-virt = yes
subscribe_publish_sample_obj  = samples/subscribe_publish_sample/subscribe_publish_sample.o
$(objdir)/subscribe_publish_sample.elf:LIBS = -lcomponents -laws_iot_sdk_t2 -laws_iot_sdk_t2_pal -lmbedtls -lwifi -llwip2 -limath -linnobase -ldragonfly
$(objdir)/subscribe_publish_sample.elf: $(addprefix $(objdir)/,${subscribe_publish_sample_obj})

jobs_sample-virt = yes
jobs_sample_obj  = samples/jobs_sample/jobs_sample.o
$(objdir)/jobs_sample.elf:LIBS = -lcomponents -laws_iot_sdk_t2 -laws_iot_sdk_t2_pal -lmbedtls -lwifi -llwip2 -limath -linnobase -ldragonfly
$(objdir)/jobs_sample.elf: $(addprefix $(objdir)/,${jobs_sample_obj})

shadow_sample-virt = yes
shadow_sample_obj  = samples/shadow_sample/shadow_sample.o
$(objdir)/shadow_sample.elf:LIBS = -lcomponents -laws_iot_sdk_t2 -laws_iot_sdk_t2_pal -lmbedtls -lwifi -llwip2 -limath -linnobase -ldragonfly
$(objdir)/shadow_sample.elf: $(addprefix $(objdir)/,${shadow_sample_obj})

clean:
	rm -rf $(objdir)

-include ${DEPS}

