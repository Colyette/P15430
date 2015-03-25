/***************************************************
 This is a library for the Adafruit BMP085/BMP180 Barometric Pressure + Temp sensor
 
 Designed specifically to work with the Adafruit BMP085 or BMP180 Breakout
 ----> http://www.adafruit.com/products/391
 ----> http://www.adafruit.com/products/1603
 
 These displays use I2C to communicate, 2 pins are required to
 interface
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!
 
 Written by Limor Fried/Ladyada for Adafruit Industries.
 BSD license, all text above must be included in any redistribution
 ****************************************************/
/**
 * \brief Same driver for BMP180, the interface. Modified for using on
 *          Raspberry Pi's linux i2c driver
 * \modified by Alyssa Colyette
 */

#ifndef ADAFRUIT_BMP085_H
#define ADAFRUIT_BMP085_H

#include <stdint.h>  //for int bit types
#include <pthread.h>    //for i2c dev mutex

#define BMP085_DEBUG 0

#define BMP085_I2CADDR 0x77

#define BMP085_ULTRALOWPOWER 0
#define BMP085_STANDARD      1
#define BMP085_HIGHRES       2
#define BMP085_ULTRAHIGHRES  3
#define BMP085_CAL_AC1           0xAA  // R   Calibration data (16 bits)
#define BMP085_CAL_AC2           0xAC  // R   Calibration data (16 bits)
#define BMP085_CAL_AC3           0xAE  // R   Calibration data (16 bits)
#define BMP085_CAL_AC4           0xB0  // R   Calibration data (16 bits)
#define BMP085_CAL_AC5           0xB2  // R   Calibration data (16 bits)
#define BMP085_CAL_AC6           0xB4  // R   Calibration data (16 bits)
#define BMP085_CAL_B1            0xB6  // R   Calibration data (16 bits)
#define BMP085_CAL_B2            0xB8  // R   Calibration data (16 bits)
#define BMP085_CAL_MB            0xBA  // R   Calibration data (16 bits)
#define BMP085_CAL_MC            0xBC  // R   Calibration data (16 bits)
#define BMP085_CAL_MD            0xBE  // R   Calibration data (16 bits)

#define BMP085_CONTROL           0xF4
#define BMP085_TEMPDATA          0xF6
#define BMP085_PRESSUREDATA      0xF6
#define BMP085_READTEMPCMD      0x2E
#define BMP085_READPRESSURECMD  0x34


class Adafruit_BMP085 {
public:
    Adafruit_BMP085();
    bool begin(uint8_t mode = BMP085_ULTRAHIGHRES);  // by default go highres
    float readTemperature(void);
    int32_t readPressure(void);
    int32_t readSealevelPressure(float altitude_meters = 0);
    float readAltitude(float sealevelPressure = 101325); // std atmosphere
    uint16_t readRawTemperature(void);
    uint32_t readRawPressure(void);
    
    //passes the already opened dev handle to  use within the class environment
    void set_dev_handle(int e_dev_handle);
    
    //passed mutex for the dev handle
    void set_dev_mutex(pthread_mutex_t* dev_handle_mutex);
    
    float base_alt;
private:
    int32_t computeB5(int32_t UT);
    uint8_t read8(uint8_t addr);
    uint16_t read16(uint8_t addr);
    uint8_t write8(uint8_t addr, uint8_t data);
    
    uint8_t oversampling;
    
    int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
    uint16_t ac4, ac5, ac6;
    
    //dev handle of i2c driver
    int dev_handle;
    //dev handle
    pthread_mutex_t* dev_handle_mutex_ptr;
};


#endif //  ADAFRUIT_BMP085_H