/**
  *****************************************************************************
  * @file   icp-101xx.c
  * 
  *****************************************************************************
  * @attention
  *
  *
  *  Copyright (c) 2021, InnoPhase, Inc.
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

#include <errno.h>
#include "callout_delay.h"
#include "icp-101xx.h"

static const uint8_t ICC_ADDR_PRS = 0x63;

static int read_otp_from_i2c(struct inv_invpres *s, int16_t *out);
static void init_base(struct inv_invpres *s, int16_t *otp);
static void wait_for_measurement(inv_invpres_mode_t mode);
static void calculate_conversion_constants(struct inv_invpres *s, float *p_Pa, float *p_LUT, float *out);


int inv_invpres_init(struct inv_invpres *s, struct i2c_bus *bus)
{
    int16_t otp[4];

    s->serif = i2c_create_device(bus, ICC_ADDR_PRS, I2C_CLK_400K);

    read_otp_from_i2c(s, otp);

    init_base(s, otp);

    return 0;
}

static int read_otp_from_i2c(struct inv_invpres *s, int16_t *out)
{
    uint8_t data_write[10];
    uint8_t data_read[10] = {0};
    int status;
    int i;

    // OTP Read mode
    data_write[0] = 0xC5;
    data_write[1] = 0x95;
    data_write[2] = 0x00;
    data_write[3] = 0x66;
    data_write[4] = 0x9C;

    status = inv_invpres_serif_write_reg(s, data_write, 5);
    if (status)
        return status;

    // Read OTP values
    for (i = 0; i < 4; i++) {
        data_write[0] = 0xC7;
        data_write[1] = 0xF7;
        status = inv_invpres_serif_write_reg(s, data_write, 2);
        if (status)
            return status;

        status = inv_invpres_serif_read_reg(s, data_read, 3);
        if (status)
            return status;

        out[i] = data_read[0]<<8 | data_read[1];
    }

    return 0;
}

static void init_base(struct inv_invpres *s, int16_t *otp)
{
    int i;

    for(i = 0; i < 4; i++)
        s->sensor_constants[i] = otp[i];

    s->p_Pa_calib[0] = 45000.0;
    s->p_Pa_calib[1] = 80000.0;
    s->p_Pa_calib[2] = 105000.0;
    s->LUT_lower = 3.5 * (1<<20);
    s->LUT_upper = 11.5 * (1<<20);
    s->quadr_factor = 1 / 16777216.0;
    s->offst_factor = 2048.0;
}

int inv_invpres_start_measurement(struct inv_invpres *s, inv_invpres_mode_t mode)
{
    uint8_t command[2];

    /*
     * Command is sent in Big-Endian format.
     * The sensor will transmit pressure first.
     */
    switch(mode)
    {
    case LOW_POWER:
        command[0] = 0x40;
        command[1] = 0x1A;
        break;

    case NORMAL:
        command[0] = 0x48;
        command[1] = 0xA3;
        break;

    case LOW_NOISE:
        command[0] = 0x50;
        command[1] = 0x59;
        break;

    case ULTRA_LOW_NOISE:
        command[0] = 0x58;
        command[1] = 0xE0;
        break;

    default:
        return -EINVAL;
    }

    return inv_invpres_serif_write_reg(s, command, 2);
}

int inv_invpres_get_data_raw(struct inv_invpres *s, int32_t *p_raw, int16_t *t_raw)
{
    uint8_t data[9];

    /* Sensor does not respond until measurement is complete */
    while(inv_invpres_serif_read_reg(s, data, sizeof(data)) == -ENXIO);

    /*
     * data[0]  pressure MMSB
     * data[1]  pressure MLSB
     * data[2]  checksum
     *
     * data[3]  pressure LMSB
     * data[4]  pressure LLSB (unused)
     * data[5]  checksum
     *
     * data[6]  temp MSB
     * data[7]  temp LSB
     * data[8]  checksum
     */
    if(p_raw)
        *p_raw = data[0] << 16 | data[1] << 8 | data[3];

    if(t_raw)
        *t_raw = data[6] << 8 | data[7];

    return 0;
}

int inv_invpres_get_data(struct inv_invpres *s, float *pressure, float *temperature)
{
    int ret = 0;
    int32_t p_raw;
    int16_t t_raw;

    if((ret = inv_invpres_get_data_raw(s, &p_raw, &t_raw)))
        return ret;

    return inv_invpres_process_data(s, p_raw, t_raw, pressure, temperature);
}

