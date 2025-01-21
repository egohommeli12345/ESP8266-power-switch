#include "osapi.h"
#include "user_interface.h"
#include "uart.h"
#include "ets_sys.h"
#include "espconn.h"
#include "mem.h"
#include "wifi_credentials.h"


uint32 user_rf_cal_sector_set(void) {
    // This value depends on the size of the flash chip you're using.
    // For example, for a 4MB (32 Mbit) flash:
    uint32 rf_cal_sector = 0;
    enum flash_size_map size_map = system_get_flash_size_map();
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sector = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sector = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sector = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sector = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sector = 2048 - 5;
            break;

        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sector = 4096 - 5;
            break;

        default:
            rf_cal_sector = 0;
            break;
    }
    return rf_cal_sector;
}


#define MAX_BLINKS 255

static os_timer_t timer;
static uint8 blink_count = 0;

void led() {
    if (blink_count < MAX_BLINKS) {
        // Toggle the LED
        if (blink_count % 2 == 0) {
            gpio_output_set(BIT2, 0, BIT2, 0); // Turn the LED on
        } else {
            gpio_output_set(0, BIT2, BIT2, 0); // Turn the LED off
        }

        blink_count++; // Increment the blink counter
    } else {
        os_timer_disarm(&timer); // Stop the timer after MAX_BLINKS
    }
    os_printf_plus("%u, ", blink_count);
}

void powerState(uint8_t c) {
    if(c == 49) {
//        gpio_output_set(BIT2, 0, BIT2, 0);
        gpio_output_set(BIT5, 0, BIT5, 0);
    } else if(c == 48) {
//        gpio_output_set(0, BIT2, BIT2, 0);
        gpio_output_set(0, BIT5, BIT5, 0);
    }
}

static char uart_rx_buffer[128];
static uint16_t uart_rx_index = 0;

void uart0_rx_intr_handler(void *para) {
    uint8_t received_char;

    // Read data while available in the UART RX FIFO
    while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
        // Read one character from the UART FIFO
        received_char = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;

        // Store the character in the buffer (basic handling, modify as needed)
        if (uart_rx_index < UART_RX_BUFFER_SIZE - 1) {
            uart_rx_buffer[uart_rx_index++] = received_char;
        }

        // Optionally, echo the received character
        uart_tx_one_char(UART0, received_char);

        powerState(received_char);
    }

    // Clear the UART RX interrupt
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR | UART_RXFIFO_TOUT_INT_CLR);
}

void DisconnectTask(os_event_t *e) {
    struct espconn* conn = (struct espconn*)e->par;
    espconn_disconnect(conn);
}

void Respond() {

}

