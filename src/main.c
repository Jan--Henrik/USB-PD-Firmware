/*
 * PD Buddy Sink Firmware - Smart power jack for USB Power Delivery
 * Copyright (C) 2017-2018 Clayton G. Hobbs <clay@lakeserv.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
   ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <ch.h>
#include <hal.h>

#include "shell.h"
#include "usbcfg.h"

#include <pdb.h>
#include <pd.h>
#include "led.h"
#include "device_policy_manager.h"
#include "stm32f072_bootloader.h"
#include "ssd1306.h"
#include "chprintf.h"
#include <string.h>
#include <stdio.h>

// ADCConfig structure for stm32 MCUs is empty
/*
static const ADCConversionGroup adcgrpcfg2 = {
  TRUE,
  ADC_GRP2_NUM_CHANNELS,
  adccallback,
  adcerrorcallback,
  ADC_CFGR1_CONT | ADC_CFGR1_RES_12BIT,             /* CFGR1 */
//  ADC_TR(0, 0),                                     /* TR */
//  ADC_SMPR_SMP_28P5,                                /* SMPR */
//  ADC_CHSELR_CHSEL10 | ADC_CHSELR_CHSEL11 |
//  ADC_CHSELR_CHSEL16 | ADC_CHSELR_CHSEL17           /* CHSELR */
//};

/*
 * I2C configuration object.
 * I2C2_TIMINGR: 1000 kHz with I2CCLK = 48 MHz, rise time = 100 ns,
 *               fall time = 10 ns (0x00700818)
 */
static const I2CConfig i2c2config = {
    0x00700818,
    0,
    0
};

/*
 * PD Buddy Sink DPM data
 */
static struct pdbs_dpm_data dpm_data = {
    NULL,
    fusb_tcc_none,
    true,
    true,
    false,
    ._present_voltage = 5000
};

/*
 * PD Buddy firmware library configuration object
 */
static struct pdb_config pdb_config = {
    .fusb = {
        &I2CD2,
        FUSB302B_ADDR,
        LINE_INT_N
    },
    .dpm = {
        pdbs_dpm_evaluate_capability,
        pdbs_dpm_get_sink_capability,
        pdbs_dpm_giveback_enabled,
        pdbs_dpm_evaluate_typec_current,
        pdbs_dpm_pd_start,
        pdbs_dpm_transition_default,
        pdbs_dpm_transition_min,
        pdbs_dpm_transition_standby,
        pdbs_dpm_transition_requested,
        pdbs_dpm_transition_typec,
        NULL /* not_supported_received */
    },
    .dpm_data = &dpm_data,
    .state = 0
};
/*
 * Enter setup mode
 */
static void setup(void)
{
    /* Configure the DPM to play nice with the shell */
    dpm_data.output_enabled = false;
    dpm_data.led_pd_status = false;
    dpm_data.usb_comms = true;

    /* Start the USB Power Delivery threads */
    pdb_init(&pdb_config);

    /* Indicate that we're in setup mode */
    chEvtSignal(pdbs_led_thread, PDBS_EVT_LED_CONFIG);

    /* Disconnect from USB */
    usbDisconnectBus(serusbcfg.usbp);

    /* Start the USB serial interface */
    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    /* Start USB */
    chThdSleepMilliseconds(100);
    usbStart(serusbcfg.usbp, &usbcfg);
    usbConnectBus(serusbcfg.usbp);

    /* Start the shell */
    pdbs_shell(&pdb_config);
}

/*
 * Negotiate with the power supply for the configured power
 */
static void sink(void)
{
    /* Start the USB Power Delivery threads */
    pdb_init(&pdb_config);
    chThdSleepMilliseconds(100);
    //palSetLine(LINE_FET);
    pdb_config.state = 0;
    chThdSleepMilliseconds(10);
    chEvtSignal(pdb_config.pe.thread, PDB_EVT_PE_NEW_POWER);
    uint8_t _cnt = 1;
    /* Wait, letting all the other threads do their work. */
    while (true) {
        //palSetLine(LINE_LED);
        chThdSleepMilliseconds(10);

        if (palReadLine(LINE_BUTTON) == PAL_HIGH) {
            palClearLine(LINE_LED);
            pdb_config.state = ++_cnt;
            if (_cnt > 3) _cnt = 0;
            while (palReadLine(LINE_BUTTON) == PAL_HIGH) chThdSleepMilliseconds(10);
            chEvtSignal(pdb_config.pe.thread, PDB_EVT_PE_NEW_POWER);
        }

    }
}

/*
static SSD1306Driver SSD1306D1;

static THD_WORKING_AREA(waOledDisplay, 512);

static __attribute__((noreturn)) THD_FUNCTION(OledDisplay, arg) {
    (void)arg;

    chRegSetThreadName("OledDisplay");

    ssd1306ObjectInit(&SSD1306D1);

    ssd1306Start(&SSD1306D1, &ssd1306cfg);

    ssd1306FillScreen(&SSD1306D1, 0x00);

    char otter[10];
    uint16_t pd_profiles[] = {5, 9, 15, 20};
    uint16_t i = 0;

    while (TRUE) {
        adcStartConversion(&ADCD1, &adccg, &samples_buf[0], ADC_BUF_DEPTH);

        i = samples_buf[0];
        chsnprintf(otter, sizeof(otter), "%d", i);
        ssd1306GotoXy(&SSD1306D1, 5, 0);
        ssd1306Puts(&SSD1306D1, otter, &ssd1306_font_7x10, SSD1306_COLOR_WHITE);

        //i = samples_buf[1];
        //chsnprintf(otter, sizeof(otter), "%d", i);
        //ssd1306GotoXy(&SSD1306D1, 5, 15);
        //ssd1306Puts(&SSD1306D1, otter, &ssd1306_font_7x10, SSD1306_COLOR_WHITE);

        ssd1306UpdateScreen(&SSD1306D1);
        chThdSleepMilliseconds(300);
    }
}
*/
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
    //i2cInit();

    /* Create the LED thread. */
    pdbs_led_run();

    /* Start I2C2 to make communication with the PHY possible */

    i2cStart(pdb_config.fusb.i2cp, &i2c2config);
    //i2cStart(&I2CD1, &i2c1config);

    //palSetPadMode(IOPORT2, 6, PAL_STM32_OTYPE_OPENDRAIN | PAL_MODE_ALTERNATE(1));
    //palSetPadMode(IOPORT2, 7, PAL_STM32_OTYPE_OPENDRAIN | PAL_MODE_ALTERNATE(1));

    //palSetPadMode(IOPORT1, 1, PAL_MODE_INPUT_ANALOG);
    //palSetPadMode(IOPORT1, 2, PAL_MODE_INPUT_ANALOG);
    //adcSTM32SetCCR(ADC_CCR_VBATEN | ADC_CCR_TSEN | ADC_CCR_VREFEN);
    //adcSTM32SetCCR(ADC_CCR_VREFEN);


    //adcInit();
    //adcStart(&ADCD1, &adccfg);
    //adcSTM32EnableVREF();
    //setup();
    sink();
    /*
    if (palReadLine(LINE_BUTTON) == PAL_HIGH) {
        systime_t now = chVTGetSystemTime();
        while (palReadLine(LINE_BUTTON) == PAL_HIGH) {
            if (chVTGetSystemTime() - now >= 3000) dfu_run_bootloader();
        }
        setup();
    } else {
        //chThdCreateStatic(waOledDisplay, sizeof(waOledDisplay), NORMALPRIO, OledDisplay, NULL);
        sink();
    }
    */
}
