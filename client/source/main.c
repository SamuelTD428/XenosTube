#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xenos/xe.h>
#include <xenos/xenos.h>
#include <console/console.h>
#include <usb/usbmain.h>
#include <diskio/ata.h>
#include <libfat/fat.h>
#include <network/network.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <input/input.h>

static char server_host[128] = "192.168.1.50";
static int server_port = 8080;
static int max_results = 12;
static char default_query[64] = "xbox";

void load_config(void) {
    FILE *f = fopen("uda:/config.ini", "r");
    if (!f) {
        f = fopen("sda:/config.ini", "r");
    }
    if (!f) {
        printf("[XenosTube] config.ini not found, using default %s:%d\n", server_host, server_port);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "server_url=http://", 18) == 0) {
            char *url = line + 18;
            char *colon = strchr(url, ':');
            if (colon) {
                *colon = '\0';
                strncpy(server_host, url, sizeof(server_host) - 1);
                server_port = atoi(colon + 1);
            } else {
                strncpy(server_host, url, sizeof(server_host) - 1);
            }
        } else if (strncmp(line, "max_results=", 12) == 0) {
            max_results = atoi(line + 12);
        } else if (strncmp(line, "default_query=", 14) == 0) {
            strncpy(default_query, line + 14, sizeof(default_query) - 1);
        }
    }
    fclose(f);
    printf("[XenosTube] Loaded server: %s:%d (limit=%d)\n", server_host, server_port, max_results);
}

void fetch_test_search(void) {
    int sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[XenosTube] Failed to create socket\n");
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    addr.sin_addr.s_addr = inet_addr(server_host);

    printf("[XenosTube] Connecting to %s:%d...\n", server_host, server_port);
    if (lwip_connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[XenosTube] Connection failed! Check PC firewall and config.ini\n");
        lwip_close(sock);
        return;
    }

    char req[512];
    snprintf(req, sizeof(req),
             "GET /api/search?q=%s&limit=%d HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: XenosTube-Xbox360/1.0\r\n"
             "Connection: close\r\n\r\n",
             default_query, max_results, server_host, server_port);

    lwip_send(sock, req, strlen(req), 0);

    char buf[1024];
    int len;
    printf("[XenosTube] Response received:\n");
    while ((len = lwip_recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[len] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    lwip_close(sock);
}

int main(void) {
    xenos_init(VIDEO_MODE_AUTO);
    console_init();

    printf("====================================\n");
    printf("      XenosTube - Xbox 360          \n");
    printf("====================================\n\n");

    usb_init();
    usb_do_poll();
    fatInitDefault();

    load_config();

    printf("[XenosTube] Initializing network...\n");
    network_init();
    network_print_config();

    fetch_test_search();

    printf("\nPress A + B + X + Y or power off to exit.\n");
    while (1) {
        usb_do_poll();
        network_poll();
    }
    return 0;
}