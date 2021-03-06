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
 * \brief edited for using linux userspace i2c driver for RPi
 * \modified by Alyssa Colyette
 */
#include "Adafruit_BMP085.h"
#include <stdio.h>
#include <linux/i2c-dev.h>  //linux userspace I2C driver
#include <sys/ioctl.h>      //opens posix compliant drivers
#include <errno.h>          //reading error codes delivered by bad io
#include <fcntl.h>          //read, write file handler
#include <unistd.h>         //constants for posix compliance
#include <math.h>           //for calculations

#define NUM_START_SAMPLE (50) //number of samples to take for establishing starting altitude

Adafruit_BMP085::Adafruit_BMP085() {
}

/**
 * \brief Needs to be set before any sensor readings
 */
void Adafruit_BMP085::set_dev_handle(int e_dev_handle){
    dev_handle = e_dev_handle;
}

/**
 * \brief passed mutex for the dev handle
 */
void Adafruit_BMP085::set_dev_mutex(std::recursive_mutex* e_dev_handle_mutex){
    dev_handle_mutex_ptr = e_dev_handle_mutex;
}

bool Adafruit_BMP085::begin(uint8_t mode) {
    if (mode > BMP085_ULTRAHIGHRES)
        mode = BMP085_ULTRAHIGHRES;
    oversampling = mode;
    
    //Wire.begin();
    
    if (read8(0xD0) != 0x55) return false;
    
    /* read calibration data */
    ac1 = read16(BMP085_CAL_AC1);
    ac2 = read16(BMP085_CAL_AC2);
    ac3 = read16(BMP085_CAL_AC3);
    ac4 = read16(BMP085_CAL_AC4);
    ac5 = read16(BMP085_CAL_AC5);
    ac6 = read16(BMP085_CAL_AC6);
    
    b1 = read16(BMP085_CAL_B1);
    b2 = read16(BMP085_CAL_B2);
    
    mb = read16(BMP085_CAL_MB);
    mc = read16(BMP085_CAL_MC);
    md = read16(BMP085_CAL_MD);
#if (BMP085_DEBUG == 1)
    //Serial.print("ac1 = "); Serial.println(ac1, DEC);
    printf("ac1 = %d\n",ac1);
    //Serial.print("ac2 = "); Serial.println(ac2, DEC);
    printf("ac2 = %d\n",ac2);
    //Serial.print("ac3 = "); Serial.println(ac3, DEC);
    Serial.print("ac3 = %d\n",ac3);
    Serial.print("ac4 = "); Serial.println(ac4, DEC);
    Serial.print("ac5 = "); Serial.println(ac5, DEC);
    Serial.print("ac6 = "); Serial.println(ac6, DEC);
    
    Serial.print("b1 = "); Serial.println(b1, DEC);
    Serial.print("b2 = "); Serial.println(b2, DEC);
    
    Serial.print("mb = "); Serial.println(mb, DEC);
    Serial.print("mc = "); Serial.println(mc, DEC);
    Serial.print("md = "); Serial.println(md, DEC);
#endif
    
    //TODO need to take multiple samples for median filtering
    float history[NUM_START_SAMPLE];
    float temp;
    int i;
    int n = NUM_START_SAMPLE;
    bool swapped;
    for (i=0; i<NUM_START_SAMPLE; i++) {
        history[i] = readAltitude();
    }
    //order list
    do {
        swapped = false;
        for(i = 1; i<=n-1; i++){
            if (history[i-1] > history[i]) { //then swap
                temp = history[i];
                history[i] = history[i-1];
                history[i-1] = temp;
                swapped = true;
            }// else we hadn't swapped this round
        }
        n= n-1;
    } while (!swapped);
    //get median value
    base_alt = history[(int)ceil(NUM_START_SAMPLE/2.0)];
    //base_alt = readAltitude(); //TODO may have to manually alter
    printf("base_line altitude set as %f meters\n",base_alt);
    return true;
}

int32_t Adafruit_BMP085::computeB5(int32_t UT) {
    int32_t X1 = (UT - (int32_t)ac6) * ((int32_t)ac5) >> 15;
    int32_t X2 = ((int32_t)mc << 11) / (X1+(int32_t)md);
    return X1 + X2;
}

uint16_t Adafruit_BMP085::readRawTemperature(void) {
    write8(BMP085_CONTROL, BMP085_READTEMPCMD);
    ::usleep(5000);
#if BMP085_DEBUG == 1
    Serial.print("Raw temp: "); Serial.println(read16(BMP085_TEMPDATA));
#endif
    return read16(BMP085_TEMPDATA);
}

