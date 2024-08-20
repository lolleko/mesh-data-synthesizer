FROM nvidia/cuda:12.1.1-runtime-ubuntu22.04
RUN apt-get update && \
    apt-get install -y python3-pip python3-dev && \
    rm -rf /var/lib/apt/lists/*

COPY requirements_torch.txt /tmp/requirements_torch.txt
RUN pip3 install -r /tmp/requirements_torch.txt

COPY requirements_server.txt /tmp/requirements_server.txt
RUN pip3 install -r /tmp/requirements_server.txt

COPY requirements_salad.txt /tmp/requirements_salad.txt
RUN pip3 install -r /tmp/requirements_salad.txt

COPY app/ /app
