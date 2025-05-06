//Código desenvolvido por Andressa Sousa Fonseca

/*
*O presente projeto simula um sistema de semaforos, com sinalização do ciclo de cores
*nos lEDS RGB e na Matriz de Leds. Além de sinalização sonoro para alertar deficientes visuais.
*E no display há a representação de um semaforo para pedestres.
*/

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

#define I2C_PORT i2c1 //Comucação I2C
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define led_Green 11    //Defininco os pinos para o semaforo
//#define led_Blue 12
#define led_Red 13

bool modoNoturno = false;   //Variável global para alternar entre os modos
#define BotaoA 5            //Botão que será usado na interrupção
static volatile uint32_t tempo_anterior = 0;    //Controle de deboucing

bool ciclo_normal_inicado = false;      //Variáveis para sincronizar os sistema com base no ciclo dos leds RGB
bool ciclo_noturno_iniciado = false;

TaskHandle_t xHandleA; //Suspender e ativar a task

uint sinal_atual=0; //Variável para alternar as cores e sinalizações na matriz, no buzzer e no display

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
            ciclo_normal_inicado = true;        //Muda o estado indicando que o ciclo normal foi iniciado
            ciclo_noturno_iniciado = false;     //para sincronizar os sistema
            gpio_put(led_Green,1);              //Liga o sinal Verde
            sinal_atual=1;                      //Atualiza o estado do sistema
            vTaskDelay(pdMS_TO_TICKS(15000));   //Tempo do sinal verde de 15s
            gpio_put(led_Green,0);              //Desliga o sinal verde
            vTaskDelay(pdMS_TO_TICKS(500));     //Pausa para mudar entre os sinais
            gpio_put(led_Green,1);              
            gpio_put(led_Red,1);                //Liga o sinal amarelo (verde e vermelho)
            sinal_atual=2;                      
            vTaskDelay(pdMS_TO_TICKS(5000));    //Tempo do sinal amarelo de 5s
            gpio_put(led_Green,0);
            gpio_put(led_Red,0);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_put(led_Red,1);                //Liga o sinal vermelho
            sinal_atual = 3;
            vTaskDelay(pdMS_TO_TICKS(30000));   //Tempo do sinal Vermelho de 30s
            gpio_put(led_Red,0);                
            vTaskDelay(pdMS_TO_TICKS(500));     
        }else{
            ciclo_normal_inicado = false;       
            ciclo_noturno_iniciado = true;      //Indica que o ciclo noturno iniciou para sincronização
            //Modo noturno
            gpio_put(led_Green,1);              //Liga o sinal amarelo
            gpio_put(led_Red,1);
            vTaskDelay(pdMS_TO_TICKS(3000));    //Fica ligado por 3s
            gpio_put(led_Green,0);
            gpio_put(led_Red,0);
            vTaskDelay(pdMS_TO_TICKS(3000));    //Fica desligado por 3s

        };

    };

};

