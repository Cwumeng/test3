#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>
#include<sqlite3.h>
#include<signal.h>

#define SERPORT  33333

// 服务器和客户端传输的结构体
struct Client
{
    int id;               // 自己id
    int toid;             // 发消息目标id
    int xxid;             // 管理员禁言目标id

    int choice;           // 功能选择
    int flag;             // 注册       1：成功
                          // 登录是否成功1：账号不存在； 2密码错误； 3 登录成功

    char name[20];        // 用户名 
    char password[20];    // 密码
    char msg[100];        // 发送消息
    char online[20];      // 在线用户id; 每次接受一个，存入online_user
    char history[100];    // 聊天记录保存，从服务端接收
    char filename[20];    // 发送文件名
    char filetext[10240]; // 发送文件缓冲区
};

// 登录用户在线链表
typedef struct online_client
{
    int id;       // 用户id
    int connfd;   // 客户端连接成功套接字
    int xx;       // 禁言标志 正确应该写进数据库，现在先不写用着

    struct online_client *next;
}Online;


// 除了要有connfd， 还要有在线链表知道其他用户信息
struct thread_node
{
    int connfd;
    Online *head;
};


//创建数据库存放用户信息
void open_sqlite(sqlite3 ** C_DB)
{
    int ret;

    ret = sqlite3_open("client_database.db", C_DB);
    if (ret != SQLITE_OK)
    {
        printf("create database error!%s\n", sqlite3_errmsg(*C_DB));
        return;
    }
}


// 创建clienttable用于保存注册账号id、用户名,密码,(禁言标志)
void create_clienttable(sqlite3 * C_DB)
{
    char *sql = NULL;
    char *errmsg = NULL;
    int ret;

    sql = "create table if not exists clienttable(id integer primary key, name text, password text);";

    ret = sqlite3_exec(C_DB, sql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("create table error! %s\n", errmsg);
        sqlite3_close(C_DB);
        return;
    }
}


// 管理员信息写入 id = 10000
void insert_client(sqlite3 *C_DB, struct Client client)
{
    char sql[150];
    char *errmsg = NULL;
    int ret;
    
    char ** result;
    int nrow, ncol;
    //判断登录id是否存在于数据库中，若有则不再插入
    sprintf(sql,"select id from clienttable where id = %d;",client.id);

    ret = sqlite3_get_table(C_DB,sql,&result,&nrow,&ncol,&errmsg);
    if (ret != SQLITE_OK)
    {
        printf("select id fail!%s\n",errmsg);
        return;
    }

    if (nrow == 0)
    {
        sprintf(sql,"insert into clienttable(id,name,password)values (%d,'%s','%s');",client.id,client.name,client.password);
        ret = sqlite3_exec(C_DB,sql,NULL,NULL,&errmsg);
        if (ret != SQLITE_OK)
        {
            printf("insert error! %s\n",errmsg);
            return;
        }
    }
}


// 创建聊天记录表chat_history
void create_historytable(sqlite3 * C_DB)
{
    char *sql = NULL;
    char *errmsg = NULL;
    int ret;

    sql = "create table if not exists chat_history(id integer, toid text, chatting text, chattime text);";

    ret = sqlite3_exec(C_DB, sql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("create table error!%s\n", errmsg);
        sqlite3_close(C_DB);
        return;
    }
}


// 将私聊记录写入chat_history
void insert_into_history_one(sqlite3 *C_DB, struct Client client)
{

    char sql[250];
    char *errmsg = NULL;
    int ret;

    time_t time1 = time(NULL);
    struct tm *time = localtime(&time1);

    sprintf(sql, "insert into chat_history (id,toid,chatting,chattime) values (%d,'%d','%s','%d-%d-%d %d:%d:%d');",client.id,client.toid,client.msg,time->tm_year+1900,time->tm_mon+1,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec);

    ret = sqlite3_exec(C_DB, sql, NULL, NULL, &errmsg);
    if (ret != SQLITE_OK)
    {
        printf("insert into table error!%s\n",errmsg);
        return;
    }
}


// 将群聊记录写入chat_history
void insert_into_history_all(sqlite3 *C_DB, struct Client client)
{
    char sql[250];
    char *errmsg = NULL;
    int ret;

    time_t time1 = time(NULL);
    struct tm *time = localtime(&time1);

    sprintf(sql,"insert into chat_history (id,toid,chatting,chattime) values (%d,'all','%s','%d-%d-%d %d:%d:%d');",client.id,client.msg,time->tm_year+1900,time->tm_mon+1,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec);

    ret = sqlite3_exec(C_DB,sql,NULL,NULL,&errmsg);
    if (ret != SQLITE_OK)
    {
        printf("insert into table error!%s\n",errmsg);
        return;
    }
}


