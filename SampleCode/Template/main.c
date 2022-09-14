/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

#include "project_config.h"


/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/
volatile uint32_t BitFlag = 0;
volatile uint32_t counter_tick = 0;
volatile uint32_t counter_systick = 0;

uint32_t u32AVDDVoltage = 0;

#define VBG_VOLTAGE (1200)
#define ADC_SAMPLE_COUNT 128 /* The last line of GetAVDDCodeByADC() need revise when ADC_SAMPLE_COUNT is changed. */

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

void systick_counter(void)
{
	counter_systick++;
}

uint32_t get_systick(void)
{
	return (counter_systick);
}

void set_systick(uint32_t t)
{
	counter_systick = t;
}

void tick_counter(void)
{
	counter_tick++;
}

uint32_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void compare_buffer(uint8_t *src, uint8_t *des, int nBytes)
{
    uint16_t i = 0;	
	
    for (i = 0; i < nBytes; i++)
    {
        if (src[i] != des[i])
        {
            printf("error idx : %4d : 0x%2X , 0x%2X\r\n", i , src[i],des[i]);
			set_flag(flag_error , ENABLE);
        }
    }

	if (!is_flag_set(flag_error))
	{
    	printf("%s finish \r\n" , __FUNCTION__);	
		set_flag(flag_error , DISABLE);
	}

}

void reset_buffer(void *dest, unsigned int val, unsigned int size)
{
    uint8_t *pu8Dest;
//    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;

	#if 1
	while (size-- > 0)
		*pu8Dest++ = val;
	#else
	memset(pu8Dest, val, size * (sizeof(pu8Dest[0]) ));
	#endif
	
}

void copy_buffer(void *dest, void *src, unsigned int size)
{
    uint8_t *pu8Src, *pu8Dest;
    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;
    pu8Src  = (uint8_t *)src;


	#if 0
	  while (size--)
	    *pu8Dest++ = *pu8Src++;
	#else
    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
	#endif
}

void dump_buffer(uint8_t *pucBuff, int nBytes)
{
    uint16_t i = 0;
    
    printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        printf("0x%2X," , pucBuff[i]);
        if ((i+1)%8 ==0)
        {
            printf("\r\n");
        }            
    }
    printf("\r\n\r\n");
}

void dump_buffer_hex(uint8_t *pucBuff, int nBytes)
{
    int     nIdx, i;

    nIdx = 0;
    while (nBytes > 0)
    {
        printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            printf("%02X ", pucBuff[nIdx + i]);
        printf("  ");
        for (i = 0; i < 16; i++)
        {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
                printf("%c", pucBuff[nIdx + i]);
            else
                printf(".");
            nBytes--;
        }
        nIdx += 16;
        printf("\n");
    }
    printf("\n");
}


uint16_t get_ADC_Channel (uint8_t ch)
{
    uint16_t  val = 0;

    /* Clear the A/D interrupt flag for safe */
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);

    /* Start A/D conversion */
    ADC_START_CONV(ADC);

    /* Wait conversion done */
    while(!ADC_GET_INT_FLAG(ADC, ADC_ADF_INT));

    /* Clear the A/D interrupt flag for safe */
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);

    __WFI();
    while(ADC_IS_DATA_VALID(ADC, ch) == 0);     

    val = ADC_GET_CONVERSION_DATA(ADC, ch);

    printf("0X%4X:adc voltage is %dmV if Reference voltage is %4d mv\n", val ,(u32AVDDVoltage*val)/4095 , u32AVDDVoltage);

    return (val);    
}

