PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
PYTHONPATH ?= src
RUST_CARGO ?= cargo
RUST_ROUTERD_CRATE ?= src/irp
RUST_ROUTERD_BIN ?= bin/routingd
RUST_NODE_SUPERVISOR_BIN ?= bin/node_supervisor
RUST_ROUTERD_BUILD_MODE ?= release
RUST_ROUTERD_TARGET ?= x86_64-unknown-linux-musl
EXP_REPEATS ?= 1
EXP_TOPOLOGY_FILE ?= src/clab/topologies/ring6.clab.yaml
EXP_LINK_DELAY_MS ?= 1.0
EXP_NODE_IMAGE ?= ghcr.io/srl-labs/network-multitool:latest
EXP_MGMT_NETWORK_NAME ?=
EXP_MGMT_IPV4_SUBNET ?=
EXP_MGMT_IPV6_SUBNET ?=
EXP_MGMT_EXTERNAL_ACCESS ?= 0
EXP_USE_SUDO ?= 0
ROUTERD_RS_CONFIG ?= experiments/routerd_examples/ospf_router1.yaml
ROUTERD_RS_LOG_LEVEL ?= INFO
LABGEN_PROTOCOL ?= ospf
LABGEN_PROFILE ?=
LABGEN_TOPOLOGY_FILE ?=
LABGEN_MGMT_NETWORK_NAME ?=
LABGEN_MGMT_IPV4_SUBNET ?=
LABGEN_MGMT_IPV6_SUBNET ?=
LABGEN_MGMT_EXTERNAL_ACCESS ?= 0
CHECK_TOPOLOGY_FILE ?=
CHECK_LAB_NAME ?=
CHECK_CONFIG_DIR ?=
CHECK_EXPECT_PROTOCOL ?= ospf
CHECK_MIN_ROUTES ?= -1
CHECK_OUTPUT_JSON ?=
CHECK_USE_SUDO ?= 0
CHECK_MAX_WAIT_S ?= 10
CHECK_POLL_INTERVAL_S ?= 1
ROUTERD_NODE_IMAGE ?= romam/network-multitool-routerd:latest
CLASSIC_PROFILE ?= ring6
CLASSIC_PROTOCOL ?= ospf
CLASSIC_OUTPUT_DIR ?= results/runs/routerd_labs/classic
CLASSIC_LAB_NAME ?= $(CLASSIC_PROFILE)-routerd-$(CLASSIC_PROTOCOL)
RUNLAB_USE_SUDO ?= 1
RUNLAB_KEEP_LAB ?= 0
RUNLAB_CHECK_TAIL_LINES ?= 60
RUNLAB_CHECK_MAX_WAIT_S ?= 10
RUNLAB_CHECK_POLL_INTERVAL_S ?= 1
RUNLAB_CHECK_MIN_ROUTES ?= -1
RUNLAB_CHECK_OUTPUT_JSON ?=
TRAFFIC_LAB_NAME ?=
TRAFFIC_NODE ?=
TRAFFIC_USE_SUDO ?= 1
TRAFFIC_BACKGROUND ?= 0
TRAFFIC_LOG_FILE ?= /tmp/traffic_app.log
TRAFFIC_ARGS ?=
TRAFFIC_PLAN_FILE ?=
UNIFIED_CONFIG_FILE ?=
UNIFIED_OUTPUT_JSON ?=
UNIFIED_USE_SUDO ?= 1
UNIFIED_POLL_INTERVAL_S ?= 1
UNIFIED_KEEP_LAB ?= 0
PROBE_LAB_NAME ?=
PROBE_SRC_NODE ?=
PROBE_DST_NODE ?=
PROBE_DST_IP ?=
PROBE_PROTO ?= udp
PROBE_PORT ?= 9000
PROBE_PACKET_SIZE ?= 256
PROBE_COUNT ?= 1000
PROBE_DURATION_S ?= 0
PROBE_PPS ?= 200
PROBE_REPORT_INTERVAL_S ?= 1
PROBE_WARMUP_S ?= 0.7
PROBE_TIMEOUT_S ?= 120
PROBE_OUTPUT_JSON ?=
PROBE_USE_SUDO ?= 1
TRAFFIC_GO_BIN ?= bin/traffic_app
TRAFFIC_GOOS ?= linux
TRAFFIC_GOARCH ?= amd64
TRAFFIC_CGO_ENABLED ?= 0
INSTALL_TRAFFIC_BIN_LAB_NAME ?=
INSTALL_TRAFFIC_BIN_TOPOLOGY_FILE ?=
INSTALL_TRAFFIC_BIN_NODES ?=
INSTALL_TRAFFIC_BIN_USE_SUDO ?= 1

