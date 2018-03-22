#ifndef LEUART_H
#define LEUART_H

int write_char(int c)
{
    while (!(LEUART0->STATUS & LEUART_STATUS_TXBL))
        ;
    LEUART0->TXDATA = (uint32_t)c & 0xffUL;
    return 0;
}

int write_str(const char *s)
{
    while (*s)
        write_char(*s++);
    write_char('\r');
    write_char('\n');
    return 0;
}

/***************************************************************************//**
 * @brief  Setting up LEUART
 ******************************************************************************/
void setupLeuart(void)
{
  /* Enable peripheral clocks */
  CMU_ClockEnable(cmuClock_HFPER, true);
  /* Configure GPIO pins */
  CMU_ClockEnable(cmuClock_GPIO, true);
  /* To avoid false start, configure output as high */
  GPIO_PinModeSet(LEUART_TXPORT, LEUART_TXPIN, gpioModePushPull, 1);
  GPIO_PinModeSet(LEUART_RXPORT, LEUART_RXPIN, gpioModeInput, 0);

  LEUART_Init_TypeDef init = LEUART_INIT_DEFAULT;

  /* Enable CORE LE clock in order to access LE modules */
  CMU_ClockEnable(cmuClock_CORELE, true);

  /* Select LFXO for LEUARTs (and wait for it to stabilize) */
  CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);
  CMU_ClockEnable(cmuClock_LEUART0, true);

  /* Do not prescale clock */
  CMU_ClockDivSet(cmuClock_LEUART0, cmuClkDiv_1);

  /* Configure LEUART */
  init.enable = leuartDisable;
  //init.baudrate = BAUDRATE;
      
  LEUART_Init(LEUART0, &init);

  /* Enable pins at default location */
  LEUART0->ROUTE = LEUART_ROUTE_RXPEN | LEUART_ROUTE_TXPEN | LEUART_LOCATION;

  /* Set RXDMAWU to wake up the DMA controller in EM2 */
//LEUART_RxDmaInEM2Enable(LEUART0, true);

  /* Clear previous RX interrupts */
  LEUART_IntClear(LEUART0, LEUART_IF_RXDATAV);
  NVIC_ClearPendingIRQ(LEUART0_IRQn);

  /* Finally enable it */
  LEUART_Enable(LEUART0, leuartEnable);
}

#endif
