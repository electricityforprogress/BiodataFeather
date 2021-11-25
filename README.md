# BiodataFeather
Feather daughter board for the Adafruit ESP32 and Huzzah (and others), allowing BLE and RTPMIDI via wifi.
Still a work in progress, and can provide the framework for anyone to use other Adafruit Feather boards
     Lots of Biodata information on my website electricityforprogress.com
See also the project submission on [Hackaday.io](https://hackaday.io/project/168811-biodata-sonification-feather)
Parts Listing / BOM

Menu Modes:

Red - Note Scaling - Entering this mode will illuminate the Red LED, by turning the knob one of the other LEDs will flash indicating a different Note Scaling. Flashing Red = Chromatic, Orange = Minor, Green = Diatonic Minor, Blue = Major, White = Indian

Orange - MIDI Channel - Entering this mode will illuminate the Orange LED, by turning the knob different combinations of the LEDs are used to display the MIDI Channel in 'Binary'.  MIDI Channel can be configured between 1-16, each of the 5 LEDs are used to display the MIDI channel chosen where 1 = 1xxxx, 2 = x1xxx, 3 = 11xxx, etc. The MIDI Channel can then be saved by pressing the button.  So far, this MIDI selection method has been a big PiTA for users, but it is kinda clever.

Green - Wifi On/Off - This mode turns on and off Wifi, default is off. Control is similar to the Bluetooth mode.  If you have uploaded code with your wifi credentials, you can connect to WiFi MIDI

Blue - Bluetooth On/Off - This mode turns on and off Bluetooth, default is ON.  The Blue LED will illuminate and either the Red LED (indicating off) or White LED (indicating on) can be selected by turning the knob.  Press the button again to save the setting.

White - Battery Level - Entering into this mode will display the battery charge percentage by illuminating the white LED and one of the other colors will flash.  Flashing blue = 100%, green = 80%, Yellow = 50%, Green = 30%
