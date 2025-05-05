#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "lib/matriz.h"
#include "math.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define led_Green 11
//#define led_Blue 12
#define led_Red 13

bool modoNoturno = false; //Variável global para alternar entre os modos
#define BotaoA 5
static volatile uint32_t tempo_anterior = 0;
bool ciclo_normal_inicado = false;
bool ciclo_noturno_iniciado = false;

TaskHandle_t xHandleA; //Suspender e ativar a task
//BaseType_t xYieldRequired;
void vBotaoATask(){
    
    vTaskSuspend(NULL);
    //ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while(true){
        printf("modo noturno: %d",modoNoturno);
        modoNoturno=!modoNoturno;
        //vTaskDelay(pdMS_TO_TICKS(200));
        vTaskSuspend(xHandleA); //Suspender nates do scheduler
    };
};

uint sinal_atual=0;

void vSemaforoRGBTask(){

    //Inicializando pinos
    gpio_init(led_Green);
    gpio_init(led_Red);
    gpio_set_dir(led_Green, GPIO_OUT);
    gpio_set_dir(led_Red, GPIO_OUT);

    while(true){
        //vTaskSuspend(xHandleA);
        if(!modoNoturno){
            //Modo normal 
            ciclo_normal_inicado = true;
            ciclo_noturno_iniciado = false;
            gpio_put(led_Green,1);
            sinal_atual=1;
            vTaskDelay(pdMS_TO_TICKS(15000));
            gpio_put(led_Green,0);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_put(led_Green,1);
            gpio_put(led_Red,1);
            sinal_atual=2;
            vTaskDelay(pdMS_TO_TICKS(5000));
            gpio_put(led_Green,0);
            gpio_put(led_Red,0);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_put(led_Red,1);
            sinal_atual = 3;
            vTaskDelay(pdMS_TO_TICKS(30000));
            gpio_put(led_Red,0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }else{
            ciclo_normal_inicado = false;
            ciclo_noturno_iniciado = true;
            //Modo noturno
            gpio_put(led_Green,1);
            gpio_put(led_Red,1);
            vTaskDelay(pdMS_TO_TICKS(3000));
            gpio_put(led_Green,0);
            gpio_put(led_Red,0);
            vTaskDelay(pdMS_TO_TICKS(3000));

        };

    };

};

//Buzzer
#define Buzzer 21
void vBuzzerTask(){

    uint slice;
    gpio_set_function(Buzzer, GPIO_FUNC_PWM); //Configura pino do led como pwm
    slice = pwm_gpio_to_slice_num(Buzzer); //Adiquire o slice do pino
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (4400 * 4096));
    pwm_init(slice, &config, true);
    pwm_set_gpio_level(Buzzer, 0); //Determina com o level desejado

    while (true)
    {
        if(modoNoturno && ciclo_noturno_iniciado){
            pwm_set_gpio_level(Buzzer, 32768);
            vTaskDelay(pdMS_TO_TICKS(700));
            pwm_set_gpio_level(Buzzer, 0);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }else if (ciclo_normal_inicado){
            if(sinal_atual==1){
                pwm_set_gpio_level(Buzzer, 32768);
                vTaskDelay(pdMS_TO_TICKS(1000));
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(14500));
            }else if(sinal_atual==2){
                pwm_set_gpio_level(Buzzer, 32768);
                vTaskDelay(pdMS_TO_TICKS(275));
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(275));
            }else if(sinal_atual==3){
                pwm_set_gpio_level(Buzzer, 32768);
                vTaskDelay(pdMS_TO_TICKS(500));
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(1500));
            };
        }else{
            vTaskDelay(pdMS_TO_TICKS(50));
        };
    };

};

void vDisplay3Task()
{
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_t ssd;                                                // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    char str_y[5]; // Buffer para armazenar a string
    int contador = 0;
    bool cor = true;
    while (true) //Modificar todo o design
    {
        sprintf(str_y, "%d", contador); // Converte em string
        contador++;                     // Incrementa o contador
        ssd1306_fill(&ssd, !cor);                          // Limpa o display
        ssd1306_draw_string(&ssd,"SEMAFORO",34,3);
        ssd1306_draw_string(&ssd,"PARA",45,12);
        ssd1306_draw_string(&ssd,"PEDESTRES",32,21);
        
        if(modoNoturno && ciclo_noturno_iniciado){ //Sempre termina o ciclo e muda de modo?
            ssd1306_draw_string(&ssd,"CUIDADO!",55,41); //Tentar fazer o cuidado passando pela tela
            ssd1306_draw_pessoa_parada(&ssd,20,31);
        }else if (ciclo_normal_inicado){
            if(sinal_atual==1){
                ssd1306_draw_string(&ssd,"SIGA!",55,41);
                ssd1306_draw_pessoa_andando(&ssd,20,31);
            }else if(sinal_atual==2){
                ssd1306_draw_string(&ssd,"ATENCAO!",55,41);
                ssd1306_draw_pessoa_parada(&ssd,20,31);
            }else{
                ssd1306_draw_string(&ssd,"PARE!",55,41);
                ssd1306_draw_pessoa_parada(&ssd,20,31);
            };
        };

        ssd1306_send_data(&ssd);                           // Atualiza o display
        sleep_ms(500);
    }
}

