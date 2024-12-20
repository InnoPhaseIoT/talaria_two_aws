/**
  *****************************************************************************
  * @file   sensor2cloud-aws_demo_app_INP301x.h
  * 
  *****************************************************************************
  * @attention
  *
  *
  *  Copyright (c) 2022, InnoPhase, Inc.
  *
  *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  *  POSSIBILITY OF SUCH DAMAGE.
  *
  *****************************************************************************
  */

#ifndef SENSOR2CLOUD_AWS_INP301x_H
#define SENSOR2CLOUD_AWS_INP301x_H

#define INPUT_PARAMETER_AWS_URL "aws_host"
#define INPUT_PARAMETER_AWS_PORT "aws_port"
#define INPUT_PARAMETER_AWS_THING_NAME "thing_name"

#define AWS_IOT_MY_THING_NAME os_get_boot_arg_str(INPUT_PARAMETER_AWS_THING_NAME)

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 256

#define AWS_IOT_SHADOW_ACTION_ACK_TIMEOUT_IN_SEC 10

#define SDA_PIN (3)                     /* I2C data pin */
#define SCL_PIN (4)                     /* I2C clock pin */

typedef enum 
{
    AWS_SHADOW_ATTRIBUTE_TEMPERATURE,
    AWS_SHADOW_ATTRIBUTE_PRESSURE,
    AWS_SHADOW_ATTRIBUTE_OPTICAL_POWER,
    AWS_SHADOW_ATTRIBUTE_HUMIDITY,
    AWS_SHADOW_ATTRIBUTE_SENSOR_POLL_INTERVAL,
    AWS_SHADOW_ATTRIBUTE_SENSOR_SWITCH,
    AWS_SHADOW_ATTRIBUTES_MAX_COUNT,
} e_inp301x_aws_shadow_attributes_t;


typedef struct 
{
    float       temperature;
    float       pressure;
    float       humidity;
    float       opticalPower;
    uint32_t    sensorPollInterval;
    bool        sensorOn;
} inp301x_aws_shadow_params_t;

enum shadow_update_type
{
    AWS_SHADOW_UPDATE_DESIRED,
    AWS_SHADOW_UPDATE_REPORTED,    
};
#endif
