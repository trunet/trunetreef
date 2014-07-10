/*
This file is part of Trunetreef.

Trunetreef is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Trunetreef is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Trunetreef.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ch.h"
#include "hal.h"

#include "chprintf.h"

#include "gfx.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
void mysave(uint16_t instance, const uint8_t *calbuf, size_t sz)
{
    (void)instance;
    (void)calbuf;
    (void)sz;
}

const char *myload(uint16_t instance)
{
    (void)instance;
    return(NULL);
}
*/

#define ADC_GRP1_NUM_CHANNELS   1
#define ADC_GRP1_BUF_DEPTH      16
#define ADC_GRP2_NUM_CHANNELS   1
#define ADC_GRP2_BUF_DEPTH      16
static adcsample_t samples1[ADC_GRP1_NUM_CHANNELS * ADC_GRP1_BUF_DEPTH];
static adcsample_t samples2[ADC_GRP2_NUM_CHANNELS * ADC_GRP2_BUF_DEPTH];

static const ADCConversionGroup adcgrpcfg1 = {
  FALSE,
  ADC_GRP1_NUM_CHANNELS,
  NULL,
  NULL,
  0, 0,                         /* CR1, CR2 */
  ADC_SMPR2_SMP_AN0(ADC_SAMPLE_239P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(ADC_GRP1_NUM_CHANNELS),
  0,                            /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN0)
};

static const ADCConversionGroup adcgrpcfg2 = {
  FALSE,
  ADC_GRP1_NUM_CHANNELS,
  NULL,
  NULL,
  0, 0,                         /* CR1, CR2 */
  ADC_SMPR2_SMP_AN1(ADC_SAMPLE_239P5),
  0,                            /* SMPR2 */
  ADC_SQR1_NUM_CH(ADC_GRP2_NUM_CHANNELS),
  0,                            /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN1)
};

const GWidgetStyle RedWidgetStyle = {
        HTML2COLOR(0xFF0000),                   // window background

        // enabled color set
        {
                HTML2COLOR(0xFFFFFF),           // text
                HTML2COLOR(0x404040),           // edge
                HTML2COLOR(0xE0E0E0),           // fill
                HTML2COLOR(0xE0E0E0),           // progress - inactive area
        },

        // disabled color set
        {
                HTML2COLOR(0xC0C0C0),           // text
                HTML2COLOR(0x808080),           // edge
                HTML2COLOR(0xE0E0E0),           // fill
                HTML2COLOR(0xC0E0C0),           // progress - active area
        },

        // pressed color set
        {
                HTML2COLOR(0x404040),           // text
                HTML2COLOR(0x404040),           // edge
                HTML2COLOR(0x808080),           // fill
                HTML2COLOR(0x00E000),           // progress - active area
        },
};

const GWidgetStyle BlueWidgetStyle = {
        HTML2COLOR(0x0000FF),                   // window background

        // enabled color set
        {
                HTML2COLOR(0xFFFFFF),           // text
                HTML2COLOR(0x404040),           // edge
                HTML2COLOR(0xE0E0E0),           // fill
                HTML2COLOR(0xE0E0E0),           // progress - inactive area
        },

        // disabled color set
        {
                HTML2COLOR(0xC0C0C0),           // text
                HTML2COLOR(0x808080),           // edge
                HTML2COLOR(0xE0E0E0),           // fill
                HTML2COLOR(0xC0E0C0),           // progress - active area
        },

        // pressed color set
        {
                HTML2COLOR(0x404040),           // text
                HTML2COLOR(0x404040),           // edge
                HTML2COLOR(0x808080),           // fill
                HTML2COLOR(0x00E000),           // progress - active area
        },
};

static THD_WORKING_AREA(waThreadLed, 64);
static THD_FUNCTION(ThreadLed, arg) {

	(void) arg;

	chRegSetThreadName("blinker");
	while (TRUE) {
		palSetPad(GPIOB, GPIOB_LED1);
		chThdSleepMilliseconds(250);
		palClearPad(GPIOB, GPIOB_LED1);
		chThdSleepMilliseconds(250);
	}

	return 0;
}

