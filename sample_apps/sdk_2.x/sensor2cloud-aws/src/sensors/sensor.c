/**
  *****************************************************************************
  * @file   sensor.c
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

#include <inttypes.h>
#include <kernel/console.h>
#include <kernel/os.h>
#include <string.h>
#include "callout_delay/callout_delay.h"
#include "dtoa/dtoa.h"
#include "BMP388/bmp388.h"
#include "opt3002/opt3002.h"
#include "sensor.h"
#include "shtc1-4.1.0/shtc1.h"


#ifdef PRES_SEN_EN
static bmp388_t pres_sen;
#endif
#ifdef OPT_SEN_EN
static opt3002_t opt_sen;
#endif

struct bmp3_dev dev;

void init_sensors(struct i2c_bus *bus)
{
    if(!bus)
        return;

#ifdef PRES_SEN_EN
    // Initialize pressure sensor (icp-10100)
    os_printf("Initializing bmp388...\n");
  //  struct bmp3_dev dev;
    bmp388_init(&pres_sen,&dev,bus,0x76);
#endif

#ifdef OPT_SEN_EN
    // Initialize optical sensor (opt3002)
    os_printf("Initializing opt3002...\n");
    opt3002_init(&opt_sen, bus, 0x44);
#endif

#ifdef HUMID_SEN_EN
    // Initialize temperature / humidity sensor (shtc3)
    os_printf("Initializing shtc3...\n");
    sensirion_i2c_init(bus);
    // Probe enables or disables sleep in the driver based on product code,
    // and will put the device in sleep mode if supported
    shtc1_probe();
    // Enable low-power mode
    shtc1_enable_low_power_mode(1);
#else
    // If we're not using the shtc3, put it in sleep mode

    sensirion_i2c_init(bus);
    // Probe enables or disables sleep in the driver based on product code,
    // and will put the device in sleep mode if supported
    shtc1_probe();
    // Don't need to communicate with the device anymore
    sensirion_i2c_release();
#endif
}

void get_sensor_ids(sensor_id_t *ids)
{
    if(!ids)
        return;

#ifdef PRES_SEN_EN
    
    ids->bmp388_id = bmp3_get_device_ID(&dev);
    set_normal_mode(&dev);
#endif

#ifdef OPT_SEN_EN
    /* Read optical sensor ID */
    ids->opt3002_id = opt3002_readManufacturerID(&opt_sen);
#endif

#ifdef HUMID_SEN_EN
    ids->shtc3_serial = 0;

    /* Read temperature / humidity sensor ID (serial number) */
    shtc1_read_serial(&ids->shtc3_serial);
#endif
}

void get_sensor_calibration_data(sensor_calibration_t *calib)
{
    if(!calib)
        return;

#ifdef PRES_SEN_EN
 //   for(size_t i = 0; i < ARRAY_SIZE(calib->icp_sensor_constants); i++)
 //       calib->icp_sensor_constants[i] = pres_sen.sensor_constants[i];
#endif
}

void print_sensor_ids(sensor_id_t *ids)
{
    if(!ids)
        return;

#ifdef PRES_SEN_EN
    os_printf("bmp388 ID: 0x%" PRIX16 "\n", ids->bmp388_id);
#endif
#ifdef OPT_SEN_EN
    os_printf("opt3002 ID: 0x%" PRIX16 "\n", ids->opt3002_id);
#endif
#ifdef HUMID_SEN_EN
    os_printf("shtc3 ID: 0x%" PRIX32 "\n", ids->shtc3_serial);
#endif
}

