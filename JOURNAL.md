## Athena V2 (tier 5, design: draft, build: draft)

- Project ID: cmmb62vvn0001011i7szqlwug
- GitHub: https://github.com/notaroomba/athena
- Cover image: https://stasis.hackclub-assets.com/images/1773033597294-fss886.png
- Created: 2026-03-04
- Updated: 2026-03-09

Description:

New flight controller dropped gng

### Work sessions

#### 2026-03-09 09:19, Planning (7h, stage: DESIGN)

Categories: DESIGN_PLANNING, SCHEMATIC

So basically I kinda wanna make a v2 of athena that isn't as expensive and is super small and portable and can actually fit into a rocket AND DRONE (future project) so yea.

Bc I want this to be small and portable I want to actually learn stuff like rtos and stuff like that and what better chip to use than the STM32H755ZIT6 on LCSC! Specifically because it has 2 cores that can run different code and I can use that instead of having 3 different MCUs and stuff on my board.

I also want this flight controller to be general purpose for both drones and rockets so that I can use it in any future endeavours.

After thinking for a while and doing some research most drones use an ESC that can support up to 40A of current for the servos, this would not be needed so for any future projects I plan on just buying that apart. The thing is, to contorl that I need som dshot pins which is a digital output pin coming from the MCU that basically tells the ESC how to function. For a rocket though I can just connect it directly to the battery I believe.

Then after some more reasearching I found out that ESC's usually have a power rail to connect to an external flight controller thats 9v or 5v. So I plan on using that to connect it to the flight controller and then regulating the voltage down to 3v3. Also I want to add in a 2s battery support with the BQ25887 because I want that power just in case the esc fails or something. Also it helps because when I use the flight controller for a rocket it can power some servos and the rest of the controller and be fine.

Components list bc ye:

- GPS: https://www.lcsc.com/product-detail/C6990279.html or https://www.lcsc.com/product-detail/C4153167.html
- Bluetooth: https://www.lcsc.com/product-detail/C519278.html
- IMU: https://www.lcsc.com/product-detail/C1850418.html
- Secondary IMU: https://www.lcsc.com/product-detail/C2836813.html
- Barometer: https://www.lcsc.com/product-detail/C5362283.html
- Magnetometer: https://www.lcsc.com/product-detail/C17225907.html
- Current Sensor: https://www.lcsc.com/product-detail/C49851.html
- Battery charger: https://www.lcsc.com/product-detail/C2761614.html
- 5V Fixed Buck Boost: https://www.lcsc.com/product-detail/C2876599.html
- 3V3 @ 3A Buck: https://www.lcsc.com/product-detail/C181974.html
- MicroSD: https://www.lcsc.com/product-detail/C91145.html
- LoRa: https://www.lcsc.com/product-detail/C191341.html
- Flash 16MB: https://www.lcsc.com/product-detail/C113767.html
- Fixed 3V3 LDO for sensors: https://www.lcsc.com/product-detail/C2862740.html

SO then I oppened kicad and started doing stuff but irst I have to import all of the components using a handy script I made:

![image](https://stasis.hackclub-assets.com/images/1773036508908-9lprsz.png)

![image](https://stasis.hackclub-assets.com/images/1773036522896-22qpeu.png)

Media:

- [IMAGE] https://stasis.hackclub-assets.com/images/1773036508908-9lprsz.png
- [IMAGE] https://stasis.hackclub-assets.com/images/1773036522896-22qpeu.png

# Planning Again

So after talking wiht Claude a Bit I have decided to split up the functionality of each process again. I plan on then adding an STM32WB55 instead of having a seperate Bluetooth chip (NXP) and then I was thinking even further to divide the chips so that the h7 is focused soley on the flight software and then the WB55 is focused on all of the telemetry and all of that.

I was thinking that the WB55 could have access ot the parachute charge just in case something goes wrong I can at least fire that from the bottom and yea, I could also hook up its own Barometer or IMU so that it detects at least how high it is and then if its falling and theres no response form the H7 then it can fire the parachute and save the rocket. I could also have it send out a distress signal using the LoRa if something goes wrong and the H7 is not responding.

The H7 should have its own channel sto control pyro and also ESC's just in case I want to turn it into a drone
