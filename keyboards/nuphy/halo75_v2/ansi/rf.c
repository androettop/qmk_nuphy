/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "ansi.h"
#include "uart.h"  // qmk uart.h
#include "rf_driver.h"

USART_MGR_STRUCT Usart_Mgr;
#define RX_SBYTE    Usart_Mgr.RXDBuf[0]
#define RX_CMD      Usart_Mgr.RXDBuf[1]
#define RX_ACK      Usart_Mgr.RXDBuf[2]
#define RX_LEN      Usart_Mgr.RXDBuf[3]
#define RX_DAT      Usart_Mgr.RXDBuf[4]

extern bool f_uart_ack;
extern bool f_rf_read_data_ok;
extern bool f_rf_sts_sysc_ok;
extern bool f_rf_new_adv_ok;
extern bool f_rf_reset;
extern bool f_rf_hand_ok;
extern bool f_goto_sleep;

uint8_t  uart_bit_report_buf[32] = {0};
uint8_t  func_tab[32]            = {0};
uint8_t  bitkb_report_buf[32]    = {0};
uint8_t  bytekb_report_buf[8]    = {0};
uint16_t conkb_report            = 0;
uint16_t syskb_report            = 0;
uint8_t  sync_lost               = 0;
uint8_t  disconnect_delay        = 0;

extern DEV_INFO_STRUCT dev_info;
extern host_driver_t  *m_host_driver;
extern uint8_t         host_mode;
extern uint8_t         rf_blink_cnt;
extern uint16_t        rf_link_show_time;
extern uint16_t        rf_linking_time;
extern uint16_t        no_act_time;
extern bool            f_send_channel;
extern bool            f_dial_sw_init_ok;

report_mouse_t mousekey_get_report(void);
void           uart_init(uint32_t baud); // qmk uart.c
void           uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size);
void           UART_Send_Bytes(uint8_t *Buffer, uint32_t Length);
uint8_t        get_checksum(uint8_t *buf, uint8_t len);
void           uart_receive_pro(void);
void           m_break_all_key(void);
uint16_t       host_last_consumer_usage(void);

/**
 * @brief Uart auto nkey send
 */
bool f_bit_kb_act = 0;
static void uart_auto_nkey_send(uint8_t *pre_bit_report, uint8_t *now_bit_report, uint8_t size)
{
    uint8_t i, j, byte_index;
    uint8_t change_mask, offset_mask;
    uint8_t key_code = 0;
    bool f_byte_send = 0, f_bit_send = 0;

    if (pre_bit_report[0] ^ now_bit_report[0]) {
        bytekb_report_buf[0] = now_bit_report[0];
        f_byte_send          = 1;
    }

    for (i = 1; i < size; i++) {
        change_mask = pre_bit_report[i] ^ now_bit_report[i];
        offset_mask = 1;
        for (j = 0; j < 8; j++) {
            if (change_mask & offset_mask) {
                if (now_bit_report[i] & offset_mask) {
                    for (byte_index = 2; byte_index < 8; byte_index++) {
                        if (bytekb_report_buf[byte_index] == 0) {
                            bytekb_report_buf[byte_index] = key_code;
                            f_byte_send                   = 1;
                            break;
                        }
                    }
                    if (byte_index >= 8) {
                        uart_bit_report_buf[i] |= offset_mask;
                        f_bit_send = 1;
                    }
                } else {
                    for (byte_index = 2; byte_index < 8; byte_index++) {
                        if (bytekb_report_buf[byte_index] == key_code) {
                            bytekb_report_buf[byte_index] = 0;
                            f_byte_send                   = 1;
                            break;
                        }
                    }
                    if (byte_index >= 8) {
                        uart_bit_report_buf[i] &= ~offset_mask;
                        f_bit_send = 1;
                    }
                }
            }
            key_code++;
            offset_mask <<= 1;
        }
    }

    if (f_bit_send) {
        f_bit_kb_act = 1;
        uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, 16);
    }

    if (f_byte_send) {
        uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, 8);
    }
}


/**
 * @brief  Uart send keys report.
 */
void uart_send_report_func(void)
{
    static uint32_t interval_timer = 0;

    if (dev_info.link_mode == LINK_USB) return;
    keyboard_protocol          = 1;

    if (timer_elapsed32(interval_timer) > 300) {
        interval_timer = timer_read32();
        if (no_act_time <= 2000) {
            uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, 8);
            wait_us(200);

            if(f_bit_kb_act)
            uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, 16);
        }
        else {
            f_bit_kb_act = 0;
        }
    }
}

