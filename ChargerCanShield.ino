#include <SPI.h>
#include <mcp_can.h>

// Pin Definitions
#define CAN_CS_PIN              (10) // CAN CS pin
#define CAN_INT_PIN             (2)  // Interrupt pin for CAN bus
#define PRECHARGE_PRECHARGE_PIN (8)  // Output to precharge board to indicate precharge completion

// Precharge Constants
#define PRECHARGE_THRESHOLD_PRECHARGE (0.92) // Percentage to send signal to precharge board

MCP_CAN CAN(CAN_CS_PIN);

// Sensor Data
float batVolt = 0.0;
float chargerVolt = 0.0;

unsigned long lastPrint = 0;
#define PRINT_RATE_MS (100)


// ---- Big-endian decode helpers ----

int16_t parseInt16BE(unsigned char* buf, int offset) {
  return (int16_t)(((uint16_t)buf[offset] << 8) | (uint16_t)buf[offset + 1]);
}

uint16_t parseUint16BE(unsigned char* buf, int offset) {
  return (uint16_t)(((uint16_t)buf[offset] << 8) | (uint16_t)buf[offset + 1]);
}

int32_t parseInt32BE(unsigned char* buf, int offset) {
  return (int32_t)(((uint32_t)buf[offset]     << 24) |
                   ((uint32_t)buf[offset + 1] << 16) |
                   ((uint32_t)buf[offset + 2] <<  8) |
                    (uint32_t)buf[offset + 3]);
}


// ---- Setup ----

void setup() {
  Serial.begin(9600);

  pinMode(PRECHARGE_PRECHARGE_PIN, OUTPUT);
  digitalWrite(PRECHARGE_PRECHARGE_PIN, LOW);

  while (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) != CAN_OK) {
    delay(100);
    Serial.println("CAN Init FAILED - Check CS Pin and wiring");
  }
  Serial.println("CAN Init OK");
  CAN.setMode(MCP_NORMAL);

  pinMode(CAN_INT_PIN, INPUT_PULLUP);
}


// ---- Main Loop ----

void loop() {
  unsigned long currentTime = millis();

  // ---- CAN Receive ----
  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    long unsigned int rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];
    CAN.readMsgBuf(&rxId, &len, rxBuf);

    switch (rxId) {
      // BMS (Orion)
      case 0x6B0:
        batVolt = parseUint16BE(rxBuf, 2) / 10.0;
        break;
        
      // HK LF 540-14 Charger
      case 0x1801D08F:
        chargerVolt = parseUint16BE(rxBuf, 0) / 10.0;
        break;
    }
  }

  // Precharge completion calculations
  double prechargePercentage = chargerVolt / batVolt;
  digitalWrite(PRECHARGE_PRECHARGE_PIN, (prechargePercentage > PRECHARGE_THRESHOLD_PRECHARGE) ? HIGH : LOW);
  
  if (millis() - lastPrint >= PRINT_RATE_MS) {
    lastPrint += PRINT_RATE_MS;
    Serial.print(chargerVolt, 1);
    Serial.print(" V    ");
    Serial.print(batVolt, 1);
    Serial.print(" V    ");
    Serial.print(prechargePercentage, 2);
    Serial.println("%");
  }
}
