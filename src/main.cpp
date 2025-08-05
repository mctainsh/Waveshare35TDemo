#include <Arduino.h>
#include "../.pio/libdeps/Waveshare-S3-35/lvgl/src/demos/widgets/lv_demo_widgets.h"
#include "../.pio/libdeps/Waveshare-S3-35/lvgl/src/demos/benchmark/lv_demo_benchmark.h"
#include "../.pio/libdeps/Waveshare-S3-35/lvgl/src/demos/stress/lv_demo_stress.h"
#include "../.pio/libdeps/Waveshare-S3-35/lvgl/src/demos/keypad_encoder/lv_demo_keypad_encoder.h"
#include "../.pio/libdeps/Waveshare-S3-35/lvgl/src/demos/music/lv_demo_music.h"
#include "../.pio/libdeps/Waveshare-S3-35/lvgl/src/demos/render/lv_demo_render.h"

#include <lvgl.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////
// To use the built-in examples and demos of LVGL uncomment the includes below respectively.
// 	1) Expand folder '.pio/libdeps/Waveshare-S3-35/lvgl'
// 	2) Move the `lvgl/demos` folder to `lvgl/src/demos`
// 	3) Open file `lvgl/src/demos/widgets/lv_demo_widgets.h`
// 	4) Replace the line:
// 			#include "../../src/draw/lv_draw.h"
// 		with:
// 			#include "../../../src/draw/lv_draw.h"
// 	5) Replace the line:
// 			#include "../../src/draw/lv_draw_triangle.h"
// 		with:
// 			#include "../../../src/draw/lv_draw_triangle.h"

//#define LANDSCAPE_MODE // Uncomment to enable landscape mode (default is portrait mode)

#define DIRECT_RENDER_MODE // Uncomment to enable full frame buffer

#include <Arduino_GFX_Library.h>

#include "TCA9554.h"

#include "esp_lcd_touch_axs15231b.h"
#include <Wire.h>

#define GFX_BL 6 // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

#define LCD_QSPI_CS 12
#define LCD_QSPI_CLK 5
#define LCD_QSPI_D0 1
#define LCD_QSPI_D1 2
#define LCD_QSPI_D2 3
#define LCD_QSPI_D3 4

#define I2C_SDA 8
#define I2C_SCL 7

TCA9554 TCA(0x20);

Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_QSPI_CS, LCD_QSPI_CLK, LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);

Arduino_GFX *gfx = new Arduino_AXS15231B(bus, -1 /* RST */, 0 /* rotation */, false, 320, 480);

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf1;
lv_color_t *disp_draw_buf2;

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf)
{
	LV_UNUSED(level);
	Serial.println(buf);
	Serial.flush();
}
#endif

uint32_t millis_cb(void)
{
	return millis();
}

///////////////////////////////////////////////////////////////////////////////
// LVGL calls it when a rendered image needs to copied to the display
// .. only used when not rotated (Portrait mode)
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
	uint32_t w = lv_area_get_width(area);
	uint32_t h = lv_area_get_height(area);

	gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

	/*Call it to tell LVGL you are ready*/
	lv_disp_flush_ready(disp);
}

///////////////////////////////////////////////////////////////////////////////
// Buffer for rotated image (Landscape mode)
// This buffer is used to store the rotated image before sending it to the display
// It is allocated dynamically to fit the size of the image being rotated
lv_color16_t *pRotatedBuf = nullptr; // Buffer for rotated image
void rotated_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
	const uint16_t w = lv_area_get_width(area);
	const uint16_t h = lv_area_get_height(area);

	if (pRotatedBuf == nullptr)
		pRotatedBuf = new lv_color16_t[h * w]; // Adjust size!

	lv_color16_t *src = (lv_color16_t *)px_map;
	lv_color16_t *dst = pRotatedBuf;

	for (uint16_t y = 0; y < h; y++)
		for (uint16_t x = 0; x < w; x++)
			dst[x * h + (h - y - 1)] = src[y * w + x]; // 90° clockwise

	// Send rotated_buf to your display here
	gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)dst, h, w);

	lv_display_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
	touch_data_t touch_data;
	bsp_touch_read();

	if (bsp_touch_get_coordinates(&touch_data))
	{
		data->state = LV_INDEV_STATE_PR;
		/*Set the coordinates*/
		data->point.x = touch_data.coords[0].x;
		data->point.y = touch_data.coords[0].y;
	}
	else
	{
		data->state = LV_INDEV_STATE_REL;
	}
}

