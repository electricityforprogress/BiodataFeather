// =========================================================================
// BIODATA SONIFICATION - LEGACY V007 HARDWARE + V031 GATT / WIFI APP UPGRADE
// =========================================================================
// - Hardware: Standard PWM LEDs (No NeoPixels), V007 Pins
// - Upgraded: xthal_get_ccount() High Sensitivity ISRs
// - Upgraded: NVS Preferences replaces EEPROM
// - Upgraded: BLE GATT + UART Hardware-First SYNC Handshake
// - Upgraded: Additive CC81 (Base) + CC80 (Boost) Threshold mapping
// - Removed: Blocking Menu Modes (App-managed parameters only)
// =========================================================================

#include <Arduino.h>
#include <math.h>
#include <Preferences.h>
#include <xtensa/hal.h>
#include <Adafruit_TinyUSB.h>
#include <esp_mac.h>
#include <MIDI.h>
#include <WiFi.h>
#include <AppleMIDI.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define CONFIG_VERSION 32
const uint8_t BIO_PIN = 12; // interruptPin from V007
const uint8_t POT_PIN = A2; 

// --- Standard LEDs via PWM (V007) ---
class samFader {
  public:
  byte pinNumber;
  int maxBright;
  int currentLevel = 0;
  int destinationLevel = 0;
  unsigned long startTime; 
  int duration = 0;
  unsigned long stepSize;  
  unsigned long stepTime; 
  bool isRunning = 0;

  samFader(byte pin, byte maxB) { pinNumber = pin; maxBright = maxB; }
  
  void Set(int dest, int dur) {
    startTime = millis();
    destinationLevel = dest;
    duration = dur;
    stepTime = 0;
    int difference = abs(destinationLevel - currentLevel);
    stepSize = duration / (difference + 1);
    isRunning = 1;
    if(dur == 0) { 
      ledcWrite(pinNumber, dest);
      currentLevel = dest;
      isRunning = 0;
    }
  }
  
  void Update() {
    if(isRunning) {
      unsigned long now = millis();
      if(stepTime + stepSize < now) {
        if(currentLevel > destinationLevel) currentLevel--;
        if(currentLevel < destinationLevel) currentLevel++;
        stepTime = now;
        ledcWrite(pinNumber, currentLevel);
      }
      if(startTime + duration < now) {
        ledcWrite(pinNumber, destinationLevel);
        currentLevel = destinationLevel;
        isRunning = 0;
      }
    }
  }
  
  void Setup() { 
    ledcAttach(pinNumber, 5000, 8); 
  }
};

samFader ledFaders[5] = {
  samFader(18, 50),
  samFader(17, 110),
  samFader(8, 120),
  samFader(36, 120),
  samFader(35, 60)
};

// --- USB & RTP MIDI ---
Adafruit_USBD_MIDI usb_midi_interface;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi_interface, usbMIDI);
APPLEMIDI_CREATE_DEFAULTSESSION_INSTANCE();
bool rtpConnected = false;

// --- BLE Globals ---
BLEServer *pServer = NULL;
BLECharacteristic *pMidiCharacteristic = NULL;
BLECharacteristic *pUartTxCharacteristic = NULL;
bool deviceConnected = false;
uint8_t connectedClients = 0;

// --- Config & Parameters ---
Preferences prefs;
struct ChannelConfig {
    uint8_t version = CONFIG_VERSION;
    float threshold = 2.0; 
    float thresh_base = 2.0;
    float thresh_boost = 0.0;
    uint8_t window_size = 40;
    uint8_t density = 100;
    uint8_t note_min = 48; 
    uint8_t note_max = 84; 
    uint8_t vel_min = 60;
    uint8_t vel_max = 110;
    uint8_t spread = 75;
    float dur_mult = 1.5; 
    uint8_t quantize = 0;
    uint8_t midi_channel = 1; 
    uint8_t bpm = 120;
    uint8_t polyphony = 5;
    uint8_t steal_mode = 0; 
    uint8_t legato = 0;
    uint16_t scale_mask = 0b101010110101; 
};

