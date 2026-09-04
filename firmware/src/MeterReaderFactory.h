#ifndef METER_READER_FACTORY_H
#define METER_READER_FACTORY_H

#include "MeterConfig.h"
#include "IMeterReader.h"
#include "ElsterA150Reader.h"
#include "MiddeDTS27Reader.h"
#include "MiddeDDS26DReader.h"
#include "ElsterA1052Reader.h"

class MeterReaderFactory {
public:
  static IMeterReader* createMeterReader(uint8_t rxPin, uint8_t txPin) {
#if SELECTED_METER_MODEL == METER_MODEL_MIDDE_DTS27
    return new MiddeDTS27Reader(rxPin, txPin);
#elif SELECTED_METER_MODEL == METER_MODEL_ELSTER_A150
    return new ElsterA150Reader(rxPin, txPin);
#elif SELECTED_METER_MODEL == METER_MODEL_MIDDE_DDS26D
    return new MiddeDDS26DReader(rxPin, txPin);
#elif SELECTED_METER_MODEL == METER_MODEL_ELSTER_A1052
    return new ElsterA1052Reader(rxPin, txPin);
#else
    #error "Modelo de medidor no soportado o no seleccionado en MeterConfig.h"
#endif
  }
};

#endif // METER_READER_FACTORY_H
