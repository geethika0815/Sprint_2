#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <queue>
#include <iostream>
#include<fstream>
#include "logger.h"
using namespace std;

#define BUFFER_LEN 10
#define NAME_LEN 10
#define MAX_CLIENT_NUM 4

struct Client
{
    int valid;               /*to judge whether this user is online*/
    int fd_id;               /*user ID number*/
    int socket;              /* socket to this user*/
    char name[NAME_LEN + 1]; /* name of the user*/
} client[MAX_CLIENT_NUM] = {0};

queue<string> message_q[MAX_CLIENT_NUM]; /* message buffer  */
int current_client_num = 0;


/* sync current_client_num */
pthread_mutex_t num_mutex = PTHREAD_MUTEX_INITIALIZER;



/* 2 kinds of threads */
pthread_t chat_thread[MAX_CLIENT_NUM] = {0};
pthread_t send_thread[MAX_CLIENT_NUM] = {0};



/* used to sync */
pthread_mutex_t mutex[MAX_CLIENT_NUM] = {0};
pthread_cond_t cv[MAX_CLIENT_NUM] = {0};
void *handle_send(void *data)
{
    struct Client *pipe = (struct Client *)data;
    while (1)
    {
        pthread_mutex_lock(&mutex[pipe->fd_id]);
        /* wait until new message receive */
        while (message_q[pipe->fd_id].empty())
        {
            pthread_cond_wait(&cv[pipe->fd_id], &mutex[pipe->fd_id]);
        }
        while (!message_q[pipe->fd_id].empty())
        {
            string message_buffer = message_q[pipe->fd_id].front(); 
            int n = message_buffer.length(); 
            /* calculate one transfer length */
            int trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            /* send the message */
            while (n > 0)
            {
                int len = send(pipe->socket, message_buffer.c_str(), trans_len, 0); 
                if (len < 0)
                {
                    perror("send"); 
                    return NULL;
                }
                n -= len;  
                message_buffer.erase(0, len); /* delete data that has been transported */
                trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            }
            /* delete the message that has been sent */
            message_buffer.clear();
            message_q[pipe->fd_id].pop();
        }
        pthread_mutex_unlock(&mutex[pipe->fd_id]);
    }
    return NULL;
}
void handle_recv(void *data)
{
    struct Client *pipe = (struct Client *)data;

    /* message buffer */
    string message_buffer;
    int message_len = 0;

    /* one transfer buffer */
    char buffer[BUFFER_LEN + 1];
    int buffer_len = 0;

    /* receive */
    while ((buffer_len = recv(pipe->socket, buffer, BUFFER_LEN, 0)) > 0)
    {
        for (int ser_msg = 0; ser_msg < buffer_len; ser_msg++)  
        {
            if (message_len == 0)
            {
                char temp[100];
                sprintf(temp, "%s:", pipe->name);
                message_buffer = temp;
                message_len = message_buffer.length();
            }

            message_buffer += buffer[ser_msg]; 
            message_len++; 

            if (buffer[ser_msg] == '\n')  
            {
                /* send to every client */
                for (int msg = 0; msg < MAX_CLIENT_NUM; msg++) 
                {
                    if (client[msg].valid && client[msg].socket != pipe->socket)
                    {
                        pthread_mutex_lock(&mutex[msg]);
                        message_q[msg].push(message_buffer); 
                        pthread_cond_signal(&cv[msg]); 
                        pthread_mutex_unlock(&mutex[msg]);
                    }
                }
                /* new message start */
                message_len = 0;
                message_buffer.clear();
            }
        }
        /* clear buffer */
        buffer_len = 0;
        memset(buffer, 0, sizeof(buffer));
    }
    return;
}
void *chat(void *data)
{
    struct Client *pipe = (struct Client *)data;

    /* printf hello message */
    char hello[100];
    sprintf(hello, "Hello %s, Welcome to join the chatroom. Online User Number: %d\n", pipe->name, current_client_num);
    pthread_mutex_lock(&mutex[pipe->fd_id]);
    message_q[pipe->fd_id].push(hello);
    pthread_cond_signal(&cv[pipe->fd_id]);
    pthread_mutex_unlock(&mutex[pipe->fd_id]);
    memset(hello, 0, sizeof(hello));
    sprintf(hello, "New User %s join in! Online User Number: %d\n", pipe->name, current_client_num);
    
    
    /* send messages to other users */
    for (int send_msg = 0; send_msg < MAX_CLIENT_NUM; send_msg++)
    {
        if (client[send_msg].valid && client[send_msg].socket != pipe->socket)
        {
            pthread_mutex_lock(&mutex[send_msg]);
            message_q[send_msg].push(hello);
            pthread_cond_signal(&cv[send_msg]);
            pthread_mutex_unlock(&mutex[send_msg]);
        }
    }

    /* create a new thread to handle send messages for this socket */
    pthread_create(&send_thread[pipe->fd_id], NULL, handle_send, (void *)pipe);

    /* receive message */
    handle_recv(data);
    pthread_mutex_lock(&num_mutex);
    pipe->valid = 0;
    current_client_num--;
    pthread_mutex_unlock(&num_mutex);
    
    /* printf bye message */
    LOG_INFO("%s left the chatroom. Online Person Number: %d\n", pipe->name, current_client_num);
    char bye[100];
    sprintf(bye, "%s left the chatroom. Online Person Number: %d\n", pipe->name, current_client_num);


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
int main()
{
        
        label:
	/* Read the database; */
	ifstream findip("database.txt");
	int cnt=0;


	while (!findip.eof())
	{
		findip >> Usernames[cnt] >> Password[cnt];
		cnt++; 
	}

	/* Now the rest of the program */
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
    LOG_INFO("\n\n**** WELCOME TO CHAT BOX APPLICATION *****");

    /* create server socket */
    int server_sock;
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    /* get server port and bind */
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

    /* no more than macro size of clients */
    if (listen(server_sock, MAX_CLIENT_NUM + 1))
    {
        perror("listen");
        return -1;
    }
    LOG_INFO("Server start successfully!\n");
    LOG_INFO("You can join the chatroom by connecting to 127.0.0.1:%d\n\n", server_port);

    /* waiting for new client to join in */
    while (1)
    {
        /* create a new connect */
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == -1)
        {
            perror("accept");
            return -1;
        }

        /* check whether is full or not */
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

        /* get client's name */
        char name[NAME_LEN + 1] = {0};
        ssize_t state = recv(client_sock, name, NAME_LEN, 0);
        if (state < 0)
        {
            perror("recv");
            shutdown(client_sock, 2);
            continue;
        }
        
        /* new user do not input a name but leave directly */
        else if (state == 0)
        {
            shutdown(client_sock, 2);
            continue;
        }

        /* update client array, create new thread */
        for (int client_num = 0; client_num < MAX_CLIENT_NUM; client_num++)
        {
            
            /* find the first unused client */
            if (!client[client_num].valid)
            {
                pthread_mutex_lock(&num_mutex);
                
                /* set name */
                memset(client[client_num].name, 0, sizeof(client[client_num].name));
                strcpy(client[client_num].name, name);
                
                /* set other info */
                client[client_num].valid = 1;
                client[client_num].fd_id = client_num;
                client[client_num].socket = client_sock;

                mutex[client_num] = PTHREAD_MUTEX_INITIALIZER;
                cv[client_num] = PTHREAD_COND_INITIALIZER;

                current_client_num++;
                pthread_mutex_unlock(&num_mutex);

                /* create new receive thread for new client */
                pthread_create(&chat_thread[client_num], NULL, chat, (void *)&client[client_num]);
                LOG_INFO("%s join in the chatroom. Online User Number: %d\n", client[client_num].name, current_client_num);

                break;
            }
        }
    }

    /* close socket */
    for (int client_num = 0; client_num < MAX_CLIENT_NUM; client_num++)
        if (client[client_num].valid)
            shutdown(client[client_num].socket, 2);
    shutdown(server_sock, 2);
    
}
