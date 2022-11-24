from icp_101xx import InvensensePressureConversion
from sensorJSON import *


class SensorProcessor:
    def __init__(self, calib={}):
        if JSON_ATTR_CALIB_ICP in calib:
            icp_sensor_constants = calib[JSON_ATTR_CALIB_ICP]
        else:
            icp_sensor_constants = [0,0,0,0]
                
        self.icpConverter = InvensensePressureConversion(icp_sensor_constants)

    def process_reading(self, rawReading):
        processedReading = {}
        
        for (measurementName, measurementValue) in rawReading.items():
            # Special case:
            # pressure calculation requires both raw pressure and raw icp temperature
            if measurementName == JSON_ATTR_PRESSURE + JSON_ATTR_RAW_SUFFIX:
                pressureRaw = measurementValue
                # pressure_raw is always accompanied by temperature_icp_raw
                temperatureIcpRaw = rawReading[JSON_ATTR_TEMP_ICP + JSON_ATTR_RAW_SUFFIX]
                
                processedReading[JSON_ATTR_PRESSURE] = self.icpConverter.get_pressure(pressureRaw, temperatureIcpRaw)
                
            elif measurementName in SensorProcessor.CONVERSION_MAP:
                (newName, conversionFun) = SensorProcessor.CONVERSION_MAP[measurementName]
                newValue = conversionFun(self, measurementValue)
                processedReading[newName] = newValue
        
            else:
                processedReading[measurementName] = measurementValue
        
        return processedReading
    
    def set_calibration(self, calib):
        if JSON_ATTR_CALIB_ICP in calib:
            self.icpConverter.set_constants(calib[JSON_ATTR_CALIB_ICP])
            
    def process_temperature_icp(self, temperatureIcpRaw):
        return self.icpConverter.get_temperature(temperatureIcpRaw)
    
    def process_opt_pow(self, optPowRaw):
        result = optPowRaw & 0xFFF
        exponent = (optPowRaw >> 12) & 0xF
        
        return 1.2 * (2**exponent) * result
    
    def process_humidity(self, humidityRaw):
        return humidityRaw / 1000.0
    
    def process_temperature_shtc(self, temperatureShtcRaw):
        return temperatureShtcRaw / 1000.0
    
    
SensorProcessor.CONVERSION_MAP = {
JSON_ATTR_TEMP_ICP + JSON_ATTR_RAW_SUFFIX :  (JSON_ATTR_TEMP_ICP,  SensorProcessor.process_temperature_icp),
JSON_ATTR_OPT_POW + JSON_ATTR_RAW_SUFFIX :   (JSON_ATTR_OPT_POW,   SensorProcessor.process_opt_pow),
JSON_ATTR_HUMIDITY + JSON_ATTR_RAW_SUFFIX :  (JSON_ATTR_HUMIDITY,  SensorProcessor.process_humidity),
JSON_ATTR_TEMP_SHTC + JSON_ATTR_RAW_SUFFIX : (JSON_ATTR_TEMP_SHTC, SensorProcessor.process_temperature_shtc)}