// Callback for receiving data
void PrintReceived(void *arg, char *pdata, unsigned short len) {
    struct espconn* conn = (struct espconn*)arg;
    bool found = false;
    const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Connection: close\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

    const char html_response[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "Content-Length: 1040\r\n"
            "\r\n"
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "  <meta charset=\"UTF-8\">\n"
            "  <title>Title</title>\n"
            "  <style>\n"
            "    body {\n"
            "      display: flex;\n"
            "      flex-direction: column;\n"
            "      align-items: center;\n"
            "      width: 100%;\n"
            "    }\n"
            "    .btns {\n"
            "      display: grid;\n"
            "      grid-template-columns: 1fr 1fr;\n"
            "      column-gap: 1rem;\n"
            "      width: 100%;\n"
            "    }\n"
            "    button {\n"
            "      font-size: 3rem;\n"
            "    }\n"
            "    .currstate {\n"
            "      width: 100%;\n"
            "    }\n"
            "  </style>\n"
            "  <script>\n"
            "    const SwitchPowerState = async (state) => {\n"
            "      console.log(state);\n"
            "      if(state) {\n"
            "        document.getElementById(\"currstate\").innerText = \"ON\";\n"
            "        await fetch(\"http://192.168.0.108/1\");\n"
            "      } else {\n"
            "        document.getElementById(\"currstate\").innerText = \"OFF\";\n"
            "        await fetch(\"http://192.168.0.108/0\");\n"
            "      }\n"
            "    }\n"
            "  </script>\n"
            "</head>\n"
            "<body>\n"
            "  <div class=\"btns\">\n"
            "    <button onclick=\"SwitchPowerState(true)\">On</button>\n"
            "    <button onclick=\"SwitchPowerState(false)\">Off</button>\n"
            "  </div>\n"
            "  <div class=\"currstate\">\n"
            "    <h1 id=\"currstate\">No state</h1>\n"
            "  </div>\n"
            "</body>\n"
            "</html>";

    if (pdata == NULL || len == 0) {
        os_printf("No data received or pdata is NULL\n");
        return;
    }
    os_printf("Data length: %d\n", len);
    os_printf("Data received:\n\n%s\n\n", pdata);
    os_printf("Data (as hex): ");
    for (int i = 0; i < len; i++) {
        os_printf("%02X ", pdata[i] & 0xFF);

        if(pdata[i] == 0x2F && found == false) {
            os_printf("\n%c\n", pdata[i + 1]);

            if(pdata[i + 1] == 0x30) {
                espconn_send(conn, (uint8*)response, os_strlen(response));
                powerState('0');
            } else if(pdata[i + 1] == 0x31) {
                espconn_send(conn, (uint8*)response, os_strlen(response));
                powerState('1');
            } else if(pdata[i + 1] == 0x20) {
                espconn_send(conn, (uint8*)html_response, os_strlen(html_response));
            }
            found = true;
        }
    }

    // If raw TCP packet
    os_printf("\n");
    if(pdata[0] == 49) {
        os_printf("ON\n");
        powerState(pdata[0]);
    } else if(pdata[0] == 48) {
        os_printf("OFF\n");
        powerState(pdata[0]);
    }

    system_os_post(0, 0, (os_param_t)conn);
}

void CreateTcpSocket() {
    struct espconn* espconn =
            (struct espconn*) os_zalloc(sizeof(struct espconn));
    espconn->type = ESPCONN_TCP;
    espconn->state = ESPCONN_NONE;

    espconn->proto.tcp = (esp_tcp*) os_zalloc(sizeof(esp_tcp));
    if(espconn->proto.tcp == NULL) {
        os_free(espconn);
        os_printf("Failed to allocate memory for esp_tcp\n");
        return;
    }

    espconn->proto.tcp->local_port = 80;

    espconn_regist_recvcb(espconn, PrintReceived);

    sint8 res = espconn_accept(espconn);
    if(res != ESPCONN_OK) {
        os_printf("Failed to start TCP server, error code: %d\n", res);
        os_free(espconn->proto.tcp);
        os_free(espconn);
    } else {
        os_printf("TCP server started, listening on port 80\n");
    }
}

static os_event_t* taskQ[1];
void user_init(void) {
    system_os_task(DisconnectTask, 0, taskQ, 1);

    uart_init(BIT_RATE_115200, BIT_RATE_115200); // Initialize UART0 at 115200 baud
    ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, NULL); // Attach the RX interrupt handler
    ETS_UART_INTR_ENABLE(); // Enable the UART RX interrupt
    
    gpio_init();
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

//    os_timer_setfn(&timer, led, NULL);
//    os_timer_arm(&timer, 50, 1);

    wifi_set_opmode_current(0x01);

    // SSID and PASSWORD defined in wifi_credentials.h
    char ssid[32] = WIFI_SSID;
    char password[64] = WIFI_PASSWORD;
    struct station_config stationConfig;

    stationConfig.bssid_set = 0;
    os_memcpy(&stationConfig.ssid, ssid, 32);
    os_memcpy(&stationConfig.password, password, 64);

    wifi_station_set_config_current(&stationConfig);
    bool connected = wifi_station_connect();
    os_printf("Connected: %d", connected);

    CreateTcpSocket();
}
