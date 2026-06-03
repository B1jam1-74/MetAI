#include "lora.h"


// Vide tout octet en attente sur le LPUART1 et efface les flags d'erreur
// (Overrun, Framing, Noise, Parity). Indispensable avant chaque commande AT
// pour empêcher les URCs accumulés (+MSG: Done, +MSG: ACK..., etc.) de
// corrompre la réponse de la commande suivante.
void lora_flush_rx(void) {
    volatile uint32_t dummy;

    // Sur F1, les flags d'erreur (ORE, NE, FE, PE) se clearent
    // par une lecture de SR suivie d'une lecture de DR.
    if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_ORE) ||
        __HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_NE)  ||
        __HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_FE)  ||
        __HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_PE))
    {
        dummy = hlpuart1.Instance->ISR;   // 1. lire SR
        dummy = hlpuart1.Instance->RDR;   // 2. lire DR → clear les flags
        (void)dummy;
    }

    // Vider les octets résiduels dans le FIFO RX
    while (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_RXNE)) {
        dummy = (uint8_t)(hlpuart1.Instance->RDR); 
        (void)dummy;
    }
}


// Envoie une commande AT au lora
void lora_SendCommand(const char* cmd) {
    // Purge les octets/erreurs résiduels avant l'échange.
    lora_flush_rx();

    // Send command
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)cmd, strlen(cmd), HAL_MAX_DELAY);

    // show the answer of the module on uart2
    lora_ShowResponse();
}


// Affiche la réponse du module lora sur uart2
void lora_ShowResponse(void) {
    // show the answer of the module on uart2
    uint8_t rx_buffer[100] = {0}; 
    HAL_UART_Receive(&hlpuart1, rx_buffer, sizeof(rx_buffer), 1000);
    HAL_UART_Transmit(&huart1, rx_buffer, strlen((char*)rx_buffer), HAL_MAX_DELAY);
    //printf("%s", rx_buffer);
}


// Met le lora en sleep
void lora_Sleep(void) {
    lora_SendCommand("AT+LOWPOWER\r\n");  
}


// Reveille le lora
// Sur le Seeed LoRa-E5 en mode LOWPOWER, le 1er caractère reçu sur UART
// sert uniquement à réveiller le module et est perdu. Il faut donc envoyer
// un caractère "dummy" puis attendre ~5 ms avant la vraie commande AT.
void lora_WakeUp(void) {
    uint8_t dummy = 0xFF;
    HAL_UART_Transmit(&hlpuart1, &dummy, 1, HAL_MAX_DELAY);
    HAL_Delay(5);
    lora_SendCommand("AT\r\n");
}


// Envoie les données avec le lora
void lora_envoi_data(float temperature, float pressure, float humidity,
                     uint8_t class_idx, uint8_t confidence_pct) {
    char lora_cmd[40];

    // Packe les valeurs dans 6 octets:
    // 1 byte  temp (int8)
    // 2 bytes pression (int16, x10)
    // 1 byte  humidite (uint8)
    // 1 byte  classe predite (uint8, 0-12)
    // 1 byte  confiance en % (uint8, 0-100)
    int8_t temp_raw = (int8_t)temperature;
    int16_t pressure_raw = (int16_t)(pressure * 10.0f);
    uint8_t humidity_raw = (uint8_t)humidity;

    // AT+MSGHEX envoie directement les octets hexadécimaux sur le réseau.
    snprintf(lora_cmd, sizeof(lora_cmd), "AT+MSGHEX=%02X%04X%02X%02X%02X\r\n",
             (unsigned int)(uint8_t)temp_raw,
             (unsigned int)(uint16_t)pressure_raw,
             (unsigned int)humidity_raw,
             (unsigned int)class_idx,
             (unsigned int)confidence_pct);
    lora_SendCommand(lora_cmd);
}


// Configure le module LoRa avec les paramètres de base
void config_Lora(void) {
  lora_SendCommand("AT\r\n");
  HAL_Delay(1000);

  // reset le loRa module
  //send_at_command("AT+FDEFAULT\r\n");
  //HAL_Delay(1000);

  // Demande la version du firmaware du module LoRa
  lora_SendCommand("AT+VER\r\n");
  HAL_Delay(1000);

  lora_SendCommand("AT+MODE=LWOTAA\r\n");
  HAL_Delay(1000);

  // Send the Dr
  lora_SendCommand("AT+DR=EU868\r\n");
  HAL_Delay(1000);

  // Set the power
  lora_SendCommand("AT+POWER=8\r\n");
  HAL_Delay(1000);

  // Send the AppEUI (replace with your actual AppEUI)
  lora_SendCommand("AT+ID=APPEUI,7474747474747474\r\n");
  HAL_Delay(1000);

  // Send the AppKey (replace with your actual AppKey)
  lora_SendCommand("AT+KEY=APPKEY,1D78F0D83FED9FFB83823F9265855366\r\n");
  HAL_Delay(1000);

  // Send the device EUI (replace with your actual device EUI)
  lora_SendCommand("AT+ID=DEVEUI,70B3D57ED0077B42\r\n");
  HAL_Delay(1000);

  // Send the device EUI (replace with your actual device EUI)
  //lora_SendCommand("AT+SAVE\r\n");
  //HAL_Delay(1000);

  // Join the LoRaWAN network
  // Join avec timeout long pour capturer "+JOIN: Network joined"
  uint8_t rx_buf[200] = {0};
  send_at_command("AT+JOIN\r\n", rx_buf, sizeof(rx_buf), 15000);
}


// Version modifiée qui retourne le buffer
uint16_t send_at_command(const char *cmd, uint8_t *rx_buf, uint16_t rx_buf_size, uint32_t timeout_ms) {
    // Purge les octets/erreurs résiduels avant l'échange.
    lora_flush_rx();

    HAL_UART_Transmit(&hlpuart1, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);

    memset(rx_buf, 0, rx_buf_size);
    uint16_t to_receive = rx_buf_size - 1;

    HAL_StatusTypeDef status = HAL_UART_Receive(&hlpuart1, rx_buf, to_receive, timeout_ms);

    uint16_t received = 0;
    if (status == HAL_OK || status == HAL_TIMEOUT) {
        received = to_receive - hlpuart1.RxXferCount;
        if (received > 0) {
            HAL_UART_Transmit(&huart1, rx_buf, received, HAL_MAX_DELAY);
        }
    }
    return received;
}

