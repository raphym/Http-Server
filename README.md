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
