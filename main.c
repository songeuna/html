#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <wiringPi.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <termio.h>

int sendData(int fd, FILE* fp, char *ct, char *file_name);
int ledControl(int gpio, int onoff);
void sendOk(FILE* fp);
void sendError(FILE* fp);
void *gpio_function(void *arg);
void *clnt_connection(void *arg);
void *clnt_connection(void *arg);
int getch(void);
int kbhit(void);

#define PORT 8080
#define LED 28

extern "C" int captureImage(int fd);

int main(void)
{
   int serv_sock;
   pthread_t thread;
   struct sockaddr_in serv_addr, clnt_addr;
   unsigned int clnt_addr_size;

   pthread_t ptGpio;

   wiringPiSetup();

   pthread_create(&ptGpio, NULL, gpio_function, NULL);

   //1. socket()
   serv_sock = socket(AF_INET, SOCK_STREAM, 0);
   if (serv_sock == -1)
   {
      perror("Error : socket()");
      return -1;
   }

   //2. bind()
   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(PORT);

   int option = TRUE;
   setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

   if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
   {
      perror("Error : bind()");
      return -1;
   }

   //3. listen
   if (listen(serv_sock, 10) == 1)
   {
      perror("Error : listen()");
      return -1;
   }

   while (1)
   {
      int clnt_sock;

      clnt_addr_size = sizeof(clnt_addr);
      clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
      if (clnt_sock == -1)
      {
         perror("Error : accept()");
         return -1;
      }

      printf("Client : %s:\n", inet_ntoa(clnt_addr.sin_addr));

      pthread_create(&thread, NULL, clnt_connection, &clnt_sock);
      pthread_join(thread, 0);
   }
}

int sendData(int fd, FILE* fp, char *ct, char *file_name)
{
   /* Ŭ���̾�Ʈ�� ���� ������ ���� ���� �޽��� */
   char protocol[] = "HTTP/1.1 200 OK\r\n";
   char server[] = "Server:Netscape-Enterprise/6.0\r\n";
   char cnt_type[] = "Content-Type:text/html\r\n";
   char end[] = "\r\n"; /* ���� ����� ���� �׻� \r\n */
   char buf[BUFSIZ];
   int len;

   fputs(protocol, fp);
   fputs(server, fp);
   fputs(cnt_type, fp);
   fputs(end, fp);
   fflush(fp);

   /* ���� �̸��� capture.bmp�� ��� �̹����� ĸó�Ѵ�. */
   if (!strcmp(file_name, "capture.bmp"))
      captureImage(fd);

   fd = open(file_name, O_RDWR); /* ������ ����. */
   do {
      len = read(fd, buf, BUFSIZ); /* ������ �о Ŭ���̾�Ʈ�� ������. */
      fwrite(buf, len, sizeof(char), fp);
   } while (len == BUFSIZ);

   fflush(fp);

   close(fd); /* ������ �ݴ´�. */

   return 0;
}

int ledControl(int gpio, int onoff)
{
   pinMode(gpio, OUTPUT);             /* ��(Pin)�� ��� ���� */
   digitalWrite(gpio, (onoff) ? HIGH : LOW);   /* LED �Ѱ� ���� */

   return 0;
}

void sendOk(FILE* fp)
{
   /* Ŭ���̾�Ʈ�� ���� ������ ���� HTTP ���� �޽��� */
   char protocol[] = "HTTP/1.1 200 OK\r\n";
   char server[] = "Server: Netscape-Enterprise/6.0\r\n\r\n";

   fputs(protocol, fp);
   fputs(server, fp);
   fflush(fp);

}

void sendError(FILE* fp)
{
   /* Ŭ���̾�Ʈ�� ���� ���п� ���� HTTP ���� �޽��� */
   char protocol[] = "HTTP/1.1 400 Bad Request\r\n";
   char server[] = "Server: Netscape-Enterprise/6.0\r\n";
   char cnt_len[] = "Content-Length:1024\r\n";
   char cnt_type[] = "Content-Type:text/html\r\n\r\n";

   /* ȭ�鿡 ǥ�õ� HTML�� ���� */
   char content1[] = "<html><head><title>BAD Connection</tiitle></head>";
   char content2[] = "<body><font size=+5>Bad Request</font></body></html>";

   printf("send_error\n");
   fputs(protocol, fp);
   fputs(server, fp);
   fputs(cnt_len, fp);
   fputs(cnt_type, fp);
   fputs(content1, fp);
   fputs(content2, fp);
   fflush(fp);
}

void *gpio_function(void *arg)
{
   int ret = 0;
   return (void*)ret;
}

