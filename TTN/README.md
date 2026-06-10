<a id="lorawan"></a>
## **LoRaWAN**  

<p align="center">
  <img src="../Images/LoRa-E5.png" alt="LoRa-E5 module"  width="50%" />
</p>

### **What is LoRaWAN?**  
**LoRa** is a spread-spectrum radio modulation developed by Semtech, designed for low-power, long-range communication (up to tens of kilometres in open terrain).  **LoRaWAN** is the MAC layer protocol built on top of LoRa that defines how devices connect to a network of gateways and route packets to an application server. Its key properties for embedded IoT are:  
- Very low transmit power (typically 14–20 dBm)  
- Extremely low device power budget — devices can run for years on a battery  
- Star-of-stars topology: end-nodes → gateways → Network Server (e.g. TTN) → Application Server  

### **Payload Encoding and TTN Decoding**  

**1. Uplink used for the sensors values encoding**

On the STM32U545, the uplink payload is encoded as a compact binary frame carrying:  
1. Temperature (unsigned 8-bit, integer °C)  
2. Humidity (unsigned 8-bit, integer %)  
3. Pressure (unsigned 16-bit, integer hPa)  

**2. Uplink used for the model prediction**

Once the AI model has run and made its prediction, it outputs the class of the predicted condition along with its confidence :
1. Confidence (unsigned 8-bit, integer %)
2. Predicted class (unsigned 8-bit, integer ranging from 0 to 6)

On **The Things Network (TTN)**, a JavaScript *Payload Formatter* (uplink codec) decodes this binary frame back into a structured JSON object:  
```javascript
function decodeUplink(input) {
  const bytes = input.bytes;
  const port  = input.fPort;

  // --- fport 1 : données capteurs (temp + pression + humidité) ---
  if (port === 1) {
    const temp_raw  = (bytes[0] & 0x80) ? bytes[0] - 256 : bytes[0]; // int8
    const pres_raw  = (bytes[1] << 8) | bytes[2];                     // uint16
    const hum_raw   = bytes[3];                                        // uint8

    return {
      data: {
        temperature_C:  temp_raw,
        pressure_hPa:   pres_raw / 10.0,
        humidity_pct:   hum_raw
      },
      warnings: [], errors: []
    };
  }

  // --- fport 12 : prédiction MetAI ---
  if (port === 12) {
    const CLASS_NAMES = [
      "Clair / Ensoleille",
      "Nuageux / Couvert",
      "Pluie",
      "Averses",
      "Neige",
      "Orage",
      "Brouillard / Brume"
    ];
    const cls  = bytes[0];
    const conf = bytes[1];  // en %

    return {
      data: {
        weather_class:      cls,
        weather_label:      (cls < CLASS_NAMES.length) ? CLASS_NAMES[cls] : "Inconnu",
        confidence_pct:     conf
      },
      warnings: [], errors: []
    };
  }

  return { data: {}, warnings: ["fPort inconnu: " + port], errors: [] };
}
```

**3. Downlink used in order to send the data from the database to the U545**

Once the U545 has sent the current values of temperature, pressure and humidity, the server is going to perform a downlink in order to send back the values of 3 and 6 hours ago in order for the model to perform its prediction. 
1. Humidity from 3 hours ago (unsigned 8-bit, integer %)  
2. Humidity from 6 hours ago (unsigned 8-bit, integer %)  
3. Pressure from 3 hours ago (unsigned 16-bit, integer hPa)  
4. Pressure from 6 hours ago (unsigned 16-bit, integer hPa)  
5. Temperature from 3 hours ago (unsigned 8-bit, integer °C) 
6. Temperature from 6 hours ago (unsigned 8-bit, integer °C) 
```javascript
function encodeDownlink(input) {
    const d = input.data;
    
    if (!d || d.temp_3h === undefined) {
        return { 
            bytes: [], 
            fPort: 11, 
            warnings: ["input=" + JSON.stringify(input)], 
            errors: [] 
        };
    }
    
    const bytes = [];

    // Helper : encode une valeur en int16 big-endian
    function pushInt16(val) {
        const v = val & 0xFFFF;
        bytes.push((v >> 8) & 0xFF);
        bytes.push( v       & 0xFF);
    }

    // temp  : int16, scale ×100  (range -327..+327 °C → suffisant)
    // hum   : int16, scale ×100  (range 0..327 % → suffisant)
    // pres  : int16, offset -800 puis ×10 (range 800..4074 hPa)
    pushInt16(Math.round(d.temp_3h * 100));
    pushInt16(Math.round(d.hum_3h  * 100));
    pushInt16(Math.round((d.pres_3h - 800) * 10));

    pushInt16(Math.round(d.temp_6h * 100));
    pushInt16(Math.round(d.hum_6h  * 100));
    pushInt16(Math.round((d.pres_6h - 800) * 10));

    return { bytes: bytes, fPort: 11, warnings: [], errors: [] };
}

function decodeDownlink(input) {
  return {
    data: {
      bytes: input.bytes
    },
    warnings: [],
    errors: []
  }
}

```

