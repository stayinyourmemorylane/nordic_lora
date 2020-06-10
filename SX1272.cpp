

#include "SX1272.h"
#include <SPI.h>

#define SX1272_REGISTER_VERSION_CODE 0x22
#define SX1276_REGISTER_VERSION_CODE 0x12


#define REG_IRQ_RX_TIMEOUT_FLAG                  0x80
#define REG_IRQ_RXDONE_FLAG                      0x40
#define REG_IRQ_PAYLOAD_CRC_ERROR_FLAG           0x20
#define REG_IRQ_VALID_HEADER_FLAG                0x10

#define REG_IRQ_TX_DONE_FLAG                     0x08
#define REG_IRQ_CADDONE                          0x04
#define REG_IRQ_FHSS_CHANGE_CHAN                 0x02
#define REG_IRQ_CAD_CHAECKED                     0x01


uint8_t SX127X_SpreadFactor[11] =
{
    0x00,          // not a valid SF
    (SF_12),       // SF = 12
    (SF_12),       // SF = 12
    (SF_10),       // SF = 10
    (SF_12),       // SF = 12
    (SF_10),       // SF = 10
    (SF_11),       // SF = 11
    (SF_9),        // SF = 9
    (SF_9),        // SF = 9
    (SF_8),        // SF = 8
    (SF_7)         // SF = 7
};

uint8_t SX127X_Bandwidth[11] = 
{
    0x00,                // not a valid bw
    (BW_125),            // BW = 125 KHz           
    (BW_250),            // BW = 250 KHz           
    (BW_125),            // BW = 125 KHz           
    (BW_500),            // BW = 500 KHz           
    (BW_250),            // BW = 250 KHz           
    (BW_500),            // BW = 500 KHz           
    (BW_250),            // BW = 250 KHz           
    (BW_500),            // BW = 500 KHz           
    (BW_500),            // BW = 500 KHz           
    (BW_500)             // BW = 500 KHz           
};


__attribute__((weak)) uint8_t spi_txrx_byte(uint8_t byte)
{ 
    // this function must be overridden by the application software. 
    return 1;
}


__attribute__((weak)) uint8_t gpio_write(uint8_t pin, uint8_t data)
{ 
    // this function must be overridden by the application software. 
    return 1;
}


__attribute__((weak)) uint8_t gpio_mode(uint8_t pin, uint8_t mode)
{ 
    // this function must be overridden by the application software. 
    return 1;
}

__attribute__((weak)) uint8_t delay_ms(uint16_t delayms)
{ 
    // this function must be overridden by the application software. 
    return 1;
}



SX1272::SX1272()
{
    // Initialize class variables
    _bandwidth = BW_125;
    _codingRate = CR_5;
    _spreadingFactor = SF_7;
    _channel = CH_12_900;
    _power = 15;
    _packetNumber = 0;
    _syncWord=0x12;
};



/*
 Function: Sets the module ON.
 Returns: uint8_t setLORA state
*/
uint8_t SX1272::ON()
{
    uint8_t error = 0;
    uint8_t version; 

    // Powering the module
    gpio_mode(SX1272_SS,OUTPUT);
    gpio_write(SX1272_SS,HIGH);
    delay_ms(100);

    delay_ms(100);

    
    gpio_mode(SX1272_RST,OUTPUT);

    // request device reset 
    gpio_write(SX1272_RST,HIGH);
    delay_ms(100);
    gpio_write(SX1272_RST,LOW);
    delay_ms(100);

    // Read Version 
    version = readRegister(REG_VERSION);
    switch(version)
    {
        case SX1272_REGISTER_VERSION_CODE:
            _board = SX1272Chip;  
            break;

        case SX1276_REGISTER_VERSION_CODE:
            _board = SX1276Chip;  
            break;
        default:
            error = -1; 
            break; 
    }


    if(error != 0)
        return error; 

    // set LoRa mode
    error = setLORA();
    if(error != 0)
        return error; 

    setMaxCurrent(0x1B);

    setSyncWord(_syncWord);

    return error;
}

/*
 Function: Sets the module OFF.
 Returns: Nothing
*/
void SX1272::OFF()
{
    // Powering the module
    gpio_mode(SX1272_SS,OUTPUT);
    gpio_write(SX1272_SS,LOW);
}

/*
 Function: Reads the indicated register.
 Returns: The content of the register
 Parameters:
   address: address register to read from
*/
byte SX1272::readRegister(byte address)
{
    byte value = 0x00;

    gpio_write(SX1272_SS,LOW);
    bitClear(address, 7);       // Bit 7 cleared to write in registers
    spi_txrx_byte(address);
    value = spi_txrx_byte(0x00);
    gpio_write(SX1272_SS,HIGH);

    return value;
}

