# Authors: Fredrik Mørstad & Fredrik Fasteraune
FROM ubuntu:latest

WORKDIR /src

RUN apt update && apt upgrade -y && apt install -y build-essential gcc-multilib gdb

COPY . .

ENTRYPOINT ["tail", "-f", "/dev/null"]