static void wait_for_measurement(inv_invpres_mode_t mode)
{
    uint32_t measurement_time_us = 0;

    switch(mode)
    {
    default:
    case LOW_POWER:
        measurement_time_us = 1600;
        break;
    case NORMAL:
        measurement_time_us = 5600;
        break;
    case LOW_NOISE:
        measurement_time_us = 20800;
        break;
    case ULTRA_LOW_NOISE:
        measurement_time_us = 83200;
        break;
    }

    // Wait for the measurement to complete
    // This is to avoid the busy-wait loop in inv_invpres_get_data_raw to save power
    callout_delay_us(measurement_time_us);
}

int inv_invpres_measure_blocking(struct inv_invpres *s, inv_invpres_mode_t mode, float *pressure, float *temperature)
{
    int ret = 0;

    if((ret = inv_invpres_start_measurement(s, mode)))
        return ret;

    wait_for_measurement(mode);

    if((ret = inv_invpres_get_data(s, pressure, temperature)))
        return ret;

    return 0;
}

int inv_invpres_measure_raw_blocking(struct inv_invpres *s, inv_invpres_mode_t mode, int32_t *p_raw, int16_t *t_raw)
{
    int ret = 0;

    if((ret = inv_invpres_start_measurement(s, mode)))
        return ret;

    wait_for_measurement(mode);

    if((ret = inv_invpres_get_data_raw(s, p_raw, t_raw)))
        return ret;

    return 0;
}

// p_LSB -- Raw pressure data from sensor
// T_LSB -- Raw temperature data from sensor
int inv_invpres_process_data(struct inv_invpres *s, int32_t p_LSB, int16_t T_LSB,
        float *pressure, float *temperature)
{
    float t;
    float s1,s2,s3;
    float in[3];
    float out[3];
    float A,B,C;

    t = (float)(T_LSB - 32768);
    s1 = s->LUT_lower + (float)(s->sensor_constants[0] * t * t) * s->quadr_factor;
    s2 = s->offst_factor * s->sensor_constants[3] + (float)(s->sensor_constants[1] * t * t) * s->quadr_factor;
    s3 = s->LUT_upper + (float)(s->sensor_constants[2] * t * t) * s->quadr_factor;
    in[0] = s1;
    in[1] = s2;
    in[2] = s3;

    calculate_conversion_constants(s, s->p_Pa_calib, in, out);
    A = out[0];
    B = out[1];
    C = out[2];

    *pressure = A + B / (C + p_LSB);
    *temperature = -45.f + 175.f/65536.f * T_LSB;

    return 0;
}

// p_Pa -- List of 3 values corresponding to applied pressure in Pa
// p_LUT -- List of 3 values corresponding to the measured p_LUT values at the applied pressures.
static void calculate_conversion_constants(struct inv_invpres *s, float *p_Pa,
        float *p_LUT, float *out)
{
    float A,B,C;

    C = (p_LUT[0] * p_LUT[1] * (p_Pa[0] - p_Pa[1]) +
            p_LUT[1] * p_LUT[2] * (p_Pa[1] - p_Pa[2]) +
            p_LUT[2] * p_LUT[0] * (p_Pa[2] - p_Pa[0])) /
                    (p_LUT[2] * (p_Pa[0] - p_Pa[1]) +
                            p_LUT[0] * (p_Pa[1] - p_Pa[2]) +
                            p_LUT[1] * (p_Pa[2] - p_Pa[0]));
    A = (p_Pa[0] * p_LUT[0] - p_Pa[1] * p_LUT[1] - (p_Pa[1] - p_Pa[0]) * C) / (p_LUT[0] - p_LUT[1]);
    B = (p_Pa[0] - A) * (p_LUT[0] + C);

    out[0] = A;
    out[1] = B;
    out[2] = C;
}

int inv_invpres_serif_write_reg(struct inv_invpres *s, uint8_t *data, uint16_t count)
{
    struct i2c_msg msg;

    if(!s || !s->serif)
        return -ENODEV;

    msg.im_len = count;
    msg.im_flags = I2C_M_STOP;
    msg.im_buf = data;

    return i2c_transfer(s->serif, &msg, 1);
}

int inv_invpres_serif_read_reg(struct inv_invpres *s, uint8_t *data, uint16_t count)
{
    struct i2c_msg msg;

    if(!s || !s->serif)
        return -ENODEV;

    msg.im_len = count;
    msg.im_flags = I2C_M_RD | I2C_M_STOP;
    msg.im_buf = data;

    return i2c_transfer(s->serif, &msg, 1);
}