/*
 Function: Writes on the indicated register.
 Returns: Nothing
 Parameters:
   address: address register to write in
   data : value to write in the register
*/
void SX1272::writeRegister(byte address, byte data)
{

    gpio_write(SX1272_SS,LOW);
    bitSet(address, 7);         // Bit 7 set to read from registers
    spi_txrx_byte(address);
    spi_txrx_byte(data);
    gpio_write(SX1272_SS,HIGH);
    return; 
}

/**
 * Function: writes & read to the indicated register return 0 if equal
 * @param  address  address
 * @param  data     byte to write
 * @return          0 if equal
 */
int8_t SX1272::writeReadRegister(byte address, byte data)
{
    int8_t error =0; 
    byte dataRead = 0;
    writeRegister(address, data);

    delay(100);

    dataRead = readRegister(address);

    if(data != dataRead)
        return -1; 
    
    return error; 
}


/**
 * Function: Clears the interruption flags
 * @return Nothing
 */
int8_t SX1272::clearFlags()
{
     int8_t error =0;
    uint8_t st0;

    st0 = readRegister(REG_OP_MODE);        // Save the previous status
    
    // Stdby mode to write in registers
    error = writeReadRegister(REG_OP_MODE, LORA_STANDBY_MODE);  
    if(error !=0)
        return error;

    // LoRa mode flags register
    error = writeReadRegister(REG_IRQ_FLAGS, 0xFF); 
    if(error !=0)
        return error;

    // Getting back to previous status
    error = writeReadRegister(REG_OP_MODE, st0);   
    if(error !=0)
        return error;

    return error; 
}


/**
 * Function: requests SX127X module to be setup in lora mode 
 * @return error state
 */
int8_t SX1272::setLORA()
{
    uint8_t retry=0;
    uint8_t error = 0;
    uint8_t st0;

    do {
        writeRegister(REG_OP_MODE, FSK_SLEEP_MODE);    // Sleep mode (mandatory to set LoRa mode)
        writeRegister(REG_OP_MODE, LORA_SLEEP_MODE);    // LoRa sleep mode
        writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);
        delay_ms(200);
        // read the operation register, LoRa should exit standby mode 
        st0 = readRegister(REG_OP_MODE);
        retry++; 

        if(retry > 10)
        {
            error = -1;
            break;
        }
    } while (st0!=LORA_STANDBY_MODE); 
        
    if(error == 0 )
        _modem = LORA;

    return error;
}




int8_t SX1272::setMode(uint8_t mode)
{
    int8_t error = 0;
    uint8_t st0;
    uint8_t spreadingFactor =0, bandwidth =0; 

    st0 = readRegister(REG_OP_MODE);        // Save the previous status
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);  // LoRa standby mode

    spreadingFactor = SX127X_SpreadFactor[mode];
    bandwidth = SX127X_Bandwidth[mode];
    
    setCR(CR_5);     // always set the coding rate to 5
    setSF(spreadingFactor);       // set the spreading factor
    setBW(bandwidth);      // Set the bandwidth 

    _loraMode=mode;

    writeRegister(REG_OP_MODE, st0);    // Getting back to previous status
    delay_ms(100);
    return error;
}


/*
 Function: Checks if SF is a valid value.
 Returns: Boolean that's 'true' if the SF value exists and
          it's 'false' if the SF value does not exist.
 Parameters:
   spr: spreading factor value to check.
*/
boolean SX1272::isSF(uint8_t spr)
{

    // Checking available values for _spreadingFactor
    switch(spr)
    {
    case SF_6:
    case SF_7:
    case SF_8:
    case SF_9:
    case SF_10:
    case SF_11:
    case SF_12:
        return true;
        break;

    default:
        return false;
    }
    
    return false; 
}

/*
 Function: Gets the SF within the module is configured.
 Returns: Integer that determines if there has been any error
   state = 2  --> The command has not been executed
   state = 1  --> There has been an error while executing the command
   state = 0  --> The command has been executed with no errors
   state = -1 --> Forbidden command for this protocol
*/
int8_t  SX1272::getSF()
{
    int8_t state = 2;
    uint8_t config2;

  
    // take out bits 7-4 from REG_MODEM_CONFIG2 indicates _spreadingFactor
    config2 = (readRegister(REG_MODEM_CONFIG2)) >> 4;
    _spreadingFactor = config2;
    state = 1;

    if( (config2 == _spreadingFactor) && isSF(_spreadingFactor) )
    {
        state = 0;
    }
 
    return state;
}



