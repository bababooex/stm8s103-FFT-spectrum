//******************************************************************************
//  Simple FFT spectrum on STM8S103 + SSD1306 128x64 I2C
//******************************************************************************

#include "fft__.h"
#include "SSD1306.h"
#include "main.h"

//------------------------------------------------------------------------------
//  64-pixel vertical line (8 pages)
//------------------------------------------------------------------------------
typedef union
{
    struct
    {
        u32 low;   // pages 0..3
        u32 high;  // pages 4..7
    };
    u8 arr[8];
} line64_t;

//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------
__IO u32 count;            // delay counter
__IO u8  isr_flag = 0;     // sampling flag

line64_t my_line;

int16_t   capture   [N_SAMPLE];   // FFT magnitude output
complex_t bfly_buff [N_SAMPLE];   // FFT working buffer

//------------------------------------------------------------------------------
// Prototypes
//------------------------------------------------------------------------------
void capture_wave(uint16_t cnt);
void convert(uint8_t value);
void delay(u32 nTime);

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
#define COL_REPEAT   (256 / N_SAMPLE)
#define COL_SKIP     (COL_REPEAT / 3)
#define FFT_SCALE_SHIFT 1

#define WAIT_TX() while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED))

//------------------------------------------------------------------------------
// MAIN
//------------------------------------------------------------------------------
void main(void)
{
    uint16_t n;
    uint8_t  mm;

    Init_CLK();
    Init_GPIO();
    Init_I2C();
    Init_ADC(ADC1_CHANNEL_6);
    TIM4_Config();
    TIM1_Config();
    Init_ITC();

    delay(500);
    LCD_Init();
    while (1)
    {

        capture_wave(N_SAMPLE);
        fix_fft(capture, bfly_buff);
        fft_out(bfly_buff, capture);


        disableInterrupts();

        LCD_command(0x21);     // COLUMNADDR
        LCD_command(0);
        LCD_command(127);

        LCD_command(0x22);     // PAGEADDR
        LCD_command(0);
        LCD_command(7);

        I2C_GenerateSTART(ENABLE);
        while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

        I2C_Send7bitAddress(SLAVE_ADDRESS, I2C_DIRECTION_TX);
        while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

        I2C_SendData(DATA_MODE);
        WAIT_TX();

        enableInterrupts();

        // Draw spectrum
        for (n = 0; n < (N_SAMPLE / 2); n++)
        {
            // Scale FFT output
            uint8_t level = (uint8_t)(capture[n] >> FFT_SCALE_SHIFT);
            if (level > 64) level = 64;

            convert(level);

            for (mm = 0; mm < COL_REPEAT; mm++)
            {
                if (mm > COL_SKIP)
                {
                    my_line.low  = 0;
                    my_line.high = 0;
                }
                else
                {
                  if((n%1)==0)my_line.high |= 0x01;
                  if((n%5)==0)my_line.high |= 0x05;
                  if((n%10)==0)my_line.high |= 0x15;
                }
                // Unrolled 8-byte write
                I2C->DR = my_line.arr[7]; WAIT_TX();
                I2C->DR = my_line.arr[6]; WAIT_TX();
                I2C->DR = my_line.arr[5]; WAIT_TX();
                I2C->DR = my_line.arr[4]; WAIT_TX();
                I2C->DR = my_line.arr[3]; WAIT_TX();
                I2C->DR = my_line.arr[2]; WAIT_TX();
                I2C->DR = my_line.arr[1]; WAIT_TX();
                I2C->DR = my_line.arr[0]; WAIT_TX();
            }
        }

        I2C_GenerateSTOP(ENABLE);
    }
}

//------------------------------------------------------------------------------
// Convert FFT magnitude to 64-pixel vertical bar
//------------------------------------------------------------------------------
void convert(uint8_t value)
{
    u8 i;

    my_line.low  = 0;
    my_line.high = 0;

    for (i = 0; i < value; i++)
    {
        if (i < 32)
            my_line.low  |= (1UL << (31 - i));
        else
            my_line.high |= (1UL << (63 - i));
    }
}

//------------------------------------------------------------------------------
// ADC capture routine (timer driven)
//------------------------------------------------------------------------------
void capture_wave(uint16_t cnt)
{
    u8  i   = 0;
    u16 adc = 0;

    isr_flag = RESET;

    do
    {
        if (isr_flag)
        {
            isr_flag = RESET;

            ADC1->CR1 |= ADC1_CR1_ADON;
            while (ADC1_GetFlagStatus(ADC1_FLAG_EOC) == RESET);

            adc = ADC1_GetConversionValue() - 511;

            bfly_buff[position[i]].real = FIX(adc, ham[i]);
            bfly_buff[i].image = 0;

            i++;
            cnt--;
        }
    }
    while (cnt);
}

void delay(u32 nTime)
{
    count = nTime;
    while (count)
    {
        continue;
    }
}

