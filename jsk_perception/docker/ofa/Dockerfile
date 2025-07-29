FROM pytorch/pytorch:1.8.1-cuda11.1-cudnn8-devel
ARG DEBIAN_FRONTEND=noninteractive
RUN apt -o Acquire::AllowInsecureRepositories=true update \
    && apt-get install -y \
    curl \
    git \
    libopencv-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
# use requirements.txt generetaed by `pip freeze | grep ==` in the container created on 2023-03-31
RUN git clone https://github.com/OFA-Sys/OFA \
    && mkdir -p /var/mount
RUN pip install "Cython<3" "numpy>=1.20.3,<1.24" "setuptools<60" wheel
RUN PIP_NO_BUILD_ISOLATION=1 pip install --no-build-isolation "pycocotools==2.0.6"
RUN cd OFA \
    && git checkout 1809b55 \
    && sed -i 's/pycocotools==2\.0\.4//g' requirements.txt \
    && sed -i 's/timm/timm==0.6.13/g' requirements.txt \
    && sed -i 's/datasets/datasets==2.11.0/g' requirements.txt \
    && pip install -r requirements.txt \
    && pip install "protobuf<3.20.0" "numpy>=1.20.3,<1.24" "pytorch-lightning==1.7.7" "torch==1.8.1" flask
RUN pip install torchmetrics==0.11.4
COPY server.py /workspace/OFA
ENTRYPOINT cd /workspace/OFA && python server.py