/*
 Function: Sets the indicated SF in the module.
 Returns: Integer that determines if there has been any error
   state = 2  --> The command has not been executed
   state = 1  --> There has been an error while executing the command
   state = 0  --> The command has been executed with no errors
 Parameters:
   spr: spreading factor value to set in LoRa modem configuration.
*/
uint8_t SX1272::setSF(uint8_t spr)
{
    uint8_t st0;
    int8_t state = 2;
    uint8_t config1;
    uint8_t config2;

    st0 = readRegister(REG_OP_MODE);    // Save the previous status
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);  // LoRa standby mode


    config2 = (readRegister(REG_MODEM_CONFIG2));    // Save config2 to modify SF value (bits 7-4)

    switch(spr)
    {
        case SF_6: 
        {
            config2 = config2 & B01101111;  // clears bits 7 & 4 from REG_MODEM_CONFIG2
            config2 = config2 | B01100000;  // sets bits 6 & 5 from REG_MODEM_CONFIG2
            // Mandatory headerOFF with SF = 6 (Implicit mode)

            // Set the bit field DetectionOptimize of
            // register RegLoRaDetectOptimize to value "0b101".
            writeRegister(REG_DETECT_OPTIMIZE, 0x05);

            // Write 0x0C in the register RegDetectionThreshold.
            writeRegister(REG_DETECTION_THRESHOLD, 0x0C);
            break;
        }
        case SF_7:  
        {   
            config2 = config2 & B01111111;  // clears bits 7 from REG_MODEM_CONFIG2
            config2 = config2 | B01110000;  // sets bits 6, 5 & 4
            break;
        }
        case SF_8:  
        {
            config2 = config2 & B10001111;  // clears bits 6, 5 & 4 from REG_MODEM_CONFIG2
            config2 = config2 | B10000000;  // sets bit 7 from REG_MODEM_CONFIG2
            break;
        }
        case SF_9:
        {
            config2 = config2 & B10011111;  // clears bits 6, 5 & 4 from REG_MODEM_CONFIG2
            config2 = config2 | B10010000;  // sets bits 7 & 4 from REG_MODEM_CONFIG2
            break;
        }
        case SF_10: 
        {   config2 = config2 & B10101111;  // clears bits 6 & 4 from REG_MODEM_CONFIG2
            config2 = config2 | B10100000;  // sets bits 7 & 5 from REG_MODEM_CONFIG2
            break;
        }
        case SF_11: 
        {   config2 = config2 & B10111111;  // clears bit 6 from REG_MODEM_CONFIG2
            config2 = config2 | B10110000;  // sets bits 7, 5 & 4 from REG_MODEM_CONFIG2
            break;
        }
        case SF_12: 
        {   config2 = config2 & B11001111;  // clears bits 5 & 4 from REG_MODEM_CONFIG2
            config2 = config2 | B11000000;  // sets bits 7 & 6 from REG_MODEM_CONFIG2
            break;
        }
    }

    // Check if it is neccesary to set special settings for SF=6
    if( spr != SF_6 )
    {

        // LoRa detection Optimize: 0x03 --> SF7 to SF12
        writeRegister(REG_DETECT_OPTIMIZE, 0x03);

        // LoRa detection threshold: 0x0A --> SF7 to SF12
        writeRegister(REG_DETECTION_THRESHOLD, 0x0A);
    }

    
    if (_board==SX1272Chip) {
        // comment by C. Pham
        // bit 9:8 of SymbTimeout are then 11
        // single_chan_pkt_fwd uses 00 and then 00001000
        // why?
        // sets bit 2-0 (AgcAutoOn and SymbTimout) for any SF value
        //config2 = config2 | B00000111;
        
        config2 = config2 | B00000100;
        writeRegister(REG_MODEM_CONFIG1, config1);      // Update config1
    }
    else {
        // set the AgcAutoOn in bit 2 of REG_MODEM_CONFIG3
        uint8_t config3 = (readRegister(REG_MODEM_CONFIG3));
        config3=config3 | B00000100;
        writeRegister(REG_MODEM_CONFIG3, config3);
    }

    // here we write the new SF
    writeRegister(REG_MODEM_CONFIG2, config2);      // Update config2

    delay_ms(100);

    
    byte configAgc;
    uint8_t theLDRBit;

    if (_board==SX1272Chip) {
        config1 = (readRegister(REG_MODEM_CONFIG1));    // Save config1 to check update
        config2 = (readRegister(REG_MODEM_CONFIG2));    // Save config2 to check update
        // comment by C. Pham
        // (config2 >> 4) ---> take out bits 7-4 from REG_MODEM_CONFIG2 (=_spreadingFactor)
        // bitRead(config1, 0) ---> take out bits 1 from config1 (=LowDataRateOptimize)
        // config2 is only for the AgcAutoOn
        configAgc=config2;
        theLDRBit=0;
    }
    else {
        config1 = (readRegister(REG_MODEM_CONFIG3));    // Save config1 to check update
        config2 = (readRegister(REG_MODEM_CONFIG2));
        // LowDataRateOptimize is in REG_MODEM_CONFIG3
        // AgcAutoOn is in REG_MODEM_CONFIG3
        configAgc=config1;
        theLDRBit=3;
    }

    writeRegister(REG_OP_MODE, st0);    // Getting back to previous status
    delay_ms(100);

    if( isSF(spr) )
    { // Checking available value for _spreadingFactor
        state = 0;
        _spreadingFactor = spr;
    }

    return state;
}

