PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
PYTHONPATH ?= src
EXP_REPEATS ?= 1
EXP_TOPOLOGY_FILE ?= clab_topologies/ring6.clab.yaml
EXP_LINK_DELAY_MS ?= 1.0
EXP_NODE_IMAGE ?= ghcr.io/srl-labs/network-multitool:latest
EXP_MGMT_NETWORK_NAME ?=
EXP_MGMT_IPV4_SUBNET ?=
EXP_MGMT_IPV6_SUBNET ?=
EXP_MGMT_EXTERNAL_ACCESS ?= 0
EXP_USE_SUDO ?= 0
ROUTERD_CONFIG ?= exps/routerd_examples/ospf_router1.yaml
ROUTERD_LOG_LEVEL ?= INFO
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
CLASSIC_OUTPUT_DIR ?= clab_topologies/generated
CLASSIC_LAB_NAME ?= $(CLASSIC_PROFILE)-routerd-$(CLASSIC_PROTOCOL)
RUNLAB_USE_SUDO ?= 1
RUNLAB_KEEP_LAB ?= 0
RUNLAB_CHECK_TAIL_LINES ?= 60
RUNLAB_CHECK_MAX_WAIT_S ?= 10
RUNLAB_CHECK_POLL_INTERVAL_S ?= 1
RUNLAB_CHECK_MIN_ROUTES ?= -1
RUNLAB_CHECK_OUTPUT_JSON ?=

.PHONY: install test lint build-routerd-node-image run-containerlab-exp run-ospf-convergence-exp run-routerd gen-routerd-lab gen-classic-routerd-lab check-routerd-lab run-routerd-lab clean

install:
	$(PIP) install -e .[dev]

test:
	$(PYTHON) -m pytest

lint:
	ruff check src tests scripts exps

build-routerd-node-image:
	docker build -t $(ROUTERD_NODE_IMAGE) -f exps/container_images/routerd-multitool/Dockerfile .

run-containerlab-exp:
	$(MAKE) run-ospf-convergence-exp

run-ospf-convergence-exp:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/ospf_convergence_exp.py \
		--topology-file $(EXP_TOPOLOGY_FILE) \
		--repeats $(EXP_REPEATS) \
		--link-delay-ms $(EXP_LINK_DELAY_MS) \
		--node-image $(EXP_NODE_IMAGE) \
		$(if $(strip $(EXP_MGMT_NETWORK_NAME)),--mgmt-network-name $(EXP_MGMT_NETWORK_NAME),) \
		$(if $(strip $(EXP_MGMT_IPV4_SUBNET)),--mgmt-ipv4-subnet $(EXP_MGMT_IPV4_SUBNET),) \
		$(if $(strip $(EXP_MGMT_IPV6_SUBNET)),--mgmt-ipv6-subnet $(EXP_MGMT_IPV6_SUBNET),) \
		$(if $(filter 1 yes true,$(EXP_MGMT_EXTERNAL_ACCESS)),--mgmt-external-access,) \
		$(if $(filter 1 yes true,$(EXP_USE_SUDO)),--sudo,)

run-routerd:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) -m irp.routerd \
		--config $(ROUTERD_CONFIG) \
		--log-level $(ROUTERD_LOG_LEVEL)

gen-routerd-lab:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/generate_routerd_lab.py \
		--protocol $(LABGEN_PROTOCOL) \
		$(if $(strip $(LABGEN_PROFILE)),--profile $(LABGEN_PROFILE),) \
		$(if $(strip $(LABGEN_TOPOLOGY_FILE)),--topology-file $(LABGEN_TOPOLOGY_FILE),) \
		$(if $(strip $(LABGEN_MGMT_NETWORK_NAME)),--mgmt-network-name $(LABGEN_MGMT_NETWORK_NAME),) \
		$(if $(strip $(LABGEN_MGMT_IPV4_SUBNET)),--mgmt-ipv4-subnet $(LABGEN_MGMT_IPV4_SUBNET),) \
		$(if $(strip $(LABGEN_MGMT_IPV6_SUBNET)),--mgmt-ipv6-subnet $(LABGEN_MGMT_IPV6_SUBNET),) \
		$(if $(filter 1 yes true,$(LABGEN_MGMT_EXTERNAL_ACCESS)),--mgmt-external-access,)

gen-classic-routerd-lab:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/generate_routerd_lab.py \
		--profile $(CLASSIC_PROFILE) \
		--protocol $(CLASSIC_PROTOCOL) \
		--node-image $(ROUTERD_NODE_IMAGE) \
		--output-dir $(CLASSIC_OUTPUT_DIR) \
		--lab-name $(CLASSIC_LAB_NAME)

check-routerd-lab:
	@test -n "$(CHECK_TOPOLOGY_FILE)" || (echo "CHECK_TOPOLOGY_FILE is required"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/check_routerd_lab.py \
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
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/run_routerd_lab.py \
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

clean:
	rm -rf .pytest_cache .ruff_cache dist build *.egg-info
