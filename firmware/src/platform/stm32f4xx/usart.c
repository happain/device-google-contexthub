#include <plat/inc/gpio.h>
#include <plat/inc/usart.h>
#include <plat/inc/pwr.h>
#include <usart.h>
#include <gpio.h>

struct StmUsart {
  volatile uint16_t SR;
  uint8_t unused0[2];
  volatile uint16_t DR;
  uint8_t unused1[2];
  volatile uint16_t BRR;
  uint8_t unused2[2];
  volatile uint16_t CR1;
  uint8_t unused3[2];
  volatile uint16_t CR2;
  uint8_t unused4[2];
  volatile uint16_t CR3;
  uint8_t unused5[2];
  volatile uint16_t GTPR;
  uint8_t unused6[2];
};

static const uint32_t mUsartPorts[] = {
    USART1_BASE,
    USART2_BASE,
    USART3_BASE,
    UART4_BASE,
    UART5_BASE,
    USART6_BASE,
};

static const uint32_t mUsartPeriphs[] = {
    PERIPH_APB2_USART1,
    PERIPH_APB1_USART2,
    PERIPH_APB1_USART3,
    PERIPH_APB1_UART4,
    PERIPH_APB1_UART5,
    PERIPH_APB2_USART6,
};

static uint8_t mUsartBusses[] = {
    PERIPH_BUS_APB2,
    PERIPH_BUS_APB1,
    PERIPH_BUS_APB1,
    PERIPH_BUS_APB1,
    PERIPH_BUS_APB1,
    PERIPH_BUS_APB2,
};

static bool mUsartHasFlowControl[] = {
    true,
    true,
    true,
    false,
    false,
    true,
};

static enum GpioAltFunc mUsartAlt[] = {
    GPIO_AF_USART1,
    GPIO_AF_USART2,
    GPIO_AF00,
    GPIO_AF00,
    GPIO_AF00,
    GPIO_AF_USART6,
};

void usartOpen(struct usart* __restrict usart, UsartPort port,
                GpioNum tx, GpioNum rx,
                uint32_t baud, UsartDataBitsCfg data_bits,
                UsatStopBitsCfg stop_bits, UsartParityCfg parity,
                UsartFlowControlCfg flow_control)
{
    static const uint16_t stopBitsVals[] = {0x1000, 0x0000, 0x3000, 0x2000}; // indexed by UsatStopBitsCfg
    static const uint16_t wordLengthVals[] = {0x0000, 0x1000}; // indexed by UsartDataBitsCfg
    static const uint16_t parityVals[] = {0x0000, 0x0400, 0x0600}; // indexed by UsartParityCfg
    static const uint16_t flowCtrlVals[] = {0x0000, 0x0100, 0x0200, 0x0300}; // indexed by UsartFlowControlCfg
    struct StmUsart *block = (struct StmUsart*)mUsartPorts[usart->unit = --port];
    uint32_t baseClk, div, intPart, fraPart;

    /* configure tx/rx gpios */

    usart->rx = gpioRequest(rx); /* rx */
    gpioConfigAlt(usart->rx, GPIO_SPEED_LOW, GPIO_PULL_UP, GPIO_OUT_PUSH_PULL, mUsartAlt[port]);
    usart->tx = gpioRequest(tx); /* tx */
    gpioConfigAlt(usart->tx, GPIO_SPEED_LOW, GPIO_PULL_UP, GPIO_OUT_PUSH_PULL, mUsartAlt[port]);

    /* enable clock */
    pwrUnitClock(mUsartBusses[port], mUsartPeriphs[port], true);

    /* sanity checks */
    if (!mUsartHasFlowControl[port])
        flow_control = USART_FLOW_CONTROL_NONE;

    /* basic config as required + oversample by 8, tx+rx on */
    block->CR2 = (block->CR2 &~ 0x3000) | stopBitsVals[stop_bits];
    block->CR1 = (block->CR1 &~ 0x1600) | wordLengthVals[data_bits] | parityVals[parity] | 0x800C;
    block->CR3 = (block->CR3 &~ 0x0300) | flowCtrlVals[flow_control];

    /* clocking calc */
    baseClk = pwrGetBusSpeed(mUsartBusses[port]);
    div = (baseClk * 25) / (baud * 2);
    intPart = div / 100;
    fraPart = div % 100;

    /* clocking munging */
    intPart = intPart << 4;
    fraPart = ((fraPart * 8 + 50) / 100) & 7;
    block->BRR = intPart | fraPart;
 
    /* enable */
    block->CR1 |= 0x2000;
}

void usartClose(const struct usart* __restrict usart)
{
    struct StmUsart *block = (struct StmUsart*)mUsartPorts[usart->unit];

    /* Disable USART */
    block->CR1 &=~ 0x2000;

    /* Disable USART clock */
    pwrUnitClock(mUsartBusses[usart->unit], mUsartPeriphs[usart->unit], false);

    /* Release gpios */
    gpioRelease(usart->rx);
    gpioRelease(usart->tx);
}

void usartPutchat(const struct usart* __restrict usart, char c)
{
    struct StmUsart *block = (struct StmUsart*)mUsartPorts[usart->unit];

    /* wait for ready */
    while (!(block->SR & 0x0080));

    /* send */
    block->DR = (uint8_t)c;
}