uint32_t GetAVDDCodeByADC(void)
{
    uint32_t u32Count, u32Sum, u32Data;

    /* Power on ADC */
    ADC_POWER_ON(ADC);

    /* Set input mode as single-end, Single mode, and select channel 29 (band-gap voltage) */
    ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, ADC_ADCR_ADMD_SINGLE, BIT29);

    /* Clear conversion finish flag */
    ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);

    u32Sum = 0;

    /* sample times are according to ADC_SAMPLE_COUNT definition */
    for(u32Count = 0; u32Count < ADC_SAMPLE_COUNT; u32Count++)
    {
        /* Delay for band-gap voltage stability */
        CLK_SysTickDelay(100);

        /* Start A/D conversion */
        ADC_START_CONV(ADC);

        u32Data = 0;

        /* Wait conversion done */
        while(!ADC_GET_INT_FLAG(ADC, ADC_ADF_INT));

        /* Clear the A/D interrupt flag for safe */
        ADC_CLR_INT_FLAG(ADC, ADC_ADF_INT);

        __WFI();
        while(ADC_IS_DATA_VALID(ADC, 29) == 0);     

        /* Get the conversion result */
        u32Data = ADC_GET_CONVERSION_DATA(ADC, 29);
        /* Sum each conversion data */
        u32Sum += u32Data;
    }

    /* Disable ADC */
    ADC_POWER_DOWN(ADC);

    /* Return the average of ADC_SAMPLE_COUNT samples */
    return (u32Sum >> 7);
}

uint32_t GetAVDDVoltage(void)
{
    uint32_t  u32ConversionResult;
    uint64_t u64MvAVDD;

    /* Calculate Vref by using conversion result of VBG */
    u32ConversionResult = GetAVDDCodeByADC();

    /* u32ConversionResult = VBG * 4096 / Vref, Vref = AVDD */
    /* => AVDD = VBG * 4096 / u32ConversionResult */
    u64MvAVDD = (VBG_VOLTAGE << 12) / (uint64_t)u32ConversionResult;

    printf("Conversion result: 0x%X\n", u32ConversionResult);

    return (uint32_t)u64MvAVDD;
}


void ADC_Init(void)
{
    u32AVDDVoltage = GetAVDDVoltage();
    printf("AVDD Voltage: %dmV\r\n", u32AVDDVoltage);

    /* Power on ADC module */
    ADC_POWER_ON(ADC);

    /* Set the ADC operation mode as single, input mode as single-end and enable the analog input channel 3 */
    ADC_Open(ADC, ADC_ADCR_DIFFEN_SINGLE_END, ADC_ADCR_ADMD_SINGLE, BIT2 );

    get_ADC_Channel(2);
}


void GPIO_Init (void)
{
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB14MFP_Msk)) | (SYS_GPB_MFPH_PB14MFP_GPIO);
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB15MFP_Msk)) | (SYS_GPB_MFPH_PB15MFP_GPIO);
	
    GPIO_SetMode(PB, BIT14, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PB, BIT15, GPIO_MODE_OUTPUT);	
}


void TMR1_IRQHandler(void)
{
	static uint32_t LOG = 0;

	
	if(TIMER_GetIntFlag(TIMER1) == 1)
	{
		TIMER_ClearIntFlag(TIMER1);
		tick_counter();

		if ((get_tick() % 1000) == 0)
		{
            set_flag(flag_timer_period_1000ms ,ENABLE);
		}

		if ((get_tick() % 100) == 0)
		{
            set_flag(flag_timer_period_100ms ,ENABLE);
		}

	}
}


void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
    TIMER_Start(TIMER1);
}

void loop(void)
{
	// static uint32_t LOG1 = 0;
	// static uint32_t LOG2 = 0;

    if ((get_systick() % 1000) == 0)
    {
        // printf("%s(systick) : %4d\r\n",__FUNCTION__,LOG2++);    
    }

    if (is_flag_set(flag_timer_period_1000ms))
    {
        set_flag(flag_timer_period_1000ms ,DISABLE);

        // printf("%s(timer) : %4d\r\n",__FUNCTION__,LOG1++);
        PB14 ^= 1;        
    }

    if (is_flag_set(flag_timer_period_100ms))
    {
        set_flag(flag_timer_period_100ms ,DISABLE);
        get_ADC_Channel(2);       
    }
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(UART0);

	if (res > 0x7F)
	{
		printf("invalid command\r\n");
	}
	else
	{
		switch(res)
		{
			case '1':
				break;

			case 'X':
			case 'x':
			case 'Z':
			case 'z':
				NVIC_SystemReset();		
				break;
		}
	}
}