.PHONY: install test lint build-routerd-rs run-routerd-rs build-routerd-node-image build-traffic-app-go install-traffic-app-bin run-containerlab-exp run-ospf-convergence-exp gen-routerd-lab gen-classic-routerd-lab check-routerd-lab run-routerd-lab run-traffic-app run-traffic-plan run-unified-experiment run-traffic-probe clean

install:
	$(PIP) install -e .[dev]

test:
	$(PYTHON) -m pytest

lint:
	ruff check src tests tools

build-routerd-node-image: build-routerd-rs build-traffic-app-go
	docker build -t $(ROUTERD_NODE_IMAGE) -f experiments/container_images/routerd-multitool/Dockerfile .

build-traffic-app-go:
	@command -v go >/dev/null 2>&1 || (echo "go not found in PATH"; exit 127)
	@mkdir -p $(dir $(TRAFFIC_GO_BIN))
	@cd src/applications_go && \
		GOOS=$(TRAFFIC_GOOS) GOARCH=$(TRAFFIC_GOARCH) CGO_ENABLED=$(TRAFFIC_CGO_ENABLED) \
		go build -o ../../$(TRAFFIC_GO_BIN) ./cmd/traffic_app
	@chmod +x $(TRAFFIC_GO_BIN)
	@echo "built $(TRAFFIC_GO_BIN) (GOOS=$(TRAFFIC_GOOS) GOARCH=$(TRAFFIC_GOARCH) CGO_ENABLED=$(TRAFFIC_CGO_ENABLED))"

