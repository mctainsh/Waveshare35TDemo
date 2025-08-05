# Waveshare35TDemo
Demo of the Waveshare 3.5" touch screen device

## LVGL Setup Instructions

To use the built-in examples and demos of LVGL;
1) Expand folder `.pio/libdeps/Waveshare-S3-35/lvgl`
2) Move the `lvgl/demos` folder to `lvgl/src/demos`
3) Open file `lvgl/src/demos/widgets/lv_demo_widgets.h`
4) Replace the line:
   ```cpp     
   #include "../../src/draw/lv_draw.h"   
   ```
   with:
   ```cpp
   #include "../../../src/draw/lv_draw.h"
   ```
5) Replace the line:
   ```cpp
   #include "../../src/draw/lv_draw_triangle.h"
   ```
   with:
   ```cpp
   #include "../../../src/draw/lv_draw_triangle.h"
   ```

## Configuration Options

Uncomment `LANDSCAPE_MODE` to enable landscape mode (default is portrait mode)

