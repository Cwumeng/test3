#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<pthread.h>
#include <termios.h>

#define SERPORT  33333
#define IPNET   "127.0.0.1"


// 全局变量 在线用户数是变化的，所以要单独存储
char online_user[10][20];
int online_size = 0;
// 全局变量 聊天记录缓冲区（暂时不存储在文件中）
char history_info[10][100];
int history_size = 0;

// 全局变量 读写线程要分离，则交互应该怎样？？？
// 正确应该用pthread_cond和pthread_mutex
// 这里用的是全局变量和sleep（）等待
int flag =  0;     // 登录成功判断
int flag2 = 0;     // 注册成功判断
int xx[2] = {0,0}; // 禁言标志xx[0]:私聊 xx[1]:群聊

// 服务器和客户端传输的结构体
struct Client
{
    int id;               // 自己id
    int toid;             // 发消息目标id
    int xxid;             // 管理员禁言目标id

    int choice;           // 功能选择：为正表示功能和成功返回； 负表示失败
    int flag;             // 登录是否成功1：账号不存在； 2密码错误； 3 登录成功

    char name[20];        // 用户名 
    char password[20];    // 密码
    char msg[100];       // 发送消息
    char online[20];      // 在线用户id; 每次接受一个，存入online_user
    char history[100];    // 聊天记录保存，从服务端接收
    char filename[20];    // 发送文件名
    char filetext[10240]; // 发送文件缓冲区
};


// 登录界面
void welcome_login()
{
    printf(" _________________________________________________\n");
    printf("|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|\n");
    printf("|                                                 |\n");
    printf("|              欢迎来到Jsetc聊天室                  |\n");
    printf("|                                                 |\n");
    printf("|_________________________________________________|\n");
    printf("|    __________           _____________________   |\n"); 
    printf("|   | __    __ |   ID账号|                     ||\n");   
    printf("|   |  _    _  |          *********************   |\n");
    printf("|   | |_|  |_| |          _____________________   |\n");
    printf("|   |__________|   US密码|                     ||\n");
    printf("|   |__________|          *********************   |\n");
    printf("|                                                 |\n");
    printf("|   1.注册      2.登录     0.退出  [admin:10000]    |\n");
    printf("|                                                 |\n");
    printf("|_________________________________________________|\n");
}

// 注册界面
void welcome_register()
{
    printf(" _________________________________________________\n");
    printf("|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|\n");
    printf("|                                                 |\n");
    printf("|                 欢迎来到注册界面                |\n");
    printf("|                                                 |\n");
    printf("|_________________________________________________|\n");
    printf("|       ____________________________________      |\n"); 
    printf("|      |请输入您的姓名:                      |    |\n");   
    printf("|       ************************************      |\n");
    printf("|       ____________________________________      |\n"); 
    printf("|      |请输入密    码:                      |    |\n");   
    printf("|       ************************************      |\n");
    printf("|       ____________________________________      |\n"); 
    printf("|      |请再次输入密码:                      |    |\n");   
    printf("|       ************************************      |\n");
    printf("|                                                 |\n");
    printf("|_________________________________________________|\n");
}

// 普通用户聊天室界面
void user_func(struct Client sclient)
{
     printf(" _________________JSETC聊天室_____________________\n");
    printf("|   _______                                       |\n");
    printf("|  |       |   用户名:%-11s                  |\n",sclient.name);
    printf("|  |  ^A^  |   用户id:%-11d                  |\n",sclient.id);
    printf("|  |_______|                                      |\n");
    printf("|                                                 |\n");
    printf("|—————————————————————————————————————————————————|\n");
    printf("|               在线用户列表                        |\n"); 
    printf("|—————————————————————————————————————————————————|\n");   
    printf("|  %-12s                                   |\n",online_user[0]);
    printf("|  %-12s                                   |\n",online_user[1]);
    printf("|  %-12s                                   |\n",online_user[2]);
    printf("|  %-12s                                   |\n",online_user[3]);
    printf("|  %-12s                                   |\n",online_user[4]);
    printf("|  %-12s                                   |\n",online_user[5]);
    printf("|  %-12s                                   |\n",online_user[6]);
    printf("|  %-12s                                   |\n",online_user[7]);
    printf("|  %-12s                                   |\n",online_user[8]);
    printf("|  %-12s                                   |\n",online_user[9]);
    printf("|—————————————————————————————————————————————————| \n");
    printf("|              你的身份是:普通用户                    | \n");
    printf("|—————————————————————————————————————————————————| \n");
    printf("|      4.私聊                3.查询在线用户          | \n");
    printf("|      5.群聊                6.读取聊天记录          |\n");
    printf("|      7.退出                13.发送文件            | \n");
    printf("|_________________________________________________|\n");
}


