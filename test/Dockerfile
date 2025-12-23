FROM python:3.13-slim

WORKDIR /opt

RUN apt update  \
    && apt install -y --no-install-recommends make gcc libreadline-dev libfuse3-dev \
    && rm -rf /var/lib/apt

COPY requirements.txt .

RUN pip install -r requirements.txt \
  && find / -name "*.pyc" -exec rm -f {} \; \
  && rm -rf /root/.cache/

COPY . .

CMD ["./check.sh"]
