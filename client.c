#include <stdio.h>

#include <string.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <netdb.h> 

#include <unistd.h>

#include <stdlib.h>



int main(int argc, char *argv[])

{

    int sockClient, numport, n;

    struct sockaddr_in adr_Serveur;

    struct hostent *serveur;



    char buffer[256];

    if (argc < 3) {

       fprintf(stderr,"syntaxe: %s nom_hote port\n", argv[0]);

       exit(0);

    }

    numport = atoi(argv[2]);

    sockClient = socket(AF_INET, SOCK_STREAM, 0);

    if (sockClient < 0) 

        {perror("Ouverture de la socket impossible");

		exit(1);

		}

    serveur = gethostbyname(argv[1]);

    if (serveur == NULL) {

        fprintf(stderr,"Erreur! Hôte introuvable\n");

        exit(0);

    }

    memset(&adr_Serveur,0, sizeof(adr_Serveur));

    adr_Serveur.sin_family = AF_INET;

    bcopy((char *)serveur->h_addr, 

         (char *)&adr_Serveur.sin_addr.s_addr,

         serveur->h_length);

    adr_Serveur.sin_port = htons(numport);


    if (connect(sockClient,(struct sockaddr*)&adr_Serveur,sizeof(adr_Serveur)) < 0) 

        {perror("Connexion impossible");

		exit(1);

		}

    printf("Veuillez saisir votre message: ");

    memset(buffer,0,256);

    fgets(buffer,255,stdin);

    n = write(sockClient,buffer,strlen(buffer));

    if (n < 0) 

         {perror("Erreur d’écriture sur la socket");

		exit(1);

		}

    memset(buffer,0,256);

    n = read(sockClient,buffer,255);

    if (n < 0) 

         {perror("Erreur de lecture de la socket");

		exit(1);

		}

    printf("%s\n",buffer);

    return 0;

}
















