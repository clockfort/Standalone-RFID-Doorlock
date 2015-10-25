#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <EEPROM.h>

#define RED_LED_PIN (5)
#define GREEN_LED_PIN (6)

#define PN532_IRQ   (4)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield
#define BUFFER_LENGTH (7)
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// A special card that causes us to enter a different mode where we register new valid cards;
// treat cards with short (4-byte) IDs as starting at index 0 and 0-extend to 7 bytes
uint8_t REGISTRATION_CARD[] = { 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65 };
uint8_t current_uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t prospective_entry_uid[] = { 0, 0, 0, 0, 0, 0, 0 };

#define ENTRY_COUNTER_LOCATION (0) // where we'll store number of entries
#define ENTRY_COUNTER_LENGTH (2) // I actually only need 1, but let's be nice to people with big arduinos with more EEPROM
uint16_t entry_counter;

void setup(void) {
  Serial.begin(115200);

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  
  // configure board to read RFID tags
  nfc.SAMConfig();

  EEPROM.get(ENTRY_COUNTER_LOCATION, entry_counter);
  Serial.print("Initialized from EEPROM, found ");
  Serial.print(entry_counter);
  Serial.println(" existing registered entries.");
  
  Serial.println("Waiting for an ISO14443A Card ...");
}


void loop(void) {
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, current_uid, &uidLength)) {
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Value: ");
    nfc.PrintHex(current_uid, BUFFER_LENGTH);
    Serial.println("");

    if (isRegistrationCard()) {
      Serial.println("REGISTRATION MODE ACTIVATED");
      for (uint64_t start = millis(), timePassed = 0; timePassed < 10000; timePassed = millis() - start) {
        // Do some blinkenlighten to tell user we're in registration mode and waiting for a new read
        timePassed % 1000 / 250 % 2? digitalWrite(RED_LED_PIN, HIGH): digitalWrite(RED_LED_PIN, LOW);
        timePassed % 1000 / 250 % 2? digitalWrite(GREEN_LED_PIN, LOW): digitalWrite(GREEN_LED_PIN, HIGH);
        
        if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, current_uid, &uidLength, 250)) {
          if (isRegistrationCard())
            break; // double read? PEBKAC?
            
          memcpy(prospective_entry_uid, current_uid, BUFFER_LENGTH);
          Serial.print("Received registration request for: ");
          nfc.PrintHex(current_uid, BUFFER_LENGTH);
          Serial.println("");
         
          displaySuccess();  // signal that we had a good read and are waiting for a confirmation tap
  
          if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, current_uid, &uidLength, 10000)) {
            if (memcmp(current_uid, prospective_entry_uid, BUFFER_LENGTH) == 0) {
              Serial.println("Received registration confirmation!");
              registerCard();
            } else {
              Serial.print("Failed to register - second tap not the same UID. Second UID was: ");
              nfc.PrintHex(current_uid, BUFFER_LENGTH);
              Serial.println("");
              displayErrorAndDelay();
              break;
            }
          } else {
            Serial.println("Timed out waiting for registration confirmation.");
          }
        } // read card after registration
      } // blinky registration mode active
      clearLEDState();
    } 

    if (isValidEntrant()) {
      Serial.println("Valid entrant accepted.");
      displaySuccess();
      unlockDoor();
      delay(2000);
    }
  }
}

bool isRegistrationCard() {
    return memcmp(current_uid, REGISTRATION_CARD, BUFFER_LENGTH) == 0;
}

bool isValidEntrant() {
  for (uint16_t eeprom_index = ENTRY_COUNTER_LENGTH; eeprom_index < entry_counter * BUFFER_LENGTH; eeprom_index += BUFFER_LENGTH) {
    eeprom_read_block(prospective_entry_uid, (void *) eeprom_index, BUFFER_LENGTH);
    if (memcmp(current_uid, prospective_entry_uid, BUFFER_LENGTH) == 0)
      return true;
  }
  return false;
}

void registerCard() {
  uint16_t new_address = ENTRY_COUNTER_LENGTH + entry_counter * BUFFER_LENGTH;
  
  if (new_address > EEPROM.length()) {
    Serial.println("ERROR - No space left on device to store additional registrations.");
    for (uint64_t start = millis(), timePassed = 0; timePassed < 4000; timePassed = millis() - start)
      timePassed % 1000 / 250 % 2? digitalWrite(RED_LED_PIN, HIGH): digitalWrite(RED_LED_PIN, LOW);
    return;
  }
  
  eeprom_write_block(current_uid, (void *) new_address, BUFFER_LENGTH);
  entry_counter += BUFFER_LENGTH;
  EEPROM.put(ENTRY_COUNTER_LOCATION, entry_counter);
  
  Serial.print("Success! Device: ");
  nfc.PrintHex(current_uid, BUFFER_LENGTH);
  Serial.print(" was succesfully registered into slot ");
  Serial.println(entry_counter);
}

void clearLEDState() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void displaySuccess() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
}

void displayErrorAndDelay() {
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
  delay(2000);
}


void unlockDoor() {
  // todo
}


