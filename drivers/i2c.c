/* Bit-banged I2C bus of the D168UV: SDA PB6, SCL PB7 (stock FUN_0803a738 start, FUN_0803a548 stop, FUN_0803a6bc
 * byte write, FUN_0803a5d4 byte read). Like stock, both lines are push-pull outputs; SDA is switched to an input
 * with pull-up (FUN_0808dfdc) to read data and ACK bits. Devices: FM broadcast receiver 0x10, accelerometer 0x19
 * (LIS2DW12, hal/accel.c). Delays: stock 2-4 us between edges. */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

#define I2C_PORT GPIOB
#define SDA 6
#define SCL 7

static bool s_init;
static void dly(void) { delay_us(4); }
static void sda_out(void) { gpio_mode(I2C_PORT, SDA, GPIO_OUT, PULL_NONE); }
static void sda_in(void) { gpio_set(I2C_PORT, SDA); gpio_mode(I2C_PORT, SDA, GPIO_IN, PULL_UP); }

static void init(void)
{
    if(s_init) { return; }
    gpio_set(I2C_PORT, SDA); gpio_set(I2C_PORT, SCL);
    sda_out();
    gpio_mode(I2C_PORT, SCL, GPIO_OUT, PULL_NONE);
    s_init = true;
}

static void start(void)
{
    sda_out();
    gpio_set(I2C_PORT, SDA); dly(); gpio_set(I2C_PORT, SCL); dly();
    gpio_clr(I2C_PORT, SDA); dly(); gpio_clr(I2C_PORT, SCL); dly();
}

static void stop(void)
{
    sda_out();
    gpio_clr(I2C_PORT, SDA); dly(); gpio_set(I2C_PORT, SCL); dly(); gpio_set(I2C_PORT, SDA); dly();
}

static bool put(uint8_t b)                       /* true = ACK */
{
    sda_out();
    for(int i = 7; i >= 0; i--) {
        gpio_write(I2C_PORT, SDA, (b >> i) & 1); dly();
        gpio_set(I2C_PORT, SCL); dly(); gpio_clr(I2C_PORT, SCL);
    }
    sda_in(); dly();
    gpio_set(I2C_PORT, SCL); dly();
    bool ack = !gpio_read(I2C_PORT, SDA);
    gpio_clr(I2C_PORT, SCL); dly();
    return ack;
}

static uint8_t get(bool ack)
{
    uint8_t v = 0;
    sda_in(); dly();
    for(int i = 0; i < 8; i++) {
        dly(); gpio_set(I2C_PORT, SCL); dly();
        v = (uint8_t)(v << 1 | (gpio_read(I2C_PORT, SDA) ? 1 : 0));
        gpio_clr(I2C_PORT, SCL);
    }
    sda_out();
    gpio_write(I2C_PORT, SDA, !ack); dly();
    gpio_set(I2C_PORT, SCL); dly(); gpio_clr(I2C_PORT, SCL); dly();
    return v;
}

bool i2c_reg_write(uint8_t a7, uint8_t reg, uint8_t v)
{
    init();
    start();
    bool ok = put((uint8_t)(a7 << 1)) && put(reg) && put(v);
    stop();
    return ok;
}

bool i2c_reg_read(uint8_t a7, uint8_t reg, uint8_t *v)
{
    init();
    start();
    bool ok = put((uint8_t)(a7 << 1)) && put(reg);
    if(ok) { start(); ok = put((uint8_t)(a7 << 1 | 1)); }
    if(ok) { *v = get(false); }
    stop();
    return ok;
}