/*
 Function: Checks if BW is a valid value.
 Returns: Boolean that's 'true' if the BW value exists and
          it's 'false' if the BW value does not exist.
 Parameters:
   band: bandwidth value to check.
*/
boolean SX1272::isBW(uint16_t band)
{

    if (_board==SX1272Chip) {
        switch(band)
        {
            case BW_125:
            case BW_250:
            case BW_500:
                return true;
                break;

            default:
                return false;
        }
    }
    else {
        switch(band)
        {
            case BW_7_8:
            case BW_10_4:
            case BW_15_6:
            case BW_20_8:
            case BW_31_25:
            case BW_41_7:
            case BW_62_5:
            case BW_125:
            case BW_250:
            case BW_500:
                return true;
                break;

            default:
                return false;
        }
    }

}

int8_t  SX1272::getBW()
{
    uint8_t error = 0;
    uint8_t config1;

    if (_board==SX1272Chip) 
    {
        // take out bits 7-6 from REG_MODEM_CONFIG1 indicates _bandwidth
        config1 = (readRegister(REG_MODEM_CONFIG1)) >> 6;
    }
    else {
        // take out bits 7-4 from REG_MODEM_CONFIG1 indicates _bandwidth
        config1 = (readRegister(REG_MODEM_CONFIG1)) >> 4;
    }

    _bandwidth = config1;

    if(! ((config1 == _bandwidth) && isBW(_bandwidth)) )
    {
        error = -1;
    }

    return error;
}


int8_t  SX1272::setBW(uint16_t band)
{
    uint8_t st0;
    int8_t state = 2;
    uint8_t config1;

    if(!isBW(band) )
    {
        state = 1;
        return state;
    }

    st0 = readRegister(REG_OP_MODE);    // Save the previous status

    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);  // LoRa standby mode
    config1 = (readRegister(REG_MODEM_CONFIG1));    // Save config1 to modify only the BW

    // for SX1276
    if (_board==SX1272Chip) 
    {
        switch(band)
        {
            case BW_125:  
                config1 = config1 & B00111111;    // clears bits 7 & 6 from REG_MODEM_CONFIG1
                getSF();
                if( _spreadingFactor == 11 )
                { // LowDataRateOptimize (Mandatory with BW_125 if SF_11)
                    config1 = config1 | B00000001;
                }
                if( _spreadingFactor == 12 )
                { // LowDataRateOptimize (Mandatory with BW_125 if SF_12)
                    config1 = config1 | B00000001;
                }
                break;
            case BW_250:  
                config1 = config1 & B01111111;    // clears bit 7 from REG_MODEM_CONFIG1
                config1 = config1 | B01000000;  // sets bit 6 from REG_MODEM_CONFIG1
                break;
            case BW_500: 
                config1 = config1 & B10111111;    //clears bit 6 from REG_MODEM_CONFIG1
                config1 = config1 | B10000000;  //sets bit 7 from REG_MODEM_CONFIG1
                break;
        }
    }
    else if(_board==SX1276Chip)  
    {
        // SX1276
        config1 = config1 & B00001111;  // clears bits 7 - 4 from REG_MODEM_CONFIG1
        switch(band)
        {
            case BW_125:
                // 0111
                config1 = config1 | B01110000;
                getSF();
                if( _spreadingFactor == 11 || _spreadingFactor == 12)
                { // LowDataRateOptimize (Mandatory with BW_125 if SF_11 or SF_12)
                    byte config3=readRegister(REG_MODEM_CONFIG3);
                    config3 = config3 | B0000100;
                    writeRegister(REG_MODEM_CONFIG3,config3);
                }
                break;
            case BW_250:
                // 1000
                config1 = config1 | B10000000;
                break;
            case BW_500:
                // 1001
                config1 = config1 | B10010000;
                break;
        }
    }


    uint8_t error = writeReadRegister(REG_MODEM_CONFIG1,config1);       // Update config1
    if(error ==0)
    {
        _bandwidth = band;
    }
    writeRegister(REG_OP_MODE, st0);    // Getting back to previous status
   
    return state;
}