#define Buzzer 21       //Pino do Buzzer
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
        if(modoNoturno && ciclo_noturno_iniciado){      //Verifica se o modo noturno iniciou e se o ciclo dos LEDS também
            pwm_set_gpio_level(Buzzer, 32768);          //para sincronização
            vTaskDelay(pdMS_TO_TICKS(700));             //bit lento de 0,7s
            pwm_set_gpio_level(Buzzer, 0);              
            vTaskDelay(pdMS_TO_TICKS(2000));            //pausa de 2s
        }else if (ciclo_normal_inicado){                //verifica para sincronização
            if(sinal_atual==1){
                pwm_set_gpio_level(Buzzer, 32768);      //Como é o led verde que está ligado no momento
                vTaskDelay(pdMS_TO_TICKS(1000));        // 1 bip longe de 1s
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(14500));
            }else if(sinal_atual==2){
                pwm_set_gpio_level(Buzzer, 32768);      //Como é o led amarelo que está ligado no momento
                vTaskDelay(pdMS_TO_TICKS(275));         //bip curto e rápido intermitente
                pwm_set_gpio_level(Buzzer, 0);
                vTaskDelay(pdMS_TO_TICKS(275));
            }else if(sinal_atual==3){
                pwm_set_gpio_level(Buzzer, 32768);      //Como é o led vermelho que está ligado no momento
                vTaskDelay(pdMS_TO_TICKS(500));         //bip de 0,2s desligado e 1,5s ligado
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

    bool cor = true;
    while (true) //Modificar todo o design
    {
        
        ssd1306_fill(&ssd, !cor);                          // Limpa o display
        ssd1306_draw_string(&ssd,"SEMAFORO",34,3);          //Mensagem sobre o modelo do semaforo
        ssd1306_draw_string(&ssd,"PARA",47,12);
        ssd1306_draw_string(&ssd,"PEDESTRES",32,21);
        
        if(modoNoturno && ciclo_noturno_iniciado){          //Em modo noturno, a mensagem é "CUIDADO!"
            ssd1306_draw_string(&ssd,"CUIDADO!",55,41);     //e imagem de um bonequinho parado
            ssd1306_draw_pessoa_parada(&ssd,20,31);
        }else if (ciclo_normal_inicado){                    //No modo normal, a mensagem alterna de acoro com o sinal
            if(sinal_atual==1){                             //e a imagem também
                ssd1306_draw_string(&ssd,"PARE!",55,41);
                ssd1306_draw_pessoa_parada(&ssd,20,31);    //sinal verde, bonequinho parado
            }else if(sinal_atual==2){
                ssd1306_draw_string(&ssd,"ATENCAO!",55,41); 
                ssd1306_draw_pessoa_parada(&ssd,20,31);     //sinal amarelo, bonequinho parado
            }else{
                ssd1306_draw_string(&ssd,"SIGA!",55,41);
                ssd1306_draw_pessoa_andando(&ssd,20,31);     //sinal vermelho, bonequinho andando
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
            taskYieldRequired = xTaskResumeFromISR(xHandleA);  //Reativa a task do botão
        };
    };  
};

void vBotaoATask(){
    
    //Configurando Botão A
    gpio_init(BotaoA);
    gpio_set_dir(BotaoA, GPIO_IN);
    gpio_pull_up(BotaoA);
    gpio_set_irq_enabled_with_callback(BotaoA, GPIO_IRQ_EDGE_FALL, true, &interrupcaoBotao);

    vTaskSuspend(NULL);         //Suspende a task
    while(true){
        printf("Modo noturno: %d\n",modoNoturno); 
        modoNoturno=!modoNoturno;                   //Alterna o modo
        vTaskSuspend(xHandleA);                     //Suspende a task novamente
    };
};

PIO pio = pio0; 
uint sm = 0;    
uint sinal_amarelo=0;

void vMatrizLedsTask(){

    //Configurações para matriz de leds
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, MatrizLeds, 800000, IS_RGBW);

    //Definindo cores e tonalidades
    uint sinal_verde=0;
    
    uint sinal_vermelho=0;
    COR_RGB apagado = {0.0,0.0,0.0};

    //Vetor de cores para o semaforo: APAGADO - VERDE - AMARELO - VERMELHO
    COR_RGB cores_semaforo[4] = {{0.0,0.0,0.0},{0.0,0.5,0.0},{0.5,0.5,0.0},{0.5,0.0,0.0}};

    while(true){
       if(modoNoturno && ciclo_noturno_iniciado){       
            sinal_verde=0;                              //Define as valores certos para o modo noturno
            sinal_vermelho=0;
            sinal_amarelo=abs(sinal_amarelo-2);         //Permite piscar o led amarelo no modo noturno
        }else if (ciclo_normal_inicado){
            switch (sinal_atual){                       //Modifica as variáveis de acordo com o sinal_atual
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

        //Matriz com o padrão a ser exibido é atualizada com os valores definidos acima
        Matriz_leds semaforo = {{apagado,apagado,apagado,apagado,apagado},
                            {apagado,apagado, cores_semaforo[sinal_verde],apagado,apagado},
                            {apagado,apagado, cores_semaforo[sinal_amarelo],apagado,apagado},
                            {apagado,apagado, cores_semaforo[sinal_vermelho],apagado,apagado},
                            {apagado,apagado,apagado,apagado,apagado}};
        
        //Verifica o modo para exibir a matriz com o intervalo certo
        if(modoNoturno && ciclo_noturno_iniciado){
            acender_leds(semaforo); 
            sleep_ms(3000);
        }else{
            acender_leds(semaforo);
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

    //Task criadas - Todas com a mesma prioridade, pois a sincronização é feita com variáveis globais
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
    vTaskStartScheduler();
    panic_unsupported();

    while (true) {
        
    };
};
