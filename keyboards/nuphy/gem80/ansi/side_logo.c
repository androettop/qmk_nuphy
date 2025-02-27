// Copyright 2023 Persama (@Persama)
// SPDX-License-Identifier: GPL-2.0-or-later
#include "ansi.h"

#define	STARRY_INDEX_LEN		(160)
#define	WAVE_TAB_LEN			(112 + 16)
#define	BREATHE_TAB_LEN			128
#define	MIXCOLOUR_TAB_LEN		144
#define	FLOW_COLOUR_TAB_LEN		224
#define	FIREWORK_INDEX_LEN		(158)
#define STARRY_DATA_LEN			96
#define	TIDE_DATA_LEN			120
#define SIDE_WAVE_1             0  
#define SIDE_WAVE_2             1
#define SIDE_MIX                2  
#define SIDE_STATIC             3  
#define SIDE_BREATH             4  
#define SIDE_OFF                5  
#define LOGO_LINE               7
#define LIGHT_COLOUR_MAX        8 
#define SIDE_COLOUR_MAX         8  
#define LIGHT_SPEED_MAX         4  

extern const uint8_t side_speed_table[6][5];
extern const uint8_t side_light_table[6];
extern const uint8_t light_value_tab[101];
extern const uint8_t breathe_data_tab[BREATHE_TAB_LEN];
extern const uint8_t wave_data_tab[WAVE_TAB_LEN];
extern const uint8_t flow_rainbow_colour_tab[FLOW_COLOUR_TAB_LEN][3];
extern const uint8_t colour_lib[9][3];
extern uint8_t r_temp, g_temp, b_temp;
extern user_config_t user_config;
extern void side_rgb_set_color(int index, uint8_t red, uint8_t green, uint8_t blue);


const uint8_t logo_led_index_tab[LOGO_LINE] = {
    5,
    6,
    7,
    8,
    9,
    10,
    11,
};

uint8_t logo_mode           = 3;  
uint8_t logo_light          = 3; 
uint8_t logo_speed          = 2;  
uint8_t logo_rgb            = 0; 
uint8_t logo_colour         = 1; 
uint8_t logo_play_point     = 0;
uint8_t logo_play_cnt       = 0; 
uint32_t logo_play_timer    = 0;


void logo_light_level_control(uint8_t brighten)
{
    if (brighten)      {
        if (logo_light == 5) {
            return;
        } else
            logo_light++;
    } else  
    {
        if (logo_light == 0) {
            return;
        } else
            logo_light--;
    }
    user_config.ee_logo_light = logo_light;
    eeconfig_update_user_datablock(&user_config);  
}

void logo_light_speed_control(uint8_t fast)
{
    if ((logo_speed) > LIGHT_SPEED_MAX)
        (logo_speed) = LIGHT_SPEED_MAX / 2;

    if (fast) {
        if ((logo_speed)) logo_speed--;  
    } else {
        if ((logo_speed) < LIGHT_SPEED_MAX) logo_speed++;  
    }
    user_config.ee_logo_speed = logo_speed;
    eeconfig_update_user_datablock(&user_config); 
}

void logo_side_colour_control(uint8_t dir)
{
    if ((logo_mode != SIDE_WAVE_1)&&(logo_mode != SIDE_WAVE_2)){
        if (logo_rgb) {
            logo_rgb    = 0;
            logo_colour = 0;
        }
    }

    if (dir) {  
        if (logo_rgb) {
            logo_rgb    = 0;
            logo_colour = 0;
        } else {
            logo_colour++;
            if (logo_colour >= LIGHT_COLOUR_MAX) {
                logo_rgb    = 1;
                logo_colour = 0;
            }
        }
    } else { 
        if (logo_rgb) {
            logo_rgb    = 0;
            logo_colour = LIGHT_COLOUR_MAX - 1;
        } else {
            logo_colour--;
            if (logo_colour >= LIGHT_COLOUR_MAX) {
                logo_rgb    = 1;
                logo_colour = 0;
            }
        }
    }
    user_config.ee_logo_rgb    = logo_rgb;
    user_config.ee_logo_colour = logo_colour;
    eeconfig_update_user_datablock(&user_config);  
}

void logo_side_colour_set(uint8_t col)
{
    logo_mode   = SIDE_STATIC;
    logo_rgb    = 0;
    logo_colour = col;

    user_config.ee_logo_mode = logo_mode;
    user_config.ee_logo_rgb    = logo_rgb;
    user_config.ee_logo_colour = logo_colour;
    eeconfig_update_user_datablock(&user_config);  
}