/*
 Function: Checks if CR is a valid value.
 Returns: Boolean that's 'true' if the CR value exists and
          it's 'false' if the CR value does not exist.
 Parameters:
   cod: coding rate value to check.
*/
boolean SX1272::isCR(uint8_t cod)
{

    // Checking available values for _codingRate
    switch(cod)
    {
    case CR_5:
    case CR_6:
    case CR_7:
    case CR_8:
        return true;
        break;

    default:
        return false;
    }

}

/*
 Function: Indicates the CR within the module is configured.
 Returns: Integer that determines if there has been any error
   state = 2  --> The command has not been executed
   state = 1  --> There has been an error while executing the command
   state = 0  --> The command has been executed with no errors
   state = -1 --> Forbidden command for this protocol
*/
int8_t  SX1272::getCR()
{
    int8_t state = 2;
    uint8_t config1;
  
    
    if (_board==SX1272Chip) {
        // take out bits 7-3 from REG_MODEM_CONFIG1 indicates _bandwidth & _codingRate
        config1 = (readRegister(REG_MODEM_CONFIG1)) >> 3;
        config1 = config1 & B00000111;  // clears bits 7-3 ---> clears _bandwidth
    }
    else {
        // take out bits 7-1 from REG_MODEM_CONFIG1 indicates _bandwidth & _codingRate
        config1 = (readRegister(REG_MODEM_CONFIG1)) >> 1;
        config1 = config1 & B00000111;  // clears bits 7-3 ---> clears _bandwidth
    }

    _codingRate = config1;
    state = 1;

    if( (config1 == _codingRate) && isCR(_codingRate) )
    {
        state = 0;

    }

    return state;
}

/*
 Function: Sets the indicated CR in the module.
 Returns: Integer that determines if there has been any error
   state = 2  --> The command has not been executed
   state = 1  --> There has been an error while executing the command
   state = 0  --> The command has been executed with no errors
   state = -1 --> Forbidden command for this protocol
 Parameters:
   cod: coding rate value to set in LoRa modem configuration.
*/
int8_t  SX1272::setCR(uint8_t cod)
{
    int8_t error = 2;
    uint8_t st0;
    uint8_t config1;

    st0 = readRegister(REG_OP_MODE);        // Save the previous status

    // Set Standby mode to write in registers
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);      

    config1 = readRegister(REG_MODEM_CONFIG1);  // Save config1 to modify only the CR

    
    if (_board==SX1272Chip)
    {
        switch(cod)
        {
            case CR_5: 
                // clear bits 5 & 4 from REG_MODEM_CONFIG1
                config1 = (config1 | B00001000) & B11001111;  // sets bit 3 from REG_MODEM_CONFIG1
                break;
            case CR_6: 
                // clears bits 5 & 3 from REG_MODEM_CONFIG1
                config1 = (config1 | B00010000) & B11010111;  // sets bit 4 from REG_MODEM_CONFIG1
                break;
            case CR_7: 
                 // clears bit 5 from REG_MODEM_CONFIG1
                config1 = (config1 | B00011000) & B11011111;  // sets bits 4 & 3 from REG_MODEM_CONFIG1
                break;
            case CR_8: 
                // clears bits 4 & 3 from REG_MODEM_CONFIG1
                config1 = (config1 | B00100000) & B11100111;  // sets bit 5 from REG_MODEM_CONFIG1
                break;
        }
    }
    else if(_board==SX1276Chip) 
    {
        // SX1276
        config1 = config1 & B11110001;  // clears bits 3 - 1 from REG_MODEM_CONFIG1
        switch(cod)
        {
            case CR_5:
                config1 = config1 | B00000010;
                break;
            case CR_6:
                config1 = config1 | B00000100;
                break;
            case CR_7:
                config1 = config1 | B00000110;
                break;
            case CR_8:
                config1 = config1 | B00001000;
                break;
        }
    }
    else 
    {
        return -1; 
    }

    writeRegister(REG_MODEM_CONFIG1, config1);      // Update config1


    delay_ms(50);

    if(config1 != readRegister(REG_MODEM_CONFIG1))
    {
        return 1; 
    }


    writeRegister(REG_OP_MODE,st0); // Getting back to previous status
    delay_ms(100);
    return error;
}

/*
 Function: Checks if channel is a valid value.
 Returns: Boolean that's 'true' if the CR value exists and
          it's 'false' if the CR value does not exist.
 Parameters:
   ch: frequency channel value to check.
*/
boolean SX1272::isChannel(uint32_t ch)
{


    // Checking available values for _channel
    switch(ch)
    {
    case CH_10_868:
    case CH_11_868:
    case CH_12_868:
    case CH_13_868:
    case CH_14_868:
    case CH_15_868:
    case CH_16_868:
    case CH_17_868:
        //added by C. Pham
    case CH_18_868:
        //end
    case CH_00_900:
    case CH_01_900:
    case CH_02_900:
    case CH_03_900:
    case CH_04_900:
    case CH_05_900:
    case CH_06_900:
    case CH_07_900:
    case CH_08_900:
    case CH_09_900:
    case CH_10_900:
    case CH_11_900:
        return true;
        break;

    default:
        return false;
    }

}

