#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include <string.h>
#include "esp_log.h" // added to allow logging to the terminal by LN /2/1/24
#include "esp_task_wdt.h" // add to feed the WDT so we dont time out
#include "JKBMSWrapper.h"
#include "jkbmsinterface.h"
#include "esp_wifi.h"

#include "mqtt_client.h"
#include "component_init.h"
#include "component_configuration.h"
#include "nvs_flash.h"

#include "esp_check.h"

#define EXAMPLE_TAG "VictronCanIntegration"
twai_message_t rx_message; // Object to hold incoming messages
esp_mqtt_client_handle_t client;

bool mqttConnected = false;

bool OverVoltAlarmRaised = false;
bool UnderVoltAlarmRaised = false;
bool OverCurrentAlarmRaised = false;
bool OverTempAlarmRaised = false;
bool LowTempAlarmRaised = false;

// define static thresholds
float VOLTAGE_THRESHOLD = 59.0;  // Example value for overvoltage threshold
float U_VOLTAGE_THRESHOLD = 46.0;  // Example value for overvoltage threshold
float CURRENT_THRESHOLD = 200.0;  // Example value for overcurrent threshold
float OVERTEMP_THRESHOLD = 60;
float Charge_Current_Target = 0;
float defaultAlarmClearModifier = 0.9; // when a value drops 10 below the threshold it should clear the alarm

float currentPackV = 0; // current pack voltage. 
#define Can_TX_GPIO_NUM             21
#define Can_RX_GPIO_NUM             22

static const char* hostname = "JKBMS";
static const char *mqttTAG = "mqtt_example";

char cfg_PackName[64];
char cfg_TopicName[64];

void SetBankAndModuleText(char *buffer, uint8_t cellid);

// pack values held here

int16_t packvoltage;
int16_t packcurrent;
int16_t packtemperature;

int8_t packStateOfCharge;
int8_t packStateOfHealth;
int8_t packStateOfChargeHR;
 
    bool packOverVoltAlarmRaised = false;
    bool packUnderVoltAlarmRaised = false;
    bool packOverCurrentAlarmRaised = false;
    bool packOverTempAlarmRaised = false;
    bool packLowTempAlarmRaised = false;
    bool packChargeEnabled = false;
    bool packDischargeEnabled = false;


void copy_array(int source[], int destination[], int size) {
    for (int i = 0; i < size; i++) {
        destination[i] = source[i];
    }
}

