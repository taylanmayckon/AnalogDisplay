#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

#define JOYSTICK_X 27
#define JOYSTICK_Y 26
#define LED_BLUE 12
#define LED_RED 13

uint wrap = 2047;

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

int main(){
    stdio_init_all();

    adc_init(); // Inicializando o ADC
    adc_gpio_init(JOYSTICK_X); // Canal 1
    adc_gpio_init(JOYSTICK_Y); // Canal 0

    // Habilitando os LEDs
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_put(LED_BLUE, 0);
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_put(LED_RED, 0);

    // Iniciando o PWM com 0% de duty
    uint blue_pwm_slice = led_pwm_init(LED_BLUE, wrap);
    uint red_pwm_slice = led_pwm_init(LED_RED, wrap);

    pwm_set_gpio_level(LED_BLUE, 2047);
    pwm_set_gpio_level(LED_RED, 2047);

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
        
        printf("Duty X: %d | Duty Y: %d\n", choice_pwm_side(vrx_value)*100/2047, choice_pwm_side(vry_value)*100/2047); // Print para debug

        sleep_ms(10);
    }
}