/*
 Function: Indicates the frequency channel within the module is configured.
 Returns: Integer that determines if there has been any error
   state = 2  --> The command has not been executed
   state = 1  --> There has been an error while executing the command
   state = 0  --> The command has been executed with no errors
*/
uint8_t SX1272::getChannel()
{
    uint8_t state = 2;
    uint32_t ch;
    uint8_t freq3;
    uint8_t freq2;
    uint8_t freq1;

    freq3 = readRegister(REG_FRF_MSB);  // frequency channel MSB
    freq2 = readRegister(REG_FRF_MID);  // frequency channel MID
    freq1 = readRegister(REG_FRF_LSB);  // frequency channel LSB
    ch = ((uint32_t)freq3 << 16) + ((uint32_t)freq2 << 8) + (uint32_t)freq1;
    _channel = ch;                      // frequency channel

    if( (_channel == ch) && isChannel(_channel) )
    {
        state = 0;
    }
    else
    {
        state = 1;
    }
    return state;
}


int8_t SX1272::setChannel(uint32_t ch)
{
    int8_t error = 2;
    uint8_t freq1, freq2, freq3;
  
    // LoRa Stdby mode in order to write in registers
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);

    freq3 = ((ch >> 16) & 0x0FF);       // frequency channel MSB
    freq2 = ((ch >> 8) & 0x0FF);        // frequency channel MIB
    freq1 = (ch & 0xFF);                // frequency channel LSB

    error = writeReadRegister(REG_FRF_MSB, freq3);
    if(error != 0)
    {
        return -1; 
    }

    error = writeReadRegister(REG_FRF_MID, freq2);
    if(error != 0)
    {
        return -1; 
    }

    error = writeReadRegister(REG_FRF_LSB, freq1);
    if(error != 0)
    {
        return -1; 
    }

    return error;
}


uint8_t SX1272::getPower()
{
    return readRegister(REG_PA_CONFIG);
}


int8_t SX1272::setPower(char p)
{
     uint8_t st0;
    int8_t state = 2;
    byte value = 0x00;

    p = SX1272_POWER_MAXIMUM; 
    byte RegPaDacReg = (_board==SX1272Chip)?0x5A:0x4D;

    st0 = readRegister(REG_OP_MODE);      // Save the previous status
    
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);
 

    value = _power;

    if (p==SX1272_POWER_MAXIMUM)
     {
        // we set the PA_BOOST pin
        value = value | B10000000;
        // and then set the high output power config with register REG_PA_DAC
        writeRegister(RegPaDacReg, 0x87);
        // TODO: Have to set RegOcp for OcpOn and OcpTrim
    }
    else {
        // disable high power output in all other cases
        writeRegister(RegPaDacReg, 0x84);
    }
    
    if (_board==SX1272Chip) {
        writeRegister(REG_PA_CONFIG, value);    // Setting output power value
    }
    else {
        // set MaxPower to 7 -> Pmax=10.8+0.6*MaxPower [dBm] = 15
        value = value | B01110000;
        // then Pout = Pmax-(15-_power[3:0]) if  PaSelect=0 (RFO pin for +13dBm)
        writeRegister(REG_PA_CONFIG, value);
    }

    _power=value;

    writeRegister(REG_OP_MODE, st0);    // Getting back to previous status
    delay_ms(100);
    return state;
}


/**
 * Function: Gets the preamble length from the module.
 * @return preamble length 
 */
uint16_t SX1272::getPreambleLength()
{
    int8_t state = 0;
    uint16_t preamblelength = 0;
    uint8_t p_length;

    p_length = readRegister(REG_PREAMBLE_MSB_LORA);
    // Saving MSB preamble length in LoRa mode
    preamblelength = (p_length << 8) & 0xFFFF;

    p_length = readRegister(REG_PREAMBLE_LSB_LORA);
    // Saving LSB preamble length in LoRa mode
    preamblelength += (p_length & 0xFFFF);

    return preamblelength;
}

/**
 *  Function: Sets the preamble length in the module
 * @param  l desired preamble length 
 * @return   error state
 */
int8_t SX1272::setPreambleLength(uint16_t l)
{
    int8_t error = 0;
    uint8_t p_length;

    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);    // Set Standby mode to write in registers
    p_length = ((l >> 8) & 0x0FF);
    // Storing MSB preamble length in LoRa mode
    writeRegister(REG_PREAMBLE_MSB_LORA, p_length);
    p_length = (l & 0x0FF);
    // Storing LSB preamble length in LoRa mode
    writeRegister(REG_PREAMBLE_LSB_LORA, p_length);

    return error;
}

