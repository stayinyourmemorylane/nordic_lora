
// Include the SX1272 
#include "SX1272.h"


SX1272 sx1272 = SX1272();
#define BAND868
//#define BAND900


// and SPI library on Arduino platforms
#include <SPI.h>
  
  #define PRINTLN                   Serial.println("")              
  #define PRINT_CSTSTR(fmt,param)   Serial.print(F(param))
  #define PRINT_STR(fmt,param)      Serial.print(param)
  #define PRINT_VALUE(fmt,param)    Serial.print(param)
  #define FLUSHOUTPUT               Serial.flush();



#define LORAMODE 4

#ifdef BAND868
  #define MAX_NB_CHANNEL 9
  #define STARTING_CHANNEL 10
  #define ENDING_CHANNEL 18
  uint8_t loraChannelIndex=0;
  uint32_t loraChannelArray[MAX_NB_CHANNEL]={CH_10_868,CH_11_868,CH_12_868,CH_13_868,CH_14_868,CH_15_868,CH_16_868,CH_17_868,CH_18_868};
#else // assuming #defined BAND900
  #define MAX_NB_CHANNEL 13
  #define STARTING_CHANNEL 0
  #define ENDING_CHANNEL 12
  uint8_t loraChannelIndex=5;
  uint32_t loraChannelArray[MAX_NB_CHANNEL]={CH_00_900,CH_01_900,CH_02_900,CH_03_900,CH_04_900,CH_05_900,CH_06_900,CH_07_900,CH_08_900,
                                            CH_09_900,CH_10_900,CH_11_900,CH_12_900};
#endif

// use the dynamic ACK feature of our modified SX1272 lib
#define GW_AUTO_ACK 



#define DEFAULT_DEST_ADDR 1

#define LORA_ADDR 1
// to unlock remote configuration feature
#define UNLOCK_PIN 1234
// will use 0xFF0xFE to prefix data received from LoRa, so that post-processing stage can differenciate
// data received from radio
#define WITH_DATA_PREFIX

#ifdef WITH_DATA_PREFIX
#define DATA_PREFIX_0 0xFF
#define DATA_PREFIX_1 0xFE

#endif



int dest_addr=DEFAULT_DEST_ADDR;

char sprintf_buf[100];
int msg_sn=0;

// number of retries to unlock remote configuration feature
uint8_t unlocked_try=3;
boolean unlocked=false;
boolean receivedFromSerial=false;
boolean receivedFromLoRa=false;
boolean withAck=false;


// configuration variables
//////////////////////////
bool radioON=false;
bool RSSIonSend=true;
uint8_t loraMode=LORAMODE;
uint32_t loraChannel=loraChannelArray[loraChannelIndex];
#if defined RADIO_RFM92_95 || defined RADIO_INAIR9B || defined RADIO_20DBM
// HopeRF 92W/95W and inAir9B need the PA_BOOST
// so 'x' set the PA_BOOST but then limit the power to +14dBm 
char loraPower='x';
#else
// other radio board such as Libelium LoRa or inAir9 do not need the PA_BOOST
// so 'M' set the output power to 15 to get +14dBm
char loraPower='M';
#endif
uint8_t loraAddr=LORA_ADDR;

unsigned int inter_pkt_time=0;
unsigned int random_inter_pkt_time=0;
long last_periodic_sendtime=0;

unsigned long startDoCad, endDoCad;
bool extendedIFS=true;
uint8_t SIFS_cad_number;
uint8_t send_cad_number=0;
uint8_t SIFS_value[11]={0, 183, 94, 44, 47, 23, 24, 12, 12, 7, 4};
uint8_t CAD_value[11]={0, 62, 31, 16, 16, 8, 9, 5, 3, 1, 1};

bool optAESgw=false;
uint16_t optBW=0; 
uint8_t optSF=0;
uint8_t optCR=0;
uint8_t optCH=0;
bool  optRAW=false;
double optFQ=-1.0;
uint8_t optSW=0x12;
  
//////////////////////////

#if defined ARDUINO && not defined _VARIANT_ARDUINO_DUE_X_ && not defined __MK20DX256__
int freeMemory () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
#endif