void *clnt_connection(void *arg)
{
   /* �����带 ���ؼ� �Ѿ�� arg�� int ���� ���� ��ũ���ͷ� ��ȯ�Ѵ�. */
   int clnt_sock = *((int*)arg), clnt_fd;
   FILE *clnt_read, *clnt_write;
   char reg_line[BUFSIZ], reg_buf[BUFSIZ];
   char method[10], ct[BUFSIZ], type[BUFSIZ];
   char file_name[256], file_buf[256];
   char* type_buf;
   int i = 0, j = 0, len = 0;

   /* ���� ��ũ���͸� FILE ��Ʈ������ ��ȯ�Ѵ�. */
   clnt_read = fdopen(clnt_sock, "r");
   clnt_write = fdopen(dup(clnt_sock), "w");
   clnt_fd = clnt_sock;

   /* �� ���� ���ڿ��� �о reg_line ������ �����Ѵ�. */
   fgets(reg_line, BUFSIZ, clnt_read);

   /* reg_line ������ ���ڿ��� ȭ�鿡 ����Ѵ�. */
   fputs(reg_line, stdout);

   /* �� �� ���ڷ� reg_line�� �����ؼ� ��û ������ ����(�޼ҵ�)�� �и��Ѵ�. */
   strcpy(method, strtok(reg_line, " "));
   if (strcmp(method, "POST") == 0) { /* POST �޼ҵ��� ��츦 ó���Ѵ�. */
      sendOk(clnt_write); /* �ܼ��� OK �޽����� Ŭ���̾�Ʈ�� ������. */
      fclose(clnt_read);
      fclose(clnt_write);

      return (void*)NULL;
   }
   else if (strcmp(method, "GET") != 0) { /* GET �޼ҵ尡 �ƴ� ��츦 ó���Ѵ�. */
      sendError(clnt_write); /* ���� �޽����� Ŭ���̾�Ʈ�� ������. */
      fclose(clnt_read);
      fclose(clnt_write);

      return (void*)NULL;
   }

   strcpy(file_name, strtok(NULL, " ")); /* ��û ���ο��� ���(path)�� �����´�. */
   if (file_name[0] == '/') { /* ��ΰ� ��/���� ���۵� ��� /�� �����Ѵ�. */
      for (i = 0, j = 0; i < BUFSIZ; i++) {
         if (file_name[0] == '/') j++;
         file_name[i] = file_name[j++];
         if (file_name[i + 1] == '\0') break;
      };
   }

   /* ����� ���̸� �����ϱ� ���� HTML �ڵ带 �м��ؼ� ó���Ѵ�. */
   if (strstr(file_name, "?") != NULL) {
      char optLine[32];
      char optStr[4][16];
      char opt[8], var[8];
      char* tok;
      int i, count = 0;

      strcpy(file_name, strtok(file_name, "?"));
      strcpy(optLine, strtok(NULL, "?"));

      /* �ɼ��� �м��Ѵ�. */
      tok = strtok(optLine, "&");
      while (tok != NULL) {
         strcpy(optStr[count++], tok);
         tok = strtok(NULL, "&");
      };

      /* �м��� �ɼ��� ó���Ѵ�. */
      for (i = 0; i < count; i++) {
         strcpy(opt, strtok(optStr[i], "="));
         strcpy(var, strtok(NULL, "="));
         printf("%s = %s\n", opt, var);
         if (!strcmp(opt, "led") && !strcmp(var, "On")) { /* LED�� �Ҵ�. */
            ledControl(LED, 1);
         }
         else if (!strcmp(opt, "led") && !strcmp(var, "Off")) { /* LED�� ����. */
            ledControl(LED, 0);
         }
      };
   }

   /* �޽��� ����� �о ȭ�鿡 ����ϰ� �������� �����Ѵ�. */
   do {
      fgets(reg_line, BUFSIZ, clnt_read);
      fputs(reg_line, stdout);
      strcpy(reg_buf, reg_line);
      type_buf = strchr(reg_buf, ':');
   } while (strncmp(reg_line, "\r\n", 2)); /* ��û ����� ��\r\n������ ������. */

                                 /* ������ �̸��� �̿��ؼ� Ŭ���̾�Ʈ�� ������ ������ ������. */
   strcpy(file_buf, file_name);
   sendData(clnt_fd, clnt_write, ct, file_name);

   fclose(clnt_read); /* ������ ��Ʈ���� �ݴ´�. */
   fclose(clnt_write);

   pthread_exit(0); /* �����带 �����Ų��. */

   return (void*)NULL;
}

//���;��� �Է�
int getch(void){
   int ch;
   struct termios buf, save;
   tcgetattr(0, &save);
   buf = save;
   buf.c_lflag &= ~(ICANON | ECHO);
   buf.c_cc[VMIN] = 1;
   buf.c_cc[VTIME] = 0;
   tcsetattr(0, TCSAFLUSH, &buf);
   ch = getchar();
   tcsetattr(0, TCSAFLUSH, &save);
   return ch;
}

//Ű���� ��ȣ Ȯ�� �Լ�
int kbhit(void)
{
   //�͹̳ο� ���� ����ü
   struct termios oldt, newt;
   int ch, oldf;

   //���� �͹̳ο� ������ ������ �޾ƿ´�.
   tcgetattr(STDIN_FILENO, &oldt);
   newt = oldt;

   //���� ����Է°� ���ڸ� ����, ���� ����
   newt.c_lflag &= ~(ICANON | ECHO);
   tcsetattr(STDIN_FILENO, TCSANOW, &newt);

   ch = getchar();

   //������ ������ �͹̳� �Ӽ��� �ٷ� �����Ѵ�.
   tcsetattr(STDIN_FILENO, TCSANOW, &oldt);


   return ch;
}