ChannelConfig config;
bool prefs_dirty = false;
uint32_t last_param_change = 0;
int prevKnob = 0;

// --- Bio Analysis Variables ---
#define MAX_WINDOW 100
uint32_t sample_buffer[MAX_WINDOW];
size_t head_idx = 0;
uint32_t total_pulses = 0;

uint32_t tx_buffer[10];
bool tx_ready = false;
uint32_t last_telemetry = 0;

volatile uint32_t last_rise_cc = 0;
volatile uint32_t high_time_cc = 0;
volatile bool new_pulse = false;

// --- Polyphony Voices ---
#define VOICES_PER_CH 8
struct PolyNote {
    bool active = false;
    bool note_on_sent = false;
    bool is_legato_transition = false;
    uint8_t old_pitch = 0;
    uint8_t pitch = 0;
    uint8_t vel = 0;
    uint32_t trigger_time = 0;
    uint32_t off_time = 0;
};
PolyNote voices[VOICES_PER_CH];
uint32_t last_event_time = 0;

// --- High Sensitivity ISR ---
void IRAM_ATTR isr_bio() { 
    uint32_t now = xthal_get_ccount(); 
    if (digitalRead(BIO_PIN)) last_rise_cc = now; 
    else if (now > last_rise_cc) { high_time_cc = now - last_rise_cc; new_pulse = true; } 
}

// --- Utilities & MIDI Send ---
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void sendBleMidi(uint8_t status, uint8_t data1, uint8_t data2) {
    if (!deviceConnected) return;
    uint16_t ts = millis() & 0x1FFF;
    uint8_t header = 0x80 | ((ts >> 7) & 0x3F);
    uint8_t time_lo = 0x80 | (ts & 0x7F);
    uint8_t midiPacket[] = { header, time_lo, status, data1, data2 };
    pMidiCharacteristic->setValue(midiPacket, 5);
    pMidiCharacteristic->notify();
}

uint8_t applyScaleMask(uint8_t raw_note, uint16_t mask) {
    if (mask == 0) return raw_note; 
    if (mask & (1 << (raw_note % 12))) return raw_note; 
    for (int offset = 1; offset <= 12; offset++) {
        int up = raw_note + offset;
        if (up <= 127 && (mask & (1 << (up % 12)))) return up;
        int down = raw_note - offset;
        if (down >= 0 && (mask & (1 << (down % 12)))) return down;
    }
    return raw_note;
}

// --- Web Sync & CC Updates ---
void processIncomingCC(uint8_t cc, uint8_t val) {
    prefs_dirty = true;
    last_param_change = millis();
    
    switch(cc) {
        case 80: 
            config.thresh_boost = (val / 127.0f) * 5.0f; 
            config.threshold = config.thresh_base + config.thresh_boost;
            break;
        case 81: 
            config.thresh_base = 1.0f + (val / 127.0f) * 4.0f; 
            config.threshold = config.thresh_base + config.thresh_boost; 
            break;
        case 82: config.window_size = val; break; 
        case 83: config.density = val; break; 
        case 84: config.note_min = val; break;
        case 85: config.note_max = val; break;
        case 86: config.vel_min = val; break;
        case 87: config.vel_max = val; break;
        case 88: config.spread = val; break; 
        case 89: config.dur_mult = val / 10.0f; break; 
        case 90: config.quantize = val; break; 
        case 91: config.midi_channel = val; break; 
        case 92: config.bpm = round(60.0f + (val / 127.0f) * 180.0f); break; 
        case 93: config.polyphony = val; break; 
        case 94: config.steal_mode = val; break; 
        case 95: config.legato = (val > 63) ? 1 : 0; break;
    }
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { 
        connectedClients++; deviceConnected = true; BLEDevice::startAdvertising(); 
        ledFaders[3].Set(ledFaders[3].maxBright, 500); // Blue LED feedback
        delay(500);
        ledFaders[3].Set(0, 500);
    }
    void onDisconnect(BLEServer* pServer) { 
        if (connectedClients > 0) connectedClients--; 
        if (connectedClients == 0) deviceConnected = false; 
        BLEDevice::startAdvertising(); 
    }
};

class MidiRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) {
        uint8_t* rxData = pChar->getData();
        if (pChar->getLength() >= 5) {
            uint8_t status = rxData[2];
            if ((status & 0xF0) == 0xB0) { 
                uint8_t rx_midi_ch = status & 0x0F;
                if (config.midi_channel == rx_midi_ch || rx_midi_ch == 0) {
                    processIncomingCC(rxData[3], rxData[4]);
                }
            }
        }
    }
};

class UartRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pChar) {
        uint8_t* rxData = pChar->getData();
        size_t len = pChar->getLength();
        if (len == 0) return;
        
        String rx = "";
        for (size_t i = 0; i < len; i++) rx += (char)rxData[i];
        
        if (rx.startsWith("SYNC")) {
            String stateJson = "{\"type\":\"state\",\"ch\":0" 
                ",\"thr\":" + String(config.threshold) + 
                ",\"win\":" + String(config.window_size) + 
                ",\"den\":" + String(config.density) + 
                ",\"nMin\":" + String(config.note_min) + 
                ",\"nMax\":" + String(config.note_max) + 
                ",\"vMin\":" + String(config.vel_min) + 
                ",\"vMax\":" + String(config.vel_max) + 
                ",\"spr\":" + String(config.spread) + 
                ",\"dur\":" + String(config.dur_mult) + 
                ",\"qtz\":" + String(config.quantize) + 
                ",\"mCh\":" + String(config.midi_channel) + 
                ",\"bpm\":" + String(config.bpm) + 
                ",\"poly\":" + String(config.polyphony) + 
                ",\"stl\":" + String(config.steal_mode) + 
                ",\"leg\":" + String(config.legato) + 
                ",\"scl\":" + String(config.scale_mask) + "}\n";
            pUartTxCharacteristic->setValue((uint8_t*)stateJson.c_str(), stateJson.length());
            pUartTxCharacteristic->notify();
            return;
        }

        int ch, cc, val;
        if (rx.startsWith("C:") && sscanf(rx.c_str(), "C:%d,%d,%d", &ch, &cc, &val) == 3) {
            if (ch == 0) processIncomingCC(cc, val);
        }
        else if (rx.startsWith("S:") && sscanf(rx.c_str(), "S:%d,%d", &ch, &val) == 2) {
            if (ch == 0) {
                config.scale_mask = val;
                prefs_dirty = true;
                last_param_change = millis();
            }
        }
    }
};