void startConfig() {

  int e;

  // has customized LoRa settings    
  if (optBW!=0 || optCR!=0 || optSF!=0) {

    e = sx1272.setCR(optCR-4);
    PRINT_CSTSTR("%s","^$LoRa CR ");
    PRINT_VALUE("%d", optCR);    
    PRINT_CSTSTR("%s",": state ");
    PRINT_VALUE("%d", e);
    PRINTLN;

    e = sx1272.setSF(optSF);
    PRINT_CSTSTR("%s","^$LoRa SF ");
    PRINT_VALUE("%d", optSF);    
    PRINT_CSTSTR("%s",": state ");
    PRINT_VALUE("%d", e);
    PRINTLN;
    
    e = sx1272.setBW( (optBW==125)?BW_125:((optBW==250)?BW_250:BW_500) );
    PRINT_CSTSTR("%s","^$LoRa BW ");
    PRINT_VALUE("%d", optBW);    
    PRINT_CSTSTR("%s",": state ");
    PRINT_VALUE("%d", e);
    PRINTLN;

    // indicate that we have a custom setting
    loraMode=0;
  
    if (optSF<10)
      SIFS_cad_number=6;
    else 
      SIFS_cad_number=3;
      
  }
  else {
    
    // Set transmission mode and print the result
    PRINT_CSTSTR("%s","^$LoRa mode ");
    PRINT_VALUE("%d", loraMode);
    PRINTLN;
        
    e = sx1272.setMode(loraMode);
    PRINT_CSTSTR("%s","^$Setting mode: state ");
    PRINT_VALUE("%d", e);
    PRINTLN;
  
  
    if (loraMode>7)
      SIFS_cad_number=6;
    else 
      SIFS_cad_number=3;

  }
  
  // Select frequency channel
  if (loraMode==11) {
    // if we start with mode 11, then switch to 868.1MHz for LoRaWAN test
    // Note: if you change to mode 11 later using command /@M11# for instance, you have to use /@C18# to change to the correct channel
    e = sx1272.setChannel(CH_18_868);
    PRINT_CSTSTR("%s","^$Channel CH_18_868: state ");    
  }
  else {
    // work also for loraMode 0
    e = sx1272.setChannel(loraChannel);

    if (optFQ>0.0) {
      PRINT_CSTSTR("%s","^$Frequency ");
      PRINT_VALUE("%f", optFQ);
      PRINT_CSTSTR("%s",": state ");      
    }
    else {
#ifdef BAND868      
      PRINT_CSTSTR("%s","^$Channel CH_1");
      PRINT_VALUE("%d", loraChannelIndex);
      PRINT_CSTSTR("%s","_868: state ");
#else
      PRINT_CSTSTR("%s","^$Channel CH_");
      PRINT_VALUE("%d", loraChannelIndex);
      PRINT_CSTSTR("%s","_900: state ");
#endif
    }
  }  
  PRINT_VALUE("%d", e);
  PRINTLN; 
  
  // Select output power (Max, High or Low)
  e = sx1272.setPower(loraPower);

  PRINT_CSTSTR("%s","^$Set LoRa Power to ");
  PRINT_VALUE("%c",loraPower);  
  PRINTLN;
                
  PRINT_CSTSTR("%s","^$Power: state ");
  PRINT_VALUE("%d", e);
  PRINTLN;
 
  // get preamble length
  e = sx1272.getPreambleLength();
  PRINT_CSTSTR("%s","^$Get Preamble Length: state ");
  PRINT_VALUE("%d", e);
  PRINTLN; 
  PRINT_CSTSTR("%s","^$Preamble Length: ");
  PRINT_VALUE("%d", sx1272._preamblelength);
  PRINTLN;
  
  // Set the node address and print the result
  //e = sx1272.setNodeAddress(loraAddr);
  sx1272._nodeAddress=loraAddr;
  e=0;
  PRINT_CSTSTR("%s","^$LoRa addr ");
  PRINT_VALUE("%d", loraAddr);
  PRINT_CSTSTR("%s",": state ");
  PRINT_VALUE("%d", e);
  PRINTLN;

  if (optAESgw)
      PRINT_CSTSTR("%s","^$Handle AES encrypted data\n");

  if (optRAW) {
      PRINT_CSTSTR("%s","^$Raw format, not assuming any header in reception\n");  
      // when operating n raw format, the SX1272 library do not decode the packet header but will pass all the payload to stdout
      // note that in this case, the gateway may process packet that are not addressed explicitly to it as the dst field is not checked at all
      // this would be similar to a promiscuous sniffer, but most of real LoRa gateway works this way 
      sx1272._rawFormat=true;
  }
  
  // Print a success message
  PRINT_CSTSTR("%s","^$SX1272/76 configured ");

  PRINT_CSTSTR("%s","as LR-BS. Waiting RF input for transparent RF-serial bridge\n");
 
}

