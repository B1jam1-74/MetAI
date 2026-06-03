#include "main.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart1; // UART pour debug
extern UART_HandleTypeDef hlpuart1; // UART connecté au module LoRa

// Vide tout octet en attente sur le LPUART1 et efface les flags d'erreur
void lora_flush_rx(void);

// Envoie une commande AT au lora
void lora_SendCommand(const char* cmd);

// Affiche la réponse du module lora sur uart2
void lora_ShowResponse(void);

// Met le lora en sleep
void lora_Sleep(void);

// Reveille le lora
void lora_WakeUp(void);

// Configure le module LoRa avec les paramètres de base
void config_Lora(void);

// Envoie les valeurs capteurs + prédiction IA dans une payload hex compacte
void lora_envoi_data(float temperature, float pressure, float humidity);

// Version modifiée qui retourne le buffer
uint16_t send_at_command(const char *cmd, uint8_t *rx_buf, uint16_t rx_buf_size, uint32_t timeout_ms);

/* Downlink decoded structure */
typedef struct {
    float temp_3h;
    float hum_3h;
    float pres_3h;
    float temp_6h;
    float hum_6h;
    float pres_6h;
} metai_downlink_t;

/* Receive and decode a pending downlink (fport 11) */
uint8_t lora_ReceiveDownlink(metai_downlink_t *out);

uint16_t send_at_command_until(const char *cmd, uint8_t *rx_buf,
                                uint16_t rx_buf_size, const char *end_marker,
                                uint32_t timeout_ms);

// Envoie la prédiction du modele avec le lora 
void lora_envoi_prediction(uint8_t predicted_class, uint8_t confidence);