void logo_side_mode_control(uint8_t dir)
{
    if (dir) {  
        logo_mode++;
        if (logo_mode > SIDE_OFF) {
            logo_mode = 0;
        }
    } else {  
        if (logo_mode > 0) {
            logo_mode--;
        } else {
            logo_mode = SIDE_OFF;
        }
    }
    logo_play_point          = 0;
    user_config.ee_logo_mode = logo_mode;
    eeconfig_update_user_datablock(&user_config); 
}


void set_logo_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 5; i < 12; i++)
        side_rgb_set_color(i, r >> 2, g >> 2, b >> 2);
}

static void logo_light_point_playing(uint8_t trend, uint8_t step, uint8_t len, uint8_t *point)
{
    if (trend) {
        *point += step;
        if (*point >= len) *point -= len;
    } else {
        *point -= step;
        if (*point >= len) *point = len - (255 - *point) - 1;
    }
}

static void logo_count_rgb_light(uint8_t light_temp)
{
    uint16_t temp;

    temp   = (light_temp)*r_temp + r_temp;
    r_temp = temp >> 8;

    temp   = (light_temp)*g_temp + g_temp;
    g_temp = temp >> 8;

    temp   = (light_temp)*b_temp + b_temp;
    b_temp = temp >> 8;
}

static void logo_wave_mode_show_1(void)
{
    uint8_t play_index;

    //------------------------------
    if (logo_play_cnt <= side_speed_table[logo_mode][logo_speed])
        return;
    else
        logo_play_cnt -= side_speed_table[logo_mode][logo_speed];
    if (logo_play_cnt > 20) logo_play_cnt = 0;

    //------------------------------
    if (logo_rgb)
        logo_light_point_playing(0, 1, FLOW_COLOUR_TAB_LEN, &logo_play_point);
    else
        logo_light_point_playing(0, 1, WAVE_TAB_LEN, &logo_play_point);

    play_index = logo_play_point;
    for (int i = 0; i < LOGO_LINE; i++) {
        if (logo_rgb) {
            r_temp = flow_rainbow_colour_tab[play_index][0];
            g_temp = flow_rainbow_colour_tab[play_index][1];
            b_temp = flow_rainbow_colour_tab[play_index][2];

            logo_light_point_playing(1, 5, FLOW_COLOUR_TAB_LEN, &play_index);
        } else {
            r_temp = colour_lib[logo_colour][0];
            g_temp = colour_lib[logo_colour][1];
            b_temp = colour_lib[logo_colour][2];

            logo_light_point_playing(1, 12, WAVE_TAB_LEN, &play_index);
            logo_count_rgb_light(wave_data_tab[play_index]);
        }

        logo_count_rgb_light(side_light_table[logo_light]);
        side_rgb_set_color(logo_led_index_tab[i], r_temp >> 1, g_temp >> 1, b_temp >> 1);
    }
}
static void logo_wave_mode_show_2(void)
{
    uint8_t play_index;

    //------------------------------
    if (logo_play_cnt <= side_speed_table[logo_mode][logo_speed])
        return;
    else
        logo_play_cnt -= side_speed_table[logo_mode][logo_speed];
    if (logo_play_cnt > 20) logo_play_cnt = 0;

    //------------------------------
    if (logo_rgb)
        logo_light_point_playing(0, 1, FLOW_COLOUR_TAB_LEN, &logo_play_point);
    else
        logo_light_point_playing(0, 1, WAVE_TAB_LEN, &logo_play_point);

    play_index = logo_play_point;
    for (int i = 0; i < LOGO_LINE; i++) {
        if (logo_rgb) {
            r_temp = flow_rainbow_colour_tab[play_index][0];
            g_temp = flow_rainbow_colour_tab[play_index][1];
            b_temp = flow_rainbow_colour_tab[play_index][2];

            logo_light_point_playing(1, 16, FLOW_COLOUR_TAB_LEN, &play_index);
        } else {
            r_temp = colour_lib[logo_colour][0];
            g_temp = colour_lib[logo_colour][1];
            b_temp = colour_lib[logo_colour][2];

            logo_light_point_playing(1, 12, WAVE_TAB_LEN, &play_index);
            logo_count_rgb_light(wave_data_tab[play_index]);
        }

        logo_count_rgb_light(side_light_table[logo_light]);
        side_rgb_set_color(logo_led_index_tab[i], r_temp >> 1, g_temp >> 1, b_temp >> 1);
    }
}

