// Copyright 2023 Persama (@Persama)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "ansi.h"
#include "side_table.h"

#define SIDE_WAVE_1         0  
#define SIDE_WAVE_2         1
#define SIDE_MIX            2  
#define SIDE_STATIC         3  
#define SIDE_BREATH         4  
#define SIDE_OFF            5  
#define LIGHT_COLOUR_MAX    8 
#define SIDE_COLOUR_MAX     8 
#define LIGHT_SPEED_MAX     4 
#define RF_LED_LINK_PERIOD  500
#define RF_LED_PAIR_PERIOD  250
#define SIDE_LINE           5  
#define CHARGING_SHIFT      0
#define RFLINK_SHIFT        0
#define CHARGING_BREATHE    1
#define RFLINK_BLINK        1
#define LOW_BAT_BLINK_PRIOD 500
#define SIDE_LED_NUM        12

const uint8_t side_speed_table[6][5] = {
    [SIDE_WAVE_1] = {24, 30, 36, 42, 50},
    [SIDE_WAVE_2] = {24, 30, 36, 42, 50},
    [SIDE_MIX]    = {14, 20, 28, 36, 50},
    [SIDE_STATIC] = {50, 50, 50, 50, 50},
    [SIDE_BREATH] = {14, 20, 28, 36, 50},
    [SIDE_OFF]    = {50, 50, 50, 50, 50},  
};

const uint8_t side_light_table[6] = {
    0,
    22,
    34,
    55,
    79,
    106,
};

const uint8_t side_led_index_tab[SIDE_LINE] = {
    0,
    1,
    2,
    3,
    4,
};


bool f_charging = 1;

uint8_t side_mode           = 3;  
uint8_t side_light          = 3; 
uint8_t side_speed          = 2;
uint8_t side_rgb            = 0;  
uint8_t side_colour         = 1;  
uint8_t side_play_point     = 0; 
uint8_t low_bat_blink_cnt   = 6;  
uint8_t side_play_cnt       = 0; 
uint32_t side_play_timer    = 0;

uint8_t r_temp, g_temp, b_temp;  
extern DEV_INFO_STRUCT dev_info;
extern bool         f_bat_hold;
extern bool         f_sleep_show;
extern bool         f_dial_sw_init_ok;
extern uint8_t      logo_mode;       
extern uint8_t      logo_light;      
extern uint8_t      logo_speed;      
extern uint8_t      logo_rgb;        
extern uint8_t      logo_colour;      
extern uint8_t      logo_play_point;  
extern uint8_t      rf_blink_cnt;
extern uint16_t     rf_link_show_time;
extern uint8_t      logo_play_cnt; 
extern uint32_t     logo_play_timer;

rgb_led_t side_leds[SIDE_LED_NUM] = {0};
void side_ws2812_setleds(rgb_led_t *ledarray, uint16_t leds);
void rgb_matrix_update_pwm_buffers(void);
void m_logo_led_show(void);

/**
 * @brief  side leds set color vaule.
 * @param  index: index of side_leds[].
 * @param  ...
 */
void side_rgb_set_color(int index, uint8_t red, uint8_t green, uint8_t blue)
{
    side_leds[index].r = red;
    side_leds[index].g = green;
    side_leds[index].b = blue;
}

/**
 * @brief  refresh side leds.
 */
void side_rgb_refresh(void)
{
    side_ws2812_setleds(side_leds, SIDE_LED_NUM);
}


/**
 * @brief  Adjusting the brightness of side lights.
 * @param  dir: 0 - decrease, 1 - increase.
 * @note  save to eeprom.
 */
void light_level_control(uint8_t brighten)
{
    if (brighten) 
    {
        if (side_light == 5) {
            return;
        } else
            side_light++;
    } else 
    {
        if (side_light == 0) {
            return;
        } else
            side_light--;
    }
    user_config.ee_side_light = side_light;
    eeconfig_update_user_datablock(&user_config);
}

/**
 * @brief  Adjusting the speed of side lights.
 * @param  dir: 0 - decrease, 1 - increase.
 * @note  save to eeprom.
 */
void light_speed_control(uint8_t fast)
{
    if ((side_speed) > LIGHT_SPEED_MAX)
        (side_speed) = LIGHT_SPEED_MAX / 2;

    if (fast) {
        if ((side_speed)) side_speed--; 
    } else {
        if ((side_speed) < LIGHT_SPEED_MAX) side_speed++;
    }
    user_config.ee_side_speed = side_speed;
    eeconfig_update_user_datablock(&user_config);  
}