void setup()
{
#ifdef DEV_DEVICE_INIT
	DEV_DEVICE_INIT();
#endif
	Wire.begin(I2C_SDA, I2C_SCL);
	TCA.begin();
	TCA.pinMode1(1, OUTPUT);
	TCA.write1(1, 1);
	delay(10);
	TCA.write1(1, 0);
	delay(10);
	TCA.write1(1, 1);
	delay(200);
	bsp_touch_init(&Wire, -1, 0, 320, 480);
	Serial.begin(115200);
	// Serial.setDebugOutput(true);
	// while(!Serial);
	Serial.println("Arduino_GFX LVGL_Arduino_v9 example ");
	String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
	Serial.println(LVGL_Arduino);

	// Init Display
	if (!gfx->begin())
	{
		Serial.println("gfx->begin() failed!");
	}
	gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
	pinMode(GFX_BL, OUTPUT);
	digitalWrite(GFX_BL, HIGH);
#endif

	lv_init();

	/*Set a tick source so that LVGL will know how much time elapsed. */
	lv_tick_set_cb(millis_cb);

	/* register print function for debugging */
#if LV_USE_LOG != 0
	lv_log_register_print_cb(my_print);
#endif

	screenWidth = gfx->width();
	screenHeight = gfx->height();
	// screenWidth = gfx->height();
	// screenHeight = gfx->width();

#ifdef DIRECT_RENDER_MODE
	bufSize = screenWidth * screenHeight;
	disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
	bufSize = screenWidth * 40;
	disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
	if (!disp_draw_buf1 || !disp_draw_buf2)
	{
		Serial.println("LVGL disp_draw_buf allocate failed!");
	}
	else
	{


#ifdef LANDSCAPE_MODE
		disp = lv_display_create(screenWidth, screenHeight);
		lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270); // Used to rotate touch screen
		lv_display_set_flush_cb(disp, rotated_flush_cb);		// Use this for rotation support
#else
		disp = lv_display_create(screenHeight, screenWidth);
		lv_display_set_flush_cb(disp, my_disp_flush);
#endif

#ifdef DIRECT_RENDER_MODE
		lv_display_set_buffers(disp, disp_draw_buf1, disp_draw_buf2, bufSize * 2, LV_DISPLAY_RENDER_MODE_FULL);
#else
		lv_display_set_buffers(disp, disp_draw_buf1, disp_draw_buf2, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

		/*Initialize the (dummy) input device driver*/
		lv_indev_t *indev = lv_indev_create();
		lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
		lv_indev_set_read_cb(indev, my_touchpad_read);

		// lv_obj_t * tex = lv_3dtexture_create(parent);
		// /* Render something to the texture. You can replace it with your code. */
		// lv_3dtexture_id_t gltf_texture = render_gltf_model_to_opengl_texture(path, screenWidth, screenHeight, color);
		// lv_3dtexture_set_src(tex, gltf_texture);
		// lv_obj_set_size(tex, screenWidth, screenHeight);
		// lv_obj_set_style_opa(tex, opa, 0);

		/* Option 1: Create a simple label
		 * ---------------------
		 */
		/*lv_obj_t *label = lv_label_create(lv_scr_act());
		lv_label_set_text(label, "Hello Arduino, I'm LVGL!(V" GFX_STR(LVGL_VERSION_MAJOR) "." GFX_STR(LVGL_VERSION_MINOR) "." GFX_STR(LVGL_VERSION_PATCH) ")");
		lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

		lv_obj_t *sw = lv_switch_create(lv_scr_act());
		lv_obj_align(sw, LV_ALIGN_TOP_MID, 0, 50);

		sw = lv_switch_create(lv_scr_act());
		lv_obj_align(sw, LV_ALIGN_BOTTOM_MID, 0, -50);*/

		/* Option 2: Try an example. See all the examples
		 *  - Online: https://docs.lvgl.io/master/examples.html
		 *  - Source codes: https://github.com/lvgl/lvgl/tree/master/examples
		 * ----------------------------------------------------------------
		 */
		// lv_example_btn_1();

		/* Option 3: Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMOS_WIDGETS
		 * -------------------------------------------------------------------------------------------
		 */
		lv_demo_widgets();
		// lv_demo_benchmark();
		// lv_demo_keypad_encoder();
		// lv_demo_music();
		// lv_demo_stress();
	}

	Serial.println("Setup done");
}

void loop()
{
	lv_task_handler(); /* let the GUI do its work */
	delay(5);
}
