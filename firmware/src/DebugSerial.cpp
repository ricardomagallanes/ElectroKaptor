#include "DebugSerial.h"

void debugSerialInit(uint32_t baud) {
  // Habilitar reloj para GPIOA y USART2
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  // Configurar PA2 como Salida Función Alternativa Push-Pull (CNF=10, MODE=11 -> 0xB)
  GPIOA->CRL = (GPIOA->CRL & ~(0xF << (2 * 4))) | (0xB << (2 * 4));

  // PCLK1 es 36MHz (72MHz / 2). Baudrate = PCLK1 / baud
  USART2->BRR = 36000000UL / baud;

  // Habilitar Transmisor y USART2
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void debugPrintChar(char c) {
  while (!(USART2->SR & USART_SR_TXE));
  USART2->DR = (uint8_t)c;
}

void debugPrint(const char* str) {
  if (!str) return;
  while (*str) {
    debugPrintChar(*str++);
  }
}

void debugPrintln(const char* str) {
  if (str) debugPrint(str);
  debugPrintChar('\r');
  debugPrintChar('\n');
}

static char s_debugBuf[256];

void debugPrintf(const char* format, ...) {
  if (!format) return;
  va_list args;
  va_start(args, format);
  vsnprintf(s_debugBuf, sizeof(s_debugBuf), format, args);
  va_end(args);
  debugPrint(s_debugBuf);
}
