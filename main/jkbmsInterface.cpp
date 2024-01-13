#include <stdio.h>
#include "jkbmsInterface.h"
 #include <sstream>
#include <iomanip>

#include <stdio.h>
#include <string.h>
// Extracted from vairous examples and the JKBMS library in ESPHome
// I just wanted a standalone library to work on a victron integration so used 
// two of the esp home examples and one from STM32. You will have to google
// esphome JKBMS as I dont recall the source of the other examples.


#include "driver/uart.h"
#include <stdint.h>
#include <vector>
#include "freertos/queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include <esp_log.h>
#include "esp_log.h"
#include <iostream>


#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>

#include "esp_task_wdt.h" 
#include <jkbmsInterface.h>
 

 // set this to be the same as the application
#define UART_PORT_NUM UART_NUM_0
#define UART_NUM UART_NUM_0

#define RX_BUFFER_SIZE 2048
int portNum = UART_PORT_NUM;
int rx_buffer_size = RX_BUFFER_SIZE;
uint32_t rx_timeout_ = 5000;
uint32_t taskDelay = 500;
QueueHandle_t uart_queue;
#define JK_TX_PIN 16
#define JK_RX_PIN 17
#define BAUD_RATE 115200
const int buffsize = 400;
int protocol_version = 1;
// these details pulled from ESP homes implementation


static const uint8_t FUNCTION_WRITE_REGISTER = 0x02;
static const uint8_t FUNCTION_READ_REGISTER = 0x03;
static const uint8_t FUNCTION_PASSWORD = 0x05;
static const uint8_t FUNCTION_READ_ALL_REGISTERS = 0x06;
static const uint8_t FUNCTION_READ_ALL = 0x06;

static const uint8_t ADDRESS_READ_ALL = 0x00;

static const uint8_t FRAME_SOURCE_GPS = 0x02;
static const uint8_t FRAME_SOURCE_PC = 0x03;
std::vector<uint8_t> rx_buffer;
std::vector<uint8_t> working_buffer;
std::vector<uint8_t> Result_Vector;
uint32_t last_jk_modbus_byte = 1;


// Cell voltages
float cell_voltages[] ={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
float pack_voltage = 0;
float pack_current = 0;
float temp_sensors[] = {0,0,0};
float cell_count = 0;
int highest_cell_V_num = 0;
int lowest_cell_V_num = 0;
float highest_cell_V = 0;
float lowest_cell_V = 0;
float state_of_charge =0;

#include "jkbmsinterface.h"

// expose functions to C 
extern "C" {
    const uint8_t* get_vector_data() {
        return Result_Vector.data();
    }

    size_t get_vector_size() {
        return Result_Vector.size();
    }

    float get_cell_voltage(int cellnum)
    {
      return cell_voltages[cellnum];
    }
    float get_pack_voltage()
    {
      return pack_voltage;
    }
    float get_pack_current()
    {
      return pack_current;
    }
    float get_cell_count()
    {
      return cell_count;
    }
    int get_cell_H_n()
    {
      return highest_cell_V_num;

    }
    int get_cell_L_n()
    {
      return lowest_cell_V_num;
    }
     int get_cell_H_v()
    {
      return highest_cell_V;

    }
    int get_cell_L_v()
    {
      return lowest_cell_V;
    }

    int get_temps(int tempnum)
    {
      return temp_sensors[tempnum];
    }
    float get_state_of_Charge()
    {
      return state_of_charge;
    }
}
// this is intended to be passed through to the C main thread to see if the quantity of data available
// partially implemented
const uint8_t* get_vector_data(size_t* length) {
    if (length) {
        *length = Result_Vector.size();
    }
    return Result_Vector.data();
}

// this is intended to be passed through to the C main thread to see if the data is available
// partially implemented
bool is_data_available() {
    return !Result_Vector.empty();
}
#define TAG "VictronCanIntegration-JK"


// pulled from the header file in esphomes repo. But i dont think we need this any more. 
// personally i dont have the other JK BMS types so i dont have use for it. 
// if someone else gets involved and wants to use different protocols we can re-implement
  float get_current_(const uint8_t value, const uint8_t protocol_version) {
    float current = 0.0f;
    if (protocol_version == 0x01) {
      if ((value & 0x8000) == 0x8000) {
        current = (float) (value & 0x7FFF);
      } else {
        current = (float) (value & 0x7FFF) * -1;
      }
    }
    printf("Returning Current value of %f", current);
    return current;
  };

// helper functions for debuging, will be removed or shifted to a helper class to clean up this file

// JK device portion
void dumpVectorAsHexAndFloat(const std::vector<uint8_t>& data, float offset, float multiplier) {
    std::stringstream ss;
    for (uint8_t byte : data) {
        // Calculate float representation
        float floatValue = static_cast<float>(byte) * multiplier + offset;

        // Print as hexadecimal and float
        ss << "(hex: 0x" 
           << std::uppercase 
           << std::setw(2) 
           << std::setfill('0') 
           << std::hex 
           << static_cast<unsigned int>(byte) 
           << ", " 
           << std::dec 
           << std::fixed 
           << std::setprecision(2) // Adjust precision as needed
           << floatValue 
           << ") ";
    }
    // Replace with your logging system, here I use std::cout for demonstration
    std::cout << ss.str() << std::endl;
    // If using ESP_LOGI: ESP_LOGI("VectorDump", "%s", ss.str().c_str());
}
void dumpVectorAsHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    for (uint8_t byte : data) {
        // Calculate float representation
        

        // Print as hexadecimal and float
        ss << "0x" 
           << std::uppercase 
           << std::setw(2) 
           << std::setfill('0') 
           << std::hex 
           << static_cast<unsigned int>(byte)            
           << ", ";
    }
    // Replace with your logging system, here I use std::cout for demonstration
    std::cout << ss.str() << std::endl;
    // If using ESP_LOGI: ESP_LOGI("VectorDump", "%s", ss.str().c_str());
}

