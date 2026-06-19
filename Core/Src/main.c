#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ─── RCC Registers ────────────────────────────────────────────
#define RCC_AHB1ENR  (*(volatile uint32_t*)0x40023830)
#define RCC_APB1ENR  (*(volatile uint32_t*)0x40023840)

#define GPIOA_EN     (1U << 0)
#define TIM2_EN      (1U << 0)
#define USART2_EN    (1U << 17)

// ─── GPIOA Registers ──────────────────────────────────────────
#define GPIOA_MODER   (*(volatile uint32_t*)0x40020000)
#define GPIOA_OSPEEDR (*(volatile uint32_t*)0x40020008)
#define GPIOA_AFRL    (*(volatile uint32_t*)0x40020020)
#define GPIOA_ODR     (*(volatile uint32_t*)0x40020014)
#define GPIOA_BSRR    (*(volatile uint32_t*)0x40020018)

// ─── TIM2 Registers ───────────────────────────────────────────
#define TIM2_CR1   (*(volatile uint32_t*)0x40000000)
#define TIM2_EGR   (*(volatile uint32_t*)0x40000014)
#define TIM2_CCMR1 (*(volatile uint32_t*)0x40000018)
#define TIM2_CCER  (*(volatile uint32_t*)0x40000020)
#define TIM2_PSC   (*(volatile uint32_t*)0x40000028)
#define TIM2_ARR   (*(volatile uint32_t*)0x4000002C)
#define TIM2_CCR1  (*(volatile uint32_t*)0x40000034)

// ─── USART2 Registers ─────────────────────────────────────────
#define USART2_SR  (*(volatile uint32_t*)0x40004400)
#define USART2_DR  (*(volatile uint32_t*)0x40004404)
#define USART2_BRR (*(volatile uint32_t*)0x40004408)
#define USART2_CR1 (*(volatile uint32_t*)0x4000440C)

// ─── Command Buffer ───────────────────────────────────────────
#define CMD_BUFFER_SIZE 32
char     cmd_buffer[CMD_BUFFER_SIZE];
uint8_t  cmd_index = 0;
uint8_t  cmd_ready = 0;

// ─── RCC Init ─────────────────────────────────────────────────
void rcc_init(void)
{
    RCC_AHB1ENR |= GPIOA_EN;
    RCC_APB1ENR |= TIM2_EN;
    RCC_APB1ENR |= USART2_EN;
}

// ─── GPIO Init ────────────────────────────────────────────────
void gpio_init(void)
{
    // PA0 → AF (TIM2 PWM)
    GPIOA_MODER &= ~(3U << 0);
    GPIOA_MODER |=  (2U << 0);

    // PA1 → Output (IN2)
    GPIOA_MODER &= ~(3U << 2);
    GPIOA_MODER |=  (1U << 2);

    // PA2 → AF (USART2 TX)
    GPIOA_MODER &= ~(3U << 4);
    GPIOA_MODER |=  (2U << 4);

    // PA3 → AF (USART2 RX)
    GPIOA_MODER &= ~(3U << 6);
    GPIOA_MODER |=  (2U << 6);

    // PA4 → Output (IN1)
    GPIOA_MODER &= ~(3U << 8);
    GPIOA_MODER |=  (1U << 8);

    // High speed for PA0, PA2, PA3
    GPIOA_OSPEEDR |= (3U << 0);
    GPIOA_OSPEEDR |= (3U << 4);
    GPIOA_OSPEEDR |= (3U << 6);

    // PA0 → AF1 (TIM2)
    GPIOA_AFRL &= ~(0xFU << 0);
    GPIOA_AFRL |=  (1U << 0);

    // PA2 → AF7 (USART2 TX)
    GPIOA_AFRL &= ~(0xFU << 8);
    GPIOA_AFRL |=  (7U << 8);

    // PA3 → AF7 (USART2 RX)
    GPIOA_AFRL &= ~(0xFU << 12);
    GPIOA_AFRL |=  (7U << 12);
}