static THD_WORKING_AREA(waThreadLCD, 1024);
static THD_FUNCTION(ThreadLCD, arg) {

	(void) arg;

    GWidgetInit	wi;
    GListener gl;
    GEvent* pe;
	GHandle GWTop, GWBottom, GWMain;
	GHandle	ghLabelTime, ghLabelRuntime;
	GHandle	ghLabelADC1, ghLabelADC2;
	GHandle ghProgressBarSump, ghProgressBarATO;
	GHandle ghContainerWaterLevel;
	char bufRTCTime[21];
	RTCTime rtctime;
	time_t rtctimeconv;
	struct tm ts;
	char bufRuntime[11];
	uint32_t runtime;
	uint32_t i, adc1, adc2;

	gfxInit();
	palSetPad(GPIOB, GPIOB_BL_CNT);
	gdispSetOrientation(GDISP_ROTATE_270);
	gdispClear(Black);

	//ginputSetMouseCalibrationRoutines(0, mysave, myload, FALSE);
	ginputGetMouse(0);

	// Set the widget defaults
    gwinSetDefaultFont(gdispOpenFont("UI1"));
    gwinSetDefaultStyle(&WhiteWidgetStyle, FALSE);
    gdispClear(White);

    // Attach the mouse input
    gwinAttachMouse(0);

	gwinWidgetClearInit(&wi);
	wi.g.show = TRUE; wi.g.x = 0; wi.g.y = 0; wi.g.width = 320; wi.g.height = 20;
	GWTop = gwinWindowCreate(0, &wi.g);
	wi.g.show = TRUE; wi.g.x = 0; wi.g.y = 220; wi.g.width = 320; wi.g.height = 20;
	GWBottom = gwinWindowCreate(0, &wi.g);
	wi.g.show = TRUE; wi.g.x = 0; wi.g.y = 20; wi.g.width = 320; wi.g.height = 200;
	GWMain = gwinWindowCreate(0, &wi.g);

	gwinSetColor(GWTop, White);
	gwinSetBgColor(GWTop, Red);
	gwinSetColor(GWBottom, White);
	gwinSetBgColor(GWBottom, Blue);
	gwinSetColor(GWMain, White);
	gwinSetBgColor(GWMain, Black);

	gwinClear(GWTop);
	gwinClear(GWBottom);
	gwinClear(GWMain);

	// Top Widgets
	gwinSetDefaultStyle(&RedWidgetStyle, FALSE);
	wi.g.x = 0, wi.g.y = 0; wi.g.width = 60; wi.g.height = 20;
	wi.text = "TrunetReef";
	gwinLabelCreate(0, &wi);

	wi.g.x = 200, wi.g.y = 0; wi.g.width = 120; wi.g.height = 20;
	wi.text = "";
	ghLabelTime = gwinLabelCreate(0, &wi);
	gwinSetDefaultStyle(&WhiteWidgetStyle, FALSE);

	// Bottom Widgets
	gwinSetDefaultStyle(&BlueWidgetStyle, FALSE);
	wi.g.x = 200, wi.g.y = 220; wi.g.width = 120; wi.g.height = 20;
	wi.text = "";
	ghLabelRuntime = gwinLabelCreate(0, &wi);
	gwinLabelSetAttribute(ghLabelRuntime, 54, "Runtime:");
	gwinSetDefaultStyle(&WhiteWidgetStyle, FALSE);

	// ADC Reading
	wi.g.x = 0, wi.g.y = 120; wi.g.width = 90; wi.g.height = 10;
	wi.text = "0";
	ghLabelADC1 = gwinLabelCreate(0, &wi);
	gwinLabelSetAttribute(ghLabelADC1, 42, "ADC1:");
	wi.g.x = 0, wi.g.y = 130; wi.g.width = 90; wi.g.height = 10;
	wi.text = "0";
	ghLabelADC2 = gwinLabelCreate(0, &wi);
	gwinLabelSetAttribute(ghLabelADC2, 42, "ADC2:");

	// Water Level
	wi.g.show = FALSE;
	wi.g.x = 190;
	wi.g.y = 30;
	wi.g.width = 114;
	wi.g.height = 59;
	wi.text = "Water Level";
	ghContainerWaterLevel = gwinContainerCreate(0, &wi, GWIN_CONTAINER_BORDER);
	wi.g.show = TRUE;

	wi.g.x = 5, wi.g.y = 5; wi.g.width = 100; wi.g.height = 20;
	wi.text = "ATO Container";
	wi.g.parent = ghContainerWaterLevel;
	ghProgressBarATO = gwinProgressbarCreate(0, &wi);

	wi.g.x = 5, wi.g.y = 30; wi.g.width = 100; wi.g.height = 20;
	wi.text = "Sump";
	wi.g.parent = ghContainerWaterLevel;
	ghProgressBarSump = gwinProgressbarCreate(0, &wi);

	gwinShow(ghContainerWaterLevel);
	// End Of Water Level

	geventListenerInit(&gl);
	gwinAttachListener(&gl);

	chRegSetThreadName("LCD");
	while (TRUE) {
		pe = geventEventWait(&gl, TIME_IMMEDIATE);

		adcConvert(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);
		adcConvert(&ADCD1, &adcgrpcfg2, samples2, ADC_GRP2_BUF_DEPTH);
		adc1 = 0;
		adc2 = 0;
		for (i=0; i<ADC_GRP1_BUF_DEPTH; i++)
			adc1 += samples1[i];
		for (i=0; i<ADC_GRP2_BUF_DEPTH; i++)
			adc2 += samples2[i];
		adc1 /= ADC_GRP1_BUF_DEPTH;
		adc2 /= ADC_GRP2_BUF_DEPTH;
		snprintf(bufRuntime, 10, "%lu", adc1);
		gwinSetText(ghLabelADC1, bufRuntime, TRUE);
		snprintf(bufRuntime, 10, "%lu", adc2);
		gwinSetText(ghLabelADC2, bufRuntime, TRUE);

		rtc_lld_get_time(&RTCD1, &rtctime);
		rtctimeconv = (time_t)rtctime.tv_sec;
		ts = *localtime(&rtctimeconv);
		strftime(bufRTCTime, 20, "%d/%m/%Y %H:%M:%S", &ts);
		gwinSetText(ghLabelTime, bufRTCTime, TRUE);

		runtime = ST2S(chVTGetSystemTime());
		snprintf(bufRuntime, 10, "%lu", runtime);
		gwinSetText(ghLabelRuntime, bufRuntime, TRUE);

		gwinProgressbarSetPosition(ghProgressBarATO, 80);
		gwinProgressbarSetPosition(ghProgressBarSump, 100);

		chThdSleepMilliseconds(1000);
	}

	return 0;
}

