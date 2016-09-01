#HomeWork

##Goals:
In this programming assignment you will write HTTP server. Students are not required to
implement the full HTTP specification, but only a very limited subset of it.
You will implement the following A HTTP server that:
- Constructs an HTTP response based on client's request.
- Sends the response to the client.

##Program Description and What You Need to Do:
- You will write two source files, server and threadpool.
- The server should handle the connections with the clients. As we saw in class, when using
TCP, a server creates a socket for each client it talks to. In other words, there is always
one socket where the server listens to connections and for each client connection request,
the server opens another socket.
- In order to enable multithreaded program, the server
should create threads that handle the connections with the clients.
Since, the server should maintain a limited number of threads, it construct a thread pool. In other words,
the server create the pool of threads in advanced and each time it needs a thread to
handle a client connection, it take one from the pool or enqueue the request if there is no
available thread in the pool.
- Command line usage: server port pool-size max-number-of-request
- Port is the port number your server will listen on, pool-size is the number of threads in the
pool and -number-of-request is the maximum number of request your server will handle
before it destroys the pool. 

##Program flow:
1. Server creates pool of threads, threads wait for jobs.
2. Server accept a new connection from a client (aka a new socket fd)
3. Server dispatch a job - call dispatch with the main negotiation function and fd as a
parameter. dispatch will add work_t item to the queue.
4. When there will be an available thread, it will takes a job from the queue and run
the negotiation function.
5. Read request from socket
6. Check input: The request first line should contain method, path and protocol. Here,
you only have to check that there are 3 tokens and that the last one is one of the
http versions, other checks on the method and the path will be checked later. In
case the request is wrong, send 400 "Bad Request" respond, as in file 400.txt.
7. You should support only the GET method, if you get another method, return error
message "501 not supported", as in file 501.txt
8. If the requested path does not exist, return error message "404 Not Found", as in
file 404.txt. The requested path is absolute, i.e. you should look for the path from
the server root directory.
9. If path is directory but it does not end with a '/', return 302 Found response, as in
302.txt. Note that the location header contains the original path + '/'. Real browser
will automatically look for the new path.
10. If path is directory and it ends with a '/', search for index.html
a. If there is such file, return it.
b. Else, return the contents of the directory in the format as in file
dir_content.txt.
11. If the path is a file
a. if the file is not regular (you can use S_ISREG) or the caller has no 'read'
permissions, send 403 Forbidden response, as in file 403.txt. The file has to
have read permission for everyone and if the file is in some directory, all the
directories in the path have to have executing permissions.
b. otherwise, return the file, format in file file.txt

##The threadpool
The pool is implemented by a queue. When the server gets a connection (getting back from
accept()), it should put the connection in the queue. When there will be available thread
(can be immediately), it will handle this connection (read request and write response).
You should implement the functions in threadpool.h.
The server should first init the thread pool by calling the function create_threadpool(int).
This function gets the size of the pool.

##Assumptions:
1. You can ignore headers on client requests. In other words, you need to parse only
the first line.
2. You can assume that the length of the first line in the request is no more than
4000 bytes.
3. When you're constructing a directory page, you can assume that the length of each
entity line is no more than 500 bytes.

##Compile the server:
Remember that you have to compile with the –lpthread flag.
Call your executable file 'server'.

##Test the server:
You can use a browser. The address line should be http://computer-name:portnum/your-path.
