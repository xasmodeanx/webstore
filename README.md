# webstore
A web-based arbitrary data storage service that accepts z85 encoded data

## Base Docker Image
[Ubuntu](https://hub.docker.com/_/ubuntu) 20.04 (x64)

## Software
[libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) \
[libgcrypt](https://gnupg.org/software/libgcrypt/index.html) \
[redis](https://redis.io/)

## Get the image from Docker Hub
```
docker pull fullaxx/webstore
```

## Build it locally using the github repository
```
docker build -f Dockerfile.webstore -t="fullaxx/webstore" github.com/Fullaxx/webstore
```

## Volume Options
webstore will create a log file in /log/ \
Use the following volume option to expose it
```
-v /srv/docker/webstore/log:/log
```

## Server Instructions
Start redis at 172.17.0.1:6379 \
Bind webstore to 172.17.0.1:8080 \
Save log to /srv/docker/webstore/log
```
docker run -d --rm --name redis -p 172.17.0.1:6379:6379 redis
docker run -d --name webstore -e REDISIP=172.17.0.1 -p 172.17.0.1:8080:8080 -v /srv/docker/webstore/log:/log fullaxx/webstore
```
Start redis at 172.17.0.1:7777 \
Bind webstore to 172.17.0.1:8080
```
docker run -d --rm --name redis -p 172.17.0.1:7777:6379 redis
docker run -d --name webstore -e REDISIP=172.17.0.1 -e REDISPORT=7777 -p 172.17.0.1:8080:8080 fullaxx/webstore
```

## HTTPS Instructions
In order to enable https mode, you must provide the key/certificate pair under /cert \
Use the following options to provide the files under /cert
```
-e CERTFILE=cert.pem \
-e KEYFILE=privkey.pem \
-v /srv/docker/mycerts:/cert
```
A full https example with certbot certs:
```
docker run -d --rm --name redis -p 172.17.0.1:6379:6379 redis
docker run -d --name webstore \
-p 172.17.0.1:443:8080 -e REDISIP=172.17.0.1 \
-e CERTFILE=config/live/webstore.mydomain.com/fullchain.pem \
-e KEYFILE=config/live/webstore.mydomain.com/privkey.pem \
-v /srv/docker/certbot:/cert \
fullaxx/webstore
```

## Client Instructions
In order to use the client, you will need to compile against libcurl and libgcrypt. \
In Ubuntu, you would install build-essential libcurl4-gnutls-dev and libgcrypt20-dev. \
After compiling the client source code, you can run the binaries:
```
apt-get install -y build-essential libcurl4-gnutls-dev libgcrypt20-dev
./compile_clients.sh
WSIP="172.17.0.1"
PORT="8080"
./ws_post.exe -H ${WSIP} -P ${PORT} -f LICENSE -a 1
./ws_get.exe  -H ${WSIP} -P ${PORT} -t b234ee4d69f5fce4486a80fdaf4a4263
```