/**
 * @brief  Uart send consumer keys report.
 * @note Call in rf_driver.c
 */
void uart_send_consumer_report(report_extra_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_CONSUME, (uint8_t *)(&report->usage), 2);
}

/**
 * @brief  Uart send mouse keys report.
 * @note Call in rf_driver.c
 */
void uart_send_mouse_report(report_mouse_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_MS, &report->buttons, 5);
}

/**
 * @brief  Uart send system keys report.
 * @note Call in rf_driver.c
 */
void uart_send_system_report(report_extra_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_SYS, (uint8_t *)(&report->usage), 2);
}

/**
 * @brief  Uart send byte keys report.
 * @note Call in rf_driver.c
 */
void uart_send_report_keyboard(report_keyboard_t *report) {
    no_act_time      = 0;
    report->reserved = 0;
    uart_send_report(CMD_RPT_BYTE_KB, &report->mods, 8);
    memcpy(bytekb_report_buf, &report->mods, 8);
}

/**
 * @brief  Uart send bit keys report.
 * @note Call in rf_driver.c
 */
void uart_send_report_nkro(report_nkro_t *report) {
    no_act_time = 0;
    uart_auto_nkey_send(bitkb_report_buf, &nkro_report->mods, NKRO_REPORT_BITS + 1);
    memcpy(&bitkb_report_buf[0], &nkro_report->mods, NKRO_REPORT_BITS + 1);
}

/**
 * @brief  Parsing the data received from the RF module.
 */
void RF_Protocol_Receive(void) {
    uint8_t i, check_sum = 0;

    if (Usart_Mgr.RXDState == RX_Done) {
        f_uart_ack = 1;
        sync_lost = 0;

        if (Usart_Mgr.RXDLen > 4) {
            for (i = 0; i < RX_LEN; i++)
                check_sum += Usart_Mgr.RXDBuf[4 + i];

            if (check_sum != Usart_Mgr.RXDBuf[4 + i]) {
                Usart_Mgr.RXDState = RX_SUM_ERR;
                return;
            }
        } else if (Usart_Mgr.RXDLen == 3) {
            if (Usart_Mgr.RXDBuf[2] == 0xA0) {
                f_uart_ack = 1;
            }
        }

        switch (RX_CMD) {
            case CMD_HAND: {
                f_rf_hand_ok = 1;
                break;
            }

            case CMD_24G_SUSPEND: {
                f_goto_sleep = 1;
                break;
            }

            case CMD_NEW_ADV: {
                f_rf_new_adv_ok = 1;
                break;
            }

            case CMD_RF_STS_SYSC: {
                static uint8_t error_cnt = 0;

                if (dev_info.link_mode == Usart_Mgr.RXDBuf[4]) {
                    error_cnt = 0;

                    dev_info.rf_state = Usart_Mgr.RXDBuf[5];

                    if ((dev_info.rf_state == RF_CONNECT) && ((Usart_Mgr.RXDBuf[6] & 0xf8) == 0)) {
                        dev_info.rf_led = Usart_Mgr.RXDBuf[6];
                    }

                    dev_info.rf_charge = Usart_Mgr.RXDBuf[7];

                    if (Usart_Mgr.RXDBuf[8] <= 100) dev_info.rf_baterry = Usart_Mgr.RXDBuf[8];
                    if (dev_info.rf_charge & 0x01) dev_info.rf_baterry = 100;
                }
                else {
                    if (dev_info.rf_state != RF_INVALID) {
                        if (error_cnt >= 5) {
                            error_cnt      = 0;
                            f_send_channel = 1;
                        } else {
                            error_cnt++;
                        }
                    }
                }

                f_rf_sts_sysc_ok = 1;
                break;
            }

            case CMD_READ_DATA: {
                memcpy(func_tab, &Usart_Mgr.RXDBuf[4], 32);

                if (func_tab[4] <= LINK_USB) {
                    dev_info.link_mode = func_tab[4];
                }

                if (func_tab[5] < LINK_USB) {
                    dev_info.rf_channel = func_tab[5];
                }

                if ((func_tab[6] <= LINK_BT_3) && (func_tab[6] >= LINK_BT_1)) {
                    dev_info.ble_channel = func_tab[6];
                }

                f_rf_read_data_ok = 1;
                break;
            }
        }

        Usart_Mgr.RXDLen      = 0;
        Usart_Mgr.RXDState    = RX_Idle;
        Usart_Mgr.RXDOverTime = 0;
    }
}

