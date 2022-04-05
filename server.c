/* serveur de chat sous Linux avec les thread */
/* gcc server.c -o server -lpthread -D_REENTRANT */
/* https://codes-sources.commentcamarche.net/source/25757-serveur-de-chat-multithreade-en-c-sous-linux*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>

#define BANNER "Serveur de chat v0.4 par .:MiniMoi:.\n(C) .:MiniMoi:. 2004\n"
#define CLIENT_BANNER "Connection etablie...\r\nBienvenue sur le serveur de chat de MiniMoi\r\nEntrez votre pseudo: "
#define ADMIN_PWD "MiniMoi"
#define HELP_MSG "Commandes disponibles :\r\n- /pseudo=[nouveau pseudo] --> changer de pseudo\r\n- /quit=[message] --> quitter avec (ou sans) message\r\n- /list --> obtenir la liste des clients connectes\r\n- /admin=[password] --> obtenir les droits administrateur\r\n- /kick=[pseudo] --> kicker un client(reserve aux admins)\r\n- /? --> afficher cette aide\r\n"

#define MAX_CLIENTS 500
#define LS_CLIENT_NB 5
#define INVALID_SOCKET -1
#define PORT 1987

volatile int nb_clients = 0;
int first_free = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct _s_client
{
	pthread_t id;
	int sock;
	char *pseudo;
	char admin;
} s_client;

s_client *clients[MAX_CLIENTS];

/* creation d'un serveur */
int create_server(int port)
{
  int sock,optval = 1;
  struct sockaddr_in sockname;

  if((sock = socket(PF_INET,SOCK_STREAM,0))<0)
    {
      printf("Erreur d'ouverture de la socket");
      exit(-1);
    }
  
  setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
  memset((char *) &sockname,0,sizeof(struct sockaddr_in));
  sockname.sin_family = AF_INET;
  sockname.sin_port = htons(port);
  sockname.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(sock,(struct sockaddr *) &sockname, sizeof(struct sockaddr_in)) < 0)
    {
      printf("Erreur de bind!");
      exit(-1);
    }

  if(listen(sock,LS_CLIENT_NB) <0)
    {
      printf("listen error!");
      exit(-1);
    }
  
  return sock;
}

/* accepte une connexion avec ou sans timeout */
int server_accept(int main_sock,int timeout)
{
  int sock;

  if(timeout > 0)
    alarm(timeout);

  if((sock = accept(main_sock,NULL,0)) < 0)
    {
      if(errno == EINTR)
	{
	  shutdown(main_sock,SHUT_RDWR);
	  close(main_sock);
	  if(timeout > 0)
	    alarm(0);
	  return -1;
	}
      else
	{
	  printf("\nAccept error.\n");
	  exit(-1);
	}
    }

  if(timeout > 0)
    alarm(0);
  fcntl(sock,F_SETFD,1);
  
  return sock;
}

/* envoyer une chaine de caractere a un client */
int send_msg(int sock,char *msg)
{
	return write(sock,msg,strlen(msg));
}

/* envoyer un message a tout le monde sauf a la socket not_to */
int send_all(char *msg, int not_to)
{
	int i;
	
	pthread_mutex_lock(&mutex);	// debut de la section critique
	for(i=0;i<first_free;i++)
	{
		if(clients[i]->sock != not_to)
			send_msg(clients[i]->sock,msg);
	}
	pthread_mutex_unlock(&mutex);	// fin de la section critique
	
	return 0;
}

/* gestion de fin de connection d'un client */
void client_quit(s_client *me, char *msg)
{
	int i,j;
	char buf[8192+1];
	
	if(msg)	snprintf(buf,8192,"%s nous quitte...(%s)\r\n",me->pseudo,msg);
	else	snprintf(buf,8192,"%s nous quitte...\r\n",me->pseudo);
	buf[8192] = '\0';
	send_all(buf,me->sock);
	pthread_mutex_lock(&mutex);	// debut de la section critique
	for(i=0;(clients[i]->sock != me->sock);i++);	// recherche de l'index de la structure dans le tableau
	
	close(me->sock);
	free(me->pseudo);
	free(me);
	
	for(j=i+1;j<first_free;j++)	// on reorganise le tableau en decalant les elements situes APRES celui qui est supprime
	{
		clients[j-1] = clients[j];
	}
	nb_clients--;
	first_free--;
	pthread_mutex_unlock(&mutex);	// fin de la section critique
	printf("Un client en moins...%d clients\n",nb_clients);
}