void dumpVectorAsInt(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    for (uint8_t byte : data) {
        // Print only as integer
        ss << static_cast<unsigned int>(byte) << " ";
    }
    
     ESP_LOGI("VectorDump", "%s", ss.str().c_str());
    // If using ESP_LOGI: ESP_LOGI("VectorDump", "%s", ss.str().c_str());
}

void dumpVector(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        ESP_LOGI("VectorDump", "The vector is empty.");
        return;
    }

    std::stringstream ss;
    for (uint8_t byte : data) {
        ss << "0x" 
           << std::uppercase 
           << std::setw(2) 
           << std::setfill('0') 
           << std::hex 
           << static_cast<unsigned int>(byte) 
           << " (int: " 
           << std::dec 
           << static_cast<int>(byte) 
           << ", uint: " 
           << static_cast<unsigned int>(byte) 
           << ", char: " 
           << static_cast<char>(byte) 
           << ") ";

        // Debug log for each byte (optional)
        ESP_LOGI("ByteDebug", "Processing byte: 0x%X", byte);
    }
    ESP_LOGI("VectorDump", "%s", ss.str().c_str());
}

// end of helper functions for debuging - will be removed or shifted to a helper class to clean up this file

// void dumpVector(const std::vector<uint8_t>& data) {
//     std::stringstream ss;
//     for (uint8_t byte : data) {
//         // Print as hexadecimal
//         ss << "0x" 
//            << std::uppercase 
//            << std::setw(2) 
//            << std::setfill('0') 
//            << std::hex 
//            << static_cast<unsigned int>(byte) 
//            << " (int: " 
//            << std::dec 
//            << static_cast<int>(byte) 
//            << ", uint: " 
//            << static_cast<unsigned int>(byte) 
//            << ", char: " 
//            << static_cast<char>(byte) 
//            << ") ";
//     }
//     ESP_LOGI("VectorDump", "%s", ss.str().c_str());
// }



void delay(uint32_t milliseconds) {
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

void flush() {
    rx_buffer.clear();
}

// extracted from ESP homes JKBMS implementation
// Function to write an array of bytes to UART
void write_array(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        ESP_LOGE("write_array", "Invalid input data");
        return;
    }

    int bytes_written = uart_write_bytes(UART_PORT_NUM, (const char*)data, length);

    if (bytes_written < 0) {
        ESP_LOGE("write_array", "UART write failed");
    } else {
        //ESP_LOGI("write_array", "Sent %d bytes", bytes_written);
    }
}

uint16_t chksum(const uint8_t data[], const uint16_t len) {
  uint16_t checksum = 0;
  for (uint16_t i = 0; i < len; i++) {
    checksum = checksum + data[i];
  }
  return checksum;
}

uint32_t millis()
{
return esp_timer_get_time() / 1000;
}