/**
 * @brief  Switch to the next color of side lights.
 * @param  dir: 0 - prev, 1 - next.
 * @note  save to eeprom.
 */
void side_colour_control(uint8_t dir)
{
    if ((side_mode != SIDE_WAVE_1)&&(side_mode != SIDE_WAVE_2)){
        if (side_rgb) {
            side_rgb    = 0;
            side_colour = 0;
        }
    }

    if (dir) { 
        if (side_rgb) {
            side_rgb    = 0;
            side_colour = 0;
        } else {
            side_colour++;
            if (side_colour >= LIGHT_COLOUR_MAX) {
                side_rgb    = 1;
                side_colour = 0;
            }
        }
    } else { 
        if (side_rgb) {
            side_rgb    = 0;
            side_colour = LIGHT_COLOUR_MAX - 1;
        } else {
            side_colour--;
            if (side_colour >= LIGHT_COLOUR_MAX) {
                side_rgb    = 1;
                side_colour = 0;
            }
        }
    }
    user_config.ee_side_rgb    = side_rgb;
    user_config.ee_side_colour = side_colour;
    eeconfig_update_user_datablock(&user_config);  
}

/**
 * @brief  Switch to a specific color of side lights.
 * @param  col: 0 - 7.
 * @note  save to eeprom.
 */
void side_colour_set(uint8_t col)
{
    side_mode = SIDE_STATIC;
    side_rgb = 0;
    side_colour = col;

    user_config.ee_side_mode = side_mode;
    user_config.ee_side_rgb = side_rgb;
    user_config.ee_side_colour = side_colour;
    eeconfig_update_user_datablock(&user_config);
}

/**
 * @brief  Change the color mode of side lights.
 * @param  dir: 0 - prev, 1 - next.
 * @note  save to eeprom.
 */
void side_mode_control(uint8_t dir)
{
    if (dir) { 
        side_mode++;
        if (side_mode > SIDE_OFF) {
            side_mode = 0;
        }
    } else {
        if (side_mode > 0) {
            side_mode--;
        } else {
            side_mode = SIDE_OFF;
        }
    }
    side_play_point          = 0;
    user_config.ee_side_mode = side_mode;
    eeconfig_update_user_datablock(&user_config); 
}

/**
 * @brief  set left side leds.
 * @param  ...
 */
void set_side_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < SIDE_LINE; i++)
        side_rgb_set_color(i, r >> 2, g >> 2, b >> 2);
}

/**
 * @brief  set left side leds.
 */
void sys_sw_led_show(void)
{
    static uint32_t sys_show_timer = 0;
    static bool sys_show_flag      = false;
    extern bool f_sys_show;

    if (f_sys_show) {
        f_sys_show     = false;
        sys_show_timer = timer_read32();  
        sys_show_flag  = true;
    }

    if (sys_show_flag) {
        if (dev_info.sys_sw_state == SYS_SW_MAC) {
            r_temp = 0x80;
            g_temp = 0x80;
            b_temp = 0x80;
        } else {
            r_temp = 0x00;
            g_temp = 0x00;
            b_temp = 0x80;
        }
        if ((timer_elapsed32(sys_show_timer) / 500) % 2 == 0) {
            set_side_rgb(r_temp, g_temp, b_temp);
        } else {
            set_side_rgb(0x00, 0x00, 0x00);
        }
        if (timer_elapsed32(sys_show_timer) >= (3000-50)) {
            #if(WORK_MODE == USB_MODE)
            if(timer_elapsed32(sys_show_timer) <= 4000)  set_side_rgb(r_temp, g_temp, b_temp);
            else sys_show_flag = false;
            #else
            sys_show_flag = false;
            #endif
        }
    }
}

/**
 * @brief  sleep_sw_led_show.
 */
void sleep_sw_led_show(void)
{
    static uint32_t sleep_show_timer = 0;
    static bool sleep_show_flag      = false;
  
    if (f_sleep_show) {
        f_sleep_show     = false;
        sleep_show_timer = timer_read32();  
        sleep_show_flag  = true;
    }

    if (sleep_show_flag) {
        if (f_dev_sleep_enable) {
            r_temp = 0x00;
            g_temp = 0x80;
            b_temp = 0x00;
        } else {
            r_temp = 0x80;
            g_temp = 0x00;
            b_temp = 0x00;
        }
        if ((timer_elapsed32(sleep_show_timer) / 500) % 2 == 0) {
            set_side_rgb(r_temp, g_temp, b_temp);
        } else {
            set_side_rgb(0x00, 0x00, 0x00);
        }
        if (timer_elapsed32(sleep_show_timer) >= (3000-50)) {
            sleep_show_flag = false;
        }
    }
}