install-traffic-app-bin:
	@test -n "$(INSTALL_TRAFFIC_BIN_LAB_NAME)" || (echo "INSTALL_TRAFFIC_BIN_LAB_NAME is required"; exit 2)
	@test -f "$(TRAFFIC_GO_BIN)" || (echo "binary missing: $(TRAFFIC_GO_BIN) (run make build-traffic-app-go)"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/install_traffic_app_bin.py \
		--lab-name $(INSTALL_TRAFFIC_BIN_LAB_NAME) \
		--bin-path $(TRAFFIC_GO_BIN) \
		$(if $(strip $(INSTALL_TRAFFIC_BIN_TOPOLOGY_FILE)),--topology-file $(INSTALL_TRAFFIC_BIN_TOPOLOGY_FILE),) \
		$(if $(strip $(INSTALL_TRAFFIC_BIN_NODES)),--nodes $(INSTALL_TRAFFIC_BIN_NODES),) \
		$(if $(filter 1 yes true,$(INSTALL_TRAFFIC_BIN_USE_SUDO)),--sudo,--no-sudo)

run-containerlab-exp:
	$(MAKE) run-ospf-convergence-exp

run-ospf-convergence-exp:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/ospf_convergence_exp.py \
		--topology-file $(EXP_TOPOLOGY_FILE) \
		--repeats $(EXP_REPEATS) \
		--link-delay-ms $(EXP_LINK_DELAY_MS) \
		--node-image $(EXP_NODE_IMAGE) \
		$(if $(strip $(EXP_MGMT_NETWORK_NAME)),--mgmt-network-name $(EXP_MGMT_NETWORK_NAME),) \
		$(if $(strip $(EXP_MGMT_IPV4_SUBNET)),--mgmt-ipv4-subnet $(EXP_MGMT_IPV4_SUBNET),) \
		$(if $(strip $(EXP_MGMT_IPV6_SUBNET)),--mgmt-ipv6-subnet $(EXP_MGMT_IPV6_SUBNET),) \
		$(if $(filter 1 yes true,$(EXP_MGMT_EXTERNAL_ACCESS)),--mgmt-external-access,) \
		$(if $(filter 1 yes true,$(EXP_USE_SUDO)),--sudo,)

build-routerd-rs:
	@command -v $(RUST_CARGO) >/dev/null 2>&1 || (echo "cargo not found in PATH"; exit 127)
	@mkdir -p $(dir $(RUST_ROUTERD_BIN)) $(dir $(RUST_NODE_SUPERVISOR_BIN))
	@cd $(RUST_ROUTERD_CRATE) && $(RUST_CARGO) build --$(RUST_ROUTERD_BUILD_MODE) --target $(RUST_ROUTERD_TARGET)
	@cp $(RUST_ROUTERD_CRATE)/target/$(RUST_ROUTERD_TARGET)/$(RUST_ROUTERD_BUILD_MODE)/routingd $(RUST_ROUTERD_BIN)
	@cp $(RUST_ROUTERD_CRATE)/target/$(RUST_ROUTERD_TARGET)/$(RUST_ROUTERD_BUILD_MODE)/node_supervisor $(RUST_NODE_SUPERVISOR_BIN)
	@chmod +x $(RUST_ROUTERD_BIN)
	@chmod +x $(RUST_NODE_SUPERVISOR_BIN)
	@echo "built $(RUST_ROUTERD_BIN) from $(RUST_ROUTERD_CRATE) ($(RUST_ROUTERD_BUILD_MODE), $(RUST_ROUTERD_TARGET))"
	@echo "built $(RUST_NODE_SUPERVISOR_BIN) from $(RUST_ROUTERD_CRATE) ($(RUST_ROUTERD_BUILD_MODE), $(RUST_ROUTERD_TARGET))"

run-routerd-rs:
	@test -x "$(RUST_ROUTERD_BIN)" || (echo "binary missing: $(RUST_ROUTERD_BIN) (run make build-routerd-rs)"; exit 2)
	$(RUST_ROUTERD_BIN) --config $(ROUTERD_RS_CONFIG) --log-level $(ROUTERD_RS_LOG_LEVEL)

gen-routerd-lab:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/generate_routerd_lab.py \
		--protocol $(LABGEN_PROTOCOL) \
		$(if $(strip $(LABGEN_PROFILE)),--profile $(LABGEN_PROFILE),) \
		$(if $(strip $(LABGEN_TOPOLOGY_FILE)),--topology-file $(LABGEN_TOPOLOGY_FILE),) \
		$(if $(strip $(LABGEN_MGMT_NETWORK_NAME)),--mgmt-network-name $(LABGEN_MGMT_NETWORK_NAME),) \
		$(if $(strip $(LABGEN_MGMT_IPV4_SUBNET)),--mgmt-ipv4-subnet $(LABGEN_MGMT_IPV4_SUBNET),) \
		$(if $(strip $(LABGEN_MGMT_IPV6_SUBNET)),--mgmt-ipv6-subnet $(LABGEN_MGMT_IPV6_SUBNET),) \
		$(if $(filter 1 yes true,$(LABGEN_MGMT_EXTERNAL_ACCESS)),--mgmt-external-access,)

gen-classic-routerd-lab:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/generate_routerd_lab.py \
		--profile $(CLASSIC_PROFILE) \
		--protocol $(CLASSIC_PROTOCOL) \
		--node-image $(ROUTERD_NODE_IMAGE) \
		--output-dir $(CLASSIC_OUTPUT_DIR) \
		--lab-name $(CLASSIC_LAB_NAME)

check-routerd-lab:
	@test -n "$(CHECK_TOPOLOGY_FILE)" || (echo "CHECK_TOPOLOGY_FILE is required"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/check_routerd_lab.py \
		--topology-file $(CHECK_TOPOLOGY_FILE) \
		$(if $(strip $(CHECK_LAB_NAME)),--lab-name $(CHECK_LAB_NAME),) \
		$(if $(strip $(CHECK_CONFIG_DIR)),--config-dir $(CHECK_CONFIG_DIR),) \
		--expect-protocol $(CHECK_EXPECT_PROTOCOL) \
		--min-routes $(CHECK_MIN_ROUTES) \
		--max-wait-s $(CHECK_MAX_WAIT_S) \
		--poll-interval-s $(CHECK_POLL_INTERVAL_S) \
		$(if $(strip $(CHECK_OUTPUT_JSON)),--output-json $(CHECK_OUTPUT_JSON),) \
		$(if $(filter 1 yes true,$(CHECK_USE_SUDO)),--sudo,)

run-routerd-lab:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/run_routerd_lab.py \
		--protocol $(LABGEN_PROTOCOL) \
		$(if $(strip $(LABGEN_PROFILE)),--profile $(LABGEN_PROFILE),) \
		$(if $(strip $(LABGEN_TOPOLOGY_FILE)),--topology-file $(LABGEN_TOPOLOGY_FILE),) \
		$(if $(strip $(LABGEN_MGMT_NETWORK_NAME)),--mgmt-network-name $(LABGEN_MGMT_NETWORK_NAME),) \
		$(if $(strip $(LABGEN_MGMT_IPV4_SUBNET)),--mgmt-ipv4-subnet $(LABGEN_MGMT_IPV4_SUBNET),) \
		$(if $(strip $(LABGEN_MGMT_IPV6_SUBNET)),--mgmt-ipv6-subnet $(LABGEN_MGMT_IPV6_SUBNET),) \
		$(if $(filter 1 yes true,$(LABGEN_MGMT_EXTERNAL_ACCESS)),--mgmt-external-access,) \
		$(if $(filter 1 yes true,$(RUNLAB_USE_SUDO)),--sudo,--no-sudo) \
		$(if $(filter 1 yes true,$(RUNLAB_KEEP_LAB)),--keep-lab,) \
		--check-tail-lines $(RUNLAB_CHECK_TAIL_LINES) \
		--check-max-wait-s $(RUNLAB_CHECK_MAX_WAIT_S) \
		--check-poll-interval-s $(RUNLAB_CHECK_POLL_INTERVAL_S) \
		--check-min-routes $(RUNLAB_CHECK_MIN_ROUTES) \
		$(if $(strip $(RUNLAB_CHECK_OUTPUT_JSON)),--check-output-json $(RUNLAB_CHECK_OUTPUT_JSON),)

run-traffic-app:
	@test -n "$(TRAFFIC_LAB_NAME)" || (echo "TRAFFIC_LAB_NAME is required"; exit 2)
	@test -n "$(TRAFFIC_NODE)" || (echo "TRAFFIC_NODE is required"; exit 2)
	@test -n "$(TRAFFIC_ARGS)" || (echo "TRAFFIC_ARGS is required"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/run_traffic_app.py \
		--lab-name $(TRAFFIC_LAB_NAME) \
		--node $(TRAFFIC_NODE) \
		$(if $(filter 1 yes true,$(TRAFFIC_USE_SUDO)),--sudo,--no-sudo) \
		$(if $(filter 1 yes true,$(TRAFFIC_BACKGROUND)),--background,) \
		--log-file $(TRAFFIC_LOG_FILE) \
		-- $(TRAFFIC_ARGS)

run-traffic-plan:
	@test -n "$(TRAFFIC_PLAN_FILE)" || (echo "TRAFFIC_PLAN_FILE is required"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/run_traffic_plan.py \
		--plan $(TRAFFIC_PLAN_FILE)

run-unified-experiment:
	@test -n "$(UNIFIED_CONFIG_FILE)" || (echo "UNIFIED_CONFIG_FILE is required"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/run_unified_experiment.py \
		--config $(UNIFIED_CONFIG_FILE) \
		--poll-interval-s $(UNIFIED_POLL_INTERVAL_S) \
		$(if $(strip $(UNIFIED_OUTPUT_JSON)),--output-json $(UNIFIED_OUTPUT_JSON),) \
		$(if $(filter 1 yes true,$(UNIFIED_USE_SUDO)),--sudo,--no-sudo) \
		$(if $(filter 1 yes true,$(UNIFIED_KEEP_LAB)),--keep-lab,)

run-traffic-probe:
	@test -n "$(PROBE_LAB_NAME)" || (echo "PROBE_LAB_NAME is required"; exit 2)
	@test -n "$(PROBE_SRC_NODE)" || (echo "PROBE_SRC_NODE is required"; exit 2)
	@test -n "$(PROBE_DST_NODE)" || (echo "PROBE_DST_NODE is required"; exit 2)
	@test -n "$(PROBE_DST_IP)" || (echo "PROBE_DST_IP is required"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) tools/run_traffic_probe.py \
		--lab-name $(PROBE_LAB_NAME) \
		--src-node $(PROBE_SRC_NODE) \
		--dst-node $(PROBE_DST_NODE) \
		--dst-ip $(PROBE_DST_IP) \
		--proto $(PROBE_PROTO) \
		--port $(PROBE_PORT) \
		--packet-size $(PROBE_PACKET_SIZE) \
		--count $(PROBE_COUNT) \
		--duration-s $(PROBE_DURATION_S) \
		--pps $(PROBE_PPS) \
		--report-interval-s $(PROBE_REPORT_INTERVAL_S) \
		--warmup-s $(PROBE_WARMUP_S) \
		--sender-timeout-s $(PROBE_TIMEOUT_S) \
		$(if $(strip $(PROBE_OUTPUT_JSON)),--output-json $(PROBE_OUTPUT_JSON),) \
		$(if $(filter 1 yes true,$(PROBE_USE_SUDO)),--sudo,--no-sudo)

clean:
	rm -rf .pytest_cache .ruff_cache __pycache__ experiments/__pycache__ src/__pycache__ tests/__pycache__ tools/__pycache__ dist build *.egg-info src/irp/target bin/routingd bin/irp_routerd_rs bin/node_supervisor