// 查看聊天记录
void view_history(sqlite3 *C_DB,struct Client *client,struct thread_node connfd_node)
{
    
    char sql[250];
    char *errmsg = NULL;
    char **result = NULL;
    int i = 0, ret, nrow = 0, ncol = 0;

    sprintf(sql,"select * from chat_history where id = %d;",client->id);

    ret = sqlite3_get_table(C_DB,sql,&result,&nrow,&ncol,&errmsg);
    if (ret != SQLITE_OK)
    {
        printf("view error!%s\n",errmsg);
        sqlite3_free_table(result);
        return;
    }

    for (i = 0; i < (nrow + 1)*ncol; i = i + 4)
    {
        //(id,toid,chatting,chattime)
        sprintf(client->history, "%-10s--%-10s--%-30s--%-20s",result[i], result[i + 1],result[i + 2], result[i + 3]);
        send(connfd_node.connfd,client,sizeof(*client),0);
    }

    sqlite3_free_table(result);
}


// 对链表的操作  在线用户； 
void create_online_link(Online **head)
{
    *head = (Online*)malloc(sizeof(Online));
    (*head)->next = NULL;
}

// 创建结点
void create_new_node(Online ** new_node)
{
	(*new_node) = (Online *)malloc(sizeof(Online));
}


// 服务端显示在线用户（测试用）
void display_client(Online *head)
{
    Online *p;
    p = head->next;

    while (p != NULL)
    {
        printf("id = %d-----connfd = %d\n", p->id, p->connfd);
        p = p->next;
    }
}


// 删除链表结点
void delete_node(struct thread_node *connfd_node)
{
    Online * head;
    int connfd;
    head = connfd_node->head;
    connfd = connfd_node->connfd;

    Online * p1 = NULL;
    Online * p2 = NULL;
    p1 = head->next;
    p2 = head;

    if(p1 == NULL)
    {
        printf("Link is empty!\n");
    }
    else
    {
        while(p1->next != NULL && p1->connfd != connfd)
        {
            p2 = p1;
            p1 = p1 -> next;
        }

        if( p1->next != NULL)
        {
            p2->next = p1->next;
            free (p1);
        }
        else
        {
            if(p1->connfd == connfd)
            {
                p2->next = NULL;
                free(p1);
            }
            else
            {
                printf("not node is attach!\n");
            }
        }
    }
}


// 释放链表
void release_online_link(Online **head)
{
    Online * temp;
    temp = *head;

    while(*head != NULL)
    {   
        *head = temp->next;
	    free(temp);
		temp = *head;
    }

    free(*head);
    *head = NULL;
}


// 登录、注册信息
void login_in(struct Client sclient, struct thread_node connfd_node, sqlite3 *C_DB)
{
    char sql[100];
    char *errmsg = NULL;
    char **result = NULL;
    int ret, nrow = 0,ncol = 0;

    int i;           //下标
    int bool = 0;    //判断id是否被注册
    struct Client client = sclient;


    //判断登录id是否存在于数据库中，若没有则需用户先注册
    sprintf(sql,"select id from clienttable where id = %d;",sclient.id);

    ret = sqlite3_get_table(C_DB,sql,&result,&nrow,&ncol,&errmsg);
    if (ret != SQLITE_OK)
    {
        printf("select id fail!%s\n",errmsg);
        return;
    }

    if (atoi(result[1]) != client.id)
    {
        client.flag = 1;
        send(connfd_node.connfd,&client,sizeof(client),0);

        sqlite3_free_table(result);

        return;
    }


    //判断输入的密码是否正确
    sprintf(sql,"select password from clienttable where id = %d;",sclient.id);

    ret = sqlite3_get_table(C_DB,sql,&result,&nrow,&ncol,&errmsg);
    if (ret != SQLITE_OK)
    {
        printf("select password fail!%s\n",errmsg);
        return;
    }

    if (strcmp(result[1],client.password) != 0)
    {
        client.flag = 2;
        send(connfd_node.connfd,&client,sizeof(client),0);

        sqlite3_free_table(result);

        return;
    }


    //信息验证成功，发送完成client给客户端（用户名）
    sprintf(sql,"select name from clienttable where id = %d;",sclient.id);

    ret = sqlite3_get_table(C_DB,sql,&result,&nrow,&ncol,&errmsg);
    if (ret != SQLITE_OK)
    {
        printf("select name fail!%s\n",errmsg);
        return;
    }

    //将得到的用户名暂存在client.msg中
    strcpy(client.msg, result[1]);
    
    printf("用户%d*****登录!\n",client.id);


    Online *new_node;
    create_new_node(&new_node);

    new_node->id = client.id;
    new_node->xx = 0;                       //禁言标志位，默认为0
    new_node->connfd =connfd_node.connfd;

    new_node->next = connfd_node.head->next;
    connfd_node.head->next = new_node;

    client.flag = 3;
    send(connfd_node.connfd,&client,sizeof(client),0);

    display_client(connfd_node.head);
}


