[![CodeFactor](https://www.codefactor.io/repository/github/ssbug696/bluster_cache/badge/master)](https://www.codefactor.io/repository/github/ssbug696/bluster_cache/overview/master)

# Bluster cache
####  This is very simple and lightweight in-memory storage
Supported pipelining and common operations such as SET, GET, EXPIRE, DEL, FLUSH, SIZE, EXIST

Basic implementation on Epoll and Kqueue sockets(only *nix systems)

##### Prompt pattern for banch client 
| Command | Required| Default |
| ------ | ----- |------  |
|host[-h]|false|127.0.0.1|
|port[-p]|true|-|
|requests[-r]|false|10000|
|parallel clients[-c]|false|2|
|mode[-m]|false|mono|
|key size[-ksz]|false|2|
|value size[-vsz]|false|2|
|batch size[-bsz]|only for "batch" mode|-|

```
Batch mode: ./banch -p 8888 -r 1000000 -c 3 -m mono
Per request mode: ./banch -p 8888 -r 100000 -c 3 -m batch -bsz 50
```

###### Source requests format

format [msg_len]msg
```
Simple request: [17]set key somevalue
Pipelining: [38][set key somevalue,set key2 somevalue]
```

![Illustration](https://github.com/SSbug696/bluster_cache/blob/master/banch_client/img/img.png)
