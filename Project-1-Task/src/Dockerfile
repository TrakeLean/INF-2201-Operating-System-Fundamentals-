FROM --platform=amd64 ubuntu AS build-stage

RUN apt-get update && apt-get upgrade -y
RUN apt-get install -y build-essential gcc-multilib

COPY . .

RUN make clean
RUN make

FROM scratch AS export-stage
COPY --from=build-stage image .