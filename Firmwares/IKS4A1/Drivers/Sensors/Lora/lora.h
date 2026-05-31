#include "main.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

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
void lora_envoi_data(float temperature, float pressure, float humidity,
                     uint8_t class_idx, uint8_t confidence_pct);

// Version modifiée qui retourne le buffer
uint16_t send_at_command(const char *cmd, uint8_t *rx_buf, uint16_t rx_buf_size, uint32_t timeout_ms);