void poll_sensors(sensor_reading_t *reading)
{
    if(!reading)
        return;

    // Record timestamp
    reading->timestamp = os_systime64();

#ifdef PRES_SEN_EN
    reading->pressure = 0;
    reading->temp_bmp = 0;

    /* Read pressure and temperature recorded by bmp388 */
  
	float *sensor_data;
	sensor_data = get_sensor_data(&dev);
	reading->temp_bmp = (sensor_data[0]/100);
	reading->pressure = (sensor_data[1]/100);
#endif

#ifdef OPT_SEN_EN
    opt3002_config_t opt_config_trigger = {
            .RangeNumber = 0xC,                 // Automatic full-scale mode
            .ConversionTime = 0,                // 100 ms conversion time
            .ModeOfConversionOperation = 0x1,   // Single-shot mode
            .Latch = 0x1                        // Latched operation
    };
    opt3002_config_t opt_config_read = {.rawData = 0};

    memset(&reading->light, 0, sizeof(reading->light));

    // Trigger reading
    opt3002_writeConfig(&opt_sen, opt_config_trigger);

    // Wait for reading to be complete
    callout_delay_ms(100);

    // Make sure reading is ready
    do
    {
        opt_config_read = opt3002_readConfig(&opt_sen);
    } while(!opt_config_read.ConversionReady);

    // Get result
    reading->light = opt3002_readResult(&opt_sen);
#endif

#ifdef HUMID_SEN_EN
    int32_t humidity_x1000 = 0, temp_shtc_x1000 = 0;

    /* Read humidity and temperature recorded by shtc3 */
    shtc1_measure_blocking_read(&temp_shtc_x1000, &humidity_x1000);

    reading->humidity = humidity_x1000 / 1000.0;
    reading->temp_shtc = temp_shtc_x1000 / 1000.0;
#endif
}

void poll_sensors_raw(raw_sensor_reading_t *raw_reading)
{
    if(!raw_reading)
        return;

    // Record timestamp
    raw_reading->timestamp = os_systime64();

#ifdef PRES_SEN_EN
  //  raw_reading->pressure_raw = 0;
  //  raw_reading->temp_icp_raw = 0;

    /* Read raw pressure and temperature recorded by icp-10100 */
   // inv_invpres_measure_raw_blocking(&pres_sen, LOW_POWER, &raw_reading->pressure_raw, //&raw_reading->temp_icp_raw);
#endif

#ifdef OPT_SEN_EN
    opt3002_config_t opt_config_trigger = {
            .RangeNumber = 0xC,                 // Automatic full-scale mode
            .ConversionTime = 0,                // 100 ms conversion time
            .ModeOfConversionOperation = 0x1,   // Single-shot mode
            .Latch = 0x1                        // Latched operation
    };
    opt3002_config_t opt_config_read = {.rawData = 0};

    memset(&raw_reading->light_raw, 0, sizeof(raw_reading->light_raw));

    // Trigger reading
    opt3002_writeConfig(&opt_sen, opt_config_trigger);

    // Wait for reading to be complete
    callout_delay_ms(100);

    // Make sure reading is ready
    do
    {
        opt_config_read = opt3002_readConfig(&opt_sen);
    } while(!opt_config_read.ConversionReady);

    // Get raw result
    raw_reading->light_raw = opt3002_readResult_raw(&opt_sen);
#endif

#ifdef HUMID_SEN_EN
    raw_reading->humidity_raw = 0;
    raw_reading->temp_shct_raw = 0;

    /* Read humidity and temperature recorded by shtc3 */
    shtc1_measure_blocking_read(&raw_reading->temp_shct_raw, &raw_reading->humidity_raw);
#endif
}

void print_sensor_readings(sensor_reading_t *reading, int ndigits)
{
    if(!reading)
        return;

    os_printf("Timestamp: %" PRIu64 " uS\n", reading->timestamp);

#ifdef PRES_SEN_EN
    os_printf("Pressure: ");
    print_double(reading->pressure, ndigits);
    os_printf(" Pa\n");

    os_printf("Temperature (bmp): ");
    print_double(reading->temp_bmp, ndigits);
    os_printf(" C\n");
#endif

#ifdef OPT_SEN_EN
    os_printf("Optical power: ");
    print_double(reading->light.lux, ndigits);
    os_printf(" nW/cm2\n");
#endif

#ifdef HUMID_SEN_EN
    os_printf("Humidity: ");
    print_double(reading->humidity, ndigits);
    os_printf(" %%\n");

    os_printf("Temperature (shtc): ");
    print_double(reading->temp_shtc, ndigits);
    os_printf(" C\n");
#endif
}

void print_double(double dd, int ndigits)
{
    int decpt, sign;
    char *str;

    str = dtoa(dd, 3, ndigits, &decpt, &sign, NULL);

    if(ndigits > 0)
    {
        // Trailing zeros are suppressed, but print at least one digit past the decimal point
        os_printf("%s%.*s.%s", sign? "-" : "", decpt, str, *(str+decpt)? str+decpt : "0");
    }
    else
        os_printf("%s%s", sign? "-" : "", str);

    freedtoa(str);
}
