FROM ghcr.io/srl-labs/network-multitool:latest

COPY bin/routingd /irp/bin/routingd
COPY bin/node_supervisor /irp/bin/node_supervisor
COPY bin/traffic_app /irp/bin/traffic_app

RUN chmod +x /irp/bin/routingd /irp/bin/node_supervisor /irp/bin/traffic_app \
    && ln -sf /irp/bin/routingd /irp/bin/irp_routerd_rs
