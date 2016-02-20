/*
**
**                           Main.c
**
**
**********************************************************************/
/*
   Last committed:     $Revision: 00 $
   Last changed by:    $Author: $
   Last changed date:  $Date:  $
   ID:                 $Id:  $

**********************************************************************/
#include "stm32f0xx_conf.h"
#include "settings.h"

#define PROG_MODE   0
#define WORK_MODE   1

#define FILTR_LIM   10

uint16_t CCR1_Val = 1000;
uint16_t TimerPeriod = 0, Pulse = 0;
__IO uint16_t  ADC1ConvertedValue = 0, ADC1ConvertedVoltage = 0;

unsigned char curMode = WORK_MODE;

static __IO uint32_t TimingDelay;

void Delay(__IO uint32_t nTime);
void TimingDelay_Decrement(void);

static void tmrInit(void);
static void adcInit(void);
static void outInit(void);

static void powerOn(void);

int main(void)
{
    __IO uint16_t   tmpTmr = 0;
    __IO uint16_t   filtr[FILTR_LIM];
    __IO uint16_t   filtr_cnt = 0;
    __IO uint32_t   filtr_sum = 0;
    int i = 0;

    tmrInit();
    adcInit();
    outInit();
    initSettings();
    if (SysTick_Config(SystemCoreClock / 1000))
    {
        /* Capture error */
        while (1);
    }

    powerOn();

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_64); // IWDG counter clock: 40KHz(LSI) / 64  (1.6 ms)
    IWDG_SetReload(150); //Set counter reload value
    IWDG_ReloadCounter();
    IWDG_Enable();

    while(1)
    {
        Delay(10);
        IWDG_ReloadCounter();

        if(curMode==WORK_MODE)
        {

            while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
            filtr[filtr_cnt++] = ADC_GetConversionValue(ADC1);
            if(filtr_cnt>=FILTR_LIM) {
                filtr_cnt = 0;
                filtr_sum = 0;
                for(i=0;i<FILTR_LIM;i++) {filtr_sum+=filtr[i];}
                ADC1ConvertedValue = filtr_sum / FILTR_LIM;
            }
            if(ADC1ConvertedValue>=getLimLevel()) GPIO_ResetBits(GPIOA, GPIO_Pin_9);
            else GPIO_SetBits(GPIOA, GPIO_Pin_9);

            if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==Bit_RESET) tmpTmr++;else tmpTmr=0;
            if(tmpTmr>=300) {
                curMode = PROG_MODE;
                tmpTmr = 0;
            }
        }else if(curMode==PROG_MODE)
        {
            tmpTmr++;
            if(tmpTmr>=10) {tmpTmr=0;GPIOA->ODR ^= GPIO_Pin_0;}
            if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==Bit_SET) {
                filtr_cnt = 0;
                while(filtr_cnt<FILTR_LIM) {
                    Delay(10);
                    IWDG_ReloadCounter();
                    filtr[filtr_cnt++] = ADC_GetConversionValue(ADC1);
                }
                filtr_cnt = 0;
                filtr_sum = 0;
                for(i=0;i<FILTR_LIM;i++) {filtr_sum+=filtr[i];}
                setLimLevel((filtr_sum / FILTR_LIM)*0.95);
                GPIO_SetBits(GPIOA, GPIO_Pin_0);
                curMode = WORK_MODE;
                tmpTmr = 0;
            }
        }
    }
}

void powerOn(void)
{
    __IO uint16_t tmpTmr = 0;
    while(1) {
        Delay(10);
        tmpTmr++;
        if(tmpTmr%20==0) {
            GPIOA->ODR ^= GPIO_Pin_0;
        }
        if(tmpTmr>=120) break;
    }
    GPIO_SetBits(GPIOA, GPIO_Pin_0);
}



void Delay(__IO uint32_t nTime)
{
    TimingDelay = nTime;
    while(TimingDelay != 0);
}

void TimingDelay_Decrement(void)
{
    if (TimingDelay != 0x00)
    {
        TimingDelay--;
    }
}

void tmrInit()
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    /* TIM3 clock enable */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* GPIOC clock enable */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

    /* GPIOA Configuration: TIM3 CH1 (PA6) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* Connect TIM Channels to AF2 */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_1);

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);

    /*Compute the prescaler value */
    TimerPeriod = (SystemCoreClock / 1000 ) - 1;
    Pulse = (uint16_t) (((uint32_t) 5 * (TimerPeriod - 1)) / 10);

    /* Time base configuration */
    TIM_TimeBaseStructure.TIM_Period = TimerPeriod;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    /* Output Compare Active Mode configuration: Channel1 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
    TIM_OCInitStructure.TIM_Pulse = Pulse;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

    TIM_OC1Init(TIM3, &TIM_OCInitStructure);

    /* TIM1 counter enable */
    TIM_Cmd(TIM3, ENABLE);

    /* TIM1 Main Output Enable */
    TIM_CtrlPWMOutputs(TIM3, ENABLE);
}

void adcInit()
{
    ADC_InitTypeDef     ADC_InitStructure;
    GPIO_InitTypeDef    GPIO_InitStructure;

    /* GPIOC Periph clock enable */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

    /* ADC1 Periph clock enable */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    /* Configure ADC Channel11 as analog input */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ADCs DeInit */
    ADC_DeInit(ADC1);

    /* Initialize ADC structure */
    ADC_StructInit(&ADC_InitStructure);

    /* Configure the ADC1 in continuous mode with a resolution equal to 12 bits  */
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_ScanDirection = ADC_ScanDirection_Upward;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_ChannelConfig(ADC1, ADC_Channel_1 , ADC_SampleTime_239_5Cycles);

    /* ADC Calibration */
    ADC_GetCalibrationFactor(ADC1);

    /* Enable the ADC peripheral */
    ADC_Cmd(ADC1, ENABLE);

    /* Wait the ADRDY flag */
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_ADRDY));

    /* ADC1 regular Software Start Conv */
    ADC_StartOfConversion(ADC1);
}

void outInit()
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA | RCC_AHBPeriph_GPIOB, ENABLE);
    GPIO_InitTypeDef    GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_SetBits(GPIOA, GPIO_Pin_9);
    GPIO_SetBits(GPIOA, GPIO_Pin_0);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}