void UART0_IRQHandler(void)
{

    if(UART_GET_INT_FLAG(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))     /* UART receive data available flag */
    {
        while(UART_GET_RX_EMPTY(UART0) == 0)
        {
            UARTx_Process();
        }
    }

    if(UART0->FIFOSTS & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART_ClearIntFlag(UART0, (UART_INTSTS_RLSINT_Msk| UART_INTSTS_BUFERRINT_Msk));
    }	
}

void UART0_Init(void)
{
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    NVIC_EnableIRQ(UART0_IRQn);
	
	#if (_debug_log_UART_ == 1)	//debug
	printf("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetCPUFreq());
	printf("CLK_GetHXTFreq : %8d\r\n",CLK_GetHXTFreq());
	printf("CLK_GetLXTFreq : %8d\r\n",CLK_GetLXTFreq());	
	printf("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetPCLK0Freq());
	printf("CLK_GetPCLK1Freq : %8d\r\n",CLK_GetPCLK1Freq());	
	printf("CLK_GetHCLKFreq : %8d\r\n",CLK_GetHCLKFreq());		
	#endif	

}

void SYS_Init(void)
{
    SYS_UnlockReg();	

    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

    /* Set PF multi-function pins for XT1_OUT(PF.2) and XT1_IN(PF.3) */
//    SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF2MFP_Msk)) | SYS_GPF_MFPL_PF2MFP_XT1_OUT;
//    SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF3MFP_Msk)) | SYS_GPF_MFPL_PF3MFP_XT1_IN;

    /* Enable GPIO Port F module clock */
//    CLK_EnableModuleClock(GPIOF_MODULE);

    /* Set XT1_OUT(PF.2) and XT1_IN(PF.3) as input mode to use HXT */
//    GPIO_SetMode(PF, BIT2|BIT3, GPIO_MODE_INPUT);


//    CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);	

//    CLK_EnableXtalRC(CLK_PWRCTL_LXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);		
	
    /* Set core clock to 72MHz */
    CLK_SetCoreClock(FREQ_72MHZ);

    /* Select HCLK clock source as HIRC and HCLK source divider as 1 */
//    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC_DIV2, CLK_CLKDIV0_UART0(1));

    CLK_EnableModuleClock(TMR1_MODULE);
  	CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HIRC_DIV2, 0);

    /* Enable ADC module clock */
    CLK_EnableModuleClock(ADC_MODULE);

    /* Select ADC module clock source as HIRC/2 and ADC module clock divider as 4 */
    CLK_SetModuleClock(ADC_MODULE, CLK_CLKSEL1_ADCSEL_HIRC_DIV2, CLK_CLKDIV0_ADC(4));

    /* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk)) |
                    (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

//    SYS->GPB_MFPH = (SYS->GPB_MFPH & (~(UART0_RXD_PB12_Msk | UART0_TXD_PB13_Msk))) | (UART0_RXD_PB12 | UART0_TXD_PB13);

    /* Disable digital input path of ADC analog pin to prevent leakage */
    GPIO_DISABLE_DIGITAL_PATH(PB, BIT2);

    /* Set multi-function pins for ADC channels */
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB2MFP_Msk );
    SYS->GPB_MFPL |=  SYS_GPB_MFPL_PB2MFP_ADC0_CH2;


   /* Update System Core Clock */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}

int main()
{
    SYS_Init();

	GPIO_Init();
	UART0_Init();
	TIMER1_Init();

    ADC_Init();
    
    /* Got no where to go, just loop forever */
    while(1)
    {
        loop();

    }
}

/*** (C) COPYRIGHT 2021 Nuvoton Technology Corp. ***/