// 线程要执行的任务job
void * work_tid(void *arg)
{
    struct thread_node connfd_node;
    connfd_node = *(struct thread_node *)arg;

    struct Client client;
    sqlite3  * C_DB;              //存放用户信息的数据库
    int n;

    while (1)
    {
        n = recv(connfd_node.connfd,&client,sizeof(client),0);

        if (n == 0)
        {
            break;
        }

        switch (client.choice)
        {
            //登录界面退出 
            case 0:
            {
                close(connfd_node.connfd);

                break;
            }


            // 注册账号写入数据库
            case 1:
            {
                open_sqlite(&C_DB);
                create_clienttable(C_DB);

                insert_client(C_DB,client);

                sqlite3_close(C_DB);

                client.flag = 1;
                send(connfd_node.connfd,&client,sizeof(client),0);

                break;
            }


            // 登陆后把用户id写入链表
            case 2:
            {
                open_sqlite(&C_DB);
                create_clienttable(C_DB);

                // 修改client.flag判断登录是否成功
                login_in(client, connfd_node, C_DB);

                sqlite3_close(C_DB);

                break;
            }


            //将在线用户发送给客户端
            case 3:
            {
                Online *p = NULL;
                p = connfd_node.head->next;

                while (p != NULL)
                {
                    sprintf(client.online,"%d",p->id);

                    send(connfd_node.connfd,&client,sizeof(client),0);
                    p = p->next;
                }

                break;
            }


            // 私聊
            case 4:
            {
                Online *p = NULL;
                p = connfd_node.head->next;

                while(p != NULL && p->id != client.toid)
                {
                    p = p->next;
                }

                //将私聊信息写入数据库并且发送给toid
                if(p != NULL)
                {
                    open_sqlite(&C_DB);
                    create_historytable(C_DB);

                    insert_into_history_one(C_DB,client);
                    
                    sqlite3_close(C_DB);

                    send(p->connfd,&client,sizeof(client),0);
                }
                else
                {
                    printf("client is offline!\n");
                }

                break;
            }


            // 群聊
            case 5:
            {
                Online *p = NULL;
                p = connfd_node.head->next;

                while(p != NULL)
                {
                    if (p->id == client.id)
                    {
                        p = p->next;
                        continue;
                    }

                    send(p->connfd,&client,sizeof(client),0);

                    p = p->next;
                }

                //将群聊信息写入数据库
                open_sqlite(&C_DB);
                create_historytable(C_DB);

                insert_into_history_all(C_DB,client);

                sqlite3_close(C_DB);

                break;
            }


            //读取聊天历史
            case 6:
            {
                open_sqlite(&C_DB);
                create_historytable(C_DB);

                view_history(C_DB,&client,connfd_node);

                sqlite3_close(C_DB);

                break;
            }

            //用户退出登录
            case 7:
            {
                printf("用户%d--%d退出\n",client.id,connfd_node.connfd);

                delete_node(&connfd_node);

                close(connfd_node.connfd);

                break;
            }


            //禁言某一位用户
            case 8:
            {
                Online * p = NULL;
                p = connfd_node.head->next;

                while(p != NULL && p->id != client.xxid)
                {
                    p = p->next;
                }

                if(p != NULL)
                {
                    p->xx = 1;       // 禁言
                    client.flag = 1; 
                    send(p->connfd,&client,sizeof(client),0);
                }
                else
                {
                    printf("client is offline!\n");
                }

                break;
            }


            // 全体禁言
            case 9:
            {
                Online *p = NULL;
                p = connfd_node.head->next;

                while(p != NULL)
                {
                    if (p->id == client.id)
                    {
                        p = p->next;
                        continue;
                    }

                    p->xx = 0;
                    client.flag = 1;
                    send(p->connfd,&client,sizeof(client),0);
                    p = p->next;
                }
                break;
            }


            // 解除某个人禁言
            case 10:
            {
                Online * p = NULL;
                p = connfd_node.head->next;

                while(p != NULL && p->id != client.xxid)
                {
                    p = p->next;
                }

                if(p != NULL)
                {
                    p->xx = 0;//解除禁言
                    send(p->connfd,&client,sizeof(client),0);
                }
                else
                {
                    printf("client is offline!\n");
                }

                break;
            }


            // 解除全体禁言
            case 11:
            {  
                Online *p = NULL;
                p = connfd_node.head->next;

                while(p != NULL)
                {
                    if (p->id == client.id)
                    {
                        p = p->next;
                        continue;
                    }

                    p->xx = 1;
                    send(p->connfd,&client,sizeof(client),0);

                    p = p->next;
                }

                break;
            }


            // 将用户踢出聊天室
            case 12:
            {
                Online * head;
                Online * p = NULL;

                head = connfd_node.head;
                p = head->next;

                if(p == NULL)
                {
                    printf("Link is empty!\n");
                }
                else
                {
                    while(p != NULL && p->id != client.toid)
                    {
                        p = p -> next;
                    }

                    if( p == NULL)
                    {
                        client.flag = 2;
                        strcpy(client.msg,"未找到该用户!");
                        send(connfd_node.connfd,&client,sizeof(client),0);
                    }
                    else
                    {
                        client.flag = 1;
                        send(p->connfd,&client,sizeof(client),0);

                        printf("用户%d被管理员踢出\n",p->id);
                        //函数待封装开始
                        struct thread_node p_node;
                        p_node.connfd = p->connfd;
                        p_node.head = head;
                        delete_node(&p_node);
                        //函数待封装结束
                        close(p->connfd);
                    }
                }

                break;
            }


            //发送文件
            case 13:
            {
                Online * p = NULL;
                p = connfd_node.head->next;

                while(p != NULL && p->id != client.toid)
                {
                    p = p->next;
                }

                if(p != NULL)
                {
                    send(p->connfd,&client,sizeof(client),0);
                }
                else
                {
                    printf("client is offline!\n");
                }

                break;
            }
        }

        if (client.choice == 7 || client.choice == 0)
        {
            break;
        }
    }

    return NULL;
}