/**
 * Function: returns the stored node address 
 * @return node address
 */
uint8_t SX1272::getNodeAddress()
{
    return _nodeAddress;
}


int8_t SX1272::setNodeAddress(uint8_t addr)
{
    uint8_t st0;
    byte value;
    uint8_t error = 0;

    // Saving node address
    _nodeAddress = addr;
    st0 = readRegister(REG_OP_MODE);      // Save the previous status

    if( _modem == LORA )
    { // Allowing access to FSK registers while in LoRa standby mode
        writeRegister(REG_OP_MODE, LORA_STANDBY_FSK_REGS_MODE);
    }
    else
    { //Set FSK Standby mode to write in registers
        writeRegister(REG_OP_MODE, FSK_STANDBY_MODE);
    }

    // Storing node and broadcast address
    writeRegister(REG_NODE_ADRS, addr);
    writeRegister(REG_BROADCAST_ADRS, BROADCAST_0);

    value = readRegister(REG_NODE_ADRS);
    writeRegister(REG_OP_MODE, st0);        // Getting back to previous status

    if( value != _nodeAddress )
    {
        return -1;
    }

    return error;
}


/**
 * Function: returns the SNR value 
 * @return SNR value
 */
int8_t SX1272::getSNR()
{   
    int8_t SNR = readRegister(REG_PKT_SNR_VALUE);

    if( SNR & 0x80 ) // The SNR sign bit is 1
    {
        // Invert and divide by 4
        SNR = ( ( ~SNR + 1 ) & 0xFF ) >> 2;
        return -SNR;
    }
    else
    {
        // Divide by 4
        return  ( SNR & 0xFF ) >> 2;
    }

    return 0;
}


/**
 * Function: Returns a uint16_t representing the the rssi value of the most recent recieved packet
 * @return RSSI value 
 */
int16_t SX1272::getRSSIpacket()
{  
    int8_t SNR = 0;
    int16_t RSSIpacket =0;
    RSSIpacket = readRegister(REG_PKT_RSSI_VALUE);
   
    SNR = getSNR();

    if( SNR < 0 )
    {
        RSSIpacket = -(OFFSET_RSSI+(_board==SX1276Chip?20:0)) + (double)RSSIpacket + (double)SNR*0.25;
    }
    else
    {
        RSSIpacket = -(OFFSET_RSSI+(_board==SX1276Chip?20:0)) + (double)RSSIpacket;
    }

    return RSSIpacket;
}

/**
 * Function: sets the maximum current the SX127X device has access 
 * @param  rate current rate 
 * @return      error state 0 if no error 
 */
int8_t SX1272::setMaxCurrent(uint8_t rate)
{
    int8_t error = 0;
    uint8_t st0;

    // Maximum rate value = 0x1B, because maximum current supply = 240 mA
    if (rate > 0x1B)
    {
        rate = 0x1B; // set the requested rate to 240mA 
    }

    // Enable Over Current Protection
    rate |= B00100000;

    st0 = readRegister(REG_OP_MODE);    // Save the previous status
    
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);  // Set LoRa Standby mode to write in registers
    writeRegister(REG_OCP, rate);       // Modifying maximum current supply

    writeRegister(REG_OP_MODE, st0);        // Getting back to previous status

   
    return error;
}


/**
 * Function: Places SX1272 into RX mode 
 * @return error state 0 if no error 
 */
uint8_t SX1272::receive()
{
    uint8_t state = 1;

    // Initializing packet_received struct
    memset( &packet_received, 0x00, sizeof(packet_received) );

    writeRegister(REG_PA_RAMP, 0x08);

    writeRegister(REG_LNA, LNA_MAX_GAIN);
    writeRegister(REG_FIFO_ADDR_PTR, 0x00);  // Setting address pointer in FIFO data buffer


    if (_spreadingFactor == SF_10 || _spreadingFactor == SF_11 || _spreadingFactor == SF_12) {
        writeRegister(REG_SYMB_TIMEOUT_LSB,0x05);
    } else {
        writeRegister(REG_SYMB_TIMEOUT_LSB,0x08);
    }
    //end

    writeRegister(REG_FIFO_RX_BYTE_ADDR, 0x00); // Setting current value of reception buffer pointer


    packet_sent.length =MAX_LENGTH;

    // write out packet length 
    writeRegister(REG_PAYLOAD_LENGTH_LORA, packet_sent.length);

    writeRegister(REG_OP_MODE, LORA_RX_MODE);     // LORA mode - Rx

    return state;
}

