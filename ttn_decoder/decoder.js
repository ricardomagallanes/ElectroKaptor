/**
 * Decodificador de Payload para The Things Network (TTN v3 LNS)
 * Dispositivo: Heltec ESP32-S3 (SX1262 LoRaWAN) + Medidor Elster A150 IR
 * Decodifica las tramas empaquetadas en bits (Mensajes 0, 1, 2 y 3)
 * conteniendo los 33 parámetros especificados en parametros.txt.
 */

function decodeUplink(input) {
  var bytes = input.bytes;
  var fPort = input.fPort;

  if (!bytes || bytes.length === 0) {
    return {
      errors: ["Payload vacío"]
    };
  }

  // Normalizador para soportar tanto bytes binarios crudos como cadenas ASCII Hex
  function normalizeBytes(arr) {
    function isHexArray(a) {
      if (!a || a.length < 24 || a.length % 2 !== 0) return false;
      for (var i = 0; i < a.length; i++) {
        var c = a[i];
        if (!((c >= 48 && c <= 57) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102))) return false;
      }
      return true;
    }
    function asciiHexToBytes(a) {
      var str = "";
      for (var i = 0; i < a.length; i++) {
        str += String.fromCharCode(a[i]);
      }
      var res = [];
      for (var j = 0; j < str.length; j += 2) {
        res.push(parseInt(str.substring(j, j + 2), 16));
      }
      return res;
    }
    var current = arr;
    while (isHexArray(current)) {
      current = asciiHexToBytes(current);
    }
    return current;
  }

  bytes = normalizeBytes(bytes);

  // Convierte el array de bytes a una cadena de bits (0s y 1s)
  var bits = "";
  for (var i = 0; i < bytes.length; i++) {
    var b = bytes[i];
    for (var bit = 7; bit >= 0; bit--) {
      bits += (b >> bit) & 1 ? "1" : "0";
    }
  }

  // Función aux para extraer un entero de un rango de bits [inicio, fin)
  function readBits(start, end) {
    if (end > bits.length) {
      return 0;
    }
    var sub = bits.substring(start, end);
    return parseInt(sub, 2);
  }

  var data = {};
  var mensajeNro = readBits(0, 2);
  data["mensaje_nro"] = mensajeNro;

  var estadoTextos = [
    "Normal",
    "Sin Energia",
    "Sin Lectura",
    "Sin Energia y Sin Lectura"
  ];
  var tipoMedidorTextos = [
    "Monofasico Unidireccional",
    "Monofasico Bidireccional",
    "Trifasico",
    "Grandes Clientes"
  ];

  if (mensajeNro === 0) {
    // MENSAJE 0: Telemetría principal
    var tipoMedidorCode = readBits(2, 4);
    var estadoCode = readBits(4, 7);
    
    data["tipo_medidor_cod"] = tipoMedidorCode;
    data["tipo_medidor"] = tipoMedidorTextos[tipoMedidorCode] || "Desconocido";
    data["estado_cod"] = estadoCode;
    data["estado"] = estadoTextos[estadoCode] || "Desconocido";
    
    data["bateria_pct"] = readBits(7, 13);                  // 6 bits (0-63 %)
    data["cosphi"] = (readBits(13, 20) / 100).toFixed(2);   // 7 bits
    data["voltaje_a"] = readBits(20, 31);  // 11 bits (V)
    data["voltaje_b"] = readBits(31, 42);  // 11 bits (V)
    data["voltaje_c"] = readBits(42, 53);  // 11 bits (V)
    data["corriente_a"] = (readBits(53, 66) / 100).toFixed(2); // 13 bits (A)
    data["corriente_b"] = (readBits(66, 79) / 100).toFixed(2); // 13 bits (A)
    data["corriente_c"] = (readBits(79, 92) / 100).toFixed(2); // 13 bits (A)
    data["energia_activa_importada_kwh"] = (readBits(92, 119) / 100).toFixed(2); // 27 bits (kWh en centésimas)

    if (tipoMedidorCode === 3) {
      // Grandes Clientes
      data["energia_activa_exportada_kwh"] = (readBits(119, 146) / 100).toFixed(2); // 27 bits
      data["energia_reactiva_importada_kvarh"] = (readBits(146, 173) / 100).toFixed(2); // 27 bits
      data["temperatura_c"] = readBits(173, 180); // 7 bits (°C)
    }

  } else if (mensajeNro === 1) {
    // MENSAJE 1: Configuración / Demandas / Energía secundaria
    // Bit 127 indica tipo medidor (0: Pequenos Clientes, 1: Grandes Clientes)
    var isGrandesClientesM1 = (bits.length >= 128) ? parseInt(bits[127], 2) : 0;
    data["tipo_medidor_es_grandes_clientes"] = isGrandesClientesM1 === 1;

    if (isGrandesClientesM1 === 0) {
      // Pequeños clientes (Monofasico o Trifasico)
      data["energia_activa_exportada_kwh"] = (readBits(2, 29) / 10).toFixed(1);
      data["energia_reactiva_importada_kvarh"] = (readBits(29, 56) / 10).toFixed(1);
      data["energia_reactiva_exportada_kvarh"] = (readBits(56, 83) / 10).toFixed(1);
      data["maxima_demanda_importada_kw"] = (readBits(83, 103) / 100).toFixed(2);
      data["maxima_demanda_exportada_kw"] = (readBits(103, 123) / 100).toFixed(2);
    } else {
      // Grandes clientes
      data["energia_reactiva_exportada_kvarh"] = (readBits(2, 29) / 100).toFixed(2);
      data["maxima_demanda_importada_t1_kw"] = (readBits(29, 49) / 100).toFixed(2);
      data["maxima_demanda_importada_t2_kw"] = (readBits(49, 69) / 100).toFixed(2);
      data["maxima_demanda_importada_t3_kw"] = (readBits(69, 89) / 100).toFixed(2);
    }

  } else if (mensajeNro === 2) {
    // MENSAJE 2: Facturación por Tarifas y Fases
    var isGrandesClientesM2 = (bits.length >= 192) ? parseInt(bits[191], 2) : 1;
    data["tipo_medidor_es_grandes_clientes"] = isGrandesClientesM2 === 1;

    if (isGrandesClientesM2 === 1) {
      data["acumulada_activa_importada_t1_kwh"] = (readBits(2, 29) / 100).toFixed(2);
      data["acumulada_activa_importada_t2_kwh"] = (readBits(29, 56) / 100).toFixed(2);
      data["acumulada_activa_importada_t3_kwh"] = (readBits(56, 83) / 100).toFixed(2);
      data["acumulada_reactiva_importada_t1_kvarh"] = (readBits(83, 110) / 100).toFixed(2);
      data["activa_importada_fase1_kwh"] = (readBits(110, 137) / 100).toFixed(2);
      data["activa_importada_fase2_kwh"] = (readBits(137, 164) / 100).toFixed(2);
      data["activa_importada_fase3_kwh"] = (readBits(164, 191) / 100).toFixed(2);
    }

  } else if (mensajeNro === 3) {
    // MENSAJE 3: Calidad y Exportación por Fases
    var isGrandesClientesM3 = (bits.length >= 128) ? parseInt(bits[127], 2) : 1;
    data["tipo_medidor_es_grandes_clientes"] = isGrandesClientesM3 === 1;

    if (isGrandesClientesM3 === 1) {
      data["activa_exportada_fase1_kwh"] = (readBits(2, 29) / 100).toFixed(2);
      data["activa_exportada_fase2_kwh"] = (readBits(29, 56) / 100).toFixed(2);
      data["activa_exportada_fase3_kwh"] = (readBits(56, 83) / 100).toFixed(2);
      data["cosphi_minimo"] = (readBits(83, 90) / 100).toFixed(2);
      data["cosphi_promedio"] = (readBits(90, 97) / 100).toFixed(2);
      data["frecuencia_min_hz"] = (readBits(97, 110) / 100).toFixed(2);
      data["frecuencia_max_hz"] = (readBits(110, 123) / 100).toFixed(2);
    }
  }

  return {
    data: data,
    warnings: [],
    errors: []
  };
}

// Exportación para entornos Node.js (Pruebas unitarias)
if (typeof module !== "undefined" && module.exports) {
  module.exports = { decodeUplink: decodeUplink };
}
