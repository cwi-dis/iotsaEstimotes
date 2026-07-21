# Estimote Sensor gateway

![build-platformio](https://github.com/cwi-dis/iotsaEstimotes/workflows/build-platformio/badge.svg)
![build-arduino](https://github.com/cwi-dis/iotsaEstimotes/workflows/build-arduino/badge.svg)

This iotsa application listens for Estimote sensors over Bluetooth LE and provides the readings of the individual sensors as REST values, or from a web page.

## History

Forked in late 2025 from [iotsaDMXSensor](https://github.com/cwi-dis/iotsaDMXSensor) (which
issues DMX/Art-Net commands from Estimote readings, for the older *Lit Lace* ballet-lighting
project) for the *Visboeck* project, which needs the raw sensor readings over REST for a
Unity-based VR application. Started as a quick-and-dirty hack; the project is still ongoing.
The two repos' Estimote-parsing code has since diverged (this one has been enriched with
temperature/voltage/movement parsing that iotsaDMXSensor's copy doesn't have) — see the tracking
issue about whether the two repos should eventually be unified (with DMX/Art-Net output becoming
optional).