static void logo_spectrum_mode_show(void)
{
    if (logo_play_cnt <= side_speed_table[logo_mode][logo_speed])
        return;
    else
        logo_play_cnt -= side_speed_table[logo_mode][logo_speed];
    if (logo_play_cnt > 20) logo_play_cnt = 0;

    logo_light_point_playing(1, 1, FLOW_COLOUR_TAB_LEN, &logo_play_point);

    r_temp = flow_rainbow_colour_tab[logo_play_point][0];
    g_temp = flow_rainbow_colour_tab[logo_play_point][1];
    b_temp = flow_rainbow_colour_tab[logo_play_point][2];

    logo_count_rgb_light(side_light_table[logo_light]);

    for (int i = 0; i < LOGO_LINE; i++) {
        side_rgb_set_color(logo_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}

static void logo_breathe_mode_show(void)
{
    static uint8_t play_point = 0;

    if (logo_play_cnt <= side_speed_table[logo_mode][logo_speed])
        return;
    else
        logo_play_cnt -= side_speed_table[logo_mode][logo_speed];
    if (logo_play_cnt > 20) logo_play_cnt = 0;

    logo_light_point_playing(0, 1, BREATHE_TAB_LEN, &play_point);

    if (0) {
        if (play_point == 0) {
            if (++logo_play_point >= LIGHT_COLOUR_MAX)
                logo_play_point = 0;
        }

        r_temp = colour_lib[logo_play_point][0];
        g_temp = colour_lib[logo_play_point][1];
        b_temp = colour_lib[logo_play_point][2];
    } else {
        r_temp = colour_lib[logo_colour][0];
        g_temp = colour_lib[logo_colour][1];
        b_temp = colour_lib[logo_colour][2];
    }

    logo_count_rgb_light(breathe_data_tab[play_point]);
    logo_count_rgb_light(side_light_table[logo_light]);

    for (int i = 0; i < LOGO_LINE; i++) {
        side_rgb_set_color(logo_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}

static void logo_static_mode_show(void)
{
    uint8_t play_index;

    if (logo_play_cnt <= side_speed_table[logo_mode][logo_speed])
        return;
    else
        logo_play_cnt -= side_speed_table[logo_mode][logo_speed];
    if (logo_play_cnt > 20) logo_play_cnt = 0;

    if (logo_play_point >= SIDE_COLOUR_MAX) logo_play_point = 0;

    for (int i = 0; i < LOGO_LINE; i++) {
        if (0) {
            r_temp = flow_rainbow_colour_tab[16 * i][0];
            g_temp = flow_rainbow_colour_tab[16 * i][1];
            b_temp = flow_rainbow_colour_tab[16 * i][2];
            logo_light_point_playing(0, 24, FLOW_COLOUR_TAB_LEN, &play_index);
        } else  
        {
            r_temp = colour_lib[logo_colour][0];
            g_temp = colour_lib[logo_colour][1];
            b_temp = colour_lib[logo_colour][2];
        }

        logo_count_rgb_light(side_light_table[logo_light]);

        side_rgb_set_color(logo_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}

static void logo_off_mode_show(void)
{
    if (logo_play_cnt <= side_speed_table[logo_mode][logo_speed])
        return;
    else
        logo_play_cnt -= side_speed_table[logo_mode][logo_speed];
    if (logo_play_cnt > 20) logo_play_cnt = 0;

    r_temp = 0x00;
    g_temp = 0x00;
    b_temp = 0x00;

    for (int i = 0; i < LOGO_LINE; i++) {
        side_rgb_set_color(logo_led_index_tab[i], r_temp >> 2, g_temp >> 2, b_temp >> 2);
    }
}

void m_logo_led_show(void)
{
    logo_play_cnt += timer_elapsed32(logo_play_timer);
    logo_play_timer = timer_read32(); 

    switch (logo_mode) {
        case SIDE_WAVE_1:   logo_wave_mode_show_1(); break;  
        case SIDE_WAVE_2:   logo_wave_mode_show_2(); break;  
        case SIDE_MIX:      logo_spectrum_mode_show(); break;  
        case SIDE_BREATH:   logo_breathe_mode_show(); break; 
        case SIDE_STATIC:   logo_static_mode_show(); break;  
        case SIDE_OFF:      logo_off_mode_show(); break; 
    }
}
