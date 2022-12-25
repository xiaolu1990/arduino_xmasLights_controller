# arduino_xmasLights_controller
simple WS2812B LED strip controller based on Arduino platform. 


**Features:**

1. Solid color control with dimming function.
2. Several pre-built led patterns such as twinkle effect, breathing effect, chasing effect, etc.
3. Play Christmas songs together with LED sparkling effect.

**Hardware:**

this is a small project that utilizes an Arduino Nano to control any typical addressable 5V LED strip using WS2812. The parts needed are:
- Arduino Nano x 1 (main controller)
- Rotary Encoder x 1 (menu control knob)
- Rotary potentiometer x 1 (led dimming control)
- 0.91" OLED display x 1 
- Buzzer x 1
- Some other passive compoenents, see in schematic.

I have made the parts together on a perf board and upload the code in this repo, and it works nicely :) 

Check the [Demo on Youtube](https://www.youtube.com/shorts/xX7Cnof2Gqg)

I have also designed a PCB in Kicad for this project, details inside `hardware/` folder.
![PCB_FRONT](https://github.com/xiaolu1990/arduino_xmasLights_controller/blob/main/img/pcb_front.png)
![PCB_BACK](https://github.com/xiaolu1990/arduino_xmasLights_controller/blob/main/img/pcb_back.png)

**Firmware:**

The source code and platformIO config file for the demo video are located under folder `firmware/`.

Merry Christmas and happy new year 2023,  
Zhangshun
