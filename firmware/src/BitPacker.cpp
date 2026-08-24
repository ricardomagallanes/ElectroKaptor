#include "BitPacker.h"

void BitPacker::writeBits(uint8_t *buf, uint16_t &bitOffset, uint32_t val, uint8_t numBits) {
  for (int b = numBits - 1; b >= 0; b--) {
    uint8_t bitVal = (val >> b) & 1;
    uint16_t byteIdx = bitOffset / 8;
    uint8_t bitIdx  = 7 - (bitOffset % 8);
    
    if (bitVal) {
      buf[byteIdx] |= (1 << bitIdx);
    } else {
      buf[byteIdx] &= ~(1 << bitIdx);
    }
    bitOffset++;
  }
}

uint8_t BitPacker::packMessage0(const MeterData &data, uint8_t *outBuffer) {
  memset(outBuffer, 0, 24);
  uint16_t offset = 0;

  writeBits(outBuffer, offset, 0, 2);                  // Mensaje: 0 (bits 0..2)
  writeBits(outBuffer, offset, data.tipoMedidor, 2);   // Tipo_Medidor (bits 2..4)
  writeBits(outBuffer, offset, data.estado, 3);        // Estado (bits 4..7)
  writeBits(outBuffer, offset, data.bateria, 6);       // Bateria (bits 7..13)
  writeBits(outBuffer, offset, data.cosphi, 7);        // FP (bits 13..20)
  writeBits(outBuffer, offset, data.voltajeA, 11);     // Va (bits 20..31)
  writeBits(outBuffer, offset, data.voltajeB, 11);     // Vb (bits 31..42)
  writeBits(outBuffer, offset, data.voltajeC, 11);     // Vc (bits 42..53)
  writeBits(outBuffer, offset, data.corrienteA, 13);   // Ia (bits 53..66)
  writeBits(outBuffer, offset, data.corrienteB, 13);   // Ib (bits 66..79)
  writeBits(outBuffer, offset, data.corrienteC, 13);   // Ic (bits 79..92)
  writeBits(outBuffer, offset, data.energiaActivaImp, 27); // Energia_Activa_Imp (bits 92..119)

  if (data.tipoMedidor == 3) { // Grandes Clientes
    writeBits(outBuffer, offset, data.energiaActivaExp, 27);  // (bits 119..146)
    writeBits(outBuffer, offset, data.energiaReactivaImp, 27); // (bits 146..173)
    writeBits(outBuffer, offset, data.temperatura, 7);         // (bits 173..180)
  }

  return (offset + 7) / 8; // Devuelve cantidad de bytes ocupados
}

uint8_t BitPacker::packMessage1(const MeterData &data, uint8_t *outBuffer) {
  memset(outBuffer, 0, 17);
  uint16_t offset = 0;

  writeBits(outBuffer, offset, 1, 2); // Mensaje: 1 (bits 0..2)

  if (data.tipoMedidor != 3) {
    // Pequeños clientes (Monofasico o Trifasico)
    writeBits(outBuffer, offset, data.energiaActivaExp, 27);   // bits 2..29
    writeBits(outBuffer, offset, data.energiaReactivaImp, 27);  // bits 29..56
    writeBits(outBuffer, offset, data.energiaReactivaExp, 27);  // bits 56..83
    writeBits(outBuffer, offset, data.maximaDemandaImp, 20);   // bits 83..103
    writeBits(outBuffer, offset, data.maximaDemandaExp, 20);   // bits 103..123
    
    // Forzar bit 127 = 0
    uint16_t bit127Offset = 127;
    writeBits(outBuffer, bit127Offset, 0, 1);
    offset = 128;
  } else {
    // Grandes Clientes
    writeBits(outBuffer, offset, data.energiaReactivaExp, 27);  // bits 2..29
    writeBits(outBuffer, offset, data.maximaDemandaImpT1, 20);  // bits 29..49
    writeBits(outBuffer, offset, data.maximaDemandaImpT2, 20);  // bits 49..69
    writeBits(outBuffer, offset, data.maximaDemandaImpT3, 20);  // bits 69..89

    // Rellenar hasta bit 127 y establecer bit 127 = 1
    while (offset < 127) {
      writeBits(outBuffer, offset, 0, 1);
    }
    writeBits(outBuffer, offset, 1, 1); // bit 127
  }

  return (offset + 7) / 8;
}

uint8_t BitPacker::packMessage2(const MeterData &data, uint8_t *outBuffer) {
  memset(outBuffer, 0, 24);
  uint16_t offset = 0;

  writeBits(outBuffer, offset, 2, 2); // Mensaje: 2 (bits 0..2)

  if (data.tipoMedidor == 3) { // Grandes Clientes
    writeBits(outBuffer, offset, data.acumuladaActivaImpT1, 27);   // bits 2..29
    writeBits(outBuffer, offset, data.acumuladaActivaImpT2, 27);   // bits 29..56
    writeBits(outBuffer, offset, data.acumuladaActivaImpT3, 27);   // bits 56..83
    writeBits(outBuffer, offset, data.acumuladaReactivaImpT1, 27); // bits 83..110
    writeBits(outBuffer, offset, data.activaImpFase1, 27);         // bits 110..137
    writeBits(outBuffer, offset, data.activaImpFase2, 27);         // bits 137..164
    writeBits(outBuffer, offset, data.activaImpFase3, 27);         // bits 164..191

    // bit 191 = 1 (Grandes clientes)
    uint16_t bit191Offset = 191;
    writeBits(outBuffer, bit191Offset, 1, 1);
    offset = 192;
  }

  return (offset + 7) / 8;
}

uint8_t BitPacker::packMessage3(const MeterData &data, uint8_t *outBuffer) {
  memset(outBuffer, 0, 16);
  uint16_t offset = 0;

  writeBits(outBuffer, offset, 3, 2); // Mensaje: 3 (bits 0..2)

  if (data.tipoMedidor == 3) { // Grandes Clientes
    writeBits(outBuffer, offset, data.activaExpFase1, 27);   // bits 2..29
    writeBits(outBuffer, offset, data.activaExpFase2, 27);   // bits 29..56
    writeBits(outBuffer, offset, data.activaExpFase3, 27);   // bits 56..83
    writeBits(outBuffer, offset, data.cosphiMinimo, 7);     // bits 83..90
    writeBits(outBuffer, offset, data.cosphiPromedio, 7);   // bits 90..97
    writeBits(outBuffer, offset, data.frecuenciaMin, 13);    // bits 97..110
    writeBits(outBuffer, offset, data.frecuenciaMax, 13);    // bits 110..123

    while (offset < 127) {
      writeBits(outBuffer, offset, 0, 1);
    }
    writeBits(outBuffer, offset, 1, 1); // bit 127 = 1
  }

  return (offset + 7) / 8;
}

uint8_t BitPacker::convertToAsciiHex(const uint8_t *inBuf, uint8_t inLen, uint8_t *outBuf) {
  const char hexChars[] = "0123456789ABCDEF";
  for (uint8_t i = 0; i < inLen; i++) {
    outBuf[i * 2]     = hexChars[(inBuf[i] >> 4) & 0x0F];
    outBuf[i * 2 + 1] = hexChars[inBuf[i] & 0x0F];
  }
  return inLen * 2;
}