// 管理员聊天室界面
void root_func(struct Client sclient)
{
    printf(" _________________________________________________\n");
    printf("|   _______                                       |\n");
    printf("|  |       |   用户名:%-11s                  |\n",sclient.name);
    printf("|  |  ^A^  |   用户id:%-11d                  |\n",sclient.id);
    printf("|  |_______|                                      |\n");
    printf("|                                                 |\n");
    printf("|—————————————————————————————————————————————————|\n");
    printf("|               在线用户列表                        |\n"); 
    printf("|—————————————————————————————————————————————————|\n");   
    printf("|  %-12s                                   |\n",online_user[0]);
    printf("|  %-12s                                   |\n",online_user[1]);
    printf("|  %-12s                                   |\n",online_user[2]);
    printf("|  %-12s                                   |\n",online_user[3]);
    printf("|  %-12s                                   |\n",online_user[4]);
    printf("|  %-12s                                   |\n",online_user[5]);
    printf("|  %-12s                                   |\n",online_user[6]);
    printf("|  %-12s                                   |\n",online_user[7]);
    printf("|  %-12s                                   |\n",online_user[8]);
    printf("|  %-12s                                   |\n",online_user[9]);
    printf("|—————————————————————————————————————————————————| \n");
    printf("|              你的身份是:管理员                     | \n");
    printf("|—————————————————————————————————————————————————| \n");
    printf("|      4.私聊                3.查询在线用户          | \n");
    printf("|      5.群聊                6.读取聊天记录          | \n");
    printf("|      8.禁言                9.全体禁言             | \n");
    printf("|     10.解禁               11.全体解禁             | \n");
    printf("|     12.踢人               13.发送文件             | \n");
    printf("|     7.退出                                       | \n");
    printf("|_________________________________________________|\n");
}

// 回显密码（网上写的）
int getch(void)
{
    int ch;
    struct termios tm, tm_old;
    tcgetattr(STDIN_FILENO, &tm);
    tm_old = tm;
    tm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tm);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &tm_old);
    return ch;
}

// 发送文件
void send_file(int sockfd, struct Client client)
{
    FILE *fp;   // 文件描述符
    char ch;    // 按字节读取文件
    int i = 0;  // 下标

    printf("发送对象的ID账号:");
    scanf("%d", &client.toid);
    printf("发送的本地文件名:");
    scanf("%s", client.filename);

    if ((fp = fopen(client.filename, "r")) == NULL)
    {
        perror("error!\n");
        return;
    }

    // 文件读取到client.filetext缓冲区
    ch = fgetc(fp);
    while (ch != EOF)
    {   
        client.filetext[i] = ch;
        ch  = fgetc(fp);
        i++;
    }
    client.filetext[i] = '\0';

    send(sockfd, &client, sizeof(client), 0);

    fclose(fp);
}


// 接收文件
void recv_file(struct Client sclient)
{
    FILE *fp;
    int i = 0;

    strcat(sclient.filename,"-my");
    if ((fp = fopen(sclient.filename,"w+")) == NULL)
    {
        printf("error!\n");
        return;
    }

    while (sclient.filetext[i] != '\0')
    {
        fputc(sclient.filetext[i], fp);
        i++;
    }

    fclose(fp);
}


