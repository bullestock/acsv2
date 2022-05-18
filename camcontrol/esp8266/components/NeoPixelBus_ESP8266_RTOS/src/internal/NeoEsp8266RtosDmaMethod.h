/*-------------------------------------------------------------------------
NeoPixel library helper functions for Esp8266.

RTOS port written by THK
Based on https://github.com/espressif/ESP8266_MP3_DECODER/blob/master/mp3/driver/i2s_freertos.c
-------------------------------------------------------------------------

-------------------------------------------------------------------------
This file is part of the Makuna/NeoPixelBus library.

NeoPixelBus is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

NeoPixelBus is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with NeoPixel.  If not, see
<http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------*/

#pragma once

#if defined(CONFIG_TARGET_PLATFORM_ESP8266) || defined(CONFIG_IDF_TARGET_ESP8266)

#include <esp_attr.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "i2s_reg.h"
#include "slc_register.h"

extern "C" void rom_i2c_writeReg_Mask(uint32_t block, uint32_t host_id, uint32_t reg_add, uint32_t Msb, uint32_t Lsb, uint32_t indata);

//We need some defines that aren't in some RTOS SDK versions. Define them here if we can't find them.
#ifndef i2c_bbpll
    #define i2c_bbpll                               0x67
    #define i2c_bbpll_en_audio_clock_out            4
    #define i2c_bbpll_en_audio_clock_out_msb        7
    #define i2c_bbpll_en_audio_clock_out_lsb        7
    #define i2c_bbpll_hostid                        4
    
    #define i2c_writeReg_Mask(block, host_id, reg_add, Msb, Lsb, indata)  rom_i2c_writeReg_Mask(block, host_id, reg_add, Msb, Lsb, indata)
    #define i2c_readReg_Mask(block, host_id, reg_add, Msb, Lsb)  rom_i2c_readReg_Mask(block, host_id, reg_add, Msb, Lsb)
    #define i2c_writeReg_Mask_def(block, reg_add, indata)  i2c_writeReg_Mask(block, block##_hostid,  reg_add,  reg_add##_msb,  reg_add##_lsb,  indata)
    #define i2c_readReg_Mask_def(block, reg_add)  i2c_readReg_Mask(block, block##_hostid,  reg_add,  reg_add##_msb,  reg_add##_lsb)
#endif // !i2c_bbpll

#ifndef ETS_SLC_INUM
    #define ETS_SLC_INUM       1
#endif // !ETS_SLC_INUM

typedef void (*int_handler_t)(void*);
#define ETS_SLC_INTR_ATTACH(func, arg) \
    _xt_isr_attach(ETS_SLC_INUM, (int_handler_t)(func), (void *)(arg))

#define ETS_SLC_INTR_ENABLE() \
    _xt_isr_unmask(1 << ETS_SLC_INUM)

#define ETS_SLC_INTR_DISABLE() \
    _xt_isr_mask(1 << ETS_SLC_INUM)
//==============================================================================================

struct slc_queue_item
{
    uint32_t  blocksize : 12;
    uint32_t  datalen : 12;
    uint32_t  unused : 5;
    uint32_t  sub_sof : 1;
    uint32_t  eof : 1;
    uint32_t  owner : 1;
    uint32_t  buf_ptr;
    uint32_t  next_link_ptr;
};

class NeoEsp8266DmaSpeed800KbpsBase
{
public:
    const static uint32_t I2sClockDivisor = 3;
    const static uint32_t I2sBaseClockDivisor = 16;
};

class NeoEsp8266DmaSpeedWs2812x : public NeoEsp8266DmaSpeed800KbpsBase
{
public:
    const static uint32_t ResetTimeUs = 300;
};

class NeoEsp8266DmaSpeedSk6812 : public NeoEsp8266DmaSpeed800KbpsBase
{
public:
    const static uint32_t ResetTimeUs = 80;
};

class NeoEsp8266DmaSpeed800Kbps : public NeoEsp8266DmaSpeed800KbpsBase
{
public:
    const static uint32_t ResetTimeUs = 50;
};

class NeoEsp8266DmaSpeed400Kbps
{
public:
    const static uint32_t I2sClockDivisor = 6; 
    const static uint32_t I2sBaseClockDivisor = 16;
    const static uint32_t ResetTimeUs = 50;
};

