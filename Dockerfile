FROM ubuntu:16.04
RUN apt-get update &&\
    apt-get  install -y build-essential libgtk-3-dev libcairo2-dev libgit2-dev transfig \
  imagemagick git 
COPY . /eeshow
WORKDIR /eeshow
RUN make && PREFIX=/eeshow make install


FROM ubuntu:16.04
RUN apt-get update &&\
    apt-get install -y transfig libgtk-3-0 libcairo2 libgit2-24 imagemagick git
COPY --from=0 /eeshow/bin /usr/local/bin
CMD ["eeshow"]