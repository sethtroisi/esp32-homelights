# FastLED with ESP-idf v5

This used to the the FastLED-idf repo but now it's just FastLED

First of all, if you're changing versions, you have to be quite careful. The only foolproof method
I've found is a brand new clone, a new install.sh, manually removing the project's build directory,
and using the defult sdkconfig with a menuconfig, setting the parameters you need within this version.

Within menuconfig, I tend to use the following settings. Minimum required silicon rev to 1, because there
are speed workarounds at Rev0 that get compiled in. I change the compiler options to -O2 instead of -Os or -Og.
Default flash size to 4M, because all the devices I have are 4G.


##

```
find . -type l | wc
# Should be 2 less because of WS2812Emulator
ls ~/Scripts/Arduino/photon/HomeLights/src/*{.h,cpp} | wc
ln -sf ~/Scripts/Arduino/photon/HomeLights/src/*{.h,cpp} .
ln -sf ~/Scripts/Arduino/WS2812Emulator/Particle.{cpp,h} .
ln -sf ~/FastLED/ components/FastLED

# Manual linking
#ln -f -s ~/Arduino/photon/HomeLights/src/{homegrown_effects,consts,color_consts,adopted_effects}.h .
#ln -f -s ~/Arduino/photon/HomeLights/src/{PatternRunner,fake_shader,tweaks,globals}.{cpp,h} .
#ln -sf ~/Arduino/WS2812Emulator/Particle.{cpp,h} .

```