// 从服务器接收client后，用户选择功能
void login_in_chatroom(int sockfd, struct Client sclient)
{
    // 不修改从服务端接收的sclient, 用户发送新的client给服务器
    // 重新组建自己的client
    struct Client client = sclient;


    //进入管理员系统
    if (sclient.id == 10000)
    {

        while (1)
        {
            sleep(0.5);
            system("clear");
            root_func(client);

            printf("请输入你的选择:");
            scanf("%d", &client.choice);

            switch (client.choice)
            {
                // 查询在线用户
                case 3:
                {
                    online_size = 0;
                    memset(online_user, 0, sizeof(online_user));

                    send(sockfd, &client, sizeof(client), 0);

                    sleep(0.5); 
                    
                    break;
                }


                // 私聊
                case 4:
                {
                    system("clear");
                    root_func(client);

                    printf("你想发给谁:");
                    scanf("%d",&client.toid);

                    printf("请输入消息内容:");
                    memset(client.msg, 0, sizeof(client.msg));
                    scanf("%s",client.msg);

                    send(sockfd,&client,sizeof(client),0);

                    break; 
                }


                // 群聊
                case 5:
                {  
                    system("clear");
                    root_func(client);

                    printf("请输入群发消息内容:");
                    memset(client.msg, 0, sizeof(client.msg));
                    scanf("%s",client.msg);

                    send(sockfd,&client,sizeof(client),0);                

                    break; 
                }


                // 读取自己的聊天记录
                case 6:
                {
                    history_size = 0;
                    memset(history_info, 0, sizeof(history_info));

                    send(sockfd, &client, sizeof(client), 0);

                    sleep(0.5); 

                    for (int i = 0; i < history_size; i++)
                    {
                        printf("第%d条聊天记录：%s\n",i, history_info[i]);
                    }

                    break; 
                }


                // 管理员退出登录
                case 7:
                {
                    send(sockfd,&client,sizeof(client),0);

                    printf("用户名:%-11s  用户id:%-11d   退出登录\n", client.name, client.id);
                    printf("        您已经退出登录\n");
                    sleep(1);
                    system("clear");
                    pthread_exit(NULL);
                    
                    return;
                }

                // 将xxid禁言
                case 8:
                {
                    system("clear");
                    root_func(client);

                    printf("您想将谁禁言:");
                    scanf("%d", &client.xxid);

                    send(sockfd, &client, sizeof(client), 0);

                    break; 
                }


                // 将全体用户禁言
                case 9:
                {
                    system("clear");
                    root_func(client);

                    printf("您已将全体禁言:");
                    send(sockfd, &client, sizeof(client), 0); 

                    break; 
                }


                //解除xxid禁言
                case 10:
                {
                    
                    system("clear");
                    root_func(client);

                    printf("您想解除谁的禁言:");
                    scanf("%d", &client.xxid);

                    send(sockfd, &client, sizeof(client), 0);

                    break;
                }


                // 解除全体禁言
                case 11:
                {
                    system("clear");
                    root_func(client);

                    printf("您已解除全体禁言:");
                    send(sockfd, &client, sizeof(client), 0);

                    break;
                }


                //管理员踢人
                case 12:
                {
                    system("clear");
                    root_func(client);

                    printf("请输入您想踢掉的用户:");
                    scanf("%d", &client.toid);

                    send(sockfd, &client, sizeof(client), 0);

                    break;
                }


                // 发送文件
                case 13:
                {
                    system("clear");
                    root_func(client);

                    send_file(sockfd,client);

                    break;
                }
            }
        }
    }


    //进入普通用户系统
    else
    {
         while (1)
        {
            sleep(0.5);
            system("clear");
            user_func(client);


            printf("请输入你的选择:");

            scanf("%d",&client.choice);
            
            switch (client.choice)
            {
                // 查询在线用户
                case 3:
                {
                    online_size = 0;
                    memset(online_user, 0, sizeof(online_user));

                    send(sockfd, &client, sizeof(client), 0);

                    sleep(0.5); 
                    
                    break;
                }


                // 私聊
                case 4:
                {
                    system("clear");
                    user_func(client);

                    if (xx[0] == 1)
                    {
                        sleep(0.6);
                        printf("您已经被禁言!\n");
                    }
                    else
                    {
                        printf("你想发给谁:");
                        scanf("%d",&client.toid);

                        printf("请输入消息内容:");
                        memset(client.msg, 0, sizeof(client.msg));
                        scanf("%s",client.msg);

                        send(sockfd, &client, sizeof(client), 0);
                    }
                   
                    break; 
                }


                // 群聊
                case 5:
                { 
                    system("clear");
                    user_func(client);

                    if (xx[1] == 1)
                    {
                        sleep(0.6);
                        printf("您已经被禁言!\n");
                    }
                    else
                    {
                        printf("请输入群发消息内容:");
                        memset(client.msg, 0, sizeof(client.msg));
                        scanf("%s", client.msg);
                        
                        send(sockfd , &client, sizeof(client), 0);
                    }

                    break; 
                }


                // 读取自己的聊天记录
                case 6:
                {
                    history_size = 0;
                    memset(history_info, 0, sizeof(history_info));

                    send(sockfd, &client, sizeof(client), 0);

                    sleep(0.5); 

                    for (int i = 0; i < history_size; i++)
                    {
                        printf("第%d条聊天记录：%s\n",i, history_info[i]);
                    } 

                    break; 
                }


                // 普通用户退出登录
                case 7:
                {
                    send(sockfd, &client, sizeof(client), 0);
                    
                    printf("用户名:%-11s  用户id:%-11d   退出登录\n", client.name, client.id);
                    printf("        您已经退出登录\n");
                    sleep(1);
                    system("clear");
                    pthread_exit(NULL);

                    break;
                }


                // 发送文件
                case 13:
                {
                    system("clear");
                    user_func(client);

                    send_file(sockfd,client);

                    break;
                }
            }
        }
    }
}


