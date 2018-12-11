/*
 * chatserverts.c - A concurrent chat server using threads
 * and a message buffer.
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAXUSERS 100
#define MAXROOMS 100

 /* Simplifies calls to bind(), connect(), and accept() */
typedef struct sockaddr SA;

/* Max text line length */
#define MAXLINE 8192

/* Second argument to listen() */
#define LISTENQ 1024

// A lock for the user and room lists.
pthread_mutex_t room_lock;
pthread_mutex_t user_lock;

//Room list
struct Room* roomList[MAXROOMS];

//roomlist index
int roomi = 0;

struct User {
	char nickname[30];
	int sockfd;
	struct Room *room;
};

struct Room {
	char name[30];
	struct User * user_list[MAXUSERS];
	int users;
};

struct Room *create_room(char *name) {

	struct Room *newRoom = malloc(sizeof(struct Room));
	strcpy(newRoom->name, name);
	newRoom->users = 0;
	pthread_mutex_lock(&room_lock);
	roomList[roomi++] = newRoom;
	pthread_mutex_unlock(&room_lock);
	return newRoom;
}

struct User *create_user(char *name, int connfd, struct Room* room) {
	struct User *newUser = malloc(sizeof(struct User));
	strcpy(newUser->nickname, name);
	newUser->sockfd = connfd;
	pthread_mutex_lock(&user_lock);
	room->user_list[room->users] = newUser;
	room->users++;
	pthread_mutex_unlock(&user_lock);
	newUser->room = room;
	return newUser;
}

void print_room_list() {
	for (int i = 0; i < roomi; i++) {
		printf("%s\n", roomList[i]->name);
	}
}

int find_room_index(int connfd) {
	for (int i = 0; i < roomi; i++) {
		for (int j = 0; j < roomList[i]->users; j++) {
			if (roomList[i]->user_list[j]->sockfd == connfd) {
				return i;
			}
		}
	} return -1;
}


struct User *user_from_connfd(int connfd) {
	int roomindex = find_room_index(connfd);
	for (int i = 0; i < roomList[roomindex]->users; i++) {
		if (roomList[roomindex]->user_list[i]->sockfd == connfd) return roomList[roomindex]->user_list[i];
	}
	return NULL;
}

//returns 1 on successful delete, -1 otherwise
int delete_user(int connfd, struct Room *room) {
	pthread_mutex_lock(&user_lock);
	for (int i = 0; i < room->users; i++) {
		if (room->user_list[i]->sockfd == connfd) {
			room->user_list[i] = room->user_list[room->users - 1];
			room->users--;
			return 1;
		}
	}
	pthread_mutex_lock(&user_lock);
	return -1;
}

// A wrapper around recv to simplify calls.
int receive_message(int connfd, char *message) {
	return recv(connfd, message, MAXLINE, 0);
}

// A wrapper around send to simplify calls.
int send_message(int connfd, char *message) {
	return send(connfd, message, strlen(message), 0);
}

