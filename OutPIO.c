#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/dma.h"

#include "colour.pio.h"
#include "sync.pio.h"

#define DAC 0

#define V_Sync 17
#define H_Sync 16
#define interrupt 18


#define sm1 0
#define sm2 1
#define sm3 2

// PIO declarations
PIO pio_1 = pio0;
PIO pio_2 = pio1;

// DMA
int dma_chan;
int dma_chan2;

// RGB
#define TX_COUNT 307200

void init_PIO(void);





int main()
{   
    //vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(200000, true);
    stdio_init_all();
    
    init_PIO(); // initialize the PIO channels

    uint16_t info[3] = 
    {    // Red              // Green           // Blue
      0b0001001001001001, 0b011001001001010, 0b1000100100100100
    };
    
    uint16_t * address_pointer = &info[0];      // address of the array 
    
    // claim DMA channels
    dma_chan = dma_claim_unused_channel(true);  // DMA that sends data
    dma_chan2 = dma_claim_unused_channel(true); // DMA that re-fires the other DMA responsible for pushing data
    
    // configure the DMA channel responsible for pushing RGB data to SM
    dma_channel_config c_dma = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c_dma, DMA_SIZE_16);
    channel_config_set_read_increment(&c_dma, true);   // set read increment to false
    channel_config_set_write_increment(&c_dma, false); // set write increment to false
    channel_config_set_chain_to(&c_dma, dma_chan2);    // chain to the DMA which re-enables this one
    channel_config_set_dreq(&c_dma, DREQ_PIO0_TX0);    // set the transfer request line
    
    dma_channel_configure(
        dma_chan,           // channel to be configured
        &c_dma,             // the configuration we just created
        &pio_1->txf[sm1],   // address of RGB SM
        &info,              // address of the array (not NULL since CPU is j*bless this time)
        3,                  // the transfer count (set it to the size of the array)
        false               // don't start yet - we don't ball :(
    );

    // reconfigures the first channel responsible for data transfer
    dma_channel_config c_dma2 = dma_channel_get_default_config(dma_chan2);  // default configs
    channel_config_set_transfer_data_size(&c_dma2, DMA_SIZE_32);            // 32-bit txfers
    channel_config_set_read_increment(&c_dma2, false);                      // no read incrementing
    channel_config_set_write_increment(&c_dma2, false);                     // no write incrementing
    channel_config_set_chain_to(&c_dma2, dma_chan);                         // chain to other channel

    dma_channel_configure(
        dma_chan2,                          // channel to be configured
        &c_dma2,                            // the configuration we just created
        &dma_hw->ch[dma_chan].read_addr,    // write address (channel 0 read address)
        &address_pointer,                   // read address (POINTER TO AN ADDRESS)
        1,                                  // number of transfers, in this case each is 16 byte
        false                               // don't start yet... called my happentologist, call went straight to voicemail 😂
    );

    dma_channel_start(dma_chan); // start the DMA channel responsible for data transfer

    
    while (true) 
    {  
    }
}


void init_PIO(void)
{
    // PIO 1
    
    // sm1
    uint offset = pio_add_program(pio_1, &rgb1_program); 
    pio_sm_config c = rgb1_program_get_default_config(offset);
    rgb1_program_init(pio_1, sm1, offset, DAC); 
    
    // PIO 2
    
    // sm1
    uint pio_2_offset = pio_add_program(pio_2, &Hsync_program); 
    pio_sm_config pio_2_c = Hsync_program_get_default_config(pio_2_offset);
    Hsync_program_init(pio_2, sm1, pio_2_offset, H_Sync);
    
    // sm2
    uint pio_2_offset2 = pio_add_program(pio_2, &Vsync_program);
    pio_sm_config pio_2_c2 = Vsync_program_get_default_config(pio_2_offset2);
    Vsync_program_init(pio_2, sm2, pio_2_offset2, V_Sync);
    
    //sm3
    uint pio_2_offset3 = pio_add_program(pio_2, &Synchronize_program);
    pio_sm_config pio_2_c3 = Synchronize_program_get_default_config(pio_2_offset3);
    Synchronize_program_init(pio_2, sm3, pio_2_offset3, interrupt);

    // Enable the state machines simultaneously
    pio_enable_sm_mask_in_sync(pio_2, ((1u << sm1) | (1u << sm2) | (1u << sm3))); // PIO 2, responsible for HSync, VSync and sending interrupts (toggling a pin) to RGB
}
