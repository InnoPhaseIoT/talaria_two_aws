class InvensensePressureConversion:
    """ Class for conversion of the pressure and temperature output of the Invensense sensor"""
    
    def __init__(self, sensor_constants):
        """ Initialize customer formula
        Arguments:
        sensor_constants -- list of 4 integers: [c1, c2, c3, c4]
        """
        self.sensor_constants = sensor_constants
        # configuration for ICP-101xx Samples
        self.p_Pa_calib = [45000.0, 80000.0, 105000.0]
        self.LUT_lower = 3.5 * (2**20)
        self.LUT_upper = 11.5 * (2**20)
        self.quadr_factor = 1 / 16777216.0
        self.offst_factor = 2048.0
        
    def set_constants(self, sensor_constants):
        """Update sensor constants"""
        self.sensor_constants = sensor_constants
    
    def calculate_conversion_constants(self, p_Pa, p_LUT):
        """ calculate temperature dependent constants
        Arguments:
        p_Pa -- List of 3 values corresponding to applied pressure in Pa
        p_LUT -- List of 3 values corresponding to the measured p_LUT values at the applied pressures.
        """
        C = (p_LUT[0] * p_LUT[1] * (p_Pa[0] - p_Pa[1]) +
        p_LUT[1] * p_LUT[2] * (p_Pa[1] - p_Pa[2]) +
        p_LUT[2] * p_LUT[0] * (p_Pa[2] - p_Pa[0])) / \
        (p_LUT[2] * (p_Pa[0] - p_Pa[1]) +
        p_LUT[0] * (p_Pa[1] - p_Pa[2]) +
        p_LUT[1] * (p_Pa[2] - p_Pa[0]))
        A = (p_Pa[0] * p_LUT[0] - p_Pa[1] * p_LUT[1] - (p_Pa[1] - p_Pa[0]) * C) / (p_LUT[0] - p_LUT[1])
        B = (p_Pa[0] - A) * (p_LUT[0] + C)
        return [A, B, C]
    
    def get_pressure(self, p_LSB, T_LSB):
        """ Convert an output from a calibrated sensor to a pressure in Pa.
        Arguments:
        p_LSB -- Raw pressure data from sensor
        T_LSB -- Raw temperature data from sensor
        """
        t = T_LSB - 32768.0
        s1 = self.LUT_lower + float(self.sensor_constants[0] * t * t) * self.quadr_factor
        s2 = self.offst_factor * self.sensor_constants[3] + float(self.sensor_constants[1] * t * t) * self.quadr_factor
        s3 = self.LUT_upper + float(self.sensor_constants[2] * t * t) * self.quadr_factor
        A, B, C = self.calculate_conversion_constants(self.p_Pa_calib, [s1, s2, s3])
        
        return A + B / (C + p_LSB)
    
    def get_temperature(self, T_LSB):
        return -45.0 + 175.0/65536.0 * T_LSB;
    