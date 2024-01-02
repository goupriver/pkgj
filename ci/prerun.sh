#!/bin/bash

apt update -y && apt upgrade -y && \
apt install -y build-essential git make cmake python3-pip ninja-build curl && \
pip3 install --user pipenv && \
curl -sSL https://install.python-poetry.org | python3 - && \
sed  -i  '1 i\export PATH="/root/.local/bin:$PATH"\n' ~/.bashrc && \
source ~/.bashrc && \
cd ~ && \
git clone https://github.com/blastrock/pkgj.git && \ 
cd pkgj && \

sed -i 's/export CC=gcc-12/export CC=gcc-11/' ci/ci.sh && \
sed -i 's/export CXX=g++-12/export CXX=g++-11/' ci/ci.sh && \
sed -i 's/"x$CI" != xtrue/ false == true /' ci/setup_conan.sh && \

echo -e  "\n\n  \033[32mNow you can run the script \033[36m ci/ci.sh\n \033[0m"	