/**
 * Function: Should only be called when DIO_0 is asserted, pulls recieved packet from the registers
 * @return      error state 0 if no error 
 */
int8_t SX1272::getPacket(uint16_t wait)
{
    uint8_t error = 0;
    byte value = 0x00;
    unsigned long previous;
    boolean p_received = false;


    value = readRegister(REG_IRQ_FLAGS);
    if(value && REG_IRQ_RXDONE_FLAG) // &&  (!(value, REG_IRQ_VALID_HEADER_FLAG) == 0) )
    {
        writeRegister(REG_FIFO_ADDR_PTR, 0x00);     // Setting address pointer in FIFO data buffer

        packet_received.dst = readRegister(REG_FIFO);   // Storing first byte of the received packet
        packet_received.type = readRegister(REG_FIFO);      // Reading second byte of the received packet
        packet_received.src = readRegister(REG_FIFO);       // Reading second byte of the received packet
        packet_received.packnum = readRegister(REG_FIFO);   // Reading third byte of the received packet

        packet_received.length = readRegister(REG_RX_NB_BYTES);

        _payloadlength=packet_received.length;
       
        for(unsigned int i = 0; i < _payloadlength; i++)
        {
            packet_received.data[i] = readRegister(REG_FIFO); // Storing payload
        }

        _payloadlength -= OFFSET_PAYLOADLENGTH;
        Serial.write( packet_received.data, _payloadlength);
        writeRegister(REG_FIFO_ADDR_PTR, 0x00);  // Setting address pointer in FIFO data buffer
        // 
        clearFlags();   
    }
  
    return error;
}


int8_t SX1272::checkTransmissionStatus(void)
{
    uint8_t error = 0;
    uint8_t value = 0x00;
    
    value = readRegister(REG_IRQ_FLAGS);

    if(!(value && REG_IRQ_TX_DONE_FLAG)) 
        error = -1;

 
    clearFlags();       
    return error;
}

int8_t SX1272::sendPacket(uint8_t dest, uint8_t *payload, uint8_t length)
{
    uint8_t error = 0;
    uint8_t st0;
    _payloadlength = length;
  

    st0 = readRegister(REG_OP_MODE);    // Save the previous status
    clearFlags();   // clear the flags 

    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);  // set lora module into standby mode
  
    // Sending new packet
    _destination = dest;    // Storing destination in a global variable
    packet_sent.dst = dest;  // Setting destination in packet structure
    packet_sent.src = _nodeAddress; // Setting source in packet structure
    packet_sent.packnum = _packetNumber;    // Setting packet number in packet structure
    packet_sent.length = _payloadlength + OFFSET_PAYLOADLENGTH;

    // write out packet length 
    writeRegister(REG_PAYLOAD_LENGTH_LORA, packet_sent.length);

    packet_sent.type = (PKT_TYPE_DATA | PKT_FLAG_DATA_WAPPKEY);

    // Setting address pointer in FIFO data buffer
    writeRegister(REG_FIFO_ADDR_PTR, 0x80);  

    // Writing packet to send in FIFO
    writeRegister(REG_FIFO, packet_sent.dst);       // Writing the destination in FIFO
    writeRegister(REG_FIFO, packet_sent.type);      // Writing the packet type in FIFO
    writeRegister(REG_FIFO, packet_sent.src);       // Writing the source in FIFO
    writeRegister(REG_FIFO, packet_sent.packnum);   // Writing the packet number in FIFO
   
    for(unsigned int i = 0; i < _payloadlength; i++)
    {
        writeRegister(REG_FIFO, payload[i]);  // Writing the payload in FIFO
    }

    writeRegister(REG_OP_MODE, st0);    // Getting back to previous status
   

    writeRegister(REG_OP_MODE, LORA_TX_MODE);  // LORA mode - Tx

    return error;
}



int8_t  SX1272::setSyncWord(uint8_t sw)
{
    int8_t error =0; 
    uint8_t st0;
    uint8_t config1;

    st0 = readRegister(REG_OP_MODE);        // Save the previous status
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);      // Set Standby mode to write in registers

    error = writeReadRegister(REG_SYNC_WORD, sw);
    if(error != 0)
    {
        return -1; 
    }

    _syncWord = sw;  
    writeRegister(REG_OP_MODE,st0); // Getting back to previous status
    delay_ms(100);
    return error;
}


int8_t SX1272::setSleepMode() 
{
    int8_t error = 0;
    byte value;

    // Request module is placed in sleep mode
    writeRegister(REG_OP_MODE, LORA_STANDBY_MODE);
    writeRegister(REG_OP_MODE, LORA_SLEEP_MODE);    
    
    value = readRegister(REG_OP_MODE);

    (value == LORA_SLEEP_MODE) ? ( error = 0) : (error = -1); 

    return error;
}