/**
 * @brief  Uart send cmd.
 * @param  cmd: cmd.
 * @param  wait_ack: wait time for ack after sending.
 * @param  delayms: delay before sending.
 */
uint8_t uart_send_cmd(uint8_t cmd, uint8_t wait_ack, uint8_t delayms) {
    wait_ms(delayms);

    memset(&Usart_Mgr.TXDBuf[0], 0, UART_MAX_LEN);

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = cmd;
    Usart_Mgr.TXDBuf[2] = 0x00;

    switch (cmd) {
        case CMD_SLEEP: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_HAND: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_RF_STS_SYSC: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;
            break;
        }

        case CMD_SET_LINK: {
            dev_info.rf_state   = RF_LINKING;
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;

            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            break;
        }

        case CMD_NEW_ADV: {
            dev_info.rf_state   = RF_PAIRING;
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = 1;
            Usart_Mgr.TXDBuf[6] = dev_info.link_mode + 1;

            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            f_rf_new_adv_ok  = 0;
            break;
        }

        case CMD_CLR_DEVICE: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_SET_CONFIG: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = POWER_DOWN_DELAY;
            Usart_Mgr.TXDBuf[5] = POWER_DOWN_DELAY;
            break;
        }
        case CMD_SET_NAME: {
            Usart_Mgr.TXDBuf[3]  = 18;
            Usart_Mgr.TXDBuf[4]  = 1;  
            Usart_Mgr.TXDBuf[5]  = 16;   
            Usart_Mgr.TXDBuf[6]  = 'N';
            Usart_Mgr.TXDBuf[7]  = 'u';
            Usart_Mgr.TXDBuf[8]  = 'P';
            Usart_Mgr.TXDBuf[9]  = 'h';
            Usart_Mgr.TXDBuf[10] = 'y';
            Usart_Mgr.TXDBuf[11] = ' ';
            Usart_Mgr.TXDBuf[12] = 'H';
            Usart_Mgr.TXDBuf[13] = 'a';
            Usart_Mgr.TXDBuf[14] = 'l';
            Usart_Mgr.TXDBuf[15] = 'o';
            Usart_Mgr.TXDBuf[16] = '7';
            Usart_Mgr.TXDBuf[17] = '5';
            Usart_Mgr.TXDBuf[18] = ' ';   
            Usart_Mgr.TXDBuf[19] = 'V';   
            Usart_Mgr.TXDBuf[20] = '2';   
            Usart_Mgr.TXDBuf[21] = '-';   
            Usart_Mgr.TXDBuf[22] = get_checksum(Usart_Mgr.TXDBuf + 4, Usart_Mgr.TXDBuf[3]);  // sum
            break;
        }

        case CMD_SET_24G_NAME: {
            Usart_Mgr.TXDBuf[3]  = 46;
            Usart_Mgr.TXDBuf[4]  = 46;
            Usart_Mgr.TXDBuf[5]  = 3;      
            Usart_Mgr.TXDBuf[6]  = 'N';
            Usart_Mgr.TXDBuf[8]  = 'u';
            Usart_Mgr.TXDBuf[10] = 'P';
            Usart_Mgr.TXDBuf[12] = 'h';
            Usart_Mgr.TXDBuf[14] = 'y';
            Usart_Mgr.TXDBuf[16] = ' ';
            Usart_Mgr.TXDBuf[18] = 'H';
            Usart_Mgr.TXDBuf[20] = 'a';
            Usart_Mgr.TXDBuf[22] = 'l';
            Usart_Mgr.TXDBuf[24] = 'o';
            Usart_Mgr.TXDBuf[26] = '7';
            Usart_Mgr.TXDBuf[28] = '5';
            Usart_Mgr.TXDBuf[30] = ' ';
            Usart_Mgr.TXDBuf[32] = 'V';
            Usart_Mgr.TXDBuf[34] = '2';
            Usart_Mgr.TXDBuf[36] = ' ';
            Usart_Mgr.TXDBuf[38] = 'D';
            Usart_Mgr.TXDBuf[40] = 'o';
            Usart_Mgr.TXDBuf[42] = 'n';
            Usart_Mgr.TXDBuf[44] = 'g';
            Usart_Mgr.TXDBuf[46] = 'l';
            Usart_Mgr.TXDBuf[48] = 'e';
            Usart_Mgr.TXDBuf[50] = get_checksum(Usart_Mgr.TXDBuf + 4, Usart_Mgr.TXDBuf[3]);  // sum
            break;
        }

        case CMD_READ_DATA: {
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = 0x00;
            Usart_Mgr.TXDBuf[5] = FUNC_VALID_LEN;
            Usart_Mgr.TXDBuf[6] = FUNC_VALID_LEN;
            break;
        }

        case CMD_RF_DFU: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        default:
            break;
    }

    f_uart_ack = 0;
    UART_Send_Bytes(Usart_Mgr.TXDBuf, Usart_Mgr.TXDBuf[3] + 5);

    if (wait_ack) {
        while (wait_ack--) {
            wait_ms(1);
            if (f_uart_ack) return TX_OK;
        }
    } else {
        return TX_OK;
    }

    return TX_TIMEOUT;
}