//发送给服务端
void *send_msg(void *arg)
{
    int sockfd = *(int *)arg;
    struct Client client;

    while (1)
    {
        system("clear");
        welcome_login();

        sleep(1);
        printf("\033[2A");
        printf("\033[4C");
        printf("请输入你的选择:");
        scanf("%d",&client.choice);

        switch (client.choice)
        {
            // 退出注册登录界面
            case 0:
            {
                system("clear");
                send(sockfd,&client,sizeof(client),0);
                pthread_exit(NULL);
            }


            //注册
            case 1:
            {
                char pwd[20];    // 重复验证密码

                do
                {
                    system("clear");
                    welcome_register();

                    printf("\033[24C");
                    printf("\033[10A");
                    scanf("%s",client.name);  //输入后还有回车，所以只下降2B
                    
                    printf("\033[2B");
                    printf("\033[24C");
                    scanf("%s",client.password);

                    printf("\033[2B");
                    printf("\033[24C");
                    scanf("%s", pwd);

                    if (strcmp(client.password, pwd) != 0)
                    {
                        printf("\033[1B");
                        printf("\033[10C");
                        printf("两次输入的密码不一致!请重新输入\n");
                        sleep(2);
                    }
                } while (strcmp(client.password, pwd) != 0);

                srand((unsigned)time(NULL));
                client.id = 10000000 + rand() % 90000000;

                send(sockfd,&client,sizeof(client),0);
                
                break;
            }

             
            //登录
            case 2:
            {
                printf("\033[9A");
                printf("\033[27C");
                scanf("%d",&client.id);

                getchar();
                printf("\033[2B");
                printf("\033[27C");
                char ch[20];
                char c;
                int i = 0;
                while (1)
                {
                    c = getch();
                    if (c == '\n')
                    {
                        break;
                    }
                    else if (c == '\b')
                    {
                        printf("\b\b ");
                        i--;
                    }
                    else
                    {
                        ch[i++] = c;
                        printf("*");
                    }
                }
                ch[i] = '\0';
                strcpy(client.password, ch);

                send(sockfd, &client, sizeof(client), 0);
                
                sleep(1);
                // flag=3表示服务器注册信息正确，可以继续
                if (flag == 3)
                {   
                    login_in_chatroom(sockfd,client);
                }

                break;
            }
        }
    }

    pthread_exit(NULL);
}