// 定长线程池实现
struct job
{
    void *(*func)(void *arg);
    void *arg;
    struct job *next;
};

struct threadpool
{
    int thread_num;  //已开启线程池已工作线程
    pthread_t *pthread_ids;  // 薄脆线程池中线程id


    struct job *head;
    struct job *tail;  // 任务队列的尾
    int queue_max_num;  //任务队列的最多放多少个
    int queue_cur_num;  //任务队列已有多少个任务

    pthread_mutex_t mutex;
    pthread_cond_t queue_empty;    //任务队列为空
    pthread_cond_t queue_not_emtpy;  //任务队列不为空
    pthread_cond_t queue_not_full;  //任务队列不为满

    int pool_close;
};

void * threadpool_function(void *arg)
{
    struct threadpool *pool = (struct threadpool *)arg;
    struct job *pjob = NULL;

    while (1)
    {
        pthread_mutex_lock(&(pool->mutex));

        while(pool->queue_cur_num == 0)
        {
            pthread_cond_wait(&(pool->queue_not_emtpy), &(pool->mutex));

            if (pool->pool_close == 1)
            {
                pthread_exit(NULL);
            }
        }



        pjob = pool->head;
        pool->queue_cur_num--;

        if (pool->queue_cur_num != pool->queue_max_num)
        {
            pthread_cond_broadcast(&(pool->queue_not_full));
        }
        
        if (pool->queue_cur_num == 0)
        {
            pool->head = pool->tail = NULL;
            pthread_cond_broadcast(&(pool->queue_empty));
        }
        else
        {
            pool->head = pjob->next;
        }
        
        pthread_mutex_unlock(&(pool->mutex));

        (*(pjob->func))(pjob->arg);
        free(pjob);
        pjob = NULL;
    }
}

