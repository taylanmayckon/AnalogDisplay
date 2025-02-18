#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "libs/ssd1306.h"
#include "libs/font.h"

#define JOYSTICK_X 27
#define JOYSTICK_Y 26
#define JOYSTICK_BUTTON 22
#define BUTTON_A 5
#define LED_BLUE 12
#define LED_RED 13
#define LED_GREEN 11

#define OUTPUT_MASK ((1 << LED_BLUE) | (1 << LED_RED) | (1 << LED_GREEN))
#define INPUT_MASK ((1 << JOYSTICK_BUTTON) | (1 << BUTTON_A))

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define DISPLAY_ADDRESS 0x3C

uint blue_pwm_slice, red_pwm_slice;
uint wrap = 2047;
uint32_t last_time = 0;
uint32_t last_frame = 0;
bool pwm_state = true;
bool select_border = false;
int slide_count = 0;

ssd1306_t ssd; // Inicializa a estrutura do display no escopo global


// Função de interrupção da GPIO
void gpio_irq_handler(uint gpio, uint32_t events){
    // Obtendo tempo atual (em us)
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if(current_time - last_time > 200000){
        last_time = current_time; 

        if(gpio == BUTTON_A){
            pwm_state = !pwm_state; // Alterna o booleano do estado do pwm dos leds

            pwm_set_enabled(blue_pwm_slice, pwm_state);
            pwm_set_enabled(red_pwm_slice, pwm_state);
        }

        else if(gpio == JOYSTICK_BUTTON){
            select_border = !select_border;
            gpio_put(LED_GREEN, !gpio_get(LED_GREEN));
        }
    }
}

uint led_pwm_init(uint gpio, uint wrap){
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_clkdiv(slice_num, 125);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_enabled(slice_num, true); 
    return slice_num;
}

uint choice_pwm_side(uint joy_input){
    if(joy_input < 2048){
        return 2047-joy_input; // 2047-0 -> 0-100% (decrescente)
    }
    else{
        return joy_input-2048; // 2049-2095 -> 0-100% (crescente)
    }
}

int choice_display_y(int joy_input){
    if(joy_input < 2048){
        return (2047-joy_input)/(2047/27);
    }
    else{
        return -(joy_input-2048)/(2047/27);
    }
}

int choice_display_x(int joy_input){
    if(joy_input < 2048){
        return -(2047-joy_input)/(2047/27);
    }
    else{
        return (joy_input-2048)/(2047/27);
    }
}

void change_border(){
    bool cor = true;
    if(select_border){ // Borda grossa
        // Bordas fixas
        ssd1306_rect(&ssd, 0, 0, 3, 64, cor, cor); // Linha vertical esquerda
        ssd1306_rect(&ssd, 0, 125, 3, 64, cor, cor); // Linha vertical direita
        ssd1306_rect(&ssd, 0, 0, 128, 3, cor, cor); // Linha horizontal superior
        ssd1306_rect(&ssd, 61, 0, 128, 3, cor, cor); // Linha horizontal inferior

        // Quadrado deslizante
        // Controle do frame rate para ter 30 fps
        uint32_t current_frame = to_us_since_boot(get_absolute_time());
        if(current_frame - last_frame > 33000){
            slide_count++;
            if(slide_count>63){
                slide_count = 0;
            }
        }
        ssd1306_rect(&ssd, 3, slide_count*2, 4, 4, cor, cor); // Quadrado superior
        ssd1306_rect(&ssd, 57, 127-(slide_count*2), 4, 4, cor, cor); // Quadrado inferior
        ssd1306_rect(&ssd, 63-slide_count, 3, 4, 4, cor, cor); // Quadrado esquerdo
        ssd1306_rect(&ssd, slide_count, 121, 4, 4, cor, cor); // Quadrado direito
    }
    else{
        slide_count=0;
        ssd1306_rect(&ssd, 0, 0, 127, 63, cor, !cor); // Borda padrão
    }
}

int main(){
    stdio_init_all();

    // Inicializando o I2C
    i2c_init(I2C_PORT, 400*1000);
    // Setando as funções dos pinos do I@C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    // Garantindo o Pull up do I2C
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Configuração do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, DISPLAY_ADDRESS, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    bool cor = true; // Booleano que indica a cor branca do pixel

    adc_init(); // Inicializando o ADC
    adc_gpio_init(JOYSTICK_X); // Canal 1
    adc_gpio_init(JOYSTICK_Y); // Canal 0

    // Habilitando os LEDs
    gpio_init_mask(OUTPUT_MASK);
    gpio_set_dir_out_masked(OUTPUT_MASK);
    gpio_clr_mask(OUTPUT_MASK);

    // Iniciando o PWM com 0% de duty
    blue_pwm_slice = led_pwm_init(LED_BLUE, wrap);
    red_pwm_slice = led_pwm_init(LED_RED, wrap);

    pwm_set_gpio_level(LED_BLUE, 2047);
    pwm_set_gpio_level(LED_RED, 2047);

    // Iniciando os pinos dos botões
    gpio_init_mask(INPUT_MASK);
    // Habilitando os pull ups internos
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(JOYSTICK_BUTTON);

    // Configurando a interrupção de GPIO
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    while (true){
        // Leitura do Eixo X (Canal 1)
        adc_select_input(1);
        uint16_t vrx_value = adc_read();

        // Leitura do Eixo Y (Canal 0)
        adc_select_input(0);
        uint16_t vry_value = adc_read();

        // Ajustando o duty cycle
        pwm_set_gpio_level(LED_BLUE, choice_pwm_side(vry_value)); // Eixo Y - Azul
        pwm_set_gpio_level(LED_RED, choice_pwm_side(vrx_value)); // Eixo X - Vermelho
        
        //printf("Duty X: %d | Duty Y: %d\n", choice_pwm_side(vrx_value)*100/2047, choice_pwm_side(vry_value)*100/2047); // Print para debug

        // Limpa o display
        ssd1306_fill(&ssd, false);
        change_border(); // Atualiza o estilo da borda
        // Desenha um frame nos contornos do display
        ssd1306_draw_char(&ssd, '*', 59+choice_display_x(vrx_value), 27+choice_display_y(vry_value)); // Quadrado preenchido

        printf("Display x: %d | Display Y: %d\n", choice_display_x(vrx_value), choice_display_y(vry_value));

        ssd1306_send_data(&ssd); // Atualiza o display

        sleep_ms(1); 
    }
}
