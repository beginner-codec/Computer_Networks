[24bcs084@mepcolinux cn]$cat sender.c
#include"echo.h"
int main() {
   int sockfd;
   struct sockaddr_in server;
   sockfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
   if(sockfd<0) {
      printf("Socket Creation Failed\n");
      return 1;
   }
   server.sin_family=AF_INET;
   server.sin_port=htons(PORT);
   server.sin_addr.s_addr=inet_addr(IP);

   // FIXED: changed > 0 to < 0
   if(connect(sockfd,(struct sockaddr *)&server,sizeof(server)) < 0) {
      printf("Connection failed!\n");
      close(sockfd);
      return 1;
   }
   printf("Connected to Receiver...\n");
   int flag=1;
   while(flag){
      char s[MAX_DATA] = {0};
      printf("Message: ");
      scanf("%9s", s);
      send(sockfd, s, strlen(s), 0);
      if(strcmp(s,"STOP")==0){
              flag=0;
      }
      memset(s, 0, sizeof(s));
      recv(sockfd, s, sizeof(s) - 1, 0);
      printf("Echo from server: %s\n", s);
   }
   close(sockfd);
   return 0;
}
[24bcs084@mepcolinux cn]$cat receiver.c
#include"echo.h"
int main() {
   int serverfd;
   int clientfd;
   struct sockaddr_in server;
   serverfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
   if(serverfd<0) {
      printf("Socket creation failed!\n");
      return 1;
   }
   server.sin_family=AF_INET;
   server.sin_port=htons(PORT);
   server.sin_addr.s_addr=inet_addr(IP);
   printf("Server running at %s: %d\n",IP,PORT);
   if(bind(serverfd,(struct sockaddr *)&server,sizeof(server))<0) {
      printf("Bind failed\n");
      close(serverfd);
      return 1;
   }
   int n=listen(serverfd,1);
   if(n<0) {
      printf("Listen attempt failed\n");
      close(serverfd);
      return 1;
   }
   printf("Waiting for Sender....\n");
   clientfd=accept(serverfd,NULL,NULL);
   if(clientfd<0) {
      printf("accept failed\n");
      close(serverfd);
      return 1;
   }
   int flag=1;
   while(flag) {
      char s[MAX_DATA] = {0};
      int bytes_received = recv(clientfd, s, sizeof(s) - 1, 0);
      if (bytes_received <= 0) {
                break;
      }
      if(strcmp(s,"STOP")==0) {
         flag=0;
      }
      printf("Received and echoing: %s\n", s);
      send(clientfd, s, strlen(s), 0);
   }
   close(clientfd);
   close(serverfd);
   return 0;
}
[24bcs084@mepcolinux cn]$
