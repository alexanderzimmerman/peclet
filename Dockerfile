FROM dealii/dealii:v8.5.pre.4-gcc-mpi-fulldepsmanual-debugrelease

RUN git clone https://github.com/alexanderzimmerman/peclet.git && \
    cd peclet && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    cd ~
