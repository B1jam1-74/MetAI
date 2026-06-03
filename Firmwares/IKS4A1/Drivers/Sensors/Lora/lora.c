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

  // Set the port to 1 for uplink
  lora_SendCommand("AT+PORT=1\r\n");
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
uint16_t send_at_command(const char *cmd, uint8_t *rx_buf,
                          uint16_t rx_buf_size, uint32_t timeout_ms)
{
    return send_at_command_until(cmd, rx_buf, rx_buf_size, NULL, timeout_ms);
}


// Buffer statique partagé pour la réponse du dernier uplink
static uint8_t  s_last_uplink_response[512] = {0};
static uint16_t s_last_uplink_response_len  = 0;

// Envoie les données avec le lora 
void lora_envoi_data(float temperature, float pressure, float humidity)
{
    char lora_cmd[40];

    int8_t  temp_raw     = (int8_t)temperature;
    int16_t pressure_raw = (int16_t)(pressure * 10.0f);
    uint8_t humidity_raw = (uint8_t)humidity;

    snprintf(lora_cmd, sizeof(lora_cmd), "AT+MSGHEX=%02X%04X%02X\r\n",
             (unsigned int)(uint8_t)temp_raw,
             (unsigned int)(uint16_t)pressure_raw,
             (unsigned int)humidity_raw);

    /* Lire jusqu'à "Done" — s'arrête dès que la réponse est complète */
    s_last_uplink_response_len = send_at_command_until(
        lora_cmd,
        s_last_uplink_response,
        sizeof(s_last_uplink_response),
        "+MSGHEX: Done",   /* marqueur de fin */
        8000               /* timeout de sécurité */
    );
}

// Décode le downlink depuis la réponse déjà capturée par lora_envoi_data()
uint8_t lora_ReceiveDownlink(metai_downlink_t *out)
{
    if (s_last_uplink_response_len == 0) return 0;

    // Chercher "PORT:11; RX: " dans la réponse capturée
    char *rx_marker = strstr((char *)s_last_uplink_response, "RX: \"");
    if (rx_marker == NULL) return 0;

    char *hex_start = rx_marker + 5;  // saute 'RX: "'

    // Extraire les 12 octets (24 hex chars, espaces éventuels ignorés)
    uint8_t payload[12] = {0};
    uint8_t nb_bytes = 0;
    char    hex_byte[3] = {0};

    while (nb_bytes < 12 && *hex_start != '"' && *hex_start != '\0')
    {
        // Sauter les espaces éventuels entre octets
        if (*hex_start == ' ') { hex_start++; continue; }

        hex_byte[0] = hex_start[0];
        hex_byte[1] = hex_start[1];
        hex_byte[2] = '\0';
        payload[nb_bytes++] = (uint8_t)strtol(hex_byte, NULL, 16);
        hex_start += 2;
    }

    if (nb_bytes < 12) return 0;

    // Décoder 6 × int16 big-endian, scale /100
    int16_t raw[6];
    for (uint8_t i = 0; i < 6; i++) {
        raw[i] = (int16_t)((payload[i * 2] << 8) | payload[i * 2 + 1]);
    }

    out->temp_3h = raw[0] / 100.0f;
    out->hum_3h  = raw[1] / 100.0f;
    out->pres_3h = (raw[2] / 10.0f) + 800.0f;
    out->temp_6h = raw[3] / 100.0f;
    out->hum_6h  = raw[4] / 100.0f;
    out->pres_6h = (raw[5] / 10.0f) + 800.0f;

    return 1;
}

/**
 * @brief  Envoie une commande AT et lit la réponse jusqu'à un marqueur de fin
 *         ou timeout. Évite de bloquer inutilement toute la durée du timeout.
 *
 * @param  cmd          Commande AT à envoyer (peut être "" pour lecture seule)
 * @param  rx_buf       Buffer de réception
 * @param  rx_buf_size  Taille du buffer
 * @param  end_marker   Chaîne marquant la fin de réponse (ex: "Done\r\n")
 * @param  timeout_ms   Timeout global en ms
 * @retval Nombre d'octets reçus
 */
uint16_t send_at_command_until(const char *cmd,
                                uint8_t    *rx_buf,
                                uint16_t    rx_buf_size,
                                const char *end_marker,
                                uint32_t    timeout_ms)
{
    lora_flush_rx();

    if (strlen(cmd) > 0) {
        HAL_UART_Transmit(&hlpuart1, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
    }

    memset(rx_buf, 0, rx_buf_size);
    uint16_t idx        = 0;
    uint32_t t_start    = HAL_GetTick();
    uint8_t  byte       = 0;

    while ((HAL_GetTick() - t_start) < timeout_ms && idx < (rx_buf_size - 1))
    {
        if (HAL_UART_Receive(&hlpuart1, &byte, 1, 10) == HAL_OK)
        {
            rx_buf[idx++] = byte;

            /* Vérifier si le marqueur de fin est présent dans le buffer */
            if (end_marker != NULL && idx >= strlen(end_marker))
            {
                if (strstr((char *)rx_buf, end_marker) != NULL) {
                    break;
                }
            }
        }
    }

    /* Afficher sur UART debug */
    if (idx > 0) {
        HAL_UART_Transmit(&huart1, rx_buf, idx, HAL_MAX_DELAY);
    }

    return idx;
}


// Envoie la prédiction du modele avec le lora 
// AT+MSGHEX envoie sur fport 1 par défaut
// Pour fport 12 : AT+PORT=12 avant l'envoi
void lora_envoi_prediction(uint8_t predicted_class, uint8_t confidence_pct)
{
    char lora_cmd[32];

    // Changer le port d'émission
    send_at_command_until("AT+PORT=12\r\n",
                          s_last_uplink_response,
                          sizeof(s_last_uplink_response),
                          "+PORT:", 2000);

    // 2 octets : classe + confiance %
    snprintf(lora_cmd, sizeof(lora_cmd), "AT+MSGHEX=%02X%02X\r\n",
             predicted_class, confidence_pct);

    s_last_uplink_response_len = send_at_command_until(
        lora_cmd, s_last_uplink_response,
        sizeof(s_last_uplink_response),
        "+MSGHEX: Done", 8000);

    // Remettre le port par défaut pour les données capteurs
    send_at_command_until("AT+PORT=1\r\n",
                          s_last_uplink_response,
                          sizeof(s_last_uplink_response),
                          "+PORT:", 2000);
}