struct threadpool * threadpool_init(int thread_num, int queue_max_num)
{
    struct threadpool *pool = (struct threadpool *)malloc(sizeof(struct threadpool));
    // malloc

    pool->queue_max_num = queue_max_num;
    pool->queue_cur_num = 0;
    pool->pool_close = 0;
    pool->head = NULL;
    pool->tail = NULL;

    pthread_mutex_init(&(pool->mutex), NULL);
    pthread_cond_init(&(pool->queue_empty), NULL);
    pthread_cond_init(&(pool->queue_not_emtpy), NULL);
    pthread_cond_init(&(pool->queue_not_full), NULL);

    pool->thread_num = thread_num;
    pool->pthread_ids = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);
    // malloc

    for (int i = 0; i < pool->thread_num; i++)
    {
        pthread_create(&pool->pthread_ids[i], NULL, (void *)threadpool_function, (void *)pool);
    }

    return pool;
}

void threadpool_add_job(struct threadpool *pool, void *(*func)(void *), void *arg)
{
    pthread_mutex_lock(&(pool->mutex));
    while (pool->queue_cur_num == pool->queue_max_num)
    {
        pthread_cond_wait(&pool->queue_not_full, &(pool->mutex));
    }
    
    
    struct job *pjob = (struct job *)malloc(sizeof(struct job));
    //malloc
    
    pjob->func = func;
    pjob->arg = arg;
    pjob->next = NULL;
    
    // pjob->func(pjob->arg);
    if (pool->head == NULL)
    {
        pool->head = pool->tail = pjob;
        pthread_cond_broadcast(&(pool->queue_not_emtpy));
    }
    else
    {
        pool->tail ->next = pjob;
        pool->tail = pjob;
    }

    pool->queue_cur_num++;
    pthread_mutex_unlock(&(pool->mutex));
}

void thread_destroy(struct threadpool *pool)
{
    pthread_mutex_lock(&(pool->mutex));

    while (pool->queue_cur_num != 0)
    {
         pthread_cond_wait(&(pool->queue_empty),&(pool->mutex));
    }

    pthread_mutex_unlock(&(pool->mutex));

    pthread_cond_broadcast(&(pool->queue_not_full));

    pool->pool_close = 1;

    for (int i = 0; i < pool->thread_num; i++)
    {
        pthread_cond_broadcast(&(pool->queue_not_emtpy));
        // pthread_cancel(pool->pthread_ids[i]); //有系统调用，才能销毁掉；有bug
        printf("thread exit!\n");
        pthread_join(pool->pthread_ids[i], NULL);
    }

    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->queue_empty));
    pthread_cond_destroy(&(pool->queue_not_emtpy));
    pthread_cond_destroy(&(pool->queue_not_full));

    free(pool->pthread_ids);

    struct job *temp;
    while(pool->head != NULL)
    {
        temp = pool->head;
        pool->head = temp->next;
        free(temp);
    }

    free(pool);

    printf("destroy finish!\n");
}

// 套接口初始化，线程池初始化
int sockfd_init()
{
    int listenfd;
    //创建socket
    listenfd = socket( AF_INET,SOCK_STREAM,0);
    //客户端下线后，这个端口立即复用
    int opt = 1; 
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    

    struct sockaddr_in seraddr;
    //初始化
    bzero(&seraddr,sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    seraddr.sin_port = htons(SERPORT);


    //bind绑定套接口
    bind(listenfd,(struct sockaddr*) &seraddr,sizeof(seraddr));
    
    
    //监听
    listen(listenfd,10);
    printf("监听中....\n");


    //在线链表初始化
    Online *head; 
    create_online_link(&head);

    struct thread_node connfd_node;
    connfd_node.head = head;

    //线程池初始化
    struct threadpool *pool = threadpool_init(10, 100);

    int connfd;

    for (;;)
    {
        connfd = accept(listenfd,NULL,NULL);
        printf("%dconnect\n",connfd);

        connfd_node.connfd = connfd;
        threadpool_add_job(pool, (void *)work_tid, (void *)&connfd_node);
    }
    
    release_online_link(&head);
    thread_destroy(pool);

    close(listenfd);

    return 0;
}

int main()
{
    sqlite3 * C_DB;
    struct Client client;
    client.id = 10000;
    strcpy(client.password, "admin");
    open_sqlite(&C_DB);
    create_clienttable(C_DB);
    insert_client(C_DB, client);
    sqlite3_close(C_DB);

    sockfd_init();
  
    return 0;
}