char *format_json(int8_t packStateOfCharge, int8_t packStateOfHealth, int8_t packStateOfChargeHR ,float packvoltage, float packcurrent, int16_t packtemperature,
                  bool OverVoltAlarmRaised, bool UnderVoltAlarmRaised, bool OverCurrentAlarmRaised,
                  bool OverTempAlarmRaised, bool LowTempAlarmRaised, bool ChargeEnabled, bool DischargeEnabled) {
    // Buffer to store the formatted JSON object
    static char json_buffer[512]; // Adjust the buffer size as needed
    

    // Format the JSON object
    //  snprintf(json_buffer, sizeof(json_buffer),
    //          "{"
    //          "\"State of Charge\": %i,"
    //          "\"State of Health\": %i,"
    //          "\"SOC Hi Res\": %i,"
    //          "\"packvoltage\": %.2f,"
    //          "\"packcurrent\": %.2f,"
    //          "\"packtemperature\": %d,"
    //          "\"OverVoltAlarmRaised\": %s,"
    //          "\"UnderVoltAlarmRaised\": %s,"
    //          "\"OverCurrentAlarmRaised\": %s,"
    //          "\"OverTempAlarmRaised\": %s,"
    //          "\"LowTempAlarmRaised\": %s,"
    //          "\"ChargeEnabled\": %s,"
    //          "\"DischargeEnabled\": %s"
    //          "}",
    //          packStateOfCharge, packStateOfHealth, packStateOfChargeHR,
    //          packvoltage, packcurrent, packtemperature,
    //          OverVoltAlarmRaised ? "true" : "false",
    //          UnderVoltAlarmRaised ? "true" : "false",
    //          OverCurrentAlarmRaised ? "true" : "false",
    //          OverTempAlarmRaised ? "true" : "false",
    //          LowTempAlarmRaised ? "true" : "false",
    //          ChargeEnabled ? "true" : "false",
    //          DischargeEnabled ? "true" : "false");

    snprintf(json_buffer, sizeof(json_buffer),
             "{"
             "\"State of Charge\": %i,"
             "\"State of Health\": %i,"
             "\"SOC Hi Res\": %i,"
             "\"packvoltage\": %.2f," // Use %f for float values
             "\"packcurrent\": %.2f," // Use %f for float values
             "\"packtemperature\": %d,"
             "\"OverVoltAlarmRaised\": %s,"
             "\"UnderVoltAlarmRaised\": %s,"
             "\"OverCurrentAlarmRaised\": %s,"
             "\"OverTempAlarmRaised\": %s,"
             "\"LowTempAlarmRaised\": %s,"
             "\"ChargeEnabled\": %s,"
             "\"DischargeEnabled\": %s"
             "}",
             packStateOfCharge, packStateOfHealth, packStateOfChargeHR,
             (packvoltage * 0.01), packcurrent, packtemperature,
             OverVoltAlarmRaised ? "true" : "false",
             UnderVoltAlarmRaised ? "true" : "false",
             OverCurrentAlarmRaised ? "true" : "false",
             OverTempAlarmRaised ? "true" : "false",
             LowTempAlarmRaised ? "true" : "false",
             ChargeEnabled ? "true" : "false",
             DischargeEnabled ? "true" : "false");
    
    return json_buffer;
}

char* concat_str(const char* str1, const char* str2, const char* str3) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t len3 = strlen(str3);

    // Allocate memory for the concatenated string
    char* result = (char*)malloc(len1 + len2 + len3 + 1); // +1 for the null terminator

    if (result == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Copy the strings into the result buffer
    strcpy(result, str1);
    strcat(result, str2);
    strcat(result, str3);

    return result;
}

typedef struct {
    char mqtt_host[64];
    int mqtt_port;
    char mqtt_username[64];
    char mqtt_password[64];
    char PackName[64];
    char TopicName[64];
} mqtt_config_t;

typedef struct { // Battery Limits
  
  uint16_t ChargeVoltage;          // U16 // 
  int16_t MaxChargingCurrent;      // S16
  int16_t MaxDischargingCurrent;   // S16
  uint16_t DischargeVoltageLimit;  // U16
} SmaCanMessage0x0351 ;

typedef struct  { // Soc and State of health
  
  uint16_t StateOfCharge;         // U16
  uint16_t StateOfHealth;         // U16
  uint16_t StateOfChargeHighRes;  // U16
}SmaCanMessage0x0355;

typedef struct 
  {
    uint16_t stateofchargevalue;
    uint16_t stateofhealthvalue;
    uint16_t highresolutionsoc;
  } VictronCanMessage0x355;


typedef struct  { // Battery actual values
  
  int16_t BatteryVoltage;      // S16
  int16_t BatteryCurrent;      // S16
  int16_t BatteryTemperature;  // S16
}SmaCanMessage0x0356;

typedef struct  { // not pylon tech
  
  uint32_t AlarmBitmask;    // 32 Bits
  uint32_t WarningBitmask;  // 32 Bits
}SmaCanMessage0x035A;

typedef struct  // pylon tech specific
{
  
  uint8_t ErrorBits;   // Overvoltage err, Undervoltage err, overtemperature err, under temp err, over current discharge, charge current error
  uint8_t SysErrorBits; // only 1 bit used for system error 3rd bit or location[2]
  uint8_t warningBits; // High voltage warn, Low voltage warn, hight temperature warn, low temp warn, high current discharge, charge current warn
  uint8_t internalerror; // 1 bit used for internal error 3rd bit or location[2]
  uint8_t SysStateBits; // 8 bits
}PlontechErrorWarnMessage0x0359;



