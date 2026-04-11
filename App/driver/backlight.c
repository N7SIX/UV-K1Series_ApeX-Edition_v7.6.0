/* Copyright 2025 muzkr https://github.com/muzkr
 * Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "driver/backlight.h"
#include "py32f071_ll_system.h"
#include "py32f071_ll_dma.h"
#include "py32f071_ll_bus.h"
#include "py32f071_ll_tim.h"
#include "driver/gpio.h"
#include "driver/systick.h"
#include "settings.h"
#include "external/printf/printf.h"

#ifdef ENABLE_FEAT_N7SIX
    #include "driver/system.h"
    #include "audio.h"
    #include "misc.h"
#endif

#define PWM_FREQ 240
#define DUTY_CYCLE_LEVELS 64

#define DUTY_CYCLE_ON_VALUE GPIO_PIN_MASK(GPIO_PIN_BACKLIGHT)
#define DUTY_CYCLE_OFF_VALUE (DUTY_CYCLE_ON_VALUE << 16)

#define TIMx TIM7
#define DMA_CHANNEL LL_DMA_CHANNEL_7

static uint32_t dutyCycle[DUTY_CYCLE_LEVELS];

// this is decremented once every 500ms
uint16_t gBacklightCountdown_500ms = 0;
bool backlightOn;

#ifdef ENABLE_FEAT_N7SIX
    const uint8_t value[] = {0, 3, 6, 9, 15, 24, 38, 62, 100, 159, 255};
#endif

#ifdef ENABLE_FEAT_N7SIX_SLEEP
    uint16_t gSleepModeCountdown_500ms = 0;
#endif

void BACKLIGHT_InitHardware()
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_TIM7);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_TIM7);

    // 48 MHz / ((1 + PSC) * (1 + ARR)) == PWM_freq * levels
    LL_TIM_SetPrescaler(TIMx, 0);
    LL_TIM_SetAutoReload(TIMx, SystemCoreClock / PWM_FREQ / DUTY_CYCLE_LEVELS - 1);
    LL_TIM_EnableARRPreload(TIMx);
    LL_TIM_EnableDMAReq_UPDATE(TIMx);
    LL_TIM_EnableUpdateEvent(TIMx);

    LL_DMA_DisableChannel(DMA1, DMA_CHANNEL) ;
    LL_SYSCFG_SetDMARemap(DMA1, DMA_CHANNEL, LL_SYSCFG_DMA_MAP_TIM7_UP);

    LL_DMA_ConfigTransfer(DMA1, DMA_CHANNEL,                //
                          LL_DMA_DIRECTION_MEMORY_TO_PERIPH //
                              | LL_DMA_MODE_CIRCULAR        //
                              | LL_DMA_PERIPH_NOINCREMENT   //
                              | LL_DMA_MEMORY_INCREMENT     //
                              | LL_DMA_PDATAALIGN_WORD      //
                              | LL_DMA_MDATAALIGN_WORD      //
                              | LL_DMA_PRIORITY_HIGH        //
    );

    LL_DMA_SetMemoryAddress(DMA1, DMA_CHANNEL, (uint32_t)dutyCycle);
    LL_DMA_SetPeriphAddress(DMA1, DMA_CHANNEL, (uint32_t)(&GPIO_PORT(GPIO_PIN_BACKLIGHT)->BSRR));
    LL_DMA_SetDataLength(DMA1, DMA_CHANNEL, sizeof(dutyCycle) / sizeof(uint32_t));
}

static void BACKLIGHT_Sound(void)
{
    if (gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_SOUND || gEeprom.POWER_ON_DISPLAY_MODE == POWER_ON_DISPLAY_MODE_ALL)
    {
        AUDIO_PlayBeep(BEEP_880HZ_60MS_DOUBLE_BEEP);
        AUDIO_PlayBeep(BEEP_880HZ_60MS_DOUBLE_BEEP);
    }

    gK5startup = false;
}


void BACKLIGHT_TurnOn(void)
{
    #ifdef ENABLE_FEAT_N7SIX_SLEEP
        gSleepModeCountdown_500ms = gSetting_set_off * 120;
    #endif

    #ifdef ENABLE_FEAT_N7SIX
        gBacklightBrightnessOld = BACKLIGHT_GetBrightness();
    #endif

    if (gEeprom.BACKLIGHT_TIME == 0) {
        BACKLIGHT_TurnOff();
        #ifdef ENABLE_FEAT_N7SIX
            if(gK5startup == true) 
            {
                BACKLIGHT_Sound();
            }
        #endif
        return;
    }

    backlightOn = true;

#ifdef ENABLE_FEAT_N7SIX
    if(gK5startup == true) {
        #if defined(ENABLE_FMRADIO) && defined(ENABLE_SPECTRUM)
            BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MAX);
        #else
            for(uint8_t i = 0; i <= gEeprom.BACKLIGHT_MAX; i++)
            {
                BACKLIGHT_SetBrightness(i);
                SYSTEM_DelayMs(50);
            }
        #endif

        BACKLIGHT_Sound();
    }
    else
    {
        BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MAX);
    }
#else
    BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MAX);
#endif

    switch (gEeprom.BACKLIGHT_TIME) {
        default:
        case 1 ... 60:  // 5 sec * value
            gBacklightCountdown_500ms = 1 + (gEeprom.BACKLIGHT_TIME * 5) * 2;
            break;
        case 61:    // always on
            gBacklightCountdown_500ms = 0;
            break;
    }
}

void BACKLIGHT_TurnOff()
{
#ifdef ENABLE_BLMIN_TMP_OFF
    register uint8_t tmp;

    if (gEeprom.BACKLIGHT_MIN_STAT == BLMIN_STAT_ON)
        tmp = gEeprom.BACKLIGHT_MIN;
    else
        tmp = 0;

    BACKLIGHT_SetBrightness(tmp);
#else
    BACKLIGHT_SetBrightness(gEeprom.BACKLIGHT_MIN);
#endif
    gBacklightCountdown_500ms = 0;
    backlightOn = false;
}

bool BACKLIGHT_IsOn()
{
    return backlightOn;
}

static uint8_t currentBrightness = 0;

void BACKLIGHT_SetBrightness(uint8_t brigtness)
{
    // printf("BL: %d\n", brigtness);

    if (currentBrightness == brigtness)
    {
        return;
    }

    if (0 == brigtness)
    {
        LL_TIM_DisableCounter(TIMx);
        LL_DMA_DisableChannel(DMA1, DMA_CHANNEL);
        SYSTICK_DelayUs(1);
        GPIO_TurnOffBacklight();
    }
    else
    {
        const uint32_t level = (uint32_t)(value[brigtness]) * DUTY_CYCLE_LEVELS / 255;
        if (level >= DUTY_CYCLE_LEVELS)
        {
            LL_TIM_DisableCounter(TIMx);
            LL_DMA_DisableChannel(DMA1, DMA_CHANNEL);
            GPIO_TurnOnBacklight();
        }
        else
        {
            for (uint32_t i = 0; i < DUTY_CYCLE_LEVELS; i++)
            {
                dutyCycle[i] = i < level ? DUTY_CYCLE_ON_VALUE : DUTY_CYCLE_OFF_VALUE;
            }

            if (!LL_TIM_IsEnabledCounter(TIMx))
            {
                LL_DMA_EnableChannel(DMA1, DMA_CHANNEL);
                LL_TIM_EnableCounter(TIMx);
            }
        }
    }

    currentBrightness = brigtness;
}

uint8_t BACKLIGHT_GetBrightness(void)
{
    return currentBrightness;
}
