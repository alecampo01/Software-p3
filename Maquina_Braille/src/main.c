#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h" // Para esp_rom_delay_us()

// Definición de pines del ESP32-S3
#define DIR_PIN_X    GPIO_NUM_5
#define STEP_PIN_X   GPIO_NUM_4

#define DIR_PIN_Y    GPIO_NUM_7
#define STEP_PIN_Y   GPIO_NUM_6

#define SOLENOID_PIN GPIO_NUM_15

// Parámetros mecánicos NEMA 17 (1.8° por paso)
const int PASOS_POR_VUELTA = 200;
const int PASOS_DOS_VUELTAS = 400; // 200 * 2
const int CUARTO_DE_VUELTA  = 100;  // 200 / 4

// Velocidad del motor (Microsegundos entre pulsos STEP)
// 1000us = velocidad moderada, segura para pruebas en mesa
const int DELAY_MOTOR_US = 1000; 

// -------------------------------------------------------------------------
// FUNCIÓN AUXILIAR: Mueve el motor X y simula golpes de solenoide
// -------------------------------------------------------------------------
void simularBarridoFila(int direccion, int numFila) {
    printf("-> Simulando Fila %d (Dirección: %s)\n", numFila, direccion ? "Horario (Izq->Der)" : "Antihorario (Der->Izq)");
    
    gpio_set_level(DIR_PIN_X, direccion);
    vTaskDelay(pdMS_TO_TICKS(50)); // Pequeña pausa mecánica antes de empezar a mover

    for (int paso = 1; paso <= PASOS_DOS_VUELTAS; paso++) {
        // Dar 1 paso del motor X
        gpio_set_level(STEP_PIN_X, 1);
        esp_rom_delay_us(DELAY_MOTOR_US);
        gpio_set_level(STEP_PIN_X, 0);
        esp_rom_delay_us(DELAY_MOTOR_US);

        // Cada 40 pasos (10 veces en las 2 vueltas), activamos el solenoide para simular impresión
        if (paso % 40 == 0) {
            printf("   [Solenoide] Golpe en paso %d de la Fila %d\n", paso, numFila);
            
            // Disparo del solenoide (según la tabla de pruebas del informe: ~25 ms)
            gpio_set_level(SOLENOID_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(25));
            gpio_set_level(SOLENOID_PIN, 0);
            
            // Breve reposo para no recalentar el actuador y permitir enfriamiento
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}



// -------------------------------------------------------------------------
// TAREA PRINCIPAL DE SIMULACIÓN DE IMPRESIÓN (FreeRTOS)
// -------------------------------------------------------------------------
void tareaSimulacionImpresion(void *pvParameters) {
    while (1) {
        printf("\n==================================================\n");
        printf("INICIANDO NUEVA LÍNEA DE CARACTERES BRAILLE\n");
        printf("==================================================\n");

        // 1. FILA SUPERIOR (Puntos 1 y 4): Izquierda a Derecha (2 Vueltas)
        simularBarridoFila(1, 1);
        vTaskDelay(pdMS_TO_TICKS(300)); // Pausa entre filas

        // 2. FILA INTERMEDIA (Puntos 2 y 5): Derecha a Izquierda (2 Vueltas)
        simularBarridoFila(0, 2);
        vTaskDelay(pdMS_TO_TICKS(300)); // Pausa entre filas

        // 3. FILA INFERIOR (Puntos 3 y 6): Izquierda a Derecha (2 Vueltas)
        simularBarridoFila(1, 3);
        vTaskDelay(pdMS_TO_TICKS(500)); // Pausa antes de mover el papel

        // 4. AVANCE DE PAPEL (Motor Y): Un cuarto de vuelta
        printf("-> Avanzando papel al siguiente renglón (Motor Y: 1/4 de vuelta)...\n");
        gpio_set_level(DIR_PIN_Y, 1); // Sentido de avance del rodillo
        
        for (int i = 0; i < CUARTO_DE_VUELTA; i++) {
            gpio_set_level(STEP_PIN_Y, 1);
            esp_rom_delay_us(DELAY_MOTOR_US);
            gpio_set_level(STEP_PIN_Y, 0);
            esp_rom_delay_us(DELAY_MOTOR_US);
        }

        printf("¡Línea completada! Esperando 2 segundos para reiniciar el ciclo...\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// -------------------------------------------------------------------------
// PUNTO DE ENTRADA DEL SISTEMA OPERATIVO ESP-IDF
// -------------------------------------------------------------------------
void app_main(void) {
    // Configuración de puertos como salida
    gpio_set_direction(DIR_PIN_X, GPIO_MODE_OUTPUT);
    gpio_set_direction(STEP_PIN_X, GPIO_MODE_OUTPUT);
    gpio_set_direction(DIR_PIN_Y, GPIO_MODE_OUTPUT);
    gpio_set_direction(STEP_PIN_Y, GPIO_MODE_OUTPUT);
    gpio_set_direction(SOLENOID_PIN, GPIO_MODE_OUTPUT);

    // Asegurar que el solenoide empiece apagado por seguridad
    gpio_set_level(SOLENOID_PIN, 0);

    printf("Sistema iniciado. Creando tarea de simulación en FreeRTOS...\n");

    // Creamos la tarea de FreeRTOS asignando 4KB de memoria RAM (Stack)
    xTaskCreate(tareaSimulacionImpresion, "Simulacion_Braille", 4096, NULL, 5, NULL);
}