// Trecho para modo BOOTSEL com botão B -- Adiconar outro botão para alternar os modos
#include "pico/bootrom.h"
#define BotaoB 6

void interrupcaoBotao(uint gpio, uint32_t events)
{
    BaseType_t taskYieldRequired = 0;
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if(tempo_atual-tempo_anterior>=300){
        tempo_anterior=tempo_atual;
        if(gpio==6){
            reset_usb_boot(0, 0);
        }else{
            taskYieldRequired = xTaskResumeFromISR(xHandleA);
            //Notificar task ?COMO FAZ ISSO
        };
    };  
};


PIO pio = pio0; // ou pio1, conforme o uso
uint sm = 0;    // ou outro valor, conforme sua configuração
uint sinal_amarelo=0;

void vMatrizLedsTask(){

    //Configurações para matriz de leds
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, MatrizLeds, 800000, IS_RGBW);

    //Definindo cores e tonalidades
    uint sinal_verde=0;
    
    uint sinal_vermelho=0;
    COR_RGB apagado = {0.0,0.0,0.0};

                              
    COR_RGB cores_semaforo[4] = {{0.0,0.0,0.0},{0.0,0.5,0.0},{0.5,0.5,0.0},{0.5,0.0,0.0}};

    while(true){
       if(modoNoturno && ciclo_noturno_iniciado){
            sinal_verde=0;
            sinal_vermelho=0;
            sinal_amarelo=abs(sinal_amarelo-2);
        }else if (ciclo_normal_inicado){
            switch (sinal_atual){
            case 1:
                sinal_verde=1;
                sinal_amarelo=0;
                sinal_vermelho=0;
                break;
            case 2:
                sinal_verde=0;
                sinal_amarelo=2;
                sinal_vermelho=0;
                break;
            case 3:
                sinal_verde=0;
                sinal_amarelo=0;
                sinal_vermelho=3;
                break;
            default:
                break;
            };
        }else{
            sinal_verde=0;
            sinal_amarelo=0;
            sinal_vermelho=0;
        };
        Matriz_leds semaforo = {{apagado,apagado,apagado,apagado,apagado},
                            {apagado,apagado, cores_semaforo[sinal_verde],apagado,apagado},
                            {apagado,apagado, cores_semaforo[sinal_amarelo],apagado,apagado},
                            {apagado,apagado, cores_semaforo[sinal_vermelho],apagado,apagado},
                            {apagado,apagado,apagado,apagado,apagado}};
        if(modoNoturno && ciclo_noturno_iniciado){
            acender_leds(semaforo); //stado_atual ==3 ele espera
            sleep_ms(3000);
            //sleep_ms(500);
        }else{
            acender_leds(semaforo);
            //
            sleep_ms(500);
        };
    };
};


int main()
{
    stdio_init_all();

    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(BotaoB);
    gpio_set_dir(BotaoB, GPIO_IN);
    gpio_pull_up(BotaoB);
    gpio_set_irq_enabled_with_callback(BotaoB, GPIO_IRQ_EDGE_FALL, true, &interrupcaoBotao);
    // Fim do trecho para modo BOOTSEL com botão B

    //Configurando Botão A
    gpio_init(BotaoA);
    gpio_set_dir(BotaoA, GPIO_IN);
    gpio_pull_up(BotaoA);
    gpio_set_irq_enabled_with_callback(BotaoA, GPIO_IRQ_EDGE_FALL, true, &interrupcaoBotao);

    xTaskCreate(vSemaforoRGBTask, "Task RGB", configMINIMAL_STACK_SIZE, 
        NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vBotaoATask, "Task Botao", configMINIMAL_STACK_SIZE, 
            NULL, tskIDLE_PRIORITY, &xHandleA); //Deu errado que não tava rodando com essas linhas -- acho q tava sem vtaskdelay
    xTaskCreate(vDisplay3Task, "Task Disp3", configMINIMAL_STACK_SIZE, 
       NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vBuzzerTask, "Task Buzzer", configMINIMAL_STACK_SIZE, 
        NULL, tskIDLE_PRIORITY, NULL); //Tentar depois mudar o som
    xTaskCreate(vMatrizLedsTask, "Task Matriz", configMINIMAL_STACK_SIZE, 
            NULL, tskIDLE_PRIORITY, NULL); 
    //vTaskSuspend(xHandleA);
    vTaskStartScheduler();
    panic_unsupported();

    while (true) {
        
    };
};