// Simplified packet capture/parse function -  everything happens inside of here. The danger is we will need to store it for retreival.
// so we might start hitting some resource constraints if we dont keep this cleaned up
bool parse_packet(const std::vector<uint8_t>& buffer) {
  printf("Start parse packet\n");
  // Check for minimum length (start sequence + length + checksum)
  if (buffer.size() < 4) {
    printf("Buffer to small less than 4");
    return false; // Not enough data
  }

  // Start sequence check (0x4E followed by 0x57)
  if (buffer[0] != 0x4E || buffer[1] != 0x57) {
    printf("\n Invalid start sequence \n");
    //ESP_LOGW(TAG, "Invalid start sequence");
    return false;
  }

  // Calculate packet length
  uint16_t packet_length = (static_cast<uint16_t>(buffer[2]) << 8) | static_cast<uint16_t>(buffer[3]);

  printf("Expected Packet Length: %i \n ", packet_length); //289
  printf("Buffer size at check: %i", buffer.size()); // 291
  // Verify packet length
  
  if (buffer.size() < packet_length + 2 ) { // +4 for start sequence and length bytes
    printf("Buffer to small");
    return false; // Not enough data for the full packet
  }

  // Checksum calculation and verification
  // uint16_t computed_checksum = chksum(buffer.data() + 2, packet_length); // Checksum over the packet data
  // uint16_t received_checksum = (static_cast<uint16_t>(buffer[packet_length + 2]) << 8) | static_cast<uint16_t>(buffer[packet_length + 5]);

  // if (computed_checksum != received_checksum) {
  //   printf("Checksum mismatch");
  //   //ESP_LOGW(TAG, "Checksum mismatch");
  //   return false;
  // }

  // Process packet data
  std::vector<uint8_t> packet_data(buffer.begin() + 4, buffer.begin() + 4 + packet_length);
  
  auto jk_get_16bit = [&](size_t i) -> uint16_t { return (uint16_t(buffer[i + 0]) << 8) | (uint16_t(buffer[i + 1]) << 0); };
  auto jk_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(jk_get_16bit(i + 0)) << 16) | (uint32_t(jk_get_16bit(i + 2)) << 0);
  };

  printf("PACKET FOUND!!! \n");
  Result_Vector = buffer;
  int startIdx = 13;
  int cellvalue = 1;
  cell_count = buffer[12] / 3;

  double maxVoltage = 0.0; // To store the maximum voltage
  double minVoltage = 10000.0; // To store the minimum voltage, initialized with a very high value
  double totalVoltage = 0.0; // To store the sum of all voltages for calculating average
  
  for (int i = startIdx; i < buffer.size(); i += 3) {
        uint8_t index = buffer[i];

        // Check if there are enough bytes left to form a two-byte value
        if (i + 2 >= buffer.size()) {
            break;
        }
        // Combine the bytes into a two-byte value
        // Assuming little-endian order for this example
        int value = (buffer[i + 1] << 8) | buffer[i + 2];

        double scaledValue = value * 0.001;
        cell_voltages[cellvalue] = scaledValue;
        //printf("Cell %i Raw Value %i \n", cellvalue, value );
        //printf("Cell %i V: %lf \n", cellvalue,scaledValue);
        

        // Update max, min, and total voltages
        if (scaledValue > maxVoltage) {
            maxVoltage = scaledValue;
            highest_cell_V_num = cellvalue;

        }
        if (scaledValue < minVoltage) {
            minVoltage = scaledValue;
            lowest_cell_V_num = cellvalue;
        }
        totalVoltage += scaledValue;

        cellvalue ++;
        if(cellvalue > cell_count){
          break;
        }
    }
  pack_voltage = totalVoltage;
  highest_cell_V = maxVoltage;
  lowest_cell_V = minVoltage;

  // i started adding error checking here, or atleast some form of validity check, need to go back over the 
  // previous work and replace it with the same sort of checks to ensure we are getting reasonable data  
  if(buffer[73] ==  0x84){
  uint16_t raw_value = (static_cast<uint16_t>(buffer[73 + 1]) << 8) | static_cast<uint16_t>(buffer[73 + 2]);
  bool is_charging = (raw_value & 0x8000) != 0;  // Check if MSB is set
  int magnitude = (is_charging) ? (raw_value - 32768) : raw_value;  // Adjust for charging
  pack_current = -(magnitude * 0.01f);  // Convert from 10 mA units to Amperes
  
  if (is_charging) {
    // Handle charging case
    pack_current = -pack_current; // Invert current for charging
  }
  }

  if (buffer[61] == 0x80) {
    uint16_t rawTemp = (static_cast<uint16_t>(buffer[74]) << 8) | static_cast<uint16_t>(buffer[75]);    
    temp_sensors[0] = (rawTemp > 100) ? (rawTemp - 100) * -1 : rawTemp;
    // Use powerTubeTemp as needed, ? raise alarms here or in the main thread ? 
  }
  if (buffer[64] == 0x81) {
    uint16_t rawTemp = (static_cast<uint16_t>(buffer[74]) << 8) | static_cast<uint16_t>(buffer[75]);
    temp_sensors[1] = (rawTemp > 100) ? (rawTemp - 100) * -1 : rawTemp;
    // Use batteryBoxTemp as needed, ? raise alarms here or in the main thread ? 
  }
  if (buffer[67] == 0x82) {
    uint16_t rawTemp = (static_cast<uint16_t>(buffer[74]) << 8) | static_cast<uint16_t>(buffer[75]);
    temp_sensors[2] = (rawTemp > 100) ? (rawTemp - 100) * -1 : rawTemp;

    // Use batteryTemp as needed, ? raise alarms here or in the main thread ? 
}
  
  if (buffer[76] == 0x85) {
    state_of_charge = buffer[77];   
    printf("State of charge value : %f \n", state_of_charge);
  }

  // // 0x85 0x0F: Battery remaining capacity                       15 %
  // uint8_t raw_battery_remaining_capacity = data[offset + 3 * 5];
  // this->publish_state_(this->capacity_remaining_sensor_, (float) raw_battery_remaining_capacity);

  // // 0x86 0x02: Number of battery temperature sensors             2                        1.0  count
  // this->publish_state_(this->temperature_sensors_sensor_, (float) data[offset + 2 + 3 * 5]);

  // // 0x87 0x00 0x04: Number of battery cycles                     4                        1.0  count
  // this->publish_state_(this->charging_cycles_sensor_, (float) jk_get_16bit(offset + 4 + 3 * 5));

  // // 0x89 0x00 0x00 0x00 0x00: Total battery cycle capacity
  // this->publish_state_(this->total_charging_cycle_capacity_sensor_, (float) jk_get_32bit(offset + 4 + 3 * 6));

  // // 0x8A 0x00 0x0E: Total number of battery strings             14                        1.0  count
  // this->publish_state_(this->battery_strings_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 7));

    // -----------------------------------------------------------------------------------------
  return true;
}



void send(uint8_t function, uint8_t address, uint8_t value) {
  uint8_t frame[22];
  frame[0] = 0x4E;      // start sequence
  frame[1] = 0x57;      // start sequence
  frame[2] = 0x00;      // data length lb
  frame[3] = 0x14;      // data length hb
  frame[4] = 0x00;      // bms terminal number
  frame[5] = 0x00;      // bms terminal number
  frame[6] = 0x00;      // bms terminal number
  frame[7] = 0x00;      // bms terminal number
  frame[8] = function;  // command word: 0x01 (activation), 0x02 (write), 0x03 (read), 0x05 (password), 0x06 (read all)
  frame[9] = FRAME_SOURCE_PC;  // frame source: 0x00 (bms), 0x01 (bluetooth), 0x02 (gps), 0x03 (computer)
  frame[10] = 0x00;             // frame type: 0x00 (read data), 0x01 (reply frame), 0x02 (BMS active upload)
  frame[11] = 0x00;//address;          // register: 0x00 (read all registers), 0x8E...0xBF (holding registers)
  frame[12] = value;            // data
  frame[13] = 0x00;             // record number
  frame[14] = 0x00;             // record number
  frame[15] = 0x00;             // record number
  frame[16] = 0x00;             // record number
  frame[17] = 0x68;             // end sequence
  auto crc = chksum(frame, 18);
  frame[18] = 0x00;  // crc unused
  frame[19] = 0x00;  // crc unused
  frame[20] = crc >> 8;
  frame[21] = crc >> 0;

  write_array(frame, 22);
  flush();
  
}

void authenticate_() { send(FUNCTION_PASSWORD, 0x00, 0x00); }

void write_register(uint8_t address, uint8_t value) {
  authenticate_();
  delay(150);  // NOLINT
  send(FUNCTION_WRITE_REGISTER, address, value);
}