// ─── TIM2 PWM Init ────────────────────────────────────────────
void tim2_pwm_init(void)
{
    TIM2_PSC   = 15;
    TIM2_ARR   = 999;
    TIM2_EGR  |= (1U << 0);

    TIM2_CCMR1 &= ~(7U << 4);
    TIM2_CCMR1 |=  (6U << 4);
    TIM2_CCMR1 |=  (1U << 3);

    TIM2_CCER |= (1U << 0);
    TIM2_CCR1  = 0;
    TIM2_CR1  |= (1U << 0);
}

// ─── Motor Control ────────────────────────────────────────────
void motor_set_speed(uint32_t percent)
{
    if(percent > 100) percent = 100;
    TIM2_CCR1 = (percent * 999) / 100;
}

void motor_forward(void)
{
    GPIOA_BSRR = (1U << 4);    // PA4 HIGH
    GPIOA_BSRR = (1U << 17);   // PA1 LOW
}

void motor_reverse(void)
{
    GPIOA_BSRR = (1U << 20);   // PA4 LOW
    GPIOA_BSRR = (1U << 1);    // PA1 HIGH
}

void motor_stop(void)
{
    motor_set_speed(0);
}

// ─── USART2 Init ──────────────────────────────────────────────
void usart2_init(void)
{
    USART2_BRR  = 1667;
    USART2_CR1 |= (1U << 3);
    USART2_CR1 |= (1U << 2);
    USART2_CR1 |= (1U << 13);
}

void usart2_send_char(char c)
{
    while(!(USART2_SR & (1U << 7)));
    USART2_DR = c;
}

void usart2_send_string(const char *str)
{
    while(*str)
    {
        usart2_send_char(*str);
        str++;
    }
}

uint8_t usart2_data_ready(void)
{
    return (USART2_SR & (1U << 5)) ? 1 : 0;
}

char usart2_read_char(void)
{
    return (char)USART2_DR;
}

// ─── UART Receive Handler ─────────────────────────────────────
void uart_receive_handler(void)
{
    if(!usart2_data_ready()) return;

    char c = usart2_read_char();

    if(c == '\r') return;

    if(c == '\n')
    {
        cmd_buffer[cmd_index] = '\0';
        cmd_ready = 1;
        cmd_index = 0;
        return;
    }

    if(cmd_index >= CMD_BUFFER_SIZE - 1)
    {
        cmd_index = 0;
        return;
    }

    cmd_buffer[cmd_index] = c;
    cmd_index++;
}

// ─── Command Parser ───────────────────────────────────────────
void parse_command(void)
{
    if(!cmd_ready) return;
    cmd_ready = 0;

    if(strcmp(cmd_buffer, "STOP") == 0)
    {
        motor_stop();
        usart2_send_string("Motor stopped\r\n");
    }
    else if(strcmp(cmd_buffer, "FORWARD") == 0)
    {
        motor_forward();
        usart2_send_string("Direction: FORWARD\r\n");
    }
    else if(strcmp(cmd_buffer, "REVERSE") == 0)
    {
        motor_reverse();
        usart2_send_string("Direction: REVERSE\r\n");
    }
    else
    {
        int speed_val = 0;
        if(sscanf(cmd_buffer, "SPEED %d", &speed_val) == 1)
        {
            motor_set_speed((uint32_t)speed_val);
            usart2_send_string("Speed updated\r\n");
        }
        else
        {
            usart2_send_string("Unknown command\r\n");
        }
    }
}

// ─── Main ─────────────────────────────────────────────────────
int main(void)
{
    rcc_init();
    gpio_init();
    tim2_pwm_init();
    usart2_init();

    usart2_send_string("Motor Controller Ready\r\n");

    while(1)
    {
        uart_receive_handler();
        parse_command();
    }
}