typedef struct  { // matches pylontech
  
  char Model[8] ;
}SmaCanMessage0x035E;


typedef struct  { // is not a pylon tech message
  
    uint16_t BatteryModel;
    uint16_t Firmwareversion;
    uint16_t OnlinecapacityinAh;
  // uint16_t CellChemistry;
  // uint8_t HardwareVersion[2];
  // uint16_t NominalCapacity;
  // uint8_t SoftwareVersion[2];
}VictronCanMessage0x035F;

typedef struct  {
  
  uint16_t MinCellvoltage;  // v * 1000.0f
  uint16_t MaxCellvoltage;  // v * 1000.0f
  uint16_t MinTemperature;  // v * 273.15f
  uint16_t MaxTemperature;  // v * 273.15f
}Victron_message_0x373;

typedef struct 
{
    //uint8_t _ID; // 0x372
    uint16_t numberofmodulesok;
    uint16_t numberofmodulesblockingcharge;
    uint16_t numberofmodulesblockingdischarge;
    uint16_t numberofmodulesoffline;
} Victron_message_0x372;



typedef struct  {
  uint8_t _ID;
  int64_t Alive;
}NetworkAlivePacket;

typedef struct {
    uint8_t _ID;
    char text[8];
} SmaCanMessageCustom;

// Structs for each specific message

