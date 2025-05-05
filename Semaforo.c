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
#include "hardware/clocks.h"

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

//TaskHandle_t xHandleA; //Suspender e ativar a task
//BaseType_t xYieldRequired;
void vBotaoATask(){
    
    //vTaskSuspend(); -- Suspender nates do scheduler
    //ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    //modoNoturno=!modoNoturno;
    //vTaskDelay(pdMS_TO_TICKS(200));
    //vTaskSuspend(NULL);

};

uint sinal_atual=0;

void vSemaforoRGBTask(){

    //Inicializando pinos
    gpio_init(led_Green);
    gpio_init(led_Red);
    gpio_set_dir(led_Green, GPIO_OUT);
    gpio_set_dir(led_Red, GPIO_OUT);

    while(true){

        if(!modoNoturno){
            //Modo normal 

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
            //Modo noturno
            gpio_put(led_Green,1);
            gpio_put(led_Red,1);
            vTaskDelay(pdMS_TO_TICKS(5000));
            gpio_put(led_Green,0);
            gpio_put(led_Red,0);
            vTaskDelay(pdMS_TO_TICKS(2000));

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
        if(modoNoturno){

        }else{
            if(sinal_atual==1){
                pwm_set_gpio_level(Buzzer, 32768);
                vTaskDelay(pdMS_TO_TICKS(1000));
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(14500));
            }else if(sinal_atual==2){
                pwm_set_gpio_level(Buzzer, 32768);
                vTaskDelay(pdMS_TO_TICKS(500));
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(300));
            }else if(sinal_atual==3){
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                pwm_set_gpio_level(Buzzer, 32768);
                vTaskDelay(pdMS_TO_TICKS(1500));
            };
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
        
        if(modoNoturno){ //Sempre termina o ciclo e muda de modo?
            ssd1306_draw_string(&ssd,"CUIDADO!",55,41);
            ssd1306_draw_pessoa_parada(&ssd,20,31);
        }else{
            if(sinal_atual==1){
                ssd1306_draw_string(&ssd,"SIGA!",55,41);
                ssd1306_draw_pessoa_andando(&ssd,20,31);
            }else{
                ssd1306_draw_string(&ssd,"CUIDADO!",55,41);
                ssd1306_draw_pessoa_parada(&ssd,20,31);
            };
        };
        //Design

        ssd1306_send_data(&ssd);                           // Atualiza o display
        sleep_ms(735);
    }
}

// Trecho para modo BOOTSEL com botão B -- Adiconar outro botão para alternar os modos
#include "pico/bootrom.h"
#define BotaoB 6
void interrupcaoBotao(uint gpio, uint32_t events)
{
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if(tempo_atual-tempo_anterior>=300){
        tempo_anterior=tempo_atual;
        if(gpio==6){
            reset_usb_boot(0, 0);
        }else{
            
            //Notificar task ?COMO FAZ ISSO?

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

    while (true) {
       
        xTaskCreate(vSemaforoRGBTask, "Task RGB", configMINIMAL_STACK_SIZE, 
            NULL, tskIDLE_PRIORITY, NULL);
        //xTaskCreate(vBotaoATask, "Task Botao", configMINIMAL_STACK_SIZE, 
                //NULL, tskIDLE_PRIORITY+2, &xHandleA);
        xTaskCreate(vDisplay3Task, "Task Disp3", configMINIMAL_STACK_SIZE, 
           NULL, tskIDLE_PRIORITY, NULL);
        xTaskCreate(vBuzzerTask, "Task Buzzer", configMINIMAL_STACK_SIZE, 
            NULL, tskIDLE_PRIORITY, NULL);
        //vTaskSuspend(xHandleA);
        vTaskStartScheduler();
        panic_unsupported();
        
        printf("modo: %d",modoNoturno);
        printf("--");
    }
}
