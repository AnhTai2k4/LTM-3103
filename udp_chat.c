#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Kiểm tra tham số dòng lệnh
    if (argc != 4) {
        printf("Sử dụng: %s <port_s> <ip_d> <port_d>\n", argv[0]);
        return 1;
    }

    int port_s = atoi(argv[1]); // Cổng nhận (local)
    char *ip_d = argv[2];       // IP đích
    int port_d = atoi(argv[3]); // Cổng đích

    // Tạo socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Không thể tạo socket");
        return 1;
    }

    // Thiết lập địa chỉ nhận (Local) và Bind
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port_s);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Lỗi bind cổng nhận");
        return 1;
    }

    // Thiết lập địa chỉ gửi (Destination)
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(ip_d);
    dest_addr.sin_port = htons(port_d);

    printf("UDP Chat đang chạy...\n");
    printf("- Nhận dữ liệu tại cổng: %d\n", port_s);
    printf("- Gửi dữ liệu tới: %s:%d\n", ip_d, port_d);
    printf("------------------------------------------\n");

    char buffer[BUFFER_SIZE];
    fd_set readfds;

    while (1) {
        // Xóa và thiết lập tập hợp các file descriptor cần theo dõi
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds); // Theo dõi bàn phím (stdin)
        FD_SET(sockfd, &readfds);       // Theo dõi socket (mạng)

        int activity = select(sockfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Lỗi select");
            break;
        }

        // Có dữ liệu từ bàn phím (Người dùng muốn GỬI)
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                sendto(sockfd, buffer, strlen(buffer), 0,
                       (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            }
        }

        // Có dữ liệu từ socket (Có tin nhắn ĐẾN)
        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in from_addr;
            socklen_t addr_len = sizeof(from_addr);
            int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                             (struct sockaddr *)&from_addr, &addr_len);
            
            if (n > 0) {
                buffer[n] = '\0';
                printf("[Tin nhắn từ %s]: %s", inet_ntoa(from_addr.sin_addr), buffer);
            }
        }
    }

    close(sockfd);
    return 0;
}