from socket import *

SERVER_NAME = 'localhost'
SERVER_PORT = 3000


def connect():
    # Create a socket of Address family AF_INET.
    sock = socket(AF_INET, SOCK_STREAM)
    # Client socket connection to the server
    sock.connect((SERVER_NAME, SERVER_PORT))

    return sock


def send(sock, message):
    sock.send(bytearray(message, 'utf-8'))


def recv(sock):
    return sock.recv(1024).decode('utf-8')

def client():
    connection = connect()
    sentence = input()

    while sentence != '\LEAVE':
        send(connection, sentence)
        response = recv(connection)
        print(response.strip())
        sentence = input()
    print("GOODBYE")


client()
