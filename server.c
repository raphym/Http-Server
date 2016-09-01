#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include "threadpool.h"

//DEFINE
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define INTERNAL_SERVER_ERROR 1
#define BAD_REQUEST 2
#define NOT_SUPORTED 3
#define NOT_FOUND 4
#define FOUND_RESPONSE 5
#define FORBIDEN_RESPONSE 6
#define DIRECTORY_RESPONSE 7
#define INDEX_RESPONSE 8
#define FILE_RESPONSE 9

//typedef
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

int sockServer=-1;//Socket of the server
int PORT=1024;
int POOL_SIZE=-1;
int MAX_NUMBER_OF_REQUEST=-1;
int NUMS_OF_REQUEST=0;


//Functions
int onlyNumber(char *str);
void powerServer();
int functHandler(void* arg);
char* Error(int error ,char *http,char *location);
char *get_mime_type(char *name);
char *headerConstractor(char *type,char *http,int contentLength,char* lastModified);

////////////////////////////////////////////////////////////////////////////////
																//MAIN//
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
	//if the pipe is broken
	struct sigaction sigOfPipe;
	memset(&sigOfPipe, 0, sizeof(sigOfPipe));
	sigOfPipe.sa_handler = SIG_IGN;
	sigOfPipe.sa_flags = 0;
	if (sigaction(SIGPIPE, &sigOfPipe, 0) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	//Check the numbers of arguments
	if(argc!=4)
	{
		fprintf(stderr, "Usage: server <port> <pool-size>\n");
		exit(-1);
	}
	//check the legacity of the arguments

	if(onlyNumber(argv[1])==-1 || onlyNumber(argv[2])==-1 || onlyNumber(argv[3])==-1)
	{
			fprintf(stderr, "Usage: server <port> <pool-size>\n");
			exit(-1);
	}

	PORT=atoi(argv[1]);
	POOL_SIZE=atoi(argv[2]);
	MAX_NUMBER_OF_REQUEST=atoi(argv[3]);

	if(PORT<=0 || POOL_SIZE<=0 || MAX_NUMBER_OF_REQUEST<=0)
	{
		fprintf(stderr, "Usage: server <port> <pool-size>\n");
		exit(-1);
	}

	//Power On the server
	powerServer();

	//create the thread pool
	threadpool *pool=create_threadpool(POOL_SIZE);
	if(pool==NULL)
	{
		fprintf(stderr, "error in the server.c to create the thread pool\n");
		close(sockServer);
		exit(-1);
	}
	SOCKADDR_IN sinClient = { 0 };
	int* sockClient=NULL;

	while(NUMS_OF_REQUEST<=MAX_NUMBER_OF_REQUEST)
	{
		bzero(&sinClient,sizeof(sinClient));
		sockClient=(int*)malloc(sizeof(int));
		if(sockClient == NULL)
		{
			perror("malloc()");
			continue;
		}

		*sockClient = accept(sockServer, NULL, NULL);
		if(*sockClient == -1 )
		{
			perror("accept()");
			free(sockClient);
			continue;
		}
		NUMS_OF_REQUEST++;
		dispatch(pool, functHandler, (void*)sockClient);
	}//END OF LOOP

	close(sockServer);
	destroy_threadpool(pool);
	return 0;
}//END OF MAIN

////////////////////////////////////////////////////////////////////////////////
																//POWER_SERVER//
////////////////////////////////////////////////////////////////////////////////
void powerServer()
{
	//open a new connection for the server
	/******************************************************/
	/* La création du socket du server */
	sockServer = socket(AF_INET, SOCK_STREAM, 0);
	if(sockServer == -1)
	{
		perror("socket()");
		exit(-1);
	}
	/******************************************************/
	/*Création de l'interface*/
	SOCKADDR_IN sin = { 0 };
	//nous sommes un serveur, nous acceptons n'importe quelle adresse
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);//the port
	if(bind (sockServer, (SOCKADDR *) &sin, sizeof sin) == -1)
	{
		perror("bind()");
		close(sockServer);
		exit(-1);
	}
	/******************************************************/
	/*Ecoute et connexion des clients*/
	if(listen(sockServer, 5) == -1)
	{
		perror("listen()");
		close(sockServer);
		exit(-1);
	}
}
////////////////////////////////////////////////////////////////////////////////
																//HANDLER//