uint32_t Adafruit_BMP085::readRawPressure(void) {
    uint32_t raw;
    
    write8(BMP085_CONTROL, BMP085_READPRESSURECMD + (oversampling << 6));
    
    if (oversampling == BMP085_ULTRALOWPOWER)
        ::usleep(5000);
    else if (oversampling == BMP085_STANDARD)
        ::usleep(8000);
    else if (oversampling == BMP085_HIGHRES)
        ::usleep(14000);
    else
        ::usleep(26000);
    
    raw = read16(BMP085_PRESSUREDATA);
    
    raw <<= 8;
    raw |= read8(BMP085_PRESSUREDATA+2);
    raw >>= (8 - oversampling);
    
    /* this pull broke stuff, look at it later?
     if (oversampling==0) {
     raw <<= 8;
     raw |= read8(BMP085_PRESSUREDATA+2);
     raw >>= (8 - oversampling);
     }
     */
    
#if BMP085_DEBUG == 1
    Serial.print("Raw pressure: "); Serial.println(raw);
#endif
    return raw;
}


int32_t Adafruit_BMP085::readPressure(void) {
    int32_t UT, UP, B3, B5, B6, X1, X2, X3, p;
    uint32_t B4, B7;
    
    UT = readRawTemperature();
    UP = readRawPressure();
    
#if BMP085_DEBUG == 1
    // use datasheet numbers!
    UT = 27898;
    UP = 23843;
    ac6 = 23153;
    ac5 = 32757;
    mc = -8711;
    md = 2868;
    b1 = 6190;
    b2 = 4;
    ac3 = -14383;
    ac2 = -72;
    ac1 = 408;
    ac4 = 32741;
    oversampling = 0;
#endif
    
    B5 = computeB5(UT);
    
#if BMP085_DEBUG == 1
    Serial.print("X1 = "); Serial.println(X1);
    Serial.print("X2 = "); Serial.println(X2);
    Serial.print("B5 = "); Serial.println(B5);
#endif
    
    // do pressure calcs
    B6 = B5 - 4000;
    X1 = ((int32_t)b2 * ( (B6 * B6)>>12 )) >> 11;
    X2 = ((int32_t)ac2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = ((((int32_t)ac1*4 + X3) << oversampling) + 2) / 4;
    
#if BMP085_DEBUG == 1
    Serial.print("B6 = "); Serial.println(B6);
    Serial.print("X1 = "); Serial.println(X1);
    Serial.print("X2 = "); Serial.println(X2);
    Serial.print("B3 = "); Serial.println(B3);
#endif
    
    X1 = ((int32_t)ac3 * B6) >> 13;
    X2 = ((int32_t)b1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = ((uint32_t)ac4 * (uint32_t)(X3 + 32768)) >> 15;
    B7 = ((uint32_t)UP - B3) * (uint32_t)( 50000UL >> oversampling );
    
#if BMP085_DEBUG == 1
    Serial.print("X1 = "); Serial.println(X1);
    Serial.print("X2 = "); Serial.println(X2);
    Serial.print("B4 = "); Serial.println(B4);
    Serial.print("B7 = "); Serial.println(B7);
#endif
    
    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    
#if BMP085_DEBUG == 1
    Serial.print("p = "); Serial.println(p);
    Serial.print("X1 = "); Serial.println(X1);
    Serial.print("X2 = "); Serial.println(X2);
#endif
    
    p = p + ((X1 + X2 + (int32_t)3791)>>4);
#if BMP085_DEBUG == 1
    Serial.print("p = "); Serial.println(p);
#endif
    return p;
}

int32_t Adafruit_BMP085::readSealevelPressure(float altitude_meters) {
    float pressure = readPressure();
    return (int32_t)(pressure / pow(1.0-altitude_meters/44330, 5.255));
}

float Adafruit_BMP085::readTemperature(void) {
    int32_t UT, B5;     // following ds convention
    float temp;
    
    UT = readRawTemperature();
    
#if BMP085_DEBUG == 1
    // use datasheet numbers!
    UT = 27898;
    ac6 = 23153;
    ac5 = 32757;
    mc = -8711;
    md = 2868;
#endif
    
    B5 = computeB5(UT);
    temp = (B5+8) >> 4;
    temp /= 10;
    
    return temp;
}

float Adafruit_BMP085::readAltitude(float sealevelPressure) {
    float altitude;
    
    float pressure = readPressure();
    
    altitude = 44330 * (1.0 - pow(pressure /sealevelPressure,0.1903));
    
    return altitude;
}


/*********************************************************************/
/**
 * \brief reads a byte from register 
 * \param a - the register to read from
 * \return byte read, 0xFF if fail
 */
uint8_t Adafruit_BMP085::read8(uint8_t a) {
    uint8_t ret;
    int err,rec;
    uint8_t buffer[2];
    buffer[0] =a;
    
    //LOCK
//    if (pthread_mutex_lock(dev_handle_mutex_ptr) ){
//        printf("Adafruit_BMP085::read8: Error locking thread\n");
//        return (-1);
//    }
    std::unique_lock<std::recursive_mutex> lck(*dev_handle_mutex_ptr);
    
    //Wire.beginTransmission(BMP085_I2CADDR); // start transmission to device
    if( ioctl( dev_handle, I2C_SLAVE, BMP085_I2CADDR) < 0 ){
        err = errno ;
        printf( "Adafruit_BMP085::read8: I2C bus cannot point to barometer of IMU Slave: errno %d\n",err);
        return (-1);
    }
    
//#if (ARDUINO >= 100)
//    Wire.write(a); // sends register address to read from
//#else
//    Wire.send(a); // sends register address to read from
//#endif
//    Wire.endTransmission(); // end transmission
    if ( write(dev_handle,buffer, 1 ) != 1 ){
        err = errno ;
        printf("Adafruit_BMP085::read8: change write register address: errno %d\n",err);
        return (-1);
    }
    
    //Wire.beginTransmission(BMP085_I2CADDR); // start transmission to device
//    Wire.requestFrom(BMP085_I2CADDR, 1);// send data n-bytes read
//#if (ARDUINO >= 100)
//    ret = Wire.read(); // receive DATA
//#else
//    ret = Wire.receive(); // receive DATA
//#endif
//    Wire.endTransmission(); // end transmission
    if ((rec = ::read( dev_handle,buffer, 1 )) != 1  ) { // read one byte
        err = errno ;
        printf("Adafruit_BMP085::read8: Couldn't read from register: errno %d rec: %d\n",err,rec);
        return -1;
    }
    ret = buffer[0];
    
    //UNLOCK~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    if (pthread_mutex_unlock( dev_handle_mutex_ptr) ) {
//        printf("Adafruit_BMP085::read8: Error unlocking thread\n");
//        return (-1);
//    }
    return ret;
}

/**
 * \brief reads two bytes from register
 * \param a - the register to read from
 * \return byte read, 0xFFFF if fail
 */
uint16_t Adafruit_BMP085::read16(uint8_t a) {
    uint16_t ret;
    int err,rec;
    uint8_t buffer[2];
    buffer[0] =a;
    
    std::unique_lock<std::recursive_mutex> lck(*dev_handle_mutex_ptr);
    
    if( ioctl( dev_handle, I2C_SLAVE, BMP085_I2CADDR) < 0 ){
        err = errno ;
        printf( "Adafruit_BMP085::read16: I2C bus cannot point to barometer of IMU Slave: errno %d\n",err);
        return (-1);
    }
    
    if ( write(dev_handle,buffer, 1 ) != 1 ){
        err = errno ;
        printf("Adafruit_BMP085::read16: change write register address: errno %d\n",err);
        return (-1);
    }
    
    if ((rec = ::read( dev_handle,buffer, 2 )) != 2  ) { // read one byte
        err = errno ;
        printf("Adafruit_BMP085::read16: Couldn't read from register: errno %d rec: %d\n",err,rec);
        return -1;
    }
    
    //MSB is first
    ret = ( (buffer[0]<<8) | buffer[1] );
    return ret;
}


/**
 * \brief writes a byte to a register
 * \param a - the register to write to 
 * \param d - the value to write to reg a
 */
uint8_t Adafruit_BMP085::write8(uint8_t a, uint8_t d) {
    int written_bytes;
    int err,rec;
    uint8_t buffer[2];
    
    std::unique_lock<std::recursive_mutex> lck(*dev_handle_mutex_ptr);
    
    //Wire.beginTransmission(BMP085_I2CADDR); // start transmission to device
    if( ioctl( dev_handle, I2C_SLAVE, BMP085_I2CADDR) < 0 ){
        err = errno ;
        printf( "Adafruit_BMP085::write8: I2C bus cannot point to barometer of IMU Slave: errno %d\n",err);
        return (-1);
    }
    
    //Wire.write(a); // sends register address to read from
    //Wire.write(d);  // write data
    //Wire.endTransmission(); // end transmission
    buffer[0] = a;    //assign reg to write to
    buffer[1] = d;  //value to write
    if ( write(dev_handle,buffer, 2 ) != 2 ){
        err = errno ;
        printf("ADXL345::writeRegister: change write register address: errno %d\n",err);
        return (-1);
    }

    return 1;
}