int process_message(int connfd, char *message) {
	if (strncmp(message, "\\", 1) == 0) {
		if (strncmp(message, "\\JOIN", 5) == 0) {
			char *nickname = strtok(message + 5, " ");
			char *room_name = strtok(NULL, " ");
			int room_exists = 0;
			for (int i = 0; i < roomi; i++) {
				if (strncmp(roomList[i]->name, room_name, strlen(room_name)) == 0) {
					create_user(nickname, connfd, roomList[i]);
					room_exists = 1;
				}
			}
			if (!room_exists) {
				struct Room *newRoom = create_room(room_name);
				create_user(nickname, connfd, newRoom);
			}
			return send_message(connfd, room_name);
		}
		else if (strncmp(message, "\\ROOMS", 6) == 0) {
			char room_list[MAXROOMS * 32] = "";
			for (int i = 0; i < roomi; i++) {
				strcat(room_list, roomList[i]->name);
				strcat(room_list, "\n");
			}
			return send_message(connfd, room_list);
		}
		else if (strncmp(message, "\\LEAVE", 6) == 0) {
			for (int i = 0; i < roomi; i++) {
				if (delete_user(connfd, roomList[i]) == 1) {
					return send_message(connfd, "GOODBYE");
				}
			}
			return send_message(connfd, "You are not currently in any room");
		}
		else if (strncmp(message, "\\WHO", 4) == 0) {
			char user_list[MAXUSERS * 32] = "";
			int index = find_room_index(connfd);
			for (int i = 0; i < roomList[index]->users; i++) {
				strcat(user_list, roomList[index]->user_list[i]->nickname);
				strcat(user_list, "\n");
			}
			return send_message(connfd, user_list);
		}
		else if (strncmp(message, "\\HELP", 5) == 0) {
			//sorry this is an abortion idk how to do string line spacing
			char ret[1000] = "";
			strcat(ret, "Possible commands are:\n");
			strcat(ret, "\\JOIN nickname room: When the server receives this command it will add the user to the\n list of users associated with a particular room. If the room does not exist it will create a\n new room and add the user to the list of users associated with the new room. The server must\n respond to the client with the name of the room.\n");
			strcat(ret, "\\ROOMS: When the server receives this command it will respond with a list of the available rooms.\n");
			strcat(ret, "\\LEAVE: When the server receives this command it will remove the user from the room and, send the single message GOODBYE, and disconnect the client.\n");
			strcat(ret, "\\WHO: When the server receives this command it will send a list the users in the room the user is currently in.\n");
			strcat(ret, "\\nickname message: When the server receives this command it will send the message to the user specified by nickname.\n");
			return send_message(connfd, ret);
		}
		else {
			char *nickname = strtok(message + 1, " ");
			char *direct_message = strtok(NULL, " ");
			int roomindex = find_room_index(connfd);
			struct User *target_user = NULL;
			struct Room *currentroom = roomList[roomindex];
			for (int i = 0; i < currentroom->users; i++) {
				if (strncmp(currentroom->user_list[i]->nickname, nickname, strlen(nickname)) == 0) {
					target_user = currentroom->user_list[i];
					break;
				}
			}
			if (target_user == NULL) {
				char ret[300] = "";
				strcat(ret, message);
				strcat(ret, " command not recognized.");
				return send_message(connfd, ret);
			}
			char name_message[300] = "";
			struct User *who = user_from_connfd(connfd);
			if (who == NULL) return -1;
			strcat(name_message, who->nickname);
			strcat(name_message, ": ");
			strcat(name_message, direct_message);
			return send_message(target_user->sockfd, name_message);



		}
	}
	else {
		int roomindex = find_room_index(connfd);
		char name_message[300] = "";
		struct User *who = user_from_connfd(connfd);
		if (who == NULL) return -1;
		strcat(name_message, who->nickname);
		strcat(name_message, ": ");
		strcat(name_message, message);
		for (int i = 0; i < roomList[roomindex]->users; i++) {
			send_message(roomList[roomindex]->user_list[i]->sockfd, name_message);
		}
	}

}

// The main function that each thread will execute.
void chat(int connfd) {
	size_t n;

	// Holds the received message.
	char message[MAXLINE];

	while ((n = receive_message(connfd, message)) > 0) {
		message[n] = '\0';  // null terminate message (for string operations)
		printf("Server received message %s (%d bytes)\n", message, (int)n);
		n = process_message(connfd, message);
		printf("%d\n", n);
	}
}

// Helper function to establish an open listening socket on given port.
int open_listenfd(int port) {
	int listenfd;    // the listening file descriptor.
	int optval = 1;  //
	struct sockaddr_in serveraddr;

	/* Create a socket descriptor */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

	/* Eliminates "Address already in use" error from bind */
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
		sizeof(int)) < 0)
		return -1;

	/* Listenfd will be an endpoint for all requests to port
	   on any IP address for this host */
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);
	if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) return -1;

	/* Make it a listening socket ready to accept connection requests */
	if (listen(listenfd, LISTENQ) < 0) return -1;
	return listenfd;
}

// thread function prototype as we have a forward reference in main.
void *thread(void *vargp);

int main(int argc, char **argv) {
	// Check the program arguments and print usage if necessary.
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	// Initialize the room and user locks.
	pthread_mutex_init(&user_lock, NULL);
	pthread_mutex_init(&room_lock, NULL);

	// The port number for this server.
	int port = atoi(argv[1]);

	// The listening file descriptor.
	int listenfd = open_listenfd(port);

	// The main server loop - runs forever...
	while (1) {
		// The connection file descriptor.
		int *connfdp = malloc(sizeof(int));

		// The client's IP address information.
		struct sockaddr_in clientaddr;

		// Wait for incoming connections.
		socklen_t clientlen = sizeof(struct sockaddr_in);
		*connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen);

		/* determine the domain name and IP address of the client */
		struct hostent *hp =
			gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);

		// The server IP address information.
		char *haddrp = inet_ntoa(clientaddr.sin_addr);

		// The client's port number.
		unsigned short client_port = ntohs(clientaddr.sin_port);

		printf("server connected to %s (%s), port %u\n", hp->h_name, haddrp,
			client_port);

		// Create a new thread to handle the connection.
		pthread_t tid;
		pthread_create(&tid, NULL, thread, connfdp);
	}
}

/* thread routine */
void *thread(void *vargp) {
	// Grab the connection file descriptor.
	int connfd = *((int *)vargp);
	// Detach the thread to self reap.
	pthread_detach(pthread_self());
	// Free the incoming argument - allocated in the main thread.
	free(vargp);
	// Handle the chat client requests.
	chat(connfd);
	printf("client disconnected.\n");
	// Don't forget to close the connection!
	close(connfd);
	return NULL;
}
