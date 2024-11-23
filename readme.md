compile client on client connecting to server:
clang -D_POSIX_C_SOURCE=200809L -o chat_client client.c -lpthread

compile server where chat is served from and server user:

clang -o chat_server server.c -lpthread