/*
 * Application entry point.
 */
int main(void) {
	/*
	 * System initializations.
	 * - HAL initialization, this also initializes the configured device drivers
	 *   and performs the board-specific initializations.
	 * - Kernel initialization, the main() function becomes a thread and the
	 *   RTOS is active.
	 */
	halInit();
	chSysInit();

	sdStart(&SD1, NULL);
	chprintf((BaseSequentialStream *)&SD1, "\n\nTrunetReef v0.1\n\n");

	RTCTime teste;
	teste.tv_sec = 1404866705;
	teste.tv_msec = 0;
	rtc_lld_set_time(&RTCD1, &teste);

	palClearPad(GPIOB, GPIOB_LED1);
	palClearPad(GPIOB, GPIOB_LED2);

	palSetGroupMode(GPIOA, PAL_PORT_BIT(0) | PAL_PORT_BIT(1),
	                  0, PAL_MODE_INPUT_ANALOG);
	adcStart(&ADCD1, NULL);
	adcConvert(&ADCD1, &adcgrpcfg1, samples1, ADC_GRP1_BUF_DEPTH);
	adcConvert(&ADCD1, &adcgrpcfg2, samples2, ADC_GRP2_BUF_DEPTH);

	chThdCreateStatic(waThreadLed, sizeof(waThreadLed), NORMALPRIO, ThreadLed, NULL);
	chThdCreateStatic(waThreadLCD, sizeof(waThreadLCD), NORMALPRIO, ThreadLCD, NULL);

	while (TRUE) {
		chThdSleepMilliseconds(1000);
	}
}
