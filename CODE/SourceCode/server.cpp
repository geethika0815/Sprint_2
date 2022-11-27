#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <queue>
#include <iostream>
#include<fstream>
#include<signal.h>
#include "logger.h"
using namespace std;

#define BUFFER_LEN 1024
#define NAME_LEN 10
#define MAX_CLIENT_NUM 5
//#define hello 50
/*Structure Client*/
struct Client
{
    int valid;               /* to judge whether this user is online*/
    int fd_id;               /* user ID number*/
    int socket;              /* socket to this user*/
    char name[NAME_LEN + 1]; /* name of the user*/
} client[MAX_CLIENT_NUM] = {0};

queue<string> message_q[MAX_CLIENT_NUM]; /* message buffer */
int current_client_num = 0;
/* sync current_client_num*/

pthread_mutex_t num_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 2 kinds of threads*/
pthread_t chat_thread[MAX_CLIENT_NUM] = {0};
pthread_t send_thread[MAX_CLIENT_NUM] = {0};

/* used to sync*/
pthread_mutex_t mutex[MAX_CLIENT_NUM] = {0};
pthread_cond_t cv[MAX_CLIENT_NUM] = {0};
void *handle_send(void *data)
{
    struct Client *pipe = (struct Client *)data;
    while (1)
    {
        pthread_mutex_lock(&mutex[pipe->fd_id]);
        /* wait until new message receive*/
        while (message_q[pipe->fd_id].empty())
        {
            pthread_cond_wait(&cv[pipe->fd_id], &mutex[pipe->fd_id]);
        }
        while (!message_q[pipe->fd_id].empty())
        {
            string message_buffer = message_q[pipe->fd_id].front(); 
            int n = message_buffer.length(); 
            /* calculate one transfer length*/
            int trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            /* send the message*/
            while (n > 0)
            {
                int len = send(pipe->socket, message_buffer.c_str(), trans_len, 0); 
                if (len < 0)
                {
                    perror("send"); 
                    return NULL;
                }
                n -= len;

                message_buffer.erase(0, len); /* delete data that has been transported*/
                trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            }
            /* delete the message that has been sent*/
            message_buffer.clear();
            message_q[pipe->fd_id].pop();
        }
        pthread_mutex_unlock(&mutex[pipe->fd_id]);
    }
    return NULL;
}
/*handle Receive*/
void handle_recv(void *data)
{
    struct Client *pipe = (struct Client *)data;

    /* message buffer*/
    string message_buffer;
    int message_len = 0;

    /* one transfer buffer*/
    char buffer[BUFFER_LEN + 1];
    int buffer_len = 0;

    /* receive*/
    while ((buffer_len = recv(pipe->socket, buffer, BUFFER_LEN, 0)) > 0)
    {
        for (int msg_len = 0; msg_len < buffer_len; msg_len++)  
        {
            if (message_len == 0)
            {
                char temp[100];
                sprintf(temp, "%s:", pipe->name);
                message_buffer = temp;
                message_len = message_buffer.length();
            }

            message_buffer += buffer[msg_len]; 
            message_len++; 

            if (buffer[msg_len] == '\n')  
            {
                /* send to every client*/
                for (int client_num = 0; client_num < MAX_CLIENT_NUM; client_num++) 
                {
                    if (client[client_num].valid && client[client_num].socket != pipe->socket)
                    {
                        pthread_mutex_lock(&mutex[client_num]);
                        message_q[client_num].push(message_buffer); 
                        pthread_cond_signal(&cv[client_num]); 
                        pthread_mutex_unlock(&mutex[client_num]);
                    }
                }
                /* new message start*/
                message_len = 0;
                message_buffer.clear();
            }
        }
        /* clear buffer*/
        buffer_len = 0;
        memset(buffer, 0, sizeof(buffer));
    }
    return;
}
/* chat Function*/
void *chat(void *data)
{
    struct Client *pipe = (struct Client *)data;

    /* printf hello message*/
    char hello[100];
    sprintf(hello,"hello %s, Welcome to join the chatroom. Online User Number: %d\n", pipe->name, current_client_num);
    pthread_mutex_lock(&mutex[pipe->fd_id]);
    message_q[pipe->fd_id].push(hello);
    pthread_cond_signal(&cv[pipe->fd_id]);
    pthread_mutex_unlock(&mutex[pipe->fd_id]);
    memset(hello, 0, sizeof(hello));
    sprintf(hello, "New User %s join in! Online User Number: %d\n", pipe->name, current_client_num);
    /* send messages to other users*/
    for (int client_num = 0; client_num < MAX_CLIENT_NUM; client_num++)
    {
        if (client[client_num].valid && client[client_num].socket != pipe->socket)
        {
            pthread_mutex_lock(&mutex[client_num]);
            message_q[client_num].push(hello);
            pthread_cond_signal(&cv[client_num]);
            pthread_mutex_unlock(&mutex[client_num]);
        }
    }

    /* create a new thread to handle send messages for this socket*/
    pthread_create(&send_thread[pipe->fd_id], NULL, handle_send, (void *)pipe);

    /* receive message*/
    handle_recv(data);
    pthread_mutex_lock(&num_mutex);
    pipe->valid = 0;
    current_client_num--;
    pthread_mutex_unlock(&num_mutex);
    /* printf bye message*/
    LOG_INFO("%s left the chatroom. Online Person Number: %d\n", pipe->name, current_client_num);
    char bye[100];
    sprintf(bye, "%s left the chatroom. Online Person Number: %d\n", pipe->name, current_client_num);
   /* send offline message to other clients*/
    for (int current_cli = 0; current_cli < MAX_CLIENT_NUM; current_cli++)
    {
        if (client[current_cli].valid && client[current_cli].socket != pipe->socket)
        {
            pthread_mutex_lock(&mutex[current_cli]);
            message_q[current_cli].push(bye);
            pthread_cond_signal(&cv[current_cli]);
            pthread_mutex_unlock(&mutex[current_cli]);
        }
    }

    pthread_mutex_destroy(&mutex[pipe->fd_id]);
    pthread_cond_destroy(&cv[pipe->fd_id]);
    pthread_cancel(send_thread[pipe->fd_id]);

    return NULL;
}