void read_registers() {
  uint8_t frame[21];
  frame[0] = 0x4E;                         // start sequence
  frame[1] = 0x57;                         // start sequence
  frame[2] = 0x00;                         // data length lb
  frame[3] = 0x13;                         // data length hb
  frame[4] = 0x00;                         // bms terminal number
  frame[5] = 0x00;                         // bms terminal number
  frame[6] = 0x00;                         // bms terminal number
  frame[7] = 0x00;                         // bms terminal number
  frame[8] = FUNCTION_READ_ALL_REGISTERS;  // command word: 0x01 (activation), 0x02 (write), 0x03 (read), 0x05
                                           // (password), 0x06 (read all)
  frame[9] = FRAME_SOURCE_GPS;             // frame source: 0x00 (bms), 0x01 (bluetooth), 0x02 (gps), 0x03 (computer)
  frame[10] = 0x00;                        // frame type: 0x00 (read data), 0x01 (reply frame), 0x02 (BMS active upload)
  frame[11] = ADDRESS_READ_ALL;            // register: 0x00 (read all registers), 0x8E...0xBF (holding registers)
  frame[12] = 0x00;                        // record number
  frame[13] = 0x00;                        // record number
  frame[14] = 0x00;                        // record number
  frame[15] = 0x00;                        // record number
  frame[16] = 0x68;                        // end sequence
  auto crc = chksum(frame, 17);
  frame[17] = 0x00;  // crc unused
  frame[18] = 0x00;  // crc unused
  frame[19] = crc >> 8;
  frame[20] = crc >> 0;

  write_array(frame, 21);
  flush();
  //printf("Frame Sent, Buffer Cleared");
}
void read_all() {
  uint8_t frame[21];
  frame[0] = 0x4E;                         // start sequence
  frame[1] = 0x57;                         // start sequence
  frame[2] = 0x00;                         // data length lb
  frame[3] = 0x13;                         // data length hb
  frame[4] = 0x00;                         // bms terminal number
  frame[5] = 0x00;                         // bms terminal number
  frame[6] = 0x00;                         // bms terminal number
  frame[7] = 0x00;                         // bms terminal number
  frame[8] = FUNCTION_READ_ALL;  // command word: 0x01 (activation), 0x02 (write), 0x03 (read), 0x05
                                           // (password), 0x06 (read all)
  frame[9] = FRAME_SOURCE_GPS;             // frame source: 0x00 (bms), 0x01 (bluetooth), 0x02 (gps), 0x03 (computer)
  frame[10] = 0x00;                        // frame type: 0x00 (read data), 0x01 (reply frame), 0x02 (BMS active upload)
  frame[11] = ADDRESS_READ_ALL;            // register: 0x00 (read all registers), 0x8E...0xBF (holding registers)
  frame[12] = 0x00;                        // record number
  frame[13] = 0x00;                        // record number
  frame[14] = 0x00;                        // record number
  frame[15] = 0x00;                        // record number
  frame[16] = 0x68;                        // end sequence
  auto crc = chksum(frame, 17);
  frame[17] = 0x00;  // crc unused
  frame[18] = 0x00;  // crc unused
  frame[19] = crc >> 8;
  frame[20] = crc >> 0;

  write_array(frame, 21);
  flush();
  //printf("Frame Sent, Buffer Cleared");
}

bool findStartCharacters(const std::vector<uint8_t>& data, uint8_t char1) {
    for (size_t i = 0; i < data.size() - 1; ++i) {
        if (data[i] == char1 ) {
            // Found the start characters
            return true;
        }
    }
    // Start characters not found
    return false;
}
 