/**
 * @brief RF module state sync.
 */
void dev_sts_sync(void) {
    static uint32_t interval_timer  = 0;
    static uint8_t  link_state_temp = RF_DISCONNECT;

    if (timer_elapsed32(interval_timer) < 200)
        return;
    else
        interval_timer = timer_read32();

    if (f_rf_reset) {
        f_rf_reset = 0;
        wait_ms(100);
        writePinLow(NRF_RESET_PIN);
        wait_ms(50);
        writePinHigh(NRF_RESET_PIN);
        wait_ms(50);
    }
    else if (f_send_channel) {
        f_send_channel = 0;
        uart_send_cmd(CMD_SET_LINK, 10, 10);
    }

    if (dev_info.link_mode == LINK_USB) {
        if (host_mode != HOST_USB_TYPE) {
            host_mode = HOST_USB_TYPE;
            host_set_driver(m_host_driver);
            m_break_all_key();
        }
        rf_blink_cnt = 0;
    }
    else {
        if (host_mode != HOST_RF_TYPE) {
            host_mode = HOST_RF_TYPE;
            m_break_all_key();
            host_set_driver(&rf_host_driver);
        }

        if (dev_info.rf_state != RF_CONNECT) {
            if (disconnect_delay >= 10) {
                rf_blink_cnt    = 3;
                rf_link_show_time = 0;
                link_state_temp = dev_info.rf_state;
            } else {
                disconnect_delay++;
            }
        }
        else if (dev_info.rf_state == RF_CONNECT) {
            rf_linking_time  = 0;
            disconnect_delay = 0;
            rf_blink_cnt     = 0;

            if (link_state_temp != RF_CONNECT) {
                link_state_temp   = RF_CONNECT;
                rf_link_show_time = 0;
                if (dev_info.link_mode == LINK_RF_24) {
                    uart_send_cmd(CMD_SET_24G_NAME, 10, 30);
                }
            }
        }
    }

    uart_send_cmd(CMD_RF_STS_SYSC, 1, 1);

    if (dev_info.link_mode != LINK_USB) {
        if (++sync_lost >= 5) {
            sync_lost  = 0;
            f_rf_reset = 1;
        }
    }
}

#define BAT_CFG_LEN     80
const uint8_t battery_acfg_tab[BAT_CFG_LEN] = {
    0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xB4, 0xC2, 0xB4, 0xA8, 0x9B, 0x96, 0xF8, 0xF2,
    0xF3, 0xC3, 0xA8, 0x8A, 0x65, 0x55, 0x49, 0x41,
    0x39, 0x34, 0x2E, 0xA9, 0xAE, 0xD3, 0x28, 0xFF,
    0xFF, 0xF1, 0xD3, 0xCE, 0xCB, 0xC8, 0xC3, 0xB8,
    0xAE, 0xA7, 0xA8, 0xA6, 0x82, 0x6D, 0x65, 0x63,
    0x69, 0x79, 0x8D, 0xA4, 0xB7, 0xC8, 0xA4, 0x16,
    0x20, 0x00, 0xA7, 0x10, 0x00, 0xB1, 0x28, 0x00,
    0x00, 0x00, 0x64, 0x43, 0xC0, 0x53, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81,  
};

