
simple shell for windows

[ usage

If you execute this by itself, it will listen for incoming connections on port 443.
Once a connection has been established, it will execute cmd.exe with input/output
redirected through the socket handle returned by WSASocket() API.

If you provide an address to listen on, but not -l option, it will attempt connection
to remote host and execute cmd.exe.