void setup()
{
  int e;

  delay(3000);
  randomSeed(analogRead(14));

  // Open serial communications and wait for port to open:
  Serial.begin(38400);


  // Print a start message
  Serial.print(freeMemory());
  Serial.println(F(" bytes of free memory.")); 


  // Power ON the module
  e = sx1272.ON();
  
  PRINT_CSTSTR("%s","^$**********Power ON: state ");
  PRINT_VALUE("%d", e);
  PRINTLN;

  e = sx1272.getSyncWord();

  if (!e) 
  {
    PRINT_CSTSTR("%s","^$Default sync word: 0x");

    Serial.print(sx1272._syncWord, HEX);
 
    PRINTLN;
  }    

  if (optSW!=0x12) {
    e = sx1272.setSyncWord(optSW);

    PRINT_CSTSTR("%s","^$Set sync word to 0x");
    Serial.print(optSW, HEX);    
    PRINTLN;
    PRINT_CSTSTR("%s","^$LoRa sync word: state ");
    PRINT_VALUE("%d",e);  
    PRINTLN;
  }
  
  if (!e) {
    radioON=true;
    startConfig();
  }
  
  delay(1000);

}


void loop(void)
{ 
  int i=0, e;
  int cmdValue;

  receivedFromLoRa=false;


  if (radioON) 
  {
       delay(100);

      uint16_t w_timer=1000;
      
      if (loraMode==1)
        w_timer=2500;
        
      e=1;


      e = sx1272.receivePacketTimeout(w_timer);      
    PRINT_STR("%s","wait again");

      if (!e) 
      {
         int a=0, b=0;
         uint8_t tmp_length;

         receivedFromLoRa=true;
         sx1272.getSNR();
         sx1272.getRSSIpacket();

         tmp_length=sx1272._payloadlength;
         
         sprintf(sprintf_buf,"--- rxlora. dst=%d type=0x%.2X src=%d seq=%d len=%d SNR=%d RSSIpkt=%d BW=%d CR=4/%d SF=%d\n", 
                   sx1272.packet_received.dst,
                   sx1272.packet_received.type, 
                   sx1272.packet_received.src,
                   sx1272.packet_received.packnum,
                   tmp_length, 
                   sx1272._SNR,
                   sx1272._RSSIpacket,
                   (sx1272._bandwidth==BW_125)?125:((sx1272._bandwidth==BW_250)?250:500),
                   sx1272._codingRate+4,
                   sx1272._spreadingFactor);
                   
         PRINT_STR("%s",sprintf_buf);

         // provide a short output for external program to have information about the received packet
         // ^psrc_id,seq,len,SNR,RSSI
         sprintf(sprintf_buf,"^p%d,%d,%d,%d,%d,%d,%d\n",
                   sx1272.packet_received.dst,
                   sx1272.packet_received.type,                   
                   sx1272.packet_received.src,
                   sx1272.packet_received.packnum, 
                   tmp_length,
                   sx1272._SNR,
                   sx1272._RSSIpacket);
                   
         PRINT_STR("%s",sprintf_buf);          

         // ^rbw,cr,sf
         sprintf(sprintf_buf,"^r%d,%d,%d\n", 
                   (sx1272._bandwidth==BW_125)?125:((sx1272._bandwidth==BW_250)?250:500),
                   sx1272._codingRate+4,
                   sx1272._spreadingFactor);
                   
         PRINT_STR("%s",sprintf_buf);  

    
      }  
  }  
} 