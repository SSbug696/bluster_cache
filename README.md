[![CodeFactor](https://www.codefactor.io/repository/github/ssbug696/bluster_cache/badge/master)](https://www.codefactor.io/repository/github/ssbug696/bluster_cache/overview/master)

# Bluster cache
#####  This is very simple and lightweight in-memory storage
Supported pipelining and common operations such as SET, GET, EXPIRE, DEL, FLUSH, SIZE, EXIST

Basic implementation on Epoll and Kqueue sockets(only *nix systems)

###### Prompt pattern for banch client 
```
./banch [ip] [port] [total request] [parallel clients] [mode] [key size] [value size]
Example: ./banch 127.0.0.1 8888 100000 2 mono 4 4
```

```
./banch [ip] [port] [total_request] [parallel clients] [mode] [size batch] [key size] [value size]
./banch 127.0.0.1 8888 100000 3 batch 50 2 8
```
###### Source requests format

format [msg_len]msg
```
Simple request: [17]set key somevalue
Pipelining: [38][set key somevalue,set key2 somevalue]
```

![Illustration](https://github.com/SSbug696/bluster_cache/blob/master/banch_client/img/img.png)