void UART_Send_BatCfg(void) 
{
    uint8_t buf[128] = {0};

    buf[0] = UART_HEAD;       
    buf[1] = CMD_WBAT_CFG; 
    buf[2] = 0x01;           
    buf[3] = BAT_CFG_LEN;   
    memcpy(&buf[4], battery_acfg_tab, BAT_CFG_LEN);
    buf[4 + BAT_CFG_LEN] = get_checksum(&buf[4], BAT_CFG_LEN);
    UART_Send_Bytes(buf, BAT_CFG_LEN + 5);
    wait_ms(50);
}

/**
 * @brief Uart send bytes.
 * @param Buffer data buf
 * @param Length data length
 */
void UART_Send_Bytes(uint8_t *Buffer, uint32_t Length) {
    writePinLow(NRF_WAKEUP_PIN);
    wait_us(50);

    uart_transmit(Buffer, Length);

    wait_us(50 + Length * 30);
    writePinHigh(NRF_WAKEUP_PIN);
}

/**
 * @brief get checksum.
 * @param buf data buf
 * @param len data length
 */
uint8_t get_checksum(uint8_t *buf, uint8_t len) {
    uint8_t i;
    uint8_t checksum = 0;

    for (i = 0; i < len; i++)
        checksum += *buf++;

    checksum ^= UART_HEAD;

    return checksum;
}

/**
 * @brief Uart send report.
 * @param report_type  report_type
 * @param report_buf  report_buf
 * @param report_size  report_size
 */
void uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size) {
    if (f_dial_sw_init_ok == 0) return;
    if (dev_info.link_mode == LINK_USB) return;
    if (dev_info.rf_state != RF_CONNECT) return;

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = report_type;
    Usart_Mgr.TXDBuf[2] = 0x01;
    Usart_Mgr.TXDBuf[3] = report_size;

    memcpy(&Usart_Mgr.TXDBuf[4], report_buf, report_size);
    Usart_Mgr.TXDBuf[4 + report_size] = get_checksum(&Usart_Mgr.TXDBuf[4], report_size);

    UART_Send_Bytes(&Usart_Mgr.TXDBuf[0], report_size + 5);

    wait_us(200);
}

/**
 * @brief Uart receives data and processes it after completion,.
 */
void uart_receive_pro(void) {
    static bool rcv_start = false;

    // Receiving serial data from RF module
    while (uart_available()) {
        rcv_start = true;

        if (Usart_Mgr.RXDLen >= UART_MAX_LEN) {
            uart_read();
        }
        else {
            Usart_Mgr.RXDBuf[Usart_Mgr.RXDLen++] = uart_read();
        }

        if (!uart_available()) {
            wait_us(200);
        }
    }

    // Processing received serial port protocol
    if (rcv_start) {
        rcv_start          = false;
        Usart_Mgr.RXDState = RX_Done;
        RF_Protocol_Receive();
        Usart_Mgr.RXDLen   = 0;
    }
}

/**
 * @brief  RF uart initial.
 */
void rf_uart_init(void) {
    /* set uart buad as 460800 */
    uart_init(460800);

    /* Enable parity check */
    USART1->CR1 &= ~((uint32_t)USART_CR1_UE);
    USART1->CR1 |= USART_CR1_M0 | USART_CR1_PCE;
    USART1->CR1 |= USART_CR1_UE;

    /* set Rx and Tx pin pull up */
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR7_0);
}

/**
 * @brief RF module initial.
 */
void rf_device_init(void) {
    uint8_t timeout = 0;
    void    uart_receive_pro(void);

    timeout      = 10;
    f_rf_hand_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_HAND, 0, 20);
        wait_ms(5);
        uart_receive_pro(); // receive data
        uart_receive_pro(); // parsing data
        if (f_rf_hand_ok) break;
    }

    timeout           = 10;
    f_rf_read_data_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_READ_DATA, 0, 20);
        wait_ms(5);
        uart_receive_pro();
        uart_receive_pro();
        if (f_rf_read_data_ok) break;
    }

    timeout          = 10;
    f_rf_sts_sysc_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_RF_STS_SYSC, 0, 20);
        wait_ms(5);
        uart_receive_pro();
        uart_receive_pro();
        if (f_rf_sts_sysc_ok) break;
    }

    UART_Send_BatCfg();

    uart_send_cmd(CMD_SET_NAME, 10, 20);

    uart_send_cmd(CMD_SET_24G_NAME, 10, 20);
}