// --- Analysis Engine ---
void analyze_and_map(uint32_t raw_ticks) {
    if (raw_ticks == 0) return; 
    last_event_time = millis(); 

    sample_buffer[head_idx] = raw_ticks;
    head_idx = (head_idx + 1) % config.window_size;
    if (total_pulses < config.window_size) { total_pulses++; return; }

    if (!tx_ready) {
        for (int i=0; i<10; i++) {
            tx_buffer[i] = sample_buffer[(head_idx + (i * (config.window_size/10))) % config.window_size];
        }
        tx_ready = true;
    }
    
    float sum = 0; 
    for(int i = 0; i < config.window_size; i++) sum += (10000000.0f / sample_buffer[i]); 
    float mean = sum / config.window_size;
    if (mean <= 0.0f) return;
    
    float current_rVal = 10000000.0f / raw_ticks;
    
    float sum_sq = 0; 
    for(int i = 0; i < config.window_size; i++) {
        float val = 10000000.0f / sample_buffer[i];
        sum_sq += pow(val - mean, 2);
    }
    float std_dev = sqrtf(sum_sq / config.window_size);
    float min_std_dev = mean * 0.005f; 
    
    if (abs(current_rVal - mean) > (config.threshold * std_dev) && std_dev > min_std_dev) {
        if (random(0, 100) <= config.density) {
            static uint32_t last_trigger = 0;
            if (millis() - last_trigger > 150) { 
                
                float normalized = (current_rVal - mean) / (config.threshold * std_dev);
                long norm_pct = constrain(abs(normalized) * 100, 100, 200); 

                uint8_t raw_note = map(norm_pct, 100, 200, config.note_min, config.note_max);
                raw_note = min(config.note_max, max(config.note_min, raw_note));
                uint8_t target_pitch = applyScaleMask(raw_note, config.scale_mask);
                
                uint8_t target_vel = map(norm_pct, 100, 200, config.vel_min, config.vel_max);
                target_vel = min(config.vel_max, max(config.vel_min, target_vel));
                
                float v = config.dur_mult;
                uint32_t max_ms = (v <= 1.0f) ? (500.0f + ((v - 0.1f) / 0.9f) * 5150.0f) : (5650.0f + ((v - 1.0f) / 4.0f) * 24350.0f);
                uint32_t min_ms = 150 + ((100 - config.spread) / 100.0f) * (max_ms - 150);
                uint8_t bio_entropy = raw_ticks % 127;
                uint32_t target_dur = min_ms + ((max_ms - min_ms) * (bio_entropy / 127.0f));

                int v_idx = -1;
                for (int i = 0; i < config.polyphony; i++) {
                    if (!voices[i].active) { v_idx = i; break; }
                }
                
                if (v_idx == -1) {
                    if (config.legato == 1 && config.polyphony == 1) {
                        v_idx = 0; 
                    } else if (config.steal_mode != 3) {
                        int target_idx = 0;
                        uint32_t oldest_time = 0xFFFFFFFF;
                        uint8_t lowest_pitch = 255;
                        uint8_t highest_pitch = 0;
                        
                        for (int i = 0; i < config.polyphony; i++) {
                            if (voices[i].trigger_time < oldest_time) {
                                oldest_time = voices[i].trigger_time;
                                if (config.steal_mode == 0) target_idx = i;
                            }
                            if (voices[i].pitch < lowest_pitch) {
                                lowest_pitch = voices[i].pitch;
                                if (config.steal_mode == 1) target_idx = i;
                            }
                            if (voices[i].pitch > highest_pitch) {
                                highest_pitch = voices[i].pitch;
                                if (config.steal_mode == 2) target_idx = i;
                            }
                        }
                        v_idx = target_idx;
                    }
                }
                
                if (v_idx != -1) {
                    bool was_active = (voices[v_idx].active && voices[v_idx].note_on_sent);
                    if (was_active) {
                        if (config.legato == 1 && config.polyphony == 1) {
                            voices[v_idx].is_legato_transition = true;
                            voices[v_idx].old_pitch = voices[v_idx].pitch;
                        } else {
                            sendBleMidi(0x80 + (config.midi_channel - 1), voices[v_idx].pitch, 0);
                            usbMIDI.sendNoteOff(voices[v_idx].pitch, 0, config.midi_channel);
                            if (rtpConnected) MIDI.sendNoteOff(voices[v_idx].pitch, 0, config.midi_channel);
                        }
                    }

                    voices[v_idx].active = true;
                    voices[v_idx].note_on_sent = false; 
                    voices[v_idx].pitch = target_pitch;
                    voices[v_idx].vel = target_vel;
                    
                    uint32_t now = millis();
                    if (config.quantize > 0) {
                        float beat_ms = 60000.0f / (float)config.bpm;
                        float div_ms = (config.quantize == 1) ? beat_ms/16.0f : (config.quantize == 2) ? beat_ms/8.0f : (config.quantize == 3) ? beat_ms/4.0f : (config.quantize == 4) ? beat_ms/2.0f : (config.quantize == 5) ? beat_ms : (config.quantize == 6) ? beat_ms*(2.0f/3.0f) : (config.quantize == 7) ? beat_ms*2.0f : beat_ms*4.0f;
                        voices[v_idx].trigger_time = ceil((float)now / div_ms) * div_ms;
                    } else {
                        voices[v_idx].trigger_time = now;
                    }
                    voices[v_idx].off_time = voices[v_idx].trigger_time + target_dur;
                }
                last_trigger = millis();
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Init USB MIDI
    TinyUSBDevice.setManufacturerDescriptor("Electricity for Progress");
    TinyUSBDevice.setProductDescriptor("Biodata V009"); 
    usb_midi_interface.begin();
    usbMIDI.begin(MIDI_CHANNEL_OMNI);

    // Init V007 LEDs
    for(byte i=0; i<5; i++) {
        ledFaders[i].Setup();
        ledFaders[i].Set(ledFaders[i].maxBright, 1000); 
    }
    delay(1000);
    for(byte i=0; i<5; i++) { ledFaders[i].Set(0, 1000); }

    // Init NVS
    prefs.begin("bio_cfg", false);
    if (prefs.getBytesLength("cfg") == sizeof(ChannelConfig)) {
        prefs.getBytes("cfg", &config, sizeof(ChannelConfig));
    } else {
        prefs.putBytes("cfg", &config, sizeof(ChannelConfig));
    }

//init BLE stack
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    int uniq = mac[0] + mac[1] + mac[2] + mac[3] + mac[4] + mac[5];
    String deviceName = "Biodata-" + String(uniq);

    // Init Native BLE Stack
    BLEDevice::init(deviceName.c_str());
    BLEDevice::setMTU(512);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pMidiService = pServer->createService("03b80e5a-ede8-4b33-a751-6ce34ec4c700");
    pMidiCharacteristic = pMidiService->createCharacteristic("7772e5db-3868-4112-a1a9-f2669d106bf3", BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE_NR);
    pMidiCharacteristic->addDescriptor(new BLE2902());
    pMidiCharacteristic->setCallbacks(new MidiRxCallbacks()); 
    pMidiService->start();
    
    BLEService *pUartService = pServer->createService("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
    pUartTxCharacteristic = pUartService->createCharacteristic("6e400003-b5a3-f393-e0a9-e50e24dcca9e", BLECharacteristic::PROPERTY_NOTIFY);
    pUartTxCharacteristic->addDescriptor(new BLE2902());
    
    BLECharacteristic *pUartRxCharacteristic = pUartService->createCharacteristic("6e400002-b5a3-f393-e0a9-e50e24dcca9e", BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    pUartRxCharacteristic->setCallbacks(new UartRxCallbacks());
    pUartService->start();
    
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(pMidiService->getUUID());
    pAdvertising->addServiceUUID(pUartService->getUUID());
    pAdvertising->start();

    // Init WiFi AppleMIDI (Credentials carried over from V007)
    WiFi.begin("OxfordEast", "oxfordeast");
    AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc, const char* name) { 
        rtpConnected = true; 
        ledFaders[4].Set(ledFaders[4].maxBright, 500); // White LED feedback
        delay(500);
        ledFaders[4].Set(0, 500);
    });
    AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) { rtpConnected = false; });
    MIDI.begin();

    // Init High Sensitivity Hardware Interrupt
    pinMode(BIO_PIN, INPUT_PULLUP); 
    attachInterrupt(digitalPinToInterrupt(BIO_PIN), isr_bio, CHANGE);
}

void loop() {
    uint32_t current_millis = millis();

    // Update LED Fades
    for(byte i=0; i<5; i++) ledFaders[i].Update();

    // Additive Potentiometer Control (Additive offset overriding web UI until synced again)
    int knobValue = analogRead(POT_PIN);
    if (abs(knobValue - prevKnob) > 40) { // Slight deadzone threshold 
        prevKnob = knobValue;
        config.thresh_base = mapfloat(knobValue, 0, 4095, 1.0, 5.0);
        config.threshold = config.thresh_base + config.thresh_boost; 
        prefs_dirty = true;
        last_param_change = current_millis;
    }

    // Process USB MIDI CCs
    while (usbMIDI.read()) {
        if (usbMIDI.getType() == midi::ControlChange) {
            uint8_t rx_ch = usbMIDI.getChannel(); 
            if (config.midi_channel == rx_ch) processIncomingCC(usbMIDI.getData1(), usbMIDI.getData2());
        }
    }

    // Process AppleMIDI
    if (WiFi.status() == WL_CONNECTED) MIDI.read();

    // Process Bio Pulses
    if (new_pulse) {
        noInterrupts();
        uint32_t pulse_us = high_time_cc / 240; // 240MHz CPU clock mapping
        uint32_t raw_ticks = high_time_cc; 
        new_pulse = false;
        interrupts();
        
        if (pulse_us > 5 && pulse_us < 50000) analyze_and_map(raw_ticks);
    }

    // Handle Note Trigger & Hardware Legato
    for (int v = 0; v < VOICES_PER_CH; v++) {
        PolyNote& p = voices[v];
        if (p.active) {
            if (!p.note_on_sent && current_millis >= p.trigger_time) {
                sendBleMidi(0x90 + (config.midi_channel - 1), p.pitch, p.vel);
                usbMIDI.sendNoteOn(p.pitch, p.vel, config.midi_channel);
                if (rtpConnected) MIDI.sendNoteOn(p.pitch, p.vel, config.midi_channel);
                
                if (p.is_legato_transition) {
                    sendBleMidi(0x80 + (config.midi_channel - 1), p.old_pitch, 0);
                    usbMIDI.sendNoteOff(p.old_pitch, 0, config.midi_channel);
                    if (rtpConnected) MIDI.sendNoteOff(p.old_pitch, 0, config.midi_channel);
                    p.is_legato_transition = false;
                }

                p.note_on_sent = true;
                
                // Map voices across the 5 V007 LEDs
                uint8_t target_led = v % 5;
                ledFaders[target_led].Set(ledFaders[target_led].maxBright, 350);
                
                if (deviceConnected) {
                    String noteJson = "{\"type\":\"note\",\"ch\":0,\"n\":" + String(p.pitch) + ",\"v\":" + String(p.vel) + ",\"d\":" + String(p.off_time - p.trigger_time) + "}\n";
                    pUartTxCharacteristic->setValue((uint8_t*)noteJson.c_str(), noteJson.length());
                    pUartTxCharacteristic->notify();
                }
            }
            else if (p.note_on_sent && current_millis >= p.off_time) {
                sendBleMidi(0x80 + (config.midi_channel - 1), p.pitch, 0);
                usbMIDI.sendNoteOff(p.pitch, 0, config.midi_channel);
                if (rtpConnected) MIDI.sendNoteOff(p.pitch, 0, config.midi_channel);
                
                uint8_t target_led = v % 5;
                ledFaders[target_led].Set(0, 800); // Fade out

                p.active = false;
            }
        }
    }

    // Telemetry Sync over BLE UART
    if (deviceConnected && (current_millis - last_telemetry > 25)) {
        if (tx_ready) {
            String json_payload = "{\"type\":\"raw\",\"ch\":0,\"p\":[";
            for (int i=0; i<10; i++) { json_payload += String(tx_buffer[i]); if (i < 9) json_payload += ","; }
            json_payload += "]}\n";
            tx_ready = false;
            pUartTxCharacteristic->setValue((uint8_t*)json_payload.c_str(), json_payload.length());
            pUartTxCharacteristic->notify();
        }
        last_telemetry = current_millis;
    }

    // Write NVS settings to flash after cooldown
    if (prefs_dirty && current_millis - last_param_change > 3000) { 
        prefs.putBytes("cfg", &config, sizeof(ChannelConfig)); 
        prefs_dirty = false; 
    }
}