enum NeoDmaState
{
    NeoDmaState_Idle,
    NeoDmaState_Pending,
    NeoDmaState_Sending,
};
const uint16_t c_maxDmaBlockSize = 4095;
const uint16_t c_dmaBytesPerPixelBytes = 4;
const uint8_t c_I2sPin = 3; // due to I2S hardware, the pin used is restricted to this

template<typename T_SPEED> class NeoEsp8266DmaMethodBase
{
public:
    NeoEsp8266DmaMethodBase(uint16_t pixelCount, size_t elementSize) 
    {
        interruptSignal = xSemaphoreCreateBinary();
        
        uint16_t dmaPixelSize = c_dmaBytesPerPixelBytes * elementSize;

        _pixelsSize = pixelCount * elementSize;
        _i2sBufferSize = pixelCount * dmaPixelSize;

        _pixels = (uint8_t*)malloc(_pixelsSize);
        memset(_pixels, 0x00, _pixelsSize);

        _i2sBuffer = (uint8_t*)heap_caps_malloc(_i2sBufferSize, MALLOC_CAP_DMA);
        memset(_i2sBuffer, 0x00, _i2sBufferSize);

        memset(_i2sZeroes, 0x00, sizeof(_i2sZeroes));

        _is2BufMaxBlockSize = (c_maxDmaBlockSize / dmaPixelSize) * dmaPixelSize;

        _i2sBufDescCount = (_i2sBufferSize / _is2BufMaxBlockSize) + 1 + 2; // need two more for state/latch blocks
        _i2sBufDesc = (slc_queue_item*)heap_caps_malloc(_i2sBufDescCount * sizeof(slc_queue_item), MALLOC_CAP_DMA);
    }

    NeoEsp8266DmaMethodBase(uint8_t pin, uint16_t pixelCount, size_t elementSize) : NeoEsp8266DmaMethodBase(pixelCount, elementSize)
    {
    }

    ~NeoEsp8266DmaMethodBase()
    {
        StopDma();

        free(_pixels);
        free(_i2sBuffer);
        free(_i2sBufDesc);
        
        vSemaphoreDelete(interruptSignal);
    }

    bool IsReadyToUpdate() const
    {
        return (_dmaState == NeoDmaState_Idle);
    }

    void Initialize()
    {
        StopDma();
        _dmaState = NeoDmaState_Sending; // start off sending empty buffer

        uint8_t* is2Buffer = _i2sBuffer;
        uint32_t is2BufferSize = _i2sBufferSize;
        uint16_t indexDesc;

        // prepare main data block decriptors that point into our one static dma buffer
        for (indexDesc = 0; indexDesc < (_i2sBufDescCount - 2); indexDesc++)
        {
            uint32_t blockSize = (is2BufferSize > _is2BufMaxBlockSize) ? _is2BufMaxBlockSize : is2BufferSize;

            _i2sBufDesc[indexDesc].owner = 1;
            _i2sBufDesc[indexDesc].eof = 0; // no need to trigger interrupt generally
            _i2sBufDesc[indexDesc].sub_sof = 0;
            _i2sBufDesc[indexDesc].datalen = blockSize;
            _i2sBufDesc[indexDesc].blocksize = blockSize;
            _i2sBufDesc[indexDesc].buf_ptr = (uint32_t)is2Buffer;
            _i2sBufDesc[indexDesc].unused = 0;
            _i2sBufDesc[indexDesc].next_link_ptr = (uint32_t)&(_i2sBufDesc[indexDesc + 1]);

            is2Buffer += blockSize;
            is2BufferSize -= blockSize;
        }

        // prepare the two state/latch descriptors
        for (; indexDesc < _i2sBufDescCount; indexDesc++)
        {
            _i2sBufDesc[indexDesc].owner = 1;
            _i2sBufDesc[indexDesc].eof = 0; // no need to trigger interrupt generally
            _i2sBufDesc[indexDesc].sub_sof = 0;
            _i2sBufDesc[indexDesc].datalen = sizeof(_i2sZeroes);
            _i2sBufDesc[indexDesc].blocksize = sizeof(_i2sZeroes);
            _i2sBufDesc[indexDesc].buf_ptr = (uint32_t)_i2sZeroes;
            _i2sBufDesc[indexDesc].unused = 0;
            _i2sBufDesc[indexDesc].next_link_ptr = (uint32_t)&(_i2sBufDesc[indexDesc + 1]);
        }

        // the first state block will trigger the interrupt
        _i2sBufDesc[indexDesc - 2].eof = 1;
        // the last state block will loop to the first state block by defualt
        _i2sBufDesc[indexDesc - 1].next_link_ptr = (uint32_t)&(_i2sBufDesc[indexDesc - 2]);

        // setup the rest of i2s DMA
        //
        // disable interrupt
        ETS_SLC_INTR_DISABLE();
        //Reset DMA
        SET_PERI_REG_MASK(SLC_CONF0, SLC_RXLINK_RST|SLC_TXLINK_RST);
        CLEAR_PERI_REG_MASK(SLC_CONF0, SLC_RXLINK_RST|SLC_TXLINK_RST);
    
        //Clear DMA int flags
        SET_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);
        CLEAR_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);
        
        //Enable and configure DMA
        CLEAR_PERI_REG_MASK(SLC_CONF0, (SLC_MODE<<SLC_MODE_S));
        SET_PERI_REG_MASK(SLC_CONF0,(1<<SLC_MODE_S));
        SET_PERI_REG_MASK(SLC_RX_DSCR_CONF,SLC_INFOR_NO_REPLACE|SLC_TOKEN_NO_REPLACE);
        CLEAR_PERI_REG_MASK(SLC_RX_DSCR_CONF, SLC_RX_FILL_EN|SLC_RX_EOF_MODE | SLC_RX_FILL_MODE);        
        
        //Feed dma the 1st buffer desc addr
        //To send data to the I2S subsystem, counter-intuitively we use the RXLINK part, not the TXLINK as you might
        //expect. The TXLINK part still needs a valid DMA descriptor, even if it's unused: the DMA engine will throw
        //an error at us otherwise. Just feed it any random descriptor.
        CLEAR_PERI_REG_MASK(SLC_TX_LINK,SLC_TXLINK_DESCADDR_MASK);
        SET_PERI_REG_MASK(SLC_TX_LINK, ((uint32_t)&(_i2sBufDesc[_i2sBufDescCount-1])) & SLC_TXLINK_DESCADDR_MASK); //any random desc is OK, we don't use TX but it needs something valid
        CLEAR_PERI_REG_MASK(SLC_RX_LINK,SLC_RXLINK_DESCADDR_MASK);
        SET_PERI_REG_MASK(SLC_RX_LINK, ((uint32_t)&_i2sBufDesc[0]) & SLC_RXLINK_DESCADDR_MASK);
        
        //Attach the DMA interrupt
        ETS_SLC_INTR_ATTACH(i2s_slc_isr, this);
        //Enable DMA operation intr
        WRITE_PERI_REG(SLC_INT_ENA,  SLC_RX_EOF_INT_ENA);
        //clear any interrupt flags that are set
        WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);
        //enable DMA intr in cpu
        ETS_SLC_INTR_ENABLE();
        
        //Start transmission
        SET_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
        SET_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);

        //Init pins to i2s functions
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_I2SO_DATA);
        
        //Enable clock to i2s subsystem
        i2c_writeReg_Mask_def(i2c_bbpll, i2c_bbpll_en_audio_clock_out, 1);
        
        //Reset I2S subsystem
        CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
        SET_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
        CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);        
        
        //Select 16bits per channel (FIFO_MOD=0), no DMA access (FIFO only)
        CLEAR_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN|(I2S_I2S_RX_FIFO_MOD<<I2S_I2S_RX_FIFO_MOD_S)|(I2S_I2S_TX_FIFO_MOD<<I2S_I2S_TX_FIFO_MOD_S));
        //Enable DMA in i2s subsystem
        SET_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN);        
        //tx/rx binaureal
        CLEAR_PERI_REG_MASK(I2SCONF_CHAN, (I2S_TX_CHAN_MOD<<I2S_TX_CHAN_MOD_S)|(I2S_RX_CHAN_MOD<<I2S_RX_CHAN_MOD_S));        

        // set the rate
        const uint32_t i2s_clock_div = T_SPEED::I2sClockDivisor & I2S_CLKM_DIV_NUM;
        const uint8_t i2s_bck_div = T_SPEED::I2sBaseClockDivisor & I2S_BCK_DIV_NUM;
        
        CLEAR_PERI_REG_MASK(I2SCONF, I2S_TRANS_SLAVE_MOD|(I2S_BITS_MOD<<I2S_BITS_MOD_S)|(I2S_BCK_DIV_NUM <<I2S_BCK_DIV_NUM_S)|(I2S_CLKM_DIV_NUM<<I2S_CLKM_DIV_NUM_S));
        SET_PERI_REG_MASK(I2SCONF, I2S_RIGHT_FIRST|I2S_MSB_RIGHT|I2S_RECE_SLAVE_MOD|I2S_RECE_MSB_SHIFT|I2S_TRANS_MSB_SHIFT|
                                   (i2s_bck_div<<I2S_BCK_DIV_NUM_S)|
                                   (i2s_clock_div<<I2S_CLKM_DIV_NUM_S));
        
        // clear interrupt
        SET_PERI_REG_MASK(I2SINT_CLR, I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
                                      I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);
        CLEAR_PERI_REG_MASK(I2SINT_CLR, I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
                                        I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);
        //enable int
        SET_PERI_REG_MASK(I2SINT_ENA, I2S_I2S_TX_REMPTY_INT_ENA|I2S_I2S_TX_WFULL_INT_ENA|
                          I2S_I2S_RX_REMPTY_INT_ENA|I2S_I2S_TX_PUT_DATA_INT_ENA|I2S_I2S_RX_TAKE_DATA_INT_ENA);
        //Start transmission
        SET_PERI_REG_MASK(I2SCONF,I2S_I2S_TX_START);        
    }

    void IRAM_ATTR Update()
    {
        // wait for not actively sending data
        if (xSemaphoreTake(interruptSignal, (10 * 1000) / portTICK_PERIOD_MS) != pdTRUE) {
            return;
        }
        FillBuffers();

        // toggle state so the ISR reacts
        _dmaState = NeoDmaState_Pending;
    }

    uint8_t* getPixels() const
    {
        return _pixels;
    }

    size_t getPixelsSize() const
    {
        return _pixelsSize;
    }

