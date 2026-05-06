#include <Arduino.h>
#include <WiFi.h>

// Simple utility to print the ESP32 MAC address in both formats
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  uint8_t mac[6];
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(mac);
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32 MAC Address Checker");
  Serial.println("========================================\n");
  
  // Print in standard MAC format (XX:XX:XX:XX:XX:XX)
  Serial.print("MAC Address (standard format): ");
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  // Print in C++ array format for config.h
  Serial.print("\nMAC Address (for config.h):    ");
  Serial.printf("{0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}\n\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  Serial.println("Copy one of the formats above and paste into config.h:");
  Serial.println("  - Standard format: Use in documentation/notes");
  Serial.println("  - Hex array format: Use in SECONDARY_PEER_MAC or MAIN_PEER_MAC\n");
  
  Serial.println("========================================\n");
}

void loop() {
  // Nothing to do, just display once
  delay(10000);
}
