#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <ctype.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 256

// Cấu trúc lưu trữ trạng thái của từng Client
typedef struct {
    int fd;          
    int state;         
    char name[256];     
} ClientState;

// Hàm tạo email chuẩn HUST
void generate_hust_email(char *fullname, char *mssv, char *email_out) {
    char initials[20] = "";
    char lastname[50] = "";
    char temp_name[256];
    
    strcpy(temp_name, fullname);
    
    // Tách các từ trong họ tên
    char *words[20];
    int word_count = 0;
    char *token = strtok(temp_name, " \r\n");
    while (token != NULL) {
        words[word_count++] = token;
        token = strtok(NULL, " \r\n");
    }

    if (word_count > 0) {
        strcpy(lastname, words[word_count - 1]);
        for (int i = 0; lastname[i]; i++) {
            lastname[i] = tolower(lastname[i]);
        }

        for (int i = 0; i < word_count - 1; i++) {
            int len = strlen(initials);
            initials[len] = tolower(words[i][0]);
            initials[len + 1] = '\0';
        }
    }

    // Lấy 6 số cuối của MSSV 
    char short_mssv[10] = "";
    if (strlen(mssv) >= 6) {
        strcpy(short_mssv, mssv + strlen(mssv) - 6);
    } else {
        strcpy(short_mssv, mssv); 
    }

    // Ghép thành email hoàn chỉnh
    sprintf(email_out, "%s.%s%s@sis.hust.edu.vn\n", lastname, initials, short_mssv);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Sử dụng: %s <Cổng>\n", argv[0]);
        return 1;
    }

    int server_sk = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sk, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(argv[1]));

    bind(server_sk, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sk, 5);

    printf("[Server] Đang chạy chế độ Non-blocking tại cổng %s...\n", argv[1]);

    // Khởi tạo mảng quản lý các Client
    ClientState clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0; // 0 nghĩa là slot này đang trống
    }

    fd_set readfds;
    int max_sd;

    while (1) {
        // Xóa tập hợp socket và thêm server_sk vào tập hợp
        FD_ZERO(&readfds);
        FD_SET(server_sk, &readfds);
        max_sd = server_sk;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        // Gọi select() để chờ có sự kiện mạng (hàm này sẽ chờ ở đây cho đến khi có luồng I/O)
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Lỗi select");
            continue;
        }

        if (FD_ISSET(server_sk, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int new_socket = accept(server_sk, (struct sockaddr *)&client_addr, &len);

            printf("[Server] Có kết nối mới từ %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            char *msg = "Vui long nhap Ho ten cua ban: ";
            send(new_socket, msg, strlen(msg), 0);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].state = 0; // Bắt đầu ở trạng thái chờ Họ tên
                    break;
                }
            }
        }

        //Kiểm tra xem có Client CŨ nào vừa gửi tin nhắn đến không
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;

            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int valread = recv(sd, buffer, sizeof(buffer) - 1, 0);

                if (valread == 0) {
                    printf("[Server] Client ngắt kết nối đột ngột.\n");
                    close(sd);
                    clients[i].fd = 0;
                } else {
                    buffer[valread] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (clients[i].state == 0) {
                        strcpy(clients[i].name, buffer);
                        clients[i].state = 1; // Chuyển sang chờ MSSV
                        
                        char *msg = "Vui long nhap MSSV cua ban: ";
                        send(sd, msg, strlen(msg), 0);

                    } else if (clients[i].state == 1) {
                        char email[256];
                        generate_hust_email(clients[i].name, buffer, email);
                        
                        char response[512];
                        sprintf(response, "-> Email cua ban la: %s", email);
                        send(sd, response, strlen(response), 0);
                        
                        printf("[Log] Đã cấp email: %s", email);

                        close(sd);
                        clients[i].fd = 0;
                    }
                }
            }
        }
    }
    return 0;
}