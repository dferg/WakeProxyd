# WakeProxyd - Wake your computer with a Raspberry Pi!

Some motherboards don't support wake-on-LAN. Sometimes wake-on-LAN is supported, but you just can't get it working properly (for instance, many Hackintosh's).

What to do? Use a Raspberry Pi as a wake-on-LAN proxy, of course!

## Theory of Operation

The Raspberry Pi listens for magic wake-on-LAN packets destined for your machine's MAC address. It then pulses the power switch connector on your motherboard to wake your computer. To avoid accidental "pressing" of the power button while the computer is on, the Pi also has a connection to your motherboard's power LED. If the computer is on, the daemon running on the Raspberry Pi will ignore any wake-on-LAN packets.

## Prerequisites

* A properly working system that sleeps and wakes when you press the physical power button on your case.
* A Raspberry Pi (Model A or B, 256MB or 512MB, it doesn't matter)
* Soldering skills and general electronics knowledge.
* Nerves of steel and the willingness to connnect a custom-built circuit up to your possibly very expensive motherboard.
* Linux command line skills

## Disclaimer

This is a hack I put together in a night. If you blow up your computer (and you definitely could) or if you destroy your Raspberry Pi, you can't hold me responsible.

Check my schematic to make sure you agree with the implementation!  Check your own work to ensure that you didn't miswire something!

In general, this is a potentially dangerous project. Do not try this if you do not accept this disclaimer or if you do not know what you are doing.

Also, this is as-is. No support from me. (Although Github pull requests and issues are welcome.)

## Schematic

* WAKE/SLEEP signal is on GPIO_17
* POWER_OFF_STATE signal is on GPIO_22

![WakeProxyd Schematic](https://raw.github.com/dferg/WakeProxyd/master/WakeProxyd%20Schematic.png)

## Software Installation

1. Install [Arch Linux](http://archlinuxarm.org/platforms/armv6/raspberry-pi) on your Raspberry Pi.
2. Install necessary packages with "pacman -S gcc" (There may be more though.. Can't remember)
3. Install wiringPi library. See https://github.com/WiringPi/WiringPi
4. Run "make && make install" to build the wake_proxyd binary.
5. Copy wake_proxyd.conf to /etc and edit with your computer's MAC address.
6. Run "systemctl enable wake_proxyd && systemctl start wake_proxyd" to start the daemon immediately and automatically at a reboot.

## To Manually Test (without the daemon)

You can manually test your connections with wiringPi's gpio tool.

### To intialize
```
gpio -g mode 17 out
gpio -g mode 22 in
```

### Read power state (0 = power on, 1 = power off)
```
gpio -g read 22
```

### Send wake/sleep command
```
gpio -g write 17 1; sleep 0.1; gpio -g write 17 0
```

## TODO / Future

* Add ability to detect if the computer is OFF (Currently, the daemon has no way to know if the computer is OFF or sleeping. Therefore a WOL packet received while the computer is OFF will turn it ON.)