/* interaction avec le client (thread) */
void *interact(void *param)
{
	int sck = *((int *) param);
	char msg[4096+1];
	char msg_to_send[8192+1];
	s_client *me = NULL;
	char *buf = NULL;
	int len;
	int i;
	
	me = (s_client *) malloc(sizeof(s_client));
	if(!me)
	{
		printf("\nErreur d'allocation memoire!\n");
		close(sck);
		nb_clients--;
		pthread_exit(NULL);
	}
	bzero(me,sizeof(s_client));
	
	send_msg(sck,CLIENT_BANNER);
	len = read(sck,msg,4096);
	if(len <= 0)
	{
		printf("\nErreur\n");
		close(sck);
		free(me);
		me = NULL;
		nb_clients--;
		pthread_exit(NULL);
	}
	msg[255] = '\0';	// on limite le pseudo a 255 caracteres
	for(i=0;(msg[i]!='\0') && (msg[i]!='\r') && (msg[i]!='\n') && (msg[i]!='\t');i++);
	msg[i] = '\0';	// on isole le pseudo
	
	pthread_mutex_lock(&mutex);	// debut de la section critique
	for(i=0;i<first_free;i++)
	{
		if(!strcmp(msg,clients[i]->pseudo))
		{
			send_msg(sck,"\r\nPseudo deja utilise! Deconnection...\r\n");
			close(sck);
			free(me);
			nb_clients--;
			pthread_mutex_unlock(&mutex);	// fin de la section critique
			pthread_exit(NULL);
		}
	}
	pthread_mutex_unlock(&mutex);	// fin de la section critique
	
	me->id = pthread_self();
	me->sock = sck;
	me->pseudo = strdup(msg);
	me->admin = 0;
	
	pthread_mutex_lock(&mutex);	// debut de la section critique
	clients[first_free] = me;
	first_free++;
	pthread_mutex_unlock(&mutex);	// fin de la section critique
	
	snprintf(msg_to_send,8192,"Nouveau client : %s\r\n",me->pseudo);
	msg_to_send[8192]='\0';
	send_all(msg_to_send,INVALID_SOCKET);
	while(1)
	{
		len = read(sck,msg,4096);
		if(len <= 0)
		{
			client_quit(me,"Erreur reseau");
			pthread_exit(NULL);
		}
		msg[len] = '\0';
		if(msg[0] == '/')	// le message est une commande
		{
			int valid_command = 0;
			
			if(!strncmp(msg,"/pseudo=",8))	// changement de pseudo
			{
				char *old_pseudo = NULL;
				int valid_pseudo = 1;
				
				msg[255+8] = '\0';	// on limite le pseudo a 255 caracteres
				for(i=8;(msg[i]!='\0') && (msg[i]!='\r') && (msg[i]!='\n') && (msg[i]!='\t');i++);
				msg[i] = '\0';	// on isole le pseudo
				
				/* on verifie que le nouveau pseudo n'existe pas deja */
				pthread_mutex_lock(&mutex);	// debut de la section critique
				for(i=0;i<first_free;i++)
				{
					if(!strcmp(&msg[8],clients[i]->pseudo))
						valid_pseudo = 0;
				}
				pthread_mutex_unlock(&mutex);	// fin de la section critique
								
				if(valid_pseudo)
				{
					old_pseudo = me->pseudo;
					me->pseudo = strdup(&msg[8]);
					snprintf(msg_to_send,8192,"%s s'appelle maintenant %s\r\n",old_pseudo,me->pseudo);
					free(old_pseudo);
					send_all(msg_to_send,INVALID_SOCKET);
				}
				else send_msg(sck,"Pseudo deja utilise!\r\n");
				valid_command = 1;
			}
			if(!strncmp(msg,"/quit",5))	// sortie "propre" du serveur (avec message)
			{
				int i;
				
				if(msg[5]=='=')
				{
					for(i=6;(msg[i]!='\0') && (msg[i]!='\r') && (msg[i]!='\n') && (msg[i]!='\t');i++);
					msg[i]='\0';
					client_quit(me,&msg[6]);
				}
				else client_quit(me,NULL);
				pthread_exit(NULL);
				valid_command = 1;	// inutile car pthread_exit() quitte le thread...
			}
			if(!strncmp(msg,"/list",5))	// obtenir la liste des pseudos sur le serveur
			{
				pthread_mutex_lock(&mutex);	// debut de la section critique
				for(i=0;i<first_free;i++)
				{
					send_msg(me->sock,clients[i]->pseudo);
					send_msg(me->sock,"\r\n");
				}
				pthread_mutex_unlock(&mutex);	// fin de la section critique
				valid_command = 1;
			}
			if(!strncmp(msg,"/admin=",7))	// droits admin (avec mot de passe)
			{
				if(!strncmp(&msg[7],ADMIN_PWD,strlen(ADMIN_PWD)))
				{
					send_msg(me->sock,"Droits administrateur actives.\r\n");
					me->admin = 1;
				}
				else send_msg(me->sock,"Mot de passe incorrect!\r\n");
				valid_command = 1;
			}
			if(!strncmp(msg,"/kick=",6))
			{
				if(me->admin)
				{
					int i;
					char *pseudo = &msg[6];
					int trouve = 0;
					
					pseudo[255+8] = '\0';	// on limite le pseudo a 255 caracteres
					for(i=0;(pseudo[i]!='\0') && (pseudo[i]!='\r') && (pseudo[i]!='\n') && (pseudo[i]!='\t');i++);
					pseudo[i] = '\0';	// on isole le pseudo

					/* on cherche si le pseudo existe */
					pthread_mutex_lock(&mutex);	// debut de la section critique
					for(i=0;i<first_free;i++)
					{
						if(!strcmp(pseudo,clients[i]->pseudo))
						{
							trouve = 1;
							if(!clients[i]->admin)
							{
								pthread_t th_id = clients[i]->id;
								s_client *client = clients[i];
								pthread_mutex_unlock(&mutex);
								send_msg(client->sock,"Vous etes kicke par un admin\r\n");
								client_quit(client,"Kicke par un admin");	
								pthread_cancel(th_id);	// termine le thread correspondant
								send_msg(me->sock,"Le client a ete kicke!\r\n");
								break;
							}
							else send_msg(me->sock,"Impossible de kicker un admin!\r\n");
						}
					}
					pthread_mutex_unlock(&mutex);	// fin de la section critique
					
					if(!trouve)
						send_msg(me->sock,"Impossible de trouver le pseudo!\r\n");
				}
				else send_msg(me->sock,"Cette commande necessite les droits administrateur!\r\n");
				valid_command = 1;
			}
			if(!strncmp(msg,"/?",2))
			{
				send_msg(me->sock,HELP_MSG);
				valid_command = 1;
			}
				
			if(!valid_command)	// commande invalide
				send_msg(sck,"Commande non valide!\r\n");
		}
		else			// message normal
		{	snprintf(msg_to_send,8192,"%s : %s",me->pseudo,msg);
			msg_to_send[8192] = '\0';
			send_all(msg_to_send,me->sock);
		}
	}
	
	return NULL;
}

/* fonction principale */
int main(int argc, char **argv)
{
	int server,sck;
	pthread_t th_id;
	
	printf(BANNER);
	
	server = create_server(PORT);
	while(1)
	{
		sck = server_accept(server,0);
		if(sck == INVALID_SOCKET)
		{
			printf("\nErreur de accept()!\n");
			exit(-1);
		}
		if(nb_clients < MAX_CLIENTS)
		{
			pthread_create(&th_id,NULL,interact,(void *)&sck);
			nb_clients++;
			printf("Nouveau client! %d clients\n",nb_clients);
		}
		else close(sck);
	}
	
	return 0;
}