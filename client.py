from socket import *
from threading import Thread
import sys
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

def send_client():
    sentence = input()
    leave = False
    while not leave:
        send(connection, sentence)
        sentence = input()
        if sentence == "\\LEAVE" :
            leave = True
    send(connection, sentence)

def rec_client():
    leave = False
    while not leave:
        response = recv(connection)
        print(response.strip())
        if response.strip() == "GOODBYE" : leave = True
        

connection = connect()
send_thread = Thread(target=send_client)
rec_thread = Thread(target=rec_client)
send_thread.start()
rec_thread.start()
send_thread.join()
rec_thread.join()