/**
 * @brief  sys_led_show.
 */
void sys_led_show(void) {
    if (dev_info.link_mode == LINK_USB) {
        if (host_keyboard_led_state().caps_lock) {
            set_side_rgb(0X00, 0x80, 0x80);
        }
    }

    else {
        if (dev_info.rf_led & 0x02) {
            set_side_rgb(0X00, 0x80, 0x80);
        }
    }
}

/**
 * @brief  light_point_playing.
 * @param trend:
 * @param step:
 * @param len:
 * @param point:
 */
static void light_point_playing(uint8_t trend, uint8_t step, uint8_t len, uint8_t *point)
{
    if (trend) {
        *point += step;
        if (*point >= len) *point -= len;
    } else {
        *point -= step;
        if (*point >= len) *point = len - (255 - *point) - 1;
    }
}

/**
 * @brief  count_rgb_light.
 * @param light_temp:
 */
static void count_rgb_light(uint8_t light_temp) {
    uint16_t temp;

    temp   = (light_temp)*r_temp + r_temp;
    r_temp = temp >> 8;

    temp   = (light_temp)*g_temp + g_temp;
    g_temp = temp >> 8;

    temp   = (light_temp)*b_temp + b_temp;
    b_temp = temp >> 8;
}

/**
 * @brief  side_wave_mode_show.
 */
