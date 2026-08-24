#ifndef METER_READER_FACTORY_H
#define METER_READER_FACTORY_H

#include "MeterConfig.h"
#include "IMeterReader.h"
#include "ElsterA150Reader.h"
#include "MiddeDTS27Reader.h"

class MeterReaderFactory {
public:
  static IMeterReader* createMeterReader(uint8_t rxPin, uint8_t txPin) {
#if SELECTED_METER_MODEL == METER_MODEL_MIDDE_DTS27
    return new MiddeDTS27Reader(rxPin, txPin);
#elif SELECTED_METER_MODEL == METER_MODEL_ELSTER_A150
    return new ElsterA150Reader(rxPin, txPin);
#else
    #error "Modelo de medidor no soportado o no seleccionado en MeterConfig.h"
#endif
  }
};

#endif // METER_READER_FACTORY_H