private:
    size_t    _pixelsSize;    // Size of '_pixels' buffer 
    uint8_t*  _pixels;        // Holds LED color values

    uint32_t _i2sBufferSize; // total size of _i2sBuffer
    uint8_t* _i2sBuffer;  // holds the DMA buffer that is referenced by _i2sBufDesc

    // normally 24 bytes creates the minimum 50us latch per spec, but
    // with the new logic, this latch is used to space between three states
    // buffer size = (24 * (speed / 50)) / 3
    uint8_t _i2sZeroes[(24L * (T_SPEED::ResetTimeUs / 50L)) / 3L];

    slc_queue_item* _i2sBufDesc;  // dma block descriptors
    uint16_t _i2sBufDescCount;   // count of block descriptors in _i2sBufDesc
    uint16_t _is2BufMaxBlockSize; // max size based on size of a pixel of a single block
    
    volatile SemaphoreHandle_t  interruptSignal;

    volatile NeoDmaState _dmaState;

    // This routine is called as soon as the DMA routine has something to tell us. All we
    // handle here is the RX_EOF_INT status, which indicate the DMA has sent a buffer whose
    // descriptor has the 'EOF' field set to 1.
    // in the case of this code, the second to last state descriptor
    static void IRAM_ATTR i2s_slc_isr(void* arg)
    {
        uint32_t slc_intr_status = READ_PERI_REG(SLC_INT_STATUS);
        //clear all intr flags
        WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);
        
        if (slc_intr_status & SLC_RX_EOF_INT_ST) {
            ETS_SLC_INTR_DISABLE();
            
            NeoEsp8266DmaMethodBase* s_this = static_cast<NeoEsp8266DmaMethodBase*>(arg);
            
            switch (s_this->_dmaState) {
                case NeoDmaState_Idle:
                    break;
                case NeoDmaState_Pending: {
                        slc_queue_item* finished_item = (slc_queue_item*)READ_PERI_REG(SLC_RX_EOF_DES_ADDR);
                        // data block has pending data waiting to send, prepare it
                        // point last state block to top 
                        (finished_item + 1)->next_link_ptr = (uint32_t)(s_this->_i2sBufDesc);
    
                        s_this->_dmaState = NeoDmaState_Sending;
                    }
                    break;
                case NeoDmaState_Sending: {
                        slc_queue_item* finished_item = (slc_queue_item*)READ_PERI_REG(SLC_RX_EOF_DES_ADDR);
                        // the data block had actual data sent
                        // point last state block to first state block thus
                        // just looping and not sending the data blocks
                        (finished_item + 1)->next_link_ptr = (uint32_t)(finished_item);
    
                        s_this->_dmaState = NeoDmaState_Idle;
                        BaseType_t hasTaskWoken;
                        xSemaphoreGiveFromISR(s_this->interruptSignal, &hasTaskWoken);
                        if (hasTaskWoken == pdTRUE)
                            portYIELD();
                    }
                    break;
            }
            ETS_SLC_INTR_ENABLE();
        }
    }

    void FillBuffers()
    {
        const uint16_t bitpatterns[16] =
        {
            0b1000100010001000, 0b1000100010001110, 0b1000100011101000, 0b1000100011101110,
            0b1000111010001000, 0b1000111010001110, 0b1000111011101000, 0b1000111011101110,
            0b1110100010001000, 0b1110100010001110, 0b1110100011101000, 0b1110100011101110,
            0b1110111010001000, 0b1110111010001110, 0b1110111011101000, 0b1110111011101110,
        };

        uint16_t* pDma = (uint16_t*)_i2sBuffer;
        uint8_t* pPixelsEnd = _pixels + _pixelsSize;
        for (uint8_t* pPixel = _pixels; pPixel < pPixelsEnd; pPixel++)
        {
            *(pDma++) = bitpatterns[((*pPixel) & 0x0f)];
            *(pDma++) = bitpatterns[((*pPixel) >> 4) & 0x0f];
        }
    }

    void StopDma()
    {
        ETS_SLC_INTR_DISABLE();
        SET_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);
        CLEAR_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);

        // clear TX descriptor address
        CLEAR_PERI_REG_MASK(SLC_TX_LINK,SLC_TXLINK_DESCADDR_MASK);
        // clear RX descriptor address
        CLEAR_PERI_REG_MASK(SLC_RX_LINK,SLC_RXLINK_DESCADDR_MASK);

        gpio_config_t  io_out_conf;
        io_out_conf.intr_type    = GPIO_INTR_DISABLE;
        io_out_conf.mode         = GPIO_MODE_INPUT;
        io_out_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
        io_out_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_out_conf.pin_bit_mask = BIT(c_I2sPin);
        gpio_config(&io_out_conf);
    }
};

typedef NeoEsp8266DmaMethodBase<NeoEsp8266DmaSpeedWs2812x> NeoEsp8266DmaWs2812xMethod;
typedef NeoEsp8266DmaMethodBase<NeoEsp8266DmaSpeedSk6812> NeoEsp8266DmaSk6812Method;
typedef NeoEsp8266DmaMethodBase<NeoEsp8266DmaSpeed800Kbps> NeoEsp8266Dma800KbpsMethod;
typedef NeoEsp8266DmaMethodBase<NeoEsp8266DmaSpeed400Kbps> NeoEsp8266Dma400KbpsMethod;

// Dma  method is the default method for Esp8266
typedef NeoEsp8266DmaWs2812xMethod NeoWs2813Method;
typedef NeoEsp8266DmaWs2812xMethod NeoWs2812xMethod;
typedef NeoEsp8266Dma800KbpsMethod NeoWs2812Method;
typedef NeoEsp8266DmaSk6812Method NeoSk6812Method;
typedef NeoEsp8266DmaSk6812Method NeoLc8812Method;

typedef NeoEsp8266DmaWs2812xMethod Neo800KbpsMethod;
typedef NeoEsp8266Dma400KbpsMethod Neo400KbpsMethod;

#endif