// 接收服务端
void *recv_msg(void *arg)
{
    char **result;
    int sockfd = *(int *)arg;
    struct Client client;
    int n;     //接收client实际字节数

    while (1)
    {
        n = recv(sockfd, &client, sizeof(client), 0);

        // 服务器发送的sclient什么都没有，则接收线程结束
        if (n == 0)
        {
            break;
        }

        switch (client.choice)
        {
            // 注册是否成功
            case 1:
            {
                flag2 = client.flag;
                if (flag2 == 1)
                {
                    sleep(0.5);
                    printf("          注册成功!");
                    printf("你的账号： %d\n", client.id);
                }
                else
                {
                    sleep(0.5);
                    printf("          注册失败!");
                    printf("你的账号未分配！\n");
                }

                break;
            }


            // 登录是否成功
            case 2:
            {
                flag = client.flag;
                if(flag == 1)
                {
                    printf("账号不存在请先注册!\n");
                }
                else if(flag == 2)
                {
                    printf("密码错误请重新输入!\n");
                }
                else
                {
                    printf("\033[6B");
                    printf(" 登陆成功!  loading.....\n");
                }

                break;
            }

            // 接收在线onlinec成员ID
            case 3:
            {
                stpcpy(online_user[online_size], client.online);
                online_size++;

                break;
            }


            // 接收私聊
            case 4:
            {
                printf("message from %d:%s\n", client.id, client.msg);

                break;
            }


            // 接收群聊
            case 5:
            {
                printf("message from %d:%s\n",client.id,client.msg);  

                break;
            }


            // 读取聊天记录
            case 6:
            {
                stpcpy(history_info[history_size], client.history);
                history_size++;

                break;
            }


            // 管理员设置私聊禁言
            case 8:
            {
                xx[0] = 1;
                xx[1] = 1;
                printf("您已经被禁言!\n");
        
                break;
            }


            // 管理员设置群聊禁言
            case 9:
            {
                xx[0] = 1;
                xx[1] = 1;
                printf("您已经被禁言!\n");

                break;
            }


            // 管理员解除私聊禁言
            case 10:
            {
                xx[0] = 0;
                xx[1] = 0;
                printf("您被解除禁言!\n");

                break;
            }


            // 管理员解除群聊禁言
            case 11:
            {
                xx[0] = 0;
                xx[1] = 0;
                printf("您被解除禁言!\n");

                break;
            }

            // 被管理员强制下线
            case 12:
            {
                if (client.flag == 2)
                {
                    printf("%s\n", client.msg);
                }
                else
                {
                    printf("您已被强制下线\n");

                    sleep(0.5);
                
                    pthread_exit(NULL);;
                }

                break;
            }


            // 接收文件
            case 13:
            {
                recv_file(client);
                printf("来自%d的%s文件接收成功！\n", client.id, client.filename);
                break;
            }
        }   
    }

    pthread_exit(NULL);
}


// 两个线程初始化
void pthread_init()
{
    // 创建sockfd
    int sockfd;
    sockfd = socket(AF_INET,SOCK_STREAM,0);


    // 初始化sockfd的服务器address
    struct sockaddr_in seraddr;
    bzero(&seraddr,sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = inet_addr(IPNET);//IP发送到网络
    seraddr.sin_port = htons(SERPORT);    //端口发送到网络


    // sockfd连接好服务器address
    connect(sockfd, (struct sockaddr*)&seraddr, sizeof(seraddr));
    printf("如果connect返回SQL_OK, 则连接成功\n");
    
    pthread_t send_tid;
    pthread_t recv_tid;
    pthread_create(&send_tid,NULL,(void *)send_msg,(void *)&sockfd);
    pthread_create(&recv_tid,NULL,(void *)recv_msg,(void *)&sockfd);

    pthread_detach(send_tid);      //主线程不等send结束，由系统回收资源
    pthread_join(recv_tid,NULL);   //主线程等recv结束后，主线程来回收资源

    close(sockfd);  
}

int main()
{
    pthread_init();

    return 0;
}