////////////////////////////////////////////////////////////////////////////////
int functHandler(void* arg)
{
	int socket = *((int*)arg);
	free(arg);
	//Variables
	char http[4096]={0};
	strcat(http,"HTTP/1.0 ");
	char *request[3]={0};
	char *response=NULL;
	char *header=NULL;
	char buffer1[4096];
	char *buffer2=NULL;
	char *temp=NULL;
	char *internError=NULL;
	int i=0;
	int n =0;
	int numOfFile=0;
	struct stat statOfPathRequest={0};
	struct stat statTemp={0};
	struct dirent *lecture=NULL;
	struct dirent *lecture2=NULL;
	char *type=NULL;
	DIR *rep=NULL;


	if((n = recv(socket, buffer1, sizeof(buffer1)-1, 0)) < 0)
	{
		perror("recv()");
		internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
		if(internError==NULL)
		{
			close(socket);
			return -1;
		}
		if( send(socket, internError, strlen(internError), 0) < 0)
		{
			perror("send()");
			close(socket);
			free(internError);
			return -1;
		}
		close(socket);
		free(internError);
		return -1;
	}
	buffer2 =strtok(buffer1,"\r\n");//take the first line
	//strTok on the buffer2 //cut the first line (buffer2)
	temp =strtok(buffer2," ");
	while(temp!=NULL)
	{
		if(i==3)//check if there is more than 3 tokens
		{
			response=Error(BAD_REQUEST,http,NULL);
			if(response==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, response, strlen(response), 0) < 0)
			{
				perror("send()");
			}
			close(socket);
			free(response);
			return -1;
		}
		request[i]=temp;
		temp = strtok(NULL," ");
		i++;
	}

	//check if there is less than 3 tokens
	if((!request[0] || !request[1] || !request[2] ) )
	{
		response=Error(BAD_REQUEST,http,NULL);
		if(response==NULL)
		{
			close(socket);
			return -1;
		}
		if( send(socket, response, strlen(response), 0) < 0)
		{
			perror("send()");
		}
		close(socket);
		free(response);
		return -1;
	}

	//The version of http
	if(!strcmp(request[2],"HTTP/1.0"))
		{
			bzero(http,4096);
			strcat(http,"HTTP/1.0 ");
		}
	if(!strcmp(request[2],"HTTP/1.1"))
	{
		bzero(http,4096);
		strcat(http,"HTTP/1.1 ");
	}

	//check if the http version is legal
	if(strcmp(request[2],"HTTP/1.0") && strcmp(request[2],"HTTP/1.1"))
	{
		response=Error(BAD_REQUEST,http,NULL);
		if(response==NULL)
		{
			close(socket);
			return -1;
		}
		if( send(socket, response, strlen(response), 0) < 0)
		{
			perror("send()");
		}
		close(socket);
		free(response);
		return -1;
	}

	//Check if the method is GET
	if(strcmp(request[0],"GET" ))
	{
		response=Error(NOT_SUPORTED,http,NULL);
		if(response==NULL)
		{
			close(socket);
			return -1;
		}
		if( send(socket, response, strlen(response), 0) < 0)
		{
			perror("send()");
		}
		close(socket);
		free(response);
		return -1;
	}

	//the full path
	char fullPath[6000]={0};
	char current[6000]={0};

	char *cwd = getcwd( current, 6000 + 1 );
	if( cwd == NULL )
	{
		internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
		if(internError==NULL)
		{
			close(socket);
			return -1;
		}
		if( send(socket, internError, strlen(internError), 0) < 0)
		{
			perror("send()");
			close(socket);
			free(internError);
			return -1;
		}
		free(internError);
		close(socket);
		return -1;
	}

	//if request[1]= "/"
	if(strlen(request[1])==1)
	{
		strcat(fullPath, current);
		strcat(fullPath,request[1]);
	}
	else if(strlen(request[1]) > strlen(current))
	{
		if(!strncmp(request[1], current, strlen(current)))
		{
			strcat(fullPath, request[1]);
		}
		else
			{
				strcat(fullPath, current);
				strcat(fullPath, request[1]);
			}
	}
	else if(strlen(request[1]) <= strlen(current))
	{
		strcat(fullPath, current);
		strcat(fullPath, request[1]);
	}

	//Check if the path is exist
	memset(&statOfPathRequest, 0, sizeof(statOfPathRequest));
  int returnValue = stat(fullPath, &statOfPathRequest);
  if(returnValue != 0)
	{
		response=Error(NOT_FOUND,http,NULL);
		if(response==NULL)
		{
			close(socket);
			return -1;
		}
		if( send(socket, response, strlen(response), 0) < 0)
		{
			perror("send()");
		}
		close(socket);
		free(response);
		return -1;
  }

	/*CHECK PERMITION*/
	struct stat stat_the_full_path;

	char theFullPath[strlen(fullPath)];
	bzero(theFullPath,strlen(fullPath));
	strcat(theFullPath, fullPath);

	char *toCheck=(char*)malloc(2*strlen(fullPath));
	if(toCheck==NULL)
	{
		internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
		if(internError==NULL)
		{
			close(socket);
			return -1;
		}
		if( send(socket, internError, strlen(internError), 0) < 0)
		{
			perror("send()");
			close(socket);
			free(internError);
			return -1;
		}
		close(socket);
		free(internError);
		return -1;
	}
	bzero(toCheck,2*strlen(fullPath));
	char *toCheck2=NULL;
	//strTok on the input
	toCheck2=strtok(theFullPath,"/");
	if(toCheck2!=NULL)
	{
		strcat(toCheck,"/" );
		strcat(toCheck, toCheck2);

		memset(&stat_the_full_path, 0, sizeof(stat_the_full_path));
		stat(toCheck, &stat_the_full_path);
		if(S_ISDIR(stat_the_full_path.st_mode))
		{
			if((!(( (stat_the_full_path.st_mode) & S_IRWXU))) || (!((stat_the_full_path.st_mode) & S_IRWXG)) || (!((stat_the_full_path.st_mode) & S_IRWXO)))
			{
				response=Error(FORBIDEN_RESPONSE,http,NULL);
				if(response==NULL)
				{
					close(socket);
					free(toCheck);
					return -1;
				}
				if( send(socket, response, strlen(response), 0) < 0)
				{
					perror("send()");
					free(response);
					free(toCheck);
					close(socket);
					return -1;
				}
				free(response);
				free(toCheck);
				close(socket);
				return 0;
			}
		}

		if(S_ISREG(stat_the_full_path.st_mode))
		{
			if((!(((stat_the_full_path.st_mode) & S_IRGRP))) || (!((stat_the_full_path.st_mode)& S_IRUSR)) || (!((stat_the_full_path.st_mode) & S_IROTH)))
			{
				response=Error(FORBIDEN_RESPONSE,http,NULL);
				if(response==NULL)
				{
					close(socket);
					free(toCheck);
					return -1;
				}
				if( send(socket, response, strlen(response), 0) < 0)
				{
					perror("send()");
					free(response);
					free(toCheck);
					close(socket);
					return -1;
				}
				free(response);
				free(toCheck);
				close(socket);
				return 0;
			}
		}
		strcat(toCheck,"/" );
	}

	while(toCheck2!=NULL)
	{
		toCheck2 = strtok(NULL,"/");
		if(toCheck2!=NULL)
		{
			strcat(toCheck, toCheck2);
			memset(&stat_the_full_path, 0, sizeof(stat_the_full_path));
			stat(toCheck, &stat_the_full_path);
			if(S_ISDIR(stat_the_full_path.st_mode))
			{
				if((!(( (stat_the_full_path.st_mode) & S_IRWXU))) || (!((stat_the_full_path.st_mode) & S_IRWXG)) || (!((stat_the_full_path.st_mode) & S_IRWXO)))
				{
					response=Error(FORBIDEN_RESPONSE,http,NULL);
					if(response==NULL)
					{
						close(socket);
						free(toCheck);
						return -1;
					}
					if( send(socket, response, strlen(response), 0) < 0)
					{
						perror("send()");
						free(response);
						free(toCheck);
						close(socket);
						return -1;
					}
					free(response);
					free(toCheck);
					close(socket);
					return 0;
				}
			}

			if(S_ISREG(stat_the_full_path.st_mode))
			{
				if((!(((stat_the_full_path.st_mode) & S_IRGRP))) || (!((stat_the_full_path.st_mode)& S_IRUSR)) || (!((stat_the_full_path.st_mode) & S_IROTH)))
				{
					response=Error(FORBIDEN_RESPONSE,http,NULL);
					if(response==NULL)
					{
						close(socket);
						free(toCheck);
						return -1;
					}
					if( send(socket, response, strlen(response), 0) < 0)
					{
						perror("send()");
						free(response);
						free(toCheck);
						close(socket);
						return -1;
					}
					free(response);
					free(toCheck);
					close(socket);
					return 0;
				}
			}
			strcat(toCheck,"/" );
		}
	}
	free(toCheck);
	/*END CHECK PERMITION*/

	//check if directory
	if( statOfPathRequest.st_mode & S_IFDIR)
	{
		if(fullPath[strlen(fullPath)-1]!= '/')
		{
			//have to contain '/'
			strcat(fullPath, "/");
			response=Error(FOUND_RESPONSE,http,fullPath);
			if(response==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, response, strlen(response), 0) < 0)
			{
				perror("send()");
			}
			close(socket);
			free(response);
			return 0;
		}

									/*DIRECTORY*/
		//OPEN the direcory , scan if there is index.html
		rep = opendir (fullPath);
		if(rep==NULL)
		{
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			free(internError);
			close(socket);
			return -1;
		}

    while ((lecture = readdir (rep)))
    {
			if(!strcmp(lecture->d_name,"index.html"))
			{
				char thePath[strlen(fullPath)+2*strlen(lecture->d_name)];
				bzero(thePath,strlen(fullPath)+2*strlen(lecture->d_name));
				strcat(thePath, fullPath);
				strcat(thePath, lecture->d_name);
				memset(&statTemp, 0, sizeof(statTemp));
				stat(thePath, &statTemp);

				int fdIndex=open(thePath,O_RDONLY);
				if(fdIndex==-1)
				{
					closedir(rep);
					internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
					if(internError==NULL)
					{
						close(socket);
						return -1;
					}
					if( send(socket, internError, strlen(internError), 0) < 0)
					{
						perror("send()");
						free(internError);
						close(socket);
						return -1;
					}
					free(internError);
					close(socket);
					return -1;
				}
				header=headerConstractor("text/html", http, statTemp.st_size, ctime(&statTemp.st_mtime));
				if(header==NULL)
				{
					close(fdIndex);
					closedir(rep);
					internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
					if(internError==NULL)
					{
						close(socket);
						return -1;
					}
					if( send(socket, internError, strlen(internError), 0) < 0)
					{
						perror("send()");
						close(socket);
						free(internError);
						return -1;
					}
					close(socket);
					free(internError);
					return -1;
				}
				if( send(socket, header, strlen(header), 0) < 0)
				{
					perror("send");
					free(header);
					close(fdIndex);
					closedir(rep);
					close(socket);
					return -1;
				}
				free(header);
				n=1;
				response=(char*)malloc(1000);
				if(response==NULL)
				{
					close(fdIndex);
					closedir(rep);
					internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
					if(internError==NULL)
					{
						close(socket);
						return -1;
					}
					if( send(socket, internError, strlen(internError), 0) < 0)
					{
						perror("send()");
						close(socket);
						free(internError);
						return -1;
					}
					free(internError);
					close(socket);
					return -1;
				}
				while(n!=0)
				{
					n=read(fdIndex,response,1000);
					if(n==-1)
					{
						free(response);
						close(fdIndex);
						closedir(rep);
						internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
						if(internError==NULL)
						{
							close(socket);
							return -1;
						}
						if( send(socket, internError, strlen(internError), 0) < 0)
						{
							perror("send()");
							close(socket);
							free(internError);
							return -1;
						}
						free(internError);
						close(socket);
						return -1;
					}

					//send the page index.html
					if( send(socket, response, n, 0) < 0)
					{
						perror("send");
						free(response);
						close(fdIndex);
						closedir(rep);
						close(socket);
						return -1;
					}
				}
				free(response);
				close(fdIndex);
				closedir(rep);
				close(socket);
				return 0;
			}
			numOfFile++;
    }//End of While
    closedir(rep);
		rep = NULL;
		//There is no index.html so show the directory
		//variables
		int sizeLine=600*sizeof(char); //the size of a line in the table
		int sizeOfResponse=(2*strlen(fullPath) +  sizeLine*numOfFile + 550);
		char length[4096]={0};//for the size
		response=(char*)malloc(sizeOfResponse);
		if(response==NULL)
		{
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			free(internError);
			close(socket);
			return -1;
		}
		bzero(response,sizeOfResponse);
		//open the directory to scan it
		rep = opendir(fullPath);
		if(rep==NULL)
		{
			free(response);
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			close(socket);
			free(internError);
			return -1;
		}
		//construct the response
		strcat(response, "<HTML><HEAD><TITLE>Index of ");
		strcat(response, request[1]);
		strcat(response, "</TITLE></HEAD><BODY><H4>Index of ");
		strcat(response, request[1]);
		strcat(response, "</H4><table CELLSPACING=8><tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>");

		char *thePath2=(char*)malloc(strlen(fullPath)+600);//fullPath + name of file for the struct stat
		if(thePath2==NULL)
		{
			free(response);
			closedir(rep);
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			free(internError);
			close(socket);
			return -1;
		}
		//construct , add to the response for each line of the directory
    while ((lecture2 = readdir (rep)))
    {
			bzero(length,strlen(length));
			bzero(thePath2,strlen(fullPath)+600);
			strcat(thePath2, fullPath);
			strcat(thePath2, lecture2->d_name);
			memset(&statTemp, 0, sizeof(statTemp));
			stat(thePath2, &statTemp);

			if( statTemp.st_mode & S_IFREG )
				sprintf(length, "%d", (int)statTemp.st_size);
			else
				sprintf(length, "%s", "");


			strcat(response, "<tr>" );
			strcat(response, "<td>" );
			strcat(response, "<a href='");
			strcat(response, thePath2);
			strcat(response, "'>");
			strcat(response,lecture2->d_name);
			strcat(response, "</a>");
			strcat(response, "</td>" );
			strcat(response, "<td>" );
			strcat(response, ctime(&statTemp.st_mtime) );
			strcat(response, "</td>" );
			strcat(response, "<td>" );
			strcat(response, length);
			strcat(response, "</td>" );
			strcat(response, "</tr>" );
			strcat(response, "\n" );
		}
		free(thePath2);
		closedir(rep);
		//construct , add the end of the table in the response
		strcat(response, "</table>\n");
		strcat(response, "<HR>\n");
		strcat(response, "<ADDRESS>webserver/1.0</ADDRESS>\n");
		strcat(response, "</body></html>");
		strcat(response, "\r\n\r\n");


		//contruct the header
		header=headerConstractor("text/html",http,strlen(response),ctime(&statOfPathRequest.st_mtime));
		if(header==NULL)
		{
			free(response);
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			close(socket);
			free(internError);
			return -1;
		}
		//send the header
		if( send(socket, header, strlen(header), 0) < 0)
		{
			perror("send()");
			close(socket);
			free(header);
			free(response);
			return -1;
		}
		free(header);
		//send the response
		int n2=0;
		n=0;
		while(n2<strlen(response))
		{
			n=send(socket, response + n2, 1000, 0);
			if(n<0)
			{
				perror("send");
				free(response);
				close(socket);
				return -1;
			}
			n2=n2+n;
		}
		free(response);
		close(socket);
		return 0;
	}
	//FILE Regular
	else if( statOfPathRequest.st_mode & S_IFREG )
  {
		type=get_mime_type(fullPath);
		header=headerConstractor(type,http,statOfPathRequest.st_size, ctime(&statOfPathRequest.st_mtime));
		if(header==NULL)
		{
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			close(socket);
			free(internError);
			return -1;
		}
		int fdFile = open(fullPath,O_RDONLY);
		if(fdFile==-1)
		{
			perror("fail to open() file");
			free(header);

			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			free(internError);
			close(socket);
			return -1;
		}
		if( send(socket, header, strlen(header), 0) < 0)
		{
			perror("send()");
			free(header);
			close(fdFile);
			close(socket);
			return -1;
		}
		free(header);

		response =(char*)malloc(1000*sizeof(char));
		if(response==NULL)
		{
			perror("error of malloc of response");
			close(fdFile);
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			close(socket);
			free(internError);
			return -1;
		}
		bzero(response,1000*sizeof(char));
		n=1;
		while(n!=0)
		{
			n=read(fdFile,response,1000*sizeof(char));
			if(n==-1)
			{
				free(response);
				close(fdFile);
				internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
				if(internError==NULL)
				{
					close(socket);
					return -1;
				}
				if( send(socket, internError, strlen(internError), 0) < 0)
				{
					perror("send()");
					close(socket);
					free(internError);
					return -1;
				}
				free(internError);
				close(socket);
				return -1;
			}
			if( send(socket, response, n, 0) < 0)
			{
				perror("send()");
				free(response);
				close(fdFile);
				close(socket);
				return -1;
			}
			bzero(response,1000*sizeof(char));
		}
		free(response);
		close(fdFile);
		close(socket);
		return 0;
  }
	else if(!statOfPathRequest.st_mode & S_IFREG )
	{
		response=Error(FORBIDEN_RESPONSE,http,NULL);
		if(response==NULL)
		{
			internError=Error(INTERNAL_SERVER_ERROR,http,NULL);
			if(internError==NULL)
			{
				close(socket);
				return -1;
			}
			if( send(socket, internError, strlen(internError), 0) < 0)
			{
				perror("send()");
				close(socket);
				free(internError);
				return -1;
			}
			free(internError);
			close(socket);
			return -1;
		}

		if( send(socket, response, strlen(response), 0) < 0)
		{
			perror("send()");
			free(response);
			close(socket);
			return -1;
		}
		free(response);
		close(socket);
		return 0;
	}
	close(socket);
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
																//ERROR//
////////////////////////////////////////////////////////////////////////////////
char* Error(int error,char* http,char *location)
{
	//TIME
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

	//response
	char *response=NULL;

	//-------------------------INTERNAL_SERVER_ERROR--------------------//
	if(error==INTERNAL_SERVER_ERROR)
	{
		response=(char*)malloc( ((11*26) *sizeof(char)) + strlen(timebuf) + 120*sizeof(char) );
		if(response==NULL)
		{
			perror("error of malloc in the ERROR function\n");
			return NULL;
		}
		bzero(response, ((11*26) *sizeof(char)) + strlen(timebuf) + 120*sizeof(char) );
		//header
		strcat(response, http);
		strcat(response, "500 Internal Server Error\n");
		strcat(response, "Server: webserver/1.0\n");
		strcat(response, "Date: ");
		strcat(response, timebuf);
		strcat(response, "\n");
		strcat(response, "Content-Type: text/html\n");
		strcat(response, "Content-Length: 144\n");
		strcat(response, "Connection: close\r\n");
		strcat(response, "\r\n");

		//body
		strcat(response, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n");
		strcat(response, "<BODY><H4>500 Internal Server Error</H4>\n");
		strcat(response, "Some server side error.");
		strcat(response, "</BODY></HTML>");
		strcat(response,"\r\n\r\n");
	}
	//-----------------------------BAD_REQUEST--------------------------//
	else if(error==BAD_REQUEST)
	{
		response=(char*)malloc( ((11*26) *sizeof(char)) + strlen(timebuf) + 120*sizeof(char) );
		if(response==NULL)
		{
			perror("error of malloc in the ERROR function\n");
			return NULL;
		}
		bzero(response, ((11*26) *sizeof(char)) + strlen(timebuf) + 120*sizeof(char) );
		//header
		strcat(response, http);
		strcat(response, "400 Bad Request\n");
		strcat(response, "Server: webserver/1.0\n");
		strcat(response, "Date: ");
		strcat(response, timebuf);
		strcat(response, "\n");
		strcat(response, "Content-Type: text/html\n");
		strcat(response, "Content-Length: 106\n");
		strcat(response, "Connection: close\r\n");
		strcat(response, "\r\n");

		//body
		strcat(response, "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n");
		strcat(response, "<BODY><H4>400 Bad request</H4>\n");
		strcat(response, "Bad Request.");
		strcat(response, "</BODY></HTML>");
		strcat(response,"\r\n\r\n");
	}
//-----------------------------NOT_SUPORTED--------------------------//
	else if(error==NOT_SUPORTED)
	{
		response=(char*)malloc( ((11*28) *sizeof(char)) + strlen(timebuf) + 140*sizeof(char) );
		if(response==NULL)
		{
			perror("error of malloc in the ERROR function\n");
			return NULL;
		}
		bzero(response, ((11*28) *sizeof(char)) + strlen(timebuf) + 140*sizeof(char) );
		//header
		strcat(response,http );
		strcat(response,"501 Not supported\n" );
		strcat(response, "Server: webserver/1.0\n");
		strcat(response, "Date: ");
		strcat(response, timebuf);
		strcat(response, "\n");
		strcat(response, "Content-Type: text/html\n");
		strcat(response,"Content-Length: 129\n" );
		strcat(response,"Connection: close\r\n" );
		strcat(response, "\r\n");

		//body
		strcat(response,"<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n" );
		strcat(response, "<BODY><H4>501 Not supported</H4>\n");
		strcat(response, "Method is not supported.\n");
		strcat(response,"</BODY></HTML>");
		strcat(response, "\r\n\r\n");
	}
//-----------------------------NOT_FOUND--------------------------------//
	else if(error==NOT_FOUND)
	{
		response=(char*)malloc( ((11*28) *sizeof(char)) + strlen(timebuf) + 140*sizeof(char) );
		if(response==NULL)
		{
			perror("error of malloc in the ERROR function\n");
			return NULL;
		}
		bzero(response, ((11*28) *sizeof(char)) + strlen(timebuf) + 140*sizeof(char) );

		//header
		strcat(response,http);
		strcat(response,"404 Not Found\n" );
		strcat(response, "Date: ");
		strcat(response, "Server: webserver/1.0\n");
		strcat(response, timebuf);
		strcat(response, "\n");
		strcat(response, "Content-Type: text/html\n");
		strcat(response,"Content-Length: 129\n" );
		strcat(response,"Connection: close\r\n" );
		strcat(response, "\r\n");

		//body
		strcat(response,"<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n" );
		strcat(response, "<BODY><H4>404 Not Found</H4>\n");
		strcat(response, "File not found.\n");
		strcat(response,"</BODY></HTML>");
		strcat(response, "\r\n\r\n");
	}
	//-----------------------------FOUND_RESPONSE--------------------------------//
	else if(error==FOUND_RESPONSE)
	{
		response=(char*)malloc( ((15*28) *sizeof(char)) + strlen(timebuf) + strlen(location) + 130*sizeof(char) );
		if(response==NULL)
		{
			perror("error of malloc in the ERROR function\n");
			return NULL;
		}
		bzero(response, ((15*28) *sizeof(char)) + strlen(timebuf) + strlen(location)+ 130*sizeof(char) );

		//header
		strcat(response,http);
		strcat(response,"302 Found\n" );
		strcat(response, "Server: webserver/1.0\n");
		strcat(response, "Date: ");
		strcat(response, timebuf);
		strcat(response, "\n");
		strcat(response, "Location: ");
		strcat(response, location);
		strcat(response, "\n");
		strcat(response, "Content-Type: text/html\n");
		strcat(response,"Content-Length: 123\n" );
		strcat(response,"Connection: close\r\n" );
		strcat(response, "\r\n");

		//body
		strcat(response,"<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n" );
		strcat(response, "<BODY><H4>302 Found</H4>\n");
		strcat(response, "Directories must end with a slash.\n");
		strcat(response,"</BODY></HTML>");
		strcat(response, "\r\n\r\n");
	}
	//-----------------------------FORBIDEN_RESPONSE--------------------------------//
	else if(error==FORBIDEN_RESPONSE)
	{
		response=(char*)malloc( ((11*28) *sizeof(char)) + strlen(timebuf) + 130*sizeof(char) );
		if(response==NULL)
		{
			perror("error of malloc in the ERROR function\n");
			return NULL;
		}
		bzero(response, ((11*28) *sizeof(char)) + strlen(timebuf) + 130*sizeof(char) );

		//header
		strcat(response,http);
		strcat(response,"403 Forbidden\n" );
		strcat(response, "Server: webserver/1.0\n");
		strcat(response, "Date: ");
		strcat(response, timebuf);
		strcat(response, "\n");
		strcat(response, "Content-Type: text/html\n");
		strcat(response,"Content-Length: 111\n" );
		strcat(response,"Connection: close\r\n" );
		strcat(response, "\r\n");

		//body
		strcat(response,"<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n" );
		strcat(response, "<BODY><H4>403 Forbidden</H4>\n");
		strcat(response, "Access denied.\n");
		strcat(response,"</BODY></HTML>");
		strcat(response, "\r\n\r\n");
	}
	return response;
}


////////////////////////////////////////////////////////////////////////////////
																//headerConstractor//
////////////////////////////////////////////////////////////////////////////////

char *headerConstractor(char *type,char *http,int contentLength,char* lastModified)
{
	char *header=(char*)malloc(5720*sizeof(char*));
	if(header==NULL)
	{
		perror("error of malloc in the ERROR function\n");
		return NULL;
	}
	bzero(header,5720*sizeof(char*));
	//TIME
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));

	//content length
	char length[4096];
	sprintf(length, "%d", contentLength);

	//HEADER of the response
	strcat(header,http);
	strcat(header,"200 OK\n");
	strcat(header,"Server: webserver/1.0\n");
	strcat(header,"Date:");
	strcat(header,timebuf);
	strcat(header,"\n");
	if(type!=NULL)
	{
		strcat(header,"Content-Type: ");
		strcat(header,type);
		strcat(header,"\n");
	}
	strcat(header,"Content-Length: ");
	strcat(header, length);
	strcat(header,"\n");
	strcat(header,"Last-Modified: ");
	strcat(header,lastModified);
	strcat(header,"Connection: close\r\n");
	strcat(header, "\r\n");

	return header;
}

////////////////////////////////////////////////////////////////////////////////
													//get_mime_type//
////////////////////////////////////////////////////////////////////////////////
char *get_mime_type(char *name)
{
	char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".txt") == 0 ) return "text/html";//rajout
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	if (strcmp(ext, ".mp4") == 0) return "video/mp4";
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////
															//Only numbers//
////////////////////////////////////////////////////////////////////////////////
//this function return 0 if there is only numbers
int onlyNumber(char *str)
{
	if(strlen(str)==0)
		return -1;
	int i=0;
	while(str[i] != '\0')
	{
		if(str[i]<48 || str[i]>57)
			return -1;
		i++;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////
															//END//
////////////////////////////////////////////////////////////////////////////////
