# Bluster cache
#####  This is very simple and lightweight in-memory storage
Supported pipelining and common operations such as SET, GET, EXPIRE, DEL, FLUSH, SIZE, EXIST

Basic implementation on Epoll and Kqueue sockets(only *nix systems). Epoll(in process)

##### Request example:
 format [msg_len]msg

###### Simple:
```
[17]set key somevalue
```

###### With pipeline:
```
[38][set key somevalue,set key2 somevalue]
```
