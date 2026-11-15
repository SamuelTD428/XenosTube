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
#include <lwip/tcp.h>
#include <lwip/ip_addr.h>
#include <input/input.h>
#include <time/time.h>

#define XENOSTUBE_VERSION "1.0.0"
#define XENOSTUBE_CODENAME "Jashalla"
#define MAX_VIDEOS 20

typedef struct {
    char id[24];
    int duration;
    char title[64];
} VideoItem;

static VideoItem videos[MAX_VIDEOS];
static int video_count = 0;
static int selected_index = 0;

static char server_host[128] = "192.0.2.1";
static int server_port = 8080;
static int max_results = 12;
static char default_query[64] = "xbox";

/* Raw lwIP TCP transfer state */
static char http_response[8192];
static int http_total = 0;
static volatile int http_done = 0;
static volatile int http_err = 0;
static char pending_request[512];

void load_config(void) {
    FILE *f = fopen("uda:/config.ini", "r");
    if (!f) f = fopen("sda:/config.ini", "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == ';' || line[0] == '#' || line[0] == '[') continue;

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
            if (max_results > MAX_VIDEOS) max_results = MAX_VIDEOS;
        } else if (strncmp(line, "default_query=", 14) == 0) {
            strncpy(default_query, line + 14, sizeof(default_query) - 1);
        }
    }
    fclose(f);
}

static err_t on_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        /* Remote host closed connection -> full response received */
        tcp_close(tpcb);
        http_done = 1;
        return ERR_OK;
    }

    struct pbuf *q;
    for (q = p; q != NULL; q = q->next) {
        int space = (int)sizeof(http_response) - 1 - http_total;
        if (space > 0) {
            int to_copy = (q->len < space) ? q->len : space;
            memcpy(http_response + http_total, q->payload, to_copy);
            http_total += to_copy;
        }
    }
    http_response[http_total] = '\0';

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void on_tcp_err(void *arg, err_t err) {
    http_err = 1;
    http_done = 1;
}

static err_t on_tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err != ERR_OK) {
        http_err = 1;
        http_done = 1;
        return err;
    }
    tcp_recv(tpcb, on_tcp_recv);
    tcp_write(tpcb, pending_request, strlen(pending_request), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    return ERR_OK;
}

void fetch_videos(const char *query) {
    video_count = 0;
    selected_index = 0;
    http_total = 0;
    http_done = 0;
    http_err = 0;
    http_response[0] = '\0';

    ip_addr_t dest_ip;
    if (!ipaddr_aton(server_host, &dest_ip)) {
        return;
    }

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;

    snprintf(pending_request, sizeof(pending_request),
             "GET /api/list?q=%s&limit=%d HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: XenosTube-Xbox360/%s (%s)\r\n"
             "Connection: close\r\n\r\n",
             query, max_results, server_host, server_port,
             XENOSTUBE_VERSION, XENOSTUBE_CODENAME);

    tcp_err(pcb, on_tcp_err);
    if (tcp_connect(pcb, &dest_ip, server_port, on_tcp_connected) != ERR_OK) {
        tcp_abort(pcb);
        return;
    }

    /* Poll network stack until transfer completes or ~5s timeout */
    int ticks = 0;
    while (!http_done && ticks < 500) {
        network_poll();
        mdelay(10);
        ticks++;
    }

    if (!http_done) {
        tcp_abort(pcb);
        return;
    }
    if (http_err || http_total == 0) return;

    char *body = strstr(http_response, "\r\n\r\n");
    if (!body) return;
    body += 4;

    char *line = strtok(body, "\n");
    while (line && video_count < max_results) {
        line[strcspn(line, "\r")] = 0;
        char *p1 = strchr(line, '|');
        char *p2 = p1 ? strchr(p1 + 1, '|') : NULL;
        if (p1 && p2) {
            *p1 = '\0';
            *p2 = '\0';
            strncpy(videos[video_count].id, line, sizeof(videos[video_count].id) - 1);
            videos[video_count].duration = atoi(p1 + 1);
            strncpy(videos[video_count].title, p2 + 1, sizeof(videos[video_count].title) - 1);
            video_count++;
        }
        line = strtok(NULL, "\n");
    }
}

void render_ui(const char *status_msg) {
    printf("\033[2J\033[H");
    printf("==================================================================\n");
    printf("  XenosTube v%s (%s)  |  Server: %s:%d  |  Query: %s\n",
           XENOSTUBE_VERSION, XENOSTUBE_CODENAME, server_host, server_port, default_query);
    printf("==================================================================\n\n");

    if (video_count == 0) {
        printf("  No videos loaded. Check server connection and press [X] to retry.\n");
    } else {
        for (int i = 0; i < video_count; i++) {
            int mins = videos[i].duration / 60;
            int secs = videos[i].duration % 60;
            printf("  %s [%02d:%02d] %s\n",
                   (i == selected_index) ? "->" : "  ",
                   mins, secs, videos[i].title);
        }
    }

    printf("\n------------------------------------------------------------------\n");
    printf("  [D-Pad Up/Down] Navigate   [A] Select   [X] Refresh\n");
    if (status_msg && status_msg[0]) {
        printf("  Status: %s\n", status_msg);
    }
}

int main(void) {
    xenos_init(VIDEO_MODE_AUTO);
    console_init();

    usb_init();
    usb_do_poll();
    fatInitDefault();
    load_config();

    network_init();
    fetch_videos(default_query);
    render_ui("Ready.");

    struct controller_data_s pad, prev_pad;
    memset(&prev_pad, 0, sizeof(prev_pad));

    while (1) {
        usb_do_poll();
        network_poll();

        if (get_controller_data(&pad, 0)) {
            if (pad.down && !prev_pad.down && video_count > 0) {
                selected_index = (selected_index + 1) % video_count;
                render_ui("");
            }
            if (pad.up && !prev_pad.up && video_count > 0) {
                selected_index = (selected_index - 1 + video_count) % video_count;
                render_ui("");
            }
            if (pad.x && !prev_pad.x) {
                render_ui("Refreshing feed...");
                fetch_videos(default_query);
                render_ui("Feed updated.");
            }
            if (pad.a && !prev_pad.a && video_count > 0) {
                char msg[160];
                snprintf(msg, sizeof(msg), "Selected ID: %s (Ready for playback hook)", videos[selected_index].id);
                render_ui(msg);
            }
            prev_pad = pad;
        }
        mdelay(16);
    }
    return 0;
}