void uartRxHandler(void *arg) {
    uart_event_t event;
    size_t bufferedSize;
    int rx_timeout_ = 5000;
    uint32_t now = 0;
    bool packet_start = false;
    if (uart_driver_install(portNum, rx_buffer_size, rx_buffer_size, 10, &uart_queue, 0) == ESP_OK) {
        read_all();

        while (1) {
            esp_task_wdt_reset();
            now = millis();

            if(now - last_jk_modbus_byte > rx_timeout_) {
                ESP_LOGV(TAG, "Buffer cleared due to timeout, size at clear %i \n", working_buffer.size());
                working_buffer.clear();
                read_all();
                vTaskDelay(500 / portTICK_PERIOD_MS);
                printf("Buffer Cleared due to timeout, size at clear %i \n", working_buffer.size());
            }

            if (xQueueReceive(uart_queue, (void *)&event, (portTickType)200)) {
                if (event.type == UART_DATA) {
                    uart_get_buffered_data_len(portNum, &bufferedSize);
                    std::vector<uint8_t> tempBuffer(bufferedSize);
                    printf("Buffer Size:%i \n", bufferedSize);

                    if(bufferedSize > 0){
                        uart_read_bytes(portNum, tempBuffer.data(), bufferedSize, portMAX_DELAY);                        
                        // Append received bytes to the working buffer
                        working_buffer.insert(working_buffer.end(), tempBuffer.begin(), tempBuffer.end());                        
                        // Try to parse packet
                        if (parse_packet(working_buffer)) {
                          // the parse_packet method appends bytes to a working buffer 
                          // when the packet is complete it triggers the processing 
                          // of the complete packet. 
                          // once the packet is processed we clear the buffer for the next packet                            
                            working_buffer.clear(); // or remove only the processed packet
                        }
                        else{
                            // handle bad packet
                        }
                    }
                }
            }

            esp_task_wdt_reset();
            //dumpVector(working_buffer);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}
  
void JKBMS::start(){
    uart_config_t uartConfig;
    uartConfig.baud_rate = 115200;
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.rx_flow_ctrl_thresh = 122,
    uart_param_config(portNum, &uartConfig);    
    uart_set_pin(0, JK_TX_PIN, JK_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);    
    // Create a task to handle UART events    
    xTaskCreate(uartRxHandler, "uart_rx_handler", 4096, NULL, 10, NULL);  
    const int uart_buffer_size = (1024 * 2);
    
    QueueHandle_t uart_queue;    
    esp_err_t uart_install_status = uart_driver_install(1, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0);
    vTaskDelay(pdMS_TO_TICKS(taskDelay));
    //const uint32_t timeout = 2000; // Timeout in milliseconds

 
    int request_counter = 1;
    int bufferSize = 0;
    read_all();
    // JK BMS main loop. data is received and processed in the UART task
    while(1){       
        esp_task_wdt_reset();      
        delay(100);  
      }
}  


//Kept for reference, lots left to implement and this is a good example
// void on_status_data_(const std::vector<uint8_t> &data) {
//   auto jk_get_16bit = [&](size_t i) -> uint16_t { return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0); };
//   auto jk_get_32bit = [&](size_t i) -> uint32_t { return (uint32_t(jk_get_16bit(i + 0)) << 16) | (uint32_t(jk_get_16bit(i + 2)) << 0);};
//   //dumpVector(data);
//   ESP_LOGI(TAG, "Status frame received");
//   printf(" \n   DATA SIZE    - %i \n \n",data.size());
  
//   // Status request
//   // -> 0x4E 0x57 0x00 0x13 0x00 0x00 0x00 0x00 0x06 0x03 0x00 0x00 0x00 0x00 0x00 0x00 0x68 0x00 0x00 0x01 0x29
//   //
//   // Status response
//   // <- 0x4E 0x57 0x01 0x1B 0x00 0x00 0x00 0x00 0x06 0x00 0x01: Header
//   //
//   // *Data*
//   //
//   // Address Content: Description      Decoded content                         Coeff./Unit
//   // 0x79: Individual Cell voltage
//   // 0x30: Cell count               48 / 3 bytes = 16 cells
//   // 0x01 0x0E 0xED: Cell 1         3821 * 0.001 = 3.821V                        0.001 V
//   // 0x02 0x0E 0xFA: Cell 2         3834 * 0.001 = 3.834V                        0.001 V
//   // 0x03 0x0E 0xF7: Cell 3         3831 * 0.001 = 3.831V                        0.001 V
//   // 0x04 0x0E 0xEC: Cell 4         ...                                          0.001 V
//   // 0x05 0x0E 0xF8: Cell 5         ...                                          0.001 V
//   // 0x06 0x0E 0xFA: Cell 6         ...                                          0.001 V
//   // 0x07 0x0E 0xF1: Cell 7         ...                                          0.001 V
//   // 0x08 0x0E 0xF8: Cell 8         ...                                          0.001 V
//   // 0x09 0x0E 0xE3: Cell 9         ...                                          0.001 V
//   // 0x0A 0x0E 0xFA: Cell 10        ...                                          0.001 V
//   // 0x0B 0x0E 0xF1: Cell 11        ...                                          0.001 V
//   // 0x0C 0x0E 0xFB: Cell 12        ...                                          0.001 V
//   // 0x0D 0x0E 0xFB: Cell 13        ...                                          0.001 V
//   // 0x0E 0x0E 0xF2: Cell 14        3826 * 0.001 = 3.826V                        0.001 V
//   // 0x0F 0x0E 0xF3: Cell 15        3827 * 0.001 = 3.827V                        0.001 V
//   // 0x10 0x0E 0xF4: Cell 16        3828 * 0.001 = 3.828V                        0.001 V
    
//   uint8_t cells = data[1] / 3;

//   float min_cell_voltage = 100.0f;
//   float max_cell_voltage = -100.0f;
//   float average_cell_voltage = 0.0f;
//   uint8_t min_voltage_cell = 0;
//   uint8_t max_voltage_cell = 0;
//   for (uint8_t i = 0; i < cells; i++) {
//     float cell_voltage = (float) jk_get_16bit(i * 3 + 3) * 0.001f;
//     average_cell_voltage = average_cell_voltage + cell_voltage;
//     if (cell_voltage < min_cell_voltage) {
//       min_cell_voltage = cell_voltage;
//       min_voltage_cell = i + 1;
//     }
//     if (cell_voltage > max_cell_voltage) {
//       max_cell_voltage = cell_voltage;
//       max_voltage_cell = i + 1;
//     }
//     // this->publish_state_(this->cells_[i].cell_voltage_sensor_, cell_voltage);
//   }
//   average_cell_voltage = average_cell_voltage / cells;
//   printf(" \n min Cell Voltage = %lf \n" , min_cell_voltage);
//   printf(" \n max Cell Voltage = %lf \n" , max_cell_voltage);
//   printf(" \n Average Cell Voltage = %lf \n" , average_cell_voltage);
//   // this->publish_state_(this->min_cell_voltage_sensor_, min_cell_voltage);
//   // this->publish_state_(this->max_cell_voltage_sensor_, max_cell_voltage);
//   // this->publish_state_(this->max_voltage_cell_sensor_, (float) max_voltage_cell);
//   // this->publish_state_(this->min_voltage_cell_sensor_, (float) min_voltage_cell);
//   // this->publish_state_(this->delta_cell_voltage_sensor_, max_cell_voltage - min_cell_voltage);
//   // this->publish_state_(this->average_cell_voltage_sensor_, average_cell_voltage);

//   uint16_t offset = data[1] + 3;

//   // // 0x80 0x00 0x1D: Read power tube temperature                 29°C                      1.0 °C
//   // // --->  99 = 99°C, 100 = 100°C, 101 = -1°C, 140 = -40°C
//   // this->publish_state_(this->power_tube_temperature_sensor_, get_temperature_(jk_get_16bit(offset + 3 * 0)));

//   // // 0x81 0x00 0x1E: Read the temperature in the battery box     30°C                      1.0 °C
//   // this->publish_state_(this->temperature_sensor_1_sensor_, get_temperature_(jk_get_16bit(offset + 3 * 1)));

//   // // 0x82 0x00 0x1C: Read battery temperature                    28°C                      1.0 °C
//   // this->publish_state_(this->temperature_sensor_2_sensor_, get_temperature_(jk_get_16bit(offset + 3 * 2)));

//   // // 0x83 0x14 0xEF: Total battery voltage                       5359 * 0.01 = 53.59V      0.01 V
//   // float total_voltage = (float) jk_get_16bit(offset + 3 * 3) * 0.01f;
//   // this->publish_state_(this->total_voltage_sensor_, total_voltage);

//   // // 0x84 0x80 0xD0: Current data                                32976                     0.01 A
//   // // this->publish_state_(this->current_sensor_, get_current_(jk_get_16bit(offset + 3 * 4), 0x01) * 0.01f);
//   // float current = get_current_(jk_get_16bit(offset + 3 * 4), data[offset + 84 + 3 * 45]) * 0.01f;
//   // this->publish_state_(this->current_sensor_, current);

//   // float power = total_voltage * current;
//   // this->publish_state_(this->power_sensor_, power);
//   // this->publish_state_(this->charging_power_sensor_, std::max(0.0f, power));               // 500W vs 0W -> 500W
//   // this->publish_state_(this->discharging_power_sensor_, std::abs(std::min(0.0f, power)));  // -500W vs 0W -> 500W

//   // // 0x85 0x0F: Battery remaining capacity                       15 %
//   // uint8_t raw_battery_remaining_capacity = data[offset + 3 * 5];
//   // this->publish_state_(this->capacity_remaining_sensor_, (float) raw_battery_remaining_capacity);

//   // // 0x86 0x02: Number of battery temperature sensors             2                        1.0  count
//   // this->publish_state_(this->temperature_sensors_sensor_, (float) data[offset + 2 + 3 * 5]);

//   // // 0x87 0x00 0x04: Number of battery cycles                     4                        1.0  count
//   // this->publish_state_(this->charging_cycles_sensor_, (float) jk_get_16bit(offset + 4 + 3 * 5));

//   // // 0x89 0x00 0x00 0x00 0x00: Total battery cycle capacity
//   // this->publish_state_(this->total_charging_cycle_capacity_sensor_, (float) jk_get_32bit(offset + 4 + 3 * 6));

//   // // 0x8A 0x00 0x0E: Total number of battery strings             14                        1.0  count
//   // this->publish_state_(this->battery_strings_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 7));

//   // // 0x8B 0x00 0x00: Battery warning message                     0000 0000 0000 0000
//   // //
//   // // Bit 0    Low capacity                                1 (alarm), 0 (normal)    warning
//   // // Bit 1    Power tube overtemperature                  1 (alarm), 0 (normal)    alarm
//   // // Bit 2    Charging overvoltage                        1 (alarm), 0 (normal)    alarm
//   // // Bit 3    Discharging undervoltage                    1 (alarm), 0 (normal)    alarm
//   // // Bit 4    Battery over temperature                    1 (alarm), 0 (normal)    alarm
//   // // Bit 5    Charging overcurrent                        1 (alarm), 0 (normal)    alarm
//   // // Bit 6    Discharging overcurrent                     1 (alarm), 0 (normal)    alarm
//   // // Bit 7    Cell pressure difference                    1 (alarm), 0 (normal)    alarm
//   // // Bit 8    Overtemperature alarm in the battery box    1 (alarm), 0 (normal)    alarm
//   // // Bit 9    Battery low temperature                     1 (alarm), 0 (normal)    alarm
//   // // Bit 10   Cell overvoltage                            1 (alarm), 0 (normal)    alarm
//   // // Bit 11   Cell undervoltage                           1 (alarm), 0 (normal)    alarm
//   // // Bit 12   309_A protection                            1 (alarm), 0 (normal)    alarm
//   // // Bit 13   309_A protection                            1 (alarm), 0 (normal)    alarm
//   // // Bit 14   Reserved
//   // // Bit 15   Reserved
//   // //
//   // // Examples:
//   // // 0x0001 = 00000000 00000001: Low capacity alarm
//   // // 0x0002 = 00000000 00000010: MOS tube over-temperature alarm
//   // // 0x0003 = 00000000 00000011: Low capacity alarm AND power tube over-temperature alarm
//   // uint16_t raw_errors_bitmask = jk_get_16bit(offset + 6 + 3 * 8);
//   // this->publish_state_(this->errors_bitmask_sensor_, (float) raw_errors_bitmask);
//   // this->publish_state_(this->errors_text_sensor_, this->error_bits_to_string_(raw_errors_bitmask));

//   // // 0x8C 0x00 0x07: Battery status information                  0000 0000 0000 0111
//   // // Bit 0: Charging enabled        1 (on), 0 (off)
//   // // Bit 1: Discharging enabled     1 (on), 0 (off)
//   // // Bit 2: Balancer enabled        1 (on), 0 (off)
//   // // Bit 3: Battery dropped(?)      1 (normal), 0 (offline)
//   // // Bit 4...15: Reserved

//   // // Example: 0000 0000 0000 0111 -> Charging + Discharging + Balancer enabled
//   // uint16_t raw_modes_bitmask = jk_get_16bit(offset + 6 + 3 * 9);
//   // this->publish_state_(this->operation_mode_bitmask_sensor_, (float) raw_modes_bitmask);
//   // this->publish_state_(this->operation_mode_text_sensor_, this->mode_bits_to_string_(raw_modes_bitmask));
//   // this->publish_state_(this->charging_binary_sensor_, check_bit_(raw_modes_bitmask, 1));
//   // this->publish_state_(this->discharging_binary_sensor_, check_bit_(raw_modes_bitmask, 2));
//   // this->publish_state_(this->balancing_binary_sensor_, check_bit_(raw_modes_bitmask, 4));

//   // // 0x8E 0x16 0x26: Total voltage overvoltage protection        5670 * 0.01 = 56.70V     0.01 V
//   // this->publish_state_(this->total_voltage_overvoltage_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 10) * 0.01f);

//   // // 0x8F 0x10 0xAE: Total voltage undervoltage protection       4270 * 0.01 = 42.70V     0.01 V
//   // this->publish_state_(this->total_voltage_undervoltage_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 11) * 0.01f);

//   // // 0x90 0x0F 0xD2: Cell overvoltage protection voltage         4050 * 0.001 = 4.050V     0.001 V
//   // this->publish_state_(this->cell_voltage_overvoltage_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 12) * 0.001f);

//   // // 0x91 0x0F 0xA0: Cell overvoltage recovery voltage           4000 * 0.001 = 4.000V     0.001 V
//   // this->publish_state_(this->cell_voltage_overvoltage_recovery_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 13) * 0.001f);

//   // // 0x92 0x00 0x05: Cell overvoltage protection delay            5s                         1.0 s
//   // this->publish_state_(this->cell_voltage_overvoltage_delay_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 14));

//   // // 0x93 0x0B 0xEA: Cell undervoltage protection voltage        3050 * 0.001 = 3.050V     0.001 V
//   // this->publish_state_(this->cell_voltage_undervoltage_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 15) * 0.001f);

//   // // 0x94 0x0C 0x1C: Cell undervoltage recovery voltage          3100 * 0.001 = 3.100V     0.001 V
//   // this->publish_state_(this->cell_voltage_undervoltage_recovery_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 16) * 0.001f);

//   // // 0x95 0x00 0x05: Cell undervoltage protection delay           5s                         1.0 s
//   // this->publish_state_(this->cell_voltage_undervoltage_delay_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 17));

//   // // 0x96 0x01 0x2C: Cell pressure difference protection value    300 * 0.001 = 0.300V     0.001 V     0.000-1.000V
//   // this->publish_state_(this->cell_pressure_difference_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 18) * 0.001f);

//   // // 0x97 0x00 0x07: Discharge overcurrent protection value       7A                         1.0 A
//   // this->publish_state_(this->discharging_overcurrent_protection_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 19));

//   // // 0x98 0x00 0x03: Discharge overcurrent delay                  3s                         1.0 s
//   // this->publish_state_(this->discharging_overcurrent_delay_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 20));

//   // // 0x99 0x00 0x05: Charging overcurrent protection value        5A                         1.0 A
//   // this->publish_state_(this->charging_overcurrent_protection_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 21));

//   // // 0x9A 0x00 0x05: Charge overcurrent delay                     5s                         1.0 s
//   // this->publish_state_(this->charging_overcurrent_delay_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 22));

//   // // 0x9B 0x0C 0xE4: Balanced starting voltage                   3300 * 0.001 = 3.300V     0.001 V
//   // this->publish_state_(this->balance_starting_voltage_sensor_, (float) jk_get_16bit(offset + 6 + 3 * 23) * 0.001f);

//   // // 0x9C 0x00 0x08: Balanced opening pressure difference           8 * 0.001 = 0.008V     0.001 V     0.01-1V
//   // this->publish_state_(this->balance_opening_pressure_difference_sensor_,
//   //                      (float) jk_get_16bit(offset + 6 + 3 * 24) * 0.001f);

//   // // 0x9D 0x01: Active balance switch                              1 (on)                     Bool     0 (off), 1 (on)
//   // this->publish_state_(this->balancing_switch_binary_sensor_, (bool) data[offset + 6 + 3 * 25]);
//   // this->publish_state_(this->balancer_switch_, (bool) data[offset + 6 + 3 * 25]);

//   // // 0x9E 0x00 0x5A: Power tube temperature protection value                90°C            1.0 °C     0-100°C
//   // this->publish_state_(this->power_tube_temperature_protection_sensor_, (float) jk_get_16bit(offset + 8 + 3 * 25));

//   // // 0x9F 0x00 0x46: Power tube temperature recovery value                  70°C            1.0 °C     0-100°C
//   // this->publish_state_(this->power_tube_temperature_recovery_sensor_, (float) jk_get_16bit(offset + 8 + 3 * 26));

//   // // 0xA0 0x00 0x64: Temperature protection value in the battery box       100°C            1.0 °C     40-100°C
//   // this->publish_state_(this->temperature_sensor_temperature_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 8 + 3 * 27));

//   // // 0xA1 0x00 0x64: Temperature recovery value in the battery box         100°C            1.0 °C     40-100°C
//   // this->publish_state_(this->temperature_sensor_temperature_recovery_sensor_,
//   //                      (float) jk_get_16bit(offset + 8 + 3 * 28));

//   // // 0xA2 0x00 0x14: Battery temperature difference protection value        20°C            1.0 °C     5-10°C
//   // this->publish_state_(this->temperature_sensor_temperature_difference_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 8 + 3 * 29));

//   // // 0xA3 0x00 0x46: Battery charging high temperature protection value     70°C            1.0 °C     0-100°C
//   // this->publish_state_(this->charging_high_temperature_protection_sensor_, (float) jk_get_16bit(offset + 8 + 3 * 30));

//   // // 0xA4 0x00 0x46: Battery discharge high temperature protection value    70°C            1.0 °C     0-100°C
//   // this->publish_state_(this->discharging_high_temperature_protection_sensor_,
//   //                      (float) jk_get_16bit(offset + 8 + 3 * 31));

//   // // 0xA5 0xFF 0xEC: Charging low temperature protection value             -20°C            1.0 °C     -45...25°C
//   // this->publish_state_(this->charging_low_temperature_protection_sensor_,
//   //                      (float) (int16_t) jk_get_16bit(offset + 8 + 3 * 32));

//   // // 0xA6 0xFF 0xF6: Charging low temperature protection recovery value    -10°C            1.0 °C     -45...25°C
//   // this->publish_state_(this->charging_low_temperature_recovery_sensor_,
//   //                      (float) (int16_t) jk_get_16bit(offset + 8 + 3 * 33));

//   // // 0xA7 0xFF 0xEC: Discharge low temperature protection value            -20°C            1.0 °C     -45...25°C
//   // this->publish_state_(this->discharging_low_temperature_protection_sensor_,
//   //                      (float) (int16_t) jk_get_16bit(offset + 8 + 3 * 34));

//   // // 0xA8 0xFF 0xF6: Discharge low temperature protection recovery value   -10°C            1.0 °C     -45...25°C
//   // this->publish_state_(this->discharging_low_temperature_recovery_sensor_,
//   //                      (float) (int16_t) jk_get_16bit(offset + 8 + 3 * 35));

//   // // 0xA9 0x0E: Battery string setting                                      14              1.0 count
//   // // this->publish_state_(this->battery_string_setting_sensor_, (float) data[offset + 8 + 3 * 36]);
//   // // 0xAA 0x00 0x00 0x02 0x30: Total battery capacity setting              560 Ah           1.0 Ah
//   // uint32_t raw_total_battery_capacity_setting = jk_get_32bit(offset + 10 + 3 * 36);
//   // this->publish_state_(this->total_battery_capacity_setting_sensor_, (float) raw_total_battery_capacity_setting);
//   // this->publish_state_(this->capacity_remaining_derived_sensor_,
//   //                      (float) (raw_total_battery_capacity_setting * (raw_battery_remaining_capacity * 0.01f)));

//   // // 0xAB 0x01: Charging MOS tube switch                                     1 (on)         Bool       0 (off), 1 (on)
//   // this->publish_state_(this->charging_switch_binary_sensor_, (bool) data[offset + 15 + 3 * 36]);
//   // this->publish_state_(this->charging_switch_, (bool) data[offset + 15 + 3 * 36]);

//   // // 0xAC 0x01: Discharge MOS tube switch                                    1 (on)         Bool       0 (off), 1 (on)
//   // this->publish_state_(this->discharging_switch_binary_sensor_, (bool) data[offset + 17 + 3 * 36]);
//   // this->publish_state_(this->discharging_switch_, (bool) data[offset + 17 + 3 * 36]);

//   // // 0xAD 0x04 0x11: Current calibration                       1041mA * 0.001 = 1.041A     0.001 A     0.1-2.0A
//   // this->publish_state_(this->current_calibration_sensor_, (float) jk_get_16bit(offset + 19 + 3 * 36) * 0.001f);

//   // // 0xAE 0x01: Protection board address                                     1              1.0
//   // this->publish_state_(this->device_address_sensor_, (float) data[offset + 19 + 3 * 37]);

//   // // 0xAF 0x01: Battery Type                                                 1              1.0
//   // // ---> 0 (lithium iron phosphate), 1 (ternary), 2 (lithium titanate)
//   // uint8_t raw_battery_type = data[offset + 21 + 3 * 37];
//   // if (raw_battery_type < BATTERY_TYPES_SIZE) {
//   //   this->publish_state_(this->battery_type_text_sensor_, BATTERY_TYPES[raw_battery_type]);
//   // } else {
//   //   this->publish_state_(this->battery_type_text_sensor_, "Unknown");
//   // }

//   // // 0xB0 0x00 0x0A: Sleep waiting time                                      10s            1.0 s
//   // this->publish_state_(this->sleep_wait_time_sensor_, (float) jk_get_16bit(offset + 23 + 3 * 37));

//   // // 0xB1 0x14: Low volume alarm                                             20%            1.0 %      0-80%
//   // this->publish_state_(this->alarm_low_volume_sensor_, (float) data[offset + 23 + 3 * 38]);

//   // // 0xB2 0x31 0x32 0x33 0x34 0x35 0x36 0x00 0x00 0x00 0x00: Modify parameter password
//   // this->publish_state_(this->password_text_sensor_,
//   //                      std::string(data.begin() + offset + 25 + 3 * 38, data.begin() + offset + 35 + 3 * 38));

//   // // 0xB3 0x00: Dedicated charger switch                                     1 (on)         Bool       0 (off), 1 (on)
//   // this->publish_state_(this->dedicated_charger_switch_binary_sensor_, (bool) data[offset + 36 + 3 * 38]);

//   // // 0xB4 0x49 0x6E 0x70 0x75 0x74 0x20 0x55 0x73: Device ID code
//   // this->publish_state_(this->device_type_text_sensor_,
//   //                      std::string(data.begin() + offset + 38 + 3 * 38, data.begin() + offset + 46 + 3 * 38));

//   // // 0xB5 0x32 0x31 0x30 0x31: Date of manufacture
//   // // 0xB6 0x00 0x00 0xE2 0x00: System working hours
//   // this->publish_state_(this->total_runtime_sensor_, (float) jk_get_32bit(offset + 46 + 3 * 40) * 0.0166666666667);
//   // this->publish_state_(this->total_runtime_formatted_text_sensor_,
//   //                      format_total_runtime_(jk_get_32bit(offset + 46 + 3 * 40) * 60));

//   // // 0xB7 0x48 0x36 0x2E 0x58 0x5F 0x5F 0x53
//   // //      0x36 0x2E 0x31 0x2E 0x33 0x53 0x5F 0x5F: Software version number
//   // this->publish_state_(this->software_version_text_sensor_,
//   //                      std::string(data.begin() + offset + 51 + 3 * 40, data.begin() + offset + 51 + 3 * 45));

//   // // 0xB8 0x00: Whether to start current calibration
//   // // 0xB9 0x00 0x00 0x00 0x00: Actual battery capacity
//   // //   Firmware version >= 10.10 required
//   // //   See https://github.com/syssi/esphome-jk-bms/issues/212 for details
//   // this->publish_state_(this->actual_battery_capacity_sensor_, (float) jk_get_32bit(offset + 54 + 3 * 45));

//   // // 0xBA 0x42 0x54 0x33 0x30 0x37 0x32 0x30 0x32 0x30 0x31 0x32 0x30
//   // //      0x30 0x30 0x30 0x32 0x30 0x30 0x35 0x32 0x31 0x30 0x30 0x31: Manufacturer ID naming
//   // this->publish_state_(this->manufacturer_text_sensor_,
//   //                      std::string(data.begin() + offset + 59 + 3 * 45, data.begin() + offset + 83 + 3 * 45));

//   // // 0xC0 0x01: Protocol version number
//   // this->publish_state_(this->protocol_version_sensor_, (float) data[offset + 84 + 3 * 45]);

//   // // 00 00 00 00 68 00 00 54 D1: End of frame
//   working_buffer.clear();
  
// }