static void side_wave_mode_show_1(void)
{
    uint8_t play_index;

    //------------------------------
    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    //------------------------------
    if (side_rgb)
        light_point_playing(0, 1, FLOW_COLOUR_TAB_LEN, &side_play_point);
    else
        light_point_playing(0, 2, WAVE_TAB_LEN, &side_play_point);

    play_index = side_play_point;
    for (int i = 0; i < SIDE_LINE; i++) {
        if (side_rgb) {
            r_temp = flow_rainbow_colour_tab[play_index][0];
            g_temp = flow_rainbow_colour_tab[play_index][1];
            b_temp = flow_rainbow_colour_tab[play_index][2];

            light_point_playing(1, 8, FLOW_COLOUR_TAB_LEN, &play_index);
        } else {
            r_temp = colour_lib[side_colour][0];
            g_temp = colour_lib[side_colour][1];
            b_temp = colour_lib[side_colour][2];

            light_point_playing(1, 12, WAVE_TAB_LEN, &play_index);
            count_rgb_light(wave_data_tab[play_index]);
        }

        count_rgb_light(side_light_table[side_light]);

        side_rgb_set_color(side_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
        
    }
}
static void side_wave_mode_show_2(void)
{
    uint8_t play_index;
    static uint8_t breathe = 0;
    uint8_t breath_temp;

    //------------------------------
    if (side_play_cnt <= (side_speed_table[side_mode][side_speed]))
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    //------------------------------
    if (side_rgb) {
        light_point_playing(0, 1, FLOW_COLOUR_TAB_LEN, &side_play_point);
        light_point_playing(0, 1, SIDE_WAVE_TAB_LEN, &breathe);
    }
    else
        light_point_playing(0, 1, SIDE_WAVE_TAB_LEN, &side_play_point);

    breath_temp =  breathe;
    play_index = side_play_point;
    for (int i = 0; i < SIDE_LINE; i++) {
        if (side_rgb) {
            r_temp = flow_rainbow_colour_tab[play_index][0];
            g_temp = flow_rainbow_colour_tab[play_index][1];
            b_temp = flow_rainbow_colour_tab[play_index][2];

            light_point_playing(1, 16, FLOW_COLOUR_TAB_LEN, &play_index);

            light_point_playing(1, 32, SIDE_WAVE_TAB_LEN, &breath_temp);
            count_rgb_light(side_wave_data_tab[breath_temp]);

        } else {
            r_temp = colour_lib[side_colour][0];
            g_temp = colour_lib[side_colour][1];
            b_temp = colour_lib[side_colour][2];

            light_point_playing(1, 24, SIDE_WAVE_TAB_LEN, &play_index);
            count_rgb_light(side_wave_data_tab[play_index]);
        }

        count_rgb_light(side_light_table[side_light]);
        side_rgb_set_color(side_led_index_tab[i], r_temp, g_temp, b_temp);
    }
}

/**
 * @brief  side_spectrum_mode_show.
 */
static void side_spectrum_mode_show(void)
{
    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    light_point_playing(1, 1, FLOW_COLOUR_TAB_LEN, &side_play_point);

    r_temp = flow_rainbow_colour_tab[side_play_point][0];
    g_temp = flow_rainbow_colour_tab[side_play_point][1];
    b_temp = flow_rainbow_colour_tab[side_play_point][2];

    count_rgb_light(side_light_table[side_light]);

    for (int i = 0; i < SIDE_LINE; i++) {
        side_rgb_set_color(side_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}

/**
 * @brief  side_breathe_mode_show.
 */
static void side_breathe_mode_show(void)
{
    static uint8_t play_point = 0;

    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    light_point_playing(0, 1, BREATHE_TAB_LEN, &play_point);

    if (0) {
        if (play_point == 0) {
            if (++side_play_point >= LIGHT_COLOUR_MAX)
                side_play_point = 0;
        }

        r_temp = colour_lib[side_play_point][0];
        g_temp = colour_lib[side_play_point][1];
        b_temp = colour_lib[side_play_point][2];
    } else {
        r_temp = colour_lib[side_colour][0];
        g_temp = colour_lib[side_colour][1];
        b_temp = colour_lib[side_colour][2];
    }

    count_rgb_light(breathe_data_tab[play_point]);
    count_rgb_light(side_light_table[side_light]);

    for (int i = 0; i < SIDE_LINE; i++) {
        side_rgb_set_color(side_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}

/**
 * @brief  side_static_mode_show.
 */
static void side_static_mode_show(void)
{
    uint8_t play_index;

    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    if (side_play_point >= SIDE_COLOUR_MAX) side_play_point = 0;

    for (int i = 0; i < SIDE_LINE; i++) {
        if (0) {
            r_temp = flow_rainbow_colour_tab[16 * i][0];
            g_temp = flow_rainbow_colour_tab[16 * i][1];
            b_temp = flow_rainbow_colour_tab[16 * i][2];
            light_point_playing(0, 24, FLOW_COLOUR_TAB_LEN, &play_index);
        } else  
        {
            r_temp = colour_lib[side_colour][0];
            g_temp = colour_lib[side_colour][1];
            b_temp = colour_lib[side_colour][2];
        }

        count_rgb_light(side_light_table[side_light]);

        side_rgb_set_color(side_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}

/**
 * @brief  side_off_mode_show.
 */
static void side_off_mode_show(void)
{
    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    r_temp = 0x00;
    g_temp = 0x00;
    b_temp = 0x00;

    for (int i = 0; i < SIDE_LINE; i++) {
        side_rgb_set_color(side_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}


void bat_charging_breathe(void)
{
    static uint32_t interval_timer = 0;
    static uint8_t play_point = 0;

    if (timer_elapsed32(interval_timer) > 10) {
        interval_timer = timer_read32();
        light_point_playing(0, 1, BREATHE_TAB_LEN, &play_point);
    }

    r_temp = 0x80; g_temp = 0x40; b_temp = 0x00;
    count_rgb_light(breathe_data_tab[play_point]);
    set_side_rgb(r_temp, g_temp, b_temp);
}


void bat_charging_design(uint8_t init, uint8_t r, uint8_t g, uint8_t b)
{
    static uint32_t interval_timer = 0;
    static uint16_t show_mask      = 0x00;
    static bool f_move_trend       = 0;
    uint16_t bit_mask              = 1;
    uint8_t i;

    if (timer_elapsed32(interval_timer) > 100) {
        interval_timer = timer_read32();

        if (f_move_trend) {
            show_mask >>= 1;
            if (show_mask == 0x1f >> (SIDE_LINE - init))
                f_move_trend = 0;
        } else {
            show_mask <<= 1;
            show_mask |= 1;
            if (show_mask == 0x7f)
                f_move_trend = 1;
        }
    }

    for (i = 0; i < SIDE_LINE; i++) {
        if (show_mask & bit_mask) {
            side_rgb_set_color(i, r, g, b);
        } else {
            side_rgb_set_color(i, 0x00, 0x00, 0x00);
        }
        bit_mask <<= 1;
    }
}

void rf_show_blink(void)
{
    extern uint8_t rf_blink_cnt;
    static uint32_t interval_timer = 0;
    uint16_t show_priod;

    if (rf_blink_cnt) {
        if (dev_info.rf_state == RF_PAIRING)
            show_priod = 250;
        else
            show_priod = 500;

        if (timer_elapsed32(interval_timer) > (show_priod >> 1)) {
            r_temp = 0x00; g_temp = 0x00; b_temp = 0x00;
        }

        if (timer_elapsed32(interval_timer) >= show_priod) {
            rf_blink_cnt--;
            interval_timer = timer_read32();
        }
    }
    else {
        interval_timer = timer_read32();
    }

    set_side_rgb(r_temp, g_temp, b_temp);
}

void rf_show_design(uint8_t r, uint8_t g, uint8_t b)
{
    static uint32_t interval_timer = 0;
    static uint16_t show_mask      = 0x04;
    uint16_t show_mask_temp        = 0;
    uint16_t show_priod;
    uint16_t bit_mask = 1;
    uint8_t i;

    if (dev_info.rf_state == RF_PAIRING)
        show_priod = 100;
    else
        show_priod = 200;

    if (timer_elapsed32(interval_timer) > show_priod) {
        interval_timer = timer_read32();

        show_mask_temp = (show_mask << 1) | (show_mask >> 1);
        show_mask_temp |= 0x04;
        show_mask |= show_mask_temp;

        if (show_mask == 0x7f)
            show_mask = 0x0;
    }

    for (i = 0; i < SIDE_LINE; i++) {
        if (show_mask & bit_mask) {
            side_rgb_set_color(i, r >> 2, g >> 2, b >> 2);
        } else {
            side_rgb_set_color(i, 0x00, 0x00, 0x00);
        }
        bit_mask <<= 1;
    }
}

/**
 * @brief  rf_led_show.
 */
void rf_led_show(void)
{
#if(WORK_MODE == THREE_MODE)
    static bool flag_power_on = 1;
#endif
    
    if (dev_info.link_mode == LINK_RF_24) {  
        r_temp = 0x00;
        g_temp = 0x80;
        b_temp = 0x00;
    } else if (dev_info.link_mode == LINK_USB) {  
        r_temp = 0x80;
        g_temp = 0x80;
        b_temp = 0x00;
#if(WORK_MODE == THREE_MODE)
        if (flag_power_on && (rf_link_show_time < RF_LINK_SHOW_TIME)) return;
#endif
    } else {  
        r_temp = 0x00;
        g_temp = 0x00;
        b_temp = 0x80;
    }

#if(WORK_MODE == THREE_MODE)
    flag_power_on = 0;
#endif
    if (rf_blink_cnt) {
        #if (RFLINK_SHIFT)
        rf_show_design(r_temp, g_temp, b_temp);
        #else
        rf_show_blink();
        #endif
    }
    else if (rf_link_show_time < RF_LINK_SHOW_TIME) {
        set_side_rgb(r_temp, g_temp, b_temp);
    }
}

void low_bat_show(void)
{
    static uint32_t interval_timer = 0;

    r_temp = 0x80, g_temp = 0, b_temp = 0;

    if(low_bat_blink_cnt)
    {
        if (timer_elapsed32(interval_timer) > (LOW_BAT_BLINK_PRIOD >> 1)) {
            r_temp = 0x00; g_temp = 0x00; b_temp = 0x00;
        }

        if (timer_elapsed32(interval_timer) >= LOW_BAT_BLINK_PRIOD) {
            interval_timer = timer_read32();
            low_bat_blink_cnt--;
        }
    }

    set_side_rgb(r_temp, g_temp, b_temp);
}



/**
 * @brief  bat_percent_led.
 */
void bat_percent_led(uint8_t bat_percent)
{
    uint8_t i;
    uint8_t bat_end_led = 0;
    uint8_t bat_r, bat_g, bat_b;

    if (bat_percent <= 20) {
        bat_end_led = 0;
        bat_r = 0x80, bat_g = 0, bat_b = 0;  
    } else if (bat_percent <= 40) {
        bat_end_led = 1;
        bat_r = 0x80, bat_g = 0x40, bat_b = 0;  
    } else if (bat_percent <= 50) {
        bat_end_led = 2;
        bat_r = 0x80, bat_g = 0x40, bat_b = 0; 
    } else if (bat_percent <= 80) {
        bat_end_led = 3;
        bat_r = 0x80, bat_g = 0x80, bat_b = 0; 
    } else {
        bat_end_led = 4;
        bat_r = 0, bat_g = 0x80, bat_b = 0;    
    }

    if (f_charging) {
        low_bat_blink_cnt = 6;
        #if (CHARGING_SHIFT)
        bat_charging_design(bat_end_led, bat_r >> 2, bat_g >> 2, bat_b >> 2);
        #else
        bat_charging_breathe();
        #endif
    }
    else if(bat_percent < 10) {
        low_bat_show();
    }
    else {
        bat_end_led = 4;
        low_bat_blink_cnt = 6;
        for (i = 0; i <= bat_end_led; i++)
            side_rgb_set_color(i, bat_r >> 2, bat_g >> 2, bat_b >> 2);

        for (; i < SIDE_LINE; i++)
            side_rgb_set_color(i, 0, 0, 0);
    }
}

/**
 * @brief  bat_led_show.
 */
void bat_led_show(void)
{
    static bool bat_show_flag        = true;    
    static uint32_t bat_show_time    = 0;       
    static uint32_t bat_sts_debounce = 0;      
    static uint32_t bat_per_debounce = 0;       
    static uint8_t charge_state      = 0;       
    static uint8_t bat_percent       = 0;     
    static bool f_init               = 1;     

    if(dev_info.link_mode != LINK_USB)
    {
        extern uint16_t rf_link_show_time;
        if(rf_link_show_time < RF_LINK_SHOW_TIME)
        return;

        if(dev_info.rf_state != RF_CONNECT)
        return;
    }

    if (f_init) {
        f_init        = 0;
        bat_show_time = timer_read32();
        charge_state  = dev_info.rf_charge;
        bat_percent   = dev_info.rf_baterry;
    }

    if (charge_state != dev_info.rf_charge) {
        if (timer_elapsed32(bat_sts_debounce) > 1000) {
            if (((charge_state & 0x01) == 0) && ((dev_info.rf_charge & 0x01) != 0)) {
                bat_show_flag   = true;            
                f_charging = true;                  
                bat_show_time   = timer_read32();   
            }
            charge_state = dev_info.rf_charge;
        }
    }
    else {
        bat_sts_debounce = timer_read32();
        if (timer_elapsed32(bat_show_time) > 5000) {
            bat_show_flag = false;
            f_charging = false;
        }
        if (charge_state == 0x03) {
            f_charging = true;  
        } else if (!(charge_state & 0x01)) {
            f_charging = 0;   
        }
    }

    if (bat_percent != dev_info.rf_baterry) {
        if (timer_elapsed32(bat_per_debounce) > 1000) {
            bat_percent = dev_info.rf_baterry;
        }
    }
    else {
        bat_per_debounce = timer_read32();

        if ( (bat_percent < 10) && (!(charge_state&0x01))) {
            bat_show_flag = true;  
            bat_show_time = timer_read32();

            if(rgb_matrix_config.hsv.v > RGB_MATRIX_VAL_STEP) {
                rgb_matrix_config.hsv.v = RGB_MATRIX_VAL_STEP;
            }

            if(side_light > 1) {
                side_light = 1;
            }

            if(logo_light >1) {
                logo_light = 1;
            }

        }
    }

    if (f_bat_hold || bat_show_flag) {
        bat_percent_led(bat_percent);
    }
}

/**
 * @brief  device_reset_show.
 */
void device_reset_show(void)
{
    writePinHigh(DC_BOOST_PIN); 
    setPinOutput(DRIVER_SIDE_CS_PIN);
    setPinOutput(DRIVER_LED_CS_PIN);
    writePinLow(DRIVER_SIDE_CS_PIN);
    writePinLow(DRIVER_LED_CS_PIN);
    for (int blink_cnt = 0; blink_cnt < 3; blink_cnt++) {
        rgb_matrix_set_color_all(0x10, 0x10, 0x10);
        for (int i = 0; i < SIDE_LED_NUM; i++) {
            side_rgb_set_color(i, 0x10, 0x10, 0x10);
        }
        rgb_matrix_update_pwm_buffers();
        side_rgb_refresh();
        wait_ms(200);

        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
        for (int i = 0; i < SIDE_LED_NUM; i++) {
            side_rgb_set_color(i, 0x00, 0x00, 0x00);
        }
        rgb_matrix_update_pwm_buffers();
        side_rgb_refresh();
        wait_ms(200);
    }
}

/**
 * @brief  device_reset_init.
 */
void device_reset_init(void)
{
    side_mode       = 0;  
    side_light      = 3;
    side_speed      = 2;
    side_rgb        = 1;
    side_colour     = 0;
    side_play_point = 0;

    side_play_cnt   = 0;
    side_play_timer = timer_read32(); 

    logo_mode       = 0;
    logo_light      = 3;
    logo_speed      = 2;
    logo_rgb        = 1;
    logo_colour     = 0;
    logo_play_point = 0;

    logo_play_cnt   = 0;
    logo_play_timer = timer_read32();  

    f_bat_hold = false;  

    rgb_matrix_enable();                                                                   
    rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);                                            
    rgb_matrix_set_speed(255 - RGB_MATRIX_SPD_STEP * 2);                                  
    rgb_matrix_sethsv(RGB_DEFAULT_COLOUR,255, RGB_MATRIX_MAXIMUM_BRIGHTNESS - RGB_MATRIX_VAL_STEP * 2);       

    user_config.default_brightness_flag = 0xA5;
    user_config.ee_side_mode            = side_mode;
    user_config.ee_side_light           = side_light;
    user_config.ee_side_speed           = side_speed;
    user_config.ee_side_rgb             = side_rgb;
    user_config.ee_side_colour          = side_colour;
    user_config.ee_logo_mode            = logo_mode;
    user_config.ee_logo_light           = logo_light;
    user_config.ee_logo_speed           = logo_speed;
    user_config.ee_logo_rgb             = logo_rgb;
    user_config.ee_logo_colour          = logo_colour;
#if(WORK_MODE == THREE_MODE)
    f_dev_sleep_enable                  = 1;         
#endif
    eeconfig_update_user_datablock(&user_config);
}

void rgb_test_show(void)
{
    writePinHigh(DC_BOOST_PIN);    
    setPinOutput(DRIVER_LED_CS_PIN);
    writePinLow(DRIVER_LED_CS_PIN);

    rgb_matrix_set_color_all(0xFF, 0x00, 0x00);
    for (int i = 0; i < SIDE_LED_NUM; i++)
    side_rgb_set_color(i, 0xFF, 0x00, 0x00);
    rgb_matrix_update_pwm_buffers();
    side_rgb_refresh();
    wait_ms(1000);
    rgb_matrix_set_color_all(0x00, 0xFF, 0x00);
    for (int i = 0; i < SIDE_LED_NUM; i++)
    side_rgb_set_color(i, 0x00, 0xFF, 0x00);
    rgb_matrix_update_pwm_buffers();
    side_rgb_refresh();
    wait_ms(1000);
    rgb_matrix_set_color_all(0x00, 0x00, 0xFF);
    for (int i = 0; i < SIDE_LED_NUM; i++)
    side_rgb_set_color(i, 0x00, 0x00, 0xFF);
    rgb_matrix_update_pwm_buffers();
    side_rgb_refresh();
    wait_ms(1000);
}

/**
 * @brief  side_led_show.
 */
void m_side_led_show(void)
{
    static uint32_t side_refresh_time = 0;
    static bool flag_power_on         = 1;

    if (flag_power_on) {
        if (!f_dial_sw_init_ok) return;
        flag_power_on = 0;
    }

    side_play_cnt += timer_elapsed32(side_play_timer);
    side_play_timer = timer_read32();

    switch (side_mode) {
        case SIDE_WAVE_1:   side_wave_mode_show_1();    break;  
        case SIDE_WAVE_2:   side_wave_mode_show_2();    break;   
        case SIDE_MIX:      side_spectrum_mode_show();  break; 
        case SIDE_BREATH:   side_breathe_mode_show();   break; 
        case SIDE_STATIC:   side_static_mode_show();    break; 
        case SIDE_OFF:      side_off_mode_show();       break;  
    }

#if(WORK_MODE == THREE_MODE)
    bat_led_show();     
#endif
    sys_led_show();      
    sys_sw_led_show();   
    sleep_sw_led_show();  
    rf_led_show();        

    m_logo_led_show();

    if (timer_elapsed32(side_refresh_time) > 50) {
        side_refresh_time = timer_read32();
        side_rgb_refresh();
    }
}