typedef struct {    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x374;

typedef struct {    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x375;

typedef struct {
    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x376;

typedef struct {
    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x377;

typedef struct 
    {
      char text[8];
    } candata;

NetworkAlivePacket alivePacket = {(uint8_t)0x0305, 1};

void SetFloatAsText(char *buffer, float floatValue) {
    // Use snprintf to convert float to text and store in buffer
    snprintf(buffer, 20, "%.2f", floatValue); // "%.2f" formats the float to two decimal places
}

void send_canbus_message(uint32_t id, uint8_t *data, size_t data_length) {
    //printf("Queuing Messages\n");
    for (int i = 0; i < 15; i++) {
        esp_task_wdt_reset();
        //printf("Message %i Queued\n", i);

        twai_message_t message;
        // Populate the fields of 'message' structure
        message.identifier = id; // Set the CAN message ID
        message.data_length_code = data_length; // Set the DLC (Data Length Code)
        memcpy(message.data, data, data_length); // Copy the data to the message

        int resp = twai_transmit(&message, pdMS_TO_TICKS(1000));

        if (resp == ESP_OK) {
            //printf("Message queued for transmission - code: %d\n", resp);
        } else {
            printf("Failed to queue message for transmission. Error code: %d\n", resp);
        }
    }
}

void pylon_message_359()
{
  typedef struct 
  {
    // Protection - Table 1
    uint8_t byte0;
    // Protection - Table 2
    uint8_t byte1;
    // Warnings - Table
    uint8_t byte2;
    // Warnings - Table 4
    uint8_t byte3;
    // Quantity of banks in parallel
    uint8_t byte4;
    uint8_t byte5;
    uint8_t byte6;
    // Online address of banks in parallel - Table 5
    uint8_t byte7;
  }data359;

    data359 data;

    memset(&data, 0, sizeof(data359));
    //data.byte0 = 0;
  

// Set the bit for OverVoltage alarm
if (OverVoltAlarmRaised) {
  printf("\n Raising an over voltage alarm for no reason what so ever ****** \n");
    //data.byte0 |= 0b00000010;  // Set the second bit
    data.byte2 = 0b00000010; 
}

// Set the bit for UnderVoltage alarm
if (UnderVoltAlarmRaised) {
    //data.byte0 |= 0b00000100;  // Set the third bit
    data.byte2 |= 0b00000100;
}

// Set the bit for High Temperature alarm
if (OverTempAlarmRaised) {
    //data.byte0 |= 0b00001000;  // Set the fourth bit
    data.byte2 |= 0b00001000;
}

// Set the bit for Low Temperature alarm
if (LowTempAlarmRaised) {
    // data.byte0 |= 0b00010000;  // Set the fifth bit
    data.byte2 |= 0b00010000;
}

data.byte2 = 0;
// Other bytes (assuming these are set according to some other logic in your program)
 // this is high voltage
// data.byte2 |= 0b00000100;
// data.byte2 |= 0b00001000;
// data.byte2 |= 0b00010000;

data.byte3 = 0;
data.byte4 = 1;
data.byte5 = 0x50; // 'P'
data.byte6 = 0x4E; // 'N'

    send_canbus_message(0x359, (uint8_t *)&data, sizeof(data359));
}

// 0x35C – C0 00 – Battery charge request flags //ylon_message_35c() pylon_message_35e()
void pylon_message_35c()
{
  typedef struct 
  {
    uint8_t byte0;
  }data35c; // these structs come from DIY BMS, a decision needs to be made as to whether or not they stay here or move to the top of the file with the others

  data35c data;

  // bit 0/1/2/3 unused
  // bit 4 Force charge 2
  // bit 5 Force charge 1
  // bit 6 Discharge enable
  // bit 7 Charge enable
  data.byte0 = 0;


    data.byte0  |= 0b10000000;
    data.byte0  |= 0b01000000;


  send_canbus_message(0x35c, (uint8_t *)&data, sizeof(data35c));
}

// 0x35E – 50 59 4C 4F 4E 20 20 20 – Manufacturer name ("PYLON ")
void pylon_message_35e()
{
  // Send 8 byte "magic string" PYLON (with 3 trailing spaces)
  // const char pylon[] = "\x50\x59\x4c\x4f\x4e\x20\x20\x20";
  uint8_t pylon[] = { (uint8_t)"B", (uint8_t)"A", (uint8_t)"D", (uint8_t)"I", (uint8_t)"D", (uint8_t)"E", (uint8_t)"A", (uint8_t)"!"};
  send_canbus_message(0x35e, (uint8_t *)&pylon, sizeof(pylon) - 1);
}

void Set_Charge_Limits()
{


}
// Battery voltage - 0x356 – 4e 13 02 03 04 05 – Voltage / Current / Temp
void pylon_message_356_2() // i created this second message type as i was seeing some unusual behavior. i am quite certain that there is nothing wrong with the first implementation
{
  typedef struct   
  {
    int16_t voltage;
    int16_t current;
    int16_t temperature;
  }data3562; // these structs come from DIY BMS, a decision needs to be made as to whether or not they stay here or move to the top of the file with the others

  data3562 data;

  // If current shunt is installed, use the voltage from that as it should be more accurate
    packcurrent = get_pack_current() * 10;
    packvoltage = get_pack_voltage() * 100.0;
    data.voltage = packvoltage;
    data.current = packcurrent;

  // Temperature 0.1 C using external temperature sensor
 
    data.temperature = (int16_t)25 * (int16_t)10;
 

  send_canbus_message(0x356, (uint8_t *)&data, sizeof(data3562));
}

void pylon_message_356()
{
  printf("Sending 356 packet, Voltage Current and Temperature");
  typedef struct 
  {
    int16_t voltage;
    int16_t current;
    int16_t temperature;
  }data356; // these structs come from DIY BMS, a decision needs to be made as to whether or not they stay here or move to the top of the file with the others

  data356 data;
    currentPackV = 0;
    float currentA =0;
    currentPackV = get_pack_voltage() ;  
    
    currentA = get_pack_current();
    float BMSTemp = get_temps(2);
    
    // for troubleshooting
    printf("Intermeddiate Voltage: %f\n",currentPackV);
    printf("Intermeddiate Amps: %f\n",currentA);
    printf("Intermeddiate BMSTemp: %f\n",BMSTemp );
    // end of  troubleshooting

    

    packvoltage = currentPackV * 100 ;//48 * 100.0;
    packcurrent = currentA  * 10;// * 10;  
    packtemperature = BMSTemp * 0.001;//(int16_t)25 * (int16_t)10; // assuming 25c 

    data.voltage = packvoltage;
    data.current = packcurrent;  
    data.temperature = packtemperature;//(int16_t)25 * (int16_t)10; // assuming 25c 
    send_canbus_message(0x356, (uint8_t *)&data, sizeof(data356));
    Set_Charge_Limits();
}

void victron_message_373(float lowestcellval, float highestcellval, float lowestcelltempval, float highestcelltempval )
{
//float lowestcellval, float highestcellval, float lowestcelltempval, float highestcelltempval 
  Victron_message_0x373 data;
  
  data.MinTemperature = 273 + lowestcelltempval; // rules.lowestExternalTemp; current lowest temp
  data.MaxTemperature = 273 + highestcelltempval ; // rules.highestExternalTemp; current highest Temp
  data.MaxCellvoltage = highestcellval * 1000;//rules.highestCellVoltage; current higest cell
  data.MinCellvoltage = lowestcellval  * 1000;//rules.lowestCellVoltage; current lowest cell

  send_canbus_message(0x373, (uint8_t *)&data, sizeof(Victron_message_0x373));
}

void victron_message_372(int ModulesOK, int ModulesBlockingC, int ModulesBlockingD, int OfflineModules)
{
  Victron_message_0x372 data;

  data.numberofmodulesok = ModulesOK ;//TotalNumberOfCells() - rules.invalidModuleCount; // these are overall units
  data.numberofmodulesblockingcharge = ModulesBlockingC; // units currently charging
  data.numberofmodulesblockingdischarge = ModulesBlockingD; // units currently blocking charging
  data.numberofmodulesoffline = OfflineModules;//rules.invalidModuleCount; // modules that are shut off - or shelves
  send_canbus_message(0x372, (uint8_t *)&data, sizeof(Victron_message_0x372));
}

void victron_message_351(int numactiveErrors, uint16_t CVL, int16_t MCC, int16_t MDC, uint16_t DV)
{
  typedef struct 
  {
    // CVL - 0.1V scale
    uint16_t chargevoltagelimit;
    // CCL - 0.1A scale
    int16_t maxchargecurrent;
    // DCL - 0.1A scale
    int16_t maxdischargecurrent;
    // Not currently used by Victron
    // 0.1V scale
    uint16_t dischargevoltage;
  }data351;

  data351 data;
  // do some maths here to figure out the charge shiz and maybe set alarms is we need to
  // Defaults (do nothing)
  // Don't use zero for voltage - this indicates to Victron an over voltage situation, and Victron gear attempts to dump
  // the whole battery contents!  (feedback from end users)
  data.chargevoltagelimit = CVL /100 ;//rules.lowestBankVoltage / 100; 
  data.maxchargecurrent = MCC;
  // Discharge settings....
  data.maxdischargecurrent = 0; // disabled discharge
  data.dischargevoltage = DV;//mysettings.dischargevolt;
  data.maxdischargecurrent = MDC;//mysettings.dischargecurrent;

  send_canbus_message(0x351, (uint8_t *)&data, sizeof(data351));
}

// S.o.C value
void victron_message_355(int stateofcharge, int stateofhealth, float highresolutionsoc)
{
  

    VictronCanMessage0x355 data;
    // 0 SOC value un16 1 %
    packStateOfCharge =  (int8_t)get_state_of_Charge();
    data.stateofchargevalue = packStateOfCharge; //;rules.StateOfChargeWithRulesApplied(&mysettings, currentMonitor.stateofcharge);
    // 2 SOH value un16 1 %
    packStateOfHealth = 100;
    data.stateofhealthvalue = packStateOfHealth; // calculate this based on delta difference in the cells 
    packStateOfChargeHR = get_state_of_Charge();
    data.highresolutionsoc = packStateOfChargeHR;
    

    send_canbus_message(0x355, (uint8_t *)&data, sizeof(VictronCanMessage0x355));
 // }
}

// i would like to break these up in to individual messages for keeping this clean
// but also at some point multiple variables for multiple BMS's will need to be shared amongst each other.
// the message creation will have to take all the BMS's in to account
void victron_message_374_375_376_377(int lowestcellnum, float lowestcellval, int highestcellnum, float highestcellval, int lowestcelltempid, float lowestcelltempval, int highestcelltempid, float highestcelltempval )
{
  struct candata
  {
    char text[8];
  };

  candata data;
    highestcellnum = get_cell_H_n();
    highestcellval = get_cell_H_v();
    lowestcellnum = get_cell_L_n();
    lowestcellval = get_cell_L_v();

    SetFloatAsText(data.text, lowestcellval );//;2.8);;
    SetBankAndModuleText(data.text, lowestcellnum);////rules.address_LowestCellVoltage);
    send_canbus_message(0x374, (uint8_t *)&data, sizeof(candata));

    SetFloatAsText(data.text, highestcellval );//;2.8);;  
    SetBankAndModuleText(data.text, highestcellnum );// rules.address_HighestCellVoltage);  
    send_canbus_message(0x375, (uint8_t *)&data, sizeof(candata));

    SetFloatAsText(data.text, lowestcelltempval );//;2.8);;  
    SetBankAndModuleText(data.text, lowestcelltempid  ); //rules.address_lowestExternalTemp);  
    send_canbus_message(0x376, (uint8_t *)&data, sizeof(candata));

    SetFloatAsText(data.text, highestcelltempval );//;2.8);;
    SetBankAndModuleText(data.text, highestcelltempid);//rules.address_highestExternalTemp);    
    send_canbus_message(0x377, (uint8_t *)&data, sizeof(candata));
  //}
}


// Transmit the DIYBMS hostname via two CAN Messages
void victron_message_370_371()
{
  char buffer[16+1];
  memset( buffer, 0, sizeof(buffer) );
  strncpy(buffer, hostname,sizeof(buffer)); // static const char* hostname = "JKBMS";

  send_canbus_message(0x370, (uint8_t *)&buffer[0], 8); //send_canbus_message(0x370, (const uint8_t *)&buffer[0], 8);
  send_canbus_message(0x371, (uint8_t *)&buffer[8], 8); //send_canbus_message(0x371, (const uint8_t *)&buffer[8], 8); 
}

void victron_message_35f(int BatModel, int OnlineCapacity, int firmwareMajor, int firmwareMinor)
{
  VictronCanMessage0x035F data;

  data.BatteryModel = BatModel;
  // Need to swap bytes for this to make sense.
  data.Firmwareversion = ((uint16_t)firmwareMajor << 8) | firmwareMinor; // figure out what goes in here

  data.OnlinecapacityinAh = OnlineCapacity ;// this value will need to be a total of all batteries

  send_canbus_message(0x35f, (uint8_t *)&data, sizeof(VictronCanMessage0x035F));
}

void set_defaultValues(){
// not yet implemented
}
void SetBankAndModuleText(char *buffer, uint8_t cellid) {
    uint8_t bank = cellid / 16;
    uint8_t module = cellid - (bank * 16); // Corrected the calculation
    memset(buffer, 0, 8);
    snprintf(buffer, 9, "b%d m%d", bank, module);
}

void loop_task(void *pvParameters) {
    while (1) {
        // Some of these methods take in data as arguments, this was for development only. Remediation is required here 
        pylon_message_35e(); // send pylon
        vTaskDelay(pdMS_TO_TICKS(50));
        // sends battery model, online capcity and  firmware major, firmware minor version 
        victron_message_35f(0, 200, 1, 2);
        vTaskDelay(pdMS_TO_TICKS(50));
        // sends host name 
        victron_message_370_371();
        vTaskDelay(pdMS_TO_TICKS(50));
        //int lowestcellnum, float lowestcellval, int highestcellnum, float highestcellval, int lowestcelltempid, float lowestcelltempval, int highestcelltempid, float highestcelltempval 
        victron_message_374_375_376_377(1,2.8,2,3.6,3,5.5,4,35); 
        vTaskDelay(pdMS_TO_TICKS(50));
        // state of charge, state of health, highres state of charge
        victron_message_355(75,100, 75.8); 
        vTaskDelay(pdMS_TO_TICKS(50));
        //void victron_message_351(int numactiveErrors, uint16_t CVL, int16_t MCC, int16_t MDC, uint16_t DV)
        victron_message_351(0,58, 200, 200, 48);        
        //void victron_message_372(int ModulesOK, int ModulesBlockingC, int ModulesBlockingD, int OfflineModules)
        victron_message_372(3, 0, 0, 1); //  is populated with example data, but data is not required. to be removed        
        //void victron_message_373(float lowestcellval, float highestcellval, float lowestcelltempval, float highestcelltempval )        
        pylon_message_35c();
        //pylon_message_35e();  // is duplicated ? 
        vTaskDelay(pdMS_TO_TICKS(50)); // adding delays for troubleshooting
        pylon_message_359();
        vTaskDelay(pdMS_TO_TICKS(50)); // adding delays for troubleshooting
        
        vTaskDelay(pdMS_TO_TICKS(400)); // Delay for 400 milliseconds for troubleshooting
        
        esp_task_wdt_reset();
        
        pylon_message_356_2();
    }
}

 void jkbmsTask(void *pvParameters) { // start the JKBMS task, this is a CPP interface. To be honest i dont think we need the wrapper....
    JKBMSWrapper* jkInstance = JKBMS_Create();
    JKBMS_Start(jkInstance);        
    JKBMS_Destroy(jkInstance);    
    vTaskDelete(NULL);
}

void parsePacketTask(void *pvParameters) {
    // Cast pvParameters to the appropriate type using C-style cast
    uint8_t* data = (uint8_t*)pvParameters;
    // i was trying to parse the received packet here but then went down another path.    
    vTaskDelete(NULL);
}

// Function to load MQTT configuration from NVS
esp_err_t load_existing_mqtt_config(mqtt_config_t *config) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("mqtt_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    size_t required_size;
    err = nvs_get_blob(nvs_handle, "config", NULL, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    if (required_size != sizeof(mqtt_config_t)) {
        nvs_close(nvs_handle);
        return ESP_ERR_NVS_INVALID_LENGTH;
    }

    err = nvs_get_blob(nvs_handle, "config", config, &required_size);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

char* add_mqtt_prefix(const char *uri) {
    // Check if "mqtt://" is already present
    if (strncmp(uri, "mqtt://", 7) != 0) {
        // Allocate memory for the new string, including "mqtt://" prefix
        char *new_uri = (char *)malloc(strlen(uri) + 8); // 7 for "mqtt://", 1 for null terminator
        if (new_uri == NULL) {
            // Memory allocation failed
            return NULL;
        }

        // Copy "mqtt://" prefix to the new string
        strcpy(new_uri, "mqtt://");
        
        // Concatenate the original URI to the new string
        strcat(new_uri, uri);

        return new_uri;
    } else {
        // If "mqtt://" is already present, return the original URI
        return strdup(uri);
    }
}

// Function to initialize MQTT and start publishing data
esp_err_t init_and_publish_mqtt_data() {
    mqtt_config_t mqtt_config;
    esp_err_t err = load_existing_mqtt_config(&mqtt_config);
    
    if (err != ESP_OK) {
        ESP_LOGE(mqttTAG, "Failed to load MQTT configuration from NVS (%d)", err);
        return err;
    }

    // Check if MQTT configuration is valid
    if (mqtt_config.mqtt_host[0] == '\0' || mqtt_config.mqtt_port == 0) {
        ESP_LOGE(mqttTAG, "Invalid MQTT configuration");
        return ESP_ERR_INVALID_STATE;
    }

    copy_array(cfg_PackName, mqtt_config.PackName, 64);
    copy_array(cfg_TopicName, mqtt_config.TopicName, 64);
     
    char *mqtt_uri = add_mqtt_prefix(mqtt_config.mqtt_host);

    // // Initialize MQTT client configuration
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_uri,
        .port = mqtt_config.mqtt_port,
        // Set username and password if available
        .username = mqtt_config.mqtt_username[0] != '\0' ? mqtt_config.mqtt_username : NULL,
        .password = mqtt_config.mqtt_password[0] != '\0' ? mqtt_config.mqtt_password : NULL,
    };

    // Initialize MQTT client
    client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client) {
        ESP_LOGE(mqttTAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    else {

    }

    // Start MQTT client
    err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(mqttTAG, "Failed to start MQTT client (%d)", err);
        esp_mqtt_client_destroy(client);
        return err;
    }
    // Once MQTT is initialized and connected, start publishing data
    // You can implement your data publishing logic here
    return ESP_OK;
}

void publishStats()
{
          char* statusTopic = concat_str(cfg_TopicName, cfg_PackName, "status");
          // char* cellsTopic = concat_str(cfg_TopicName, cfg_PackName, "cells");
          // char* packTopic = concat_str(cfg_TopicName, cfg_PackName, "pack");

            // data.voltage = packvoltage;
            // data.current = packcurrent;  
            // data.temperature = packtemperature;
           char *json_object = format_json(packStateOfCharge,packStateOfHealth,packStateOfChargeHR ,packvoltage, packcurrent, packtemperature,
                                    packOverVoltAlarmRaised, packUnderVoltAlarmRaised, packOverCurrentAlarmRaised,
                                    packOverTempAlarmRaised, packLowTempAlarmRaised, packChargeEnabled, packDischargeEnabled);
          esp_mqtt_client_publish(client, statusTopic, json_object, 0, 1, 0);

}

void app_main(void)
{
  set_defaultValues();  
   //Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(Can_TX_GPIO_NUM, Can_RX_GPIO_NUM, TWAI_MODE_NO_ACK);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    //Install CAN driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        printf("Driver installed\n");
    } else {
        printf("Failed to install driver\n");
        return;
    }

     if (twai_start() == ESP_OK) {
        printf("Driver started\n");
    } else {
        printf("Failed to start driver\n");
        return;
    }

    // Initialize the WiFi manager and other components
    initialize_wifi_manager();

    // Create even loop tasks for various componenets (CAN then JKBMS interface. Add more for MQTT, Influx etc)
    // This is the can send loop task, it sends the victron messages based on what is captured by the JKBMS task
    xTaskCreate(loop_task, "LoopTask", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 1, NULL);
    
    // this is the JKBMS task, it will read from the JKBMS serial port
    xTaskCreate(&jkbmsTask, "JKBMS Task", 4096, NULL, 5, NULL);   
 
    while(1){
      // main program loop, i was planning to work on the data sharing component and failover happening in this loop. 
      ESP_ERROR_CHECK(twai_receive(&rx_message, portMAX_DELAY));
      esp_task_wdt_reset();
       
       if(isWifiConnected()) // if(false == true )//
       {
         if(mqttConnected == false){
           esp_err_t err = init_and_publish_mqtt_data();
           if (err != ESP_OK) {
               ESP_LOGE(mqttTAG, "Failed to initialize and publish MQTT data (%d)", err);
            }
             else
             {
               mqttConnected = true;
             }
         }else
         {
          publishStats();
          
         }
      }
     else
     {
      //ESP_LOGE(mqttTAG, "Wifi is not connected");
     }
    }
}



