#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>


using namespace std;

struct User {
    string nickname;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
};

struct Room {
    vector<User> user_list;
    void addUser(User u){ user_list.push_back(u); }
    void delUser(User u){ 
        user_list.erase(std::remove(user_list.begin(), user_list.end(), u), user_list.end());
    }
};