const int SIZE = 4;
string Usernames[SIZE];
string Password[SIZE];

    int GetNameIndex(string query, int size)
    {
	for (int count=0; count<size; count++)
	{
		if (query == Usernames[count]) return count;
	}
	return -1;
}

bool PasswordMatches(int index, string passwd)
{
	return (passwd == Password[index]);
}
void sig_handler(int sig){
	printf("Server is shutdown\n");
	exit(0);
}
int main()
{
        label:
	/*Read the database*/
	ifstream findip("database.txt");
	int cnt=0;
	signal(SIGINT,sig_handler);

	while (!findip.eof())
	{
		findip >> Usernames[cnt] >> Password[cnt];
		cnt++; 
	}

	/*Now the rest of the program*/
	string usrname, passwd;
	LOG_INFO("Enter the Username:");
	cin >> usrname;

	int index = GetNameIndex(usrname, cnt); 

	LOG_INFO(" Enter the Password:");
	cin >> passwd;

	if (!PasswordMatches(index, passwd))
	{
		LOG_ERROR("Invalid user Details\n");
		goto label;
		
	}
    LOG_INFO( "Thank you for logging in.\n");
    LOG_INFO("\n\n********* WELCOME TO CHAT BOX APPLICATION ************");

    /* create server socket*/
    int server_sock;
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    /* get server port and bind*/
    int server_port = 0;
    while (1)
    {
        LOG_INFO("Please enter the port number of the server: ");
        cin>>server_port;
        addr.sin_port = htons(server_port); 
        if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)))
        {
            perror("bind");
            continue;
        }
        break;
    }

    /*no more than 32 clients*/
    if (listen(server_sock, MAX_CLIENT_NUM + 1))
    {
        perror("listen");
        return -1;
    }
    LOG_INFO("Server start successfully!\n");
    LOG_INFO("You can join the chatroom by connecting to 127.0.0.1:%d\n\n", server_port);

    /* waiting for new client to join in*/
    while (1)
    {
        /* create a new connect*/
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == -1)
        {
            perror("accept");
            return -1;
        }

        /* check whether is full or not*/
        if (current_client_num >= MAX_CLIENT_NUM)
        {
            if (send(client_sock, "ERROR", strlen("ERROR"), 0) < 0)
            perror("send");
            shutdown(client_sock, 2);
            continue;
        }
        else
        {
            if (send(client_sock, "OK", strlen("OK"), 0) < 0)
                perror("send");
        }

        /* get client's name*/
        char name[NAME_LEN + 1] = {0};
        ssize_t state = recv(client_sock, name, NAME_LEN, 0);
        if (state < 0)
        {
            perror("recv");
            shutdown(client_sock, 2);
            continue;
        }
        /* new user do not input a name but leave directly*/
        else if (state == 0)
        {
            shutdown(client_sock, 2);
            continue;
        }

        /* update client array, create new thread*/
        for (int new_client = 0; new_client < MAX_CLIENT_NUM; new_client++)
        {
            /* find the first unused client*/
            if (!client[new_client].valid)
            {
                pthread_mutex_lock(&num_mutex);
                /*set name*/
                memset(client[new_client].name, 0, sizeof(client[new_client].name));
                strcpy(client[new_client].name, name);
                /* set other info*/
                client[new_client].valid = 1;
                client[new_client].fd_id = new_client;
                client[new_client].socket = client_sock;

                mutex[new_client] = PTHREAD_MUTEX_INITIALIZER;
                cv[new_client] = PTHREAD_COND_INITIALIZER;

                current_client_num++;
                pthread_mutex_unlock(&num_mutex);

                /* create new receive thread for new client*/
                pthread_create(&chat_thread[new_client], NULL, chat, (void *)&client[new_client]);
                LOG_INFO("%s join in the chatroom. Online User Number: %d\n", client[new_client].name, current_client_num);

                break;
            }
        }
    }

    /*close socket*/
    for (int new_client = 0; new_client < MAX_CLIENT_NUM; new_client++)
        if (client[new_client].valid)
            shutdown(client[new_client].socket, 2);
    shutdown(server_sock, 2);
    return 0;
}
