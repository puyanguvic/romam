PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
PYTHONPATH ?= src
EXP_N_NODES ?= 6
EXP_REPEATS ?= 1
EXP_TOPOLOGY ?= er
EXP_ER_P ?= 0.12
EXP_BA_M ?= 2
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
LABGEN_TOPOLOGY ?= ring
LABGEN_N_NODES ?= 6
LABGEN_SEED ?= 42
LABGEN_MGMT_NETWORK_NAME ?=
LABGEN_MGMT_IPV4_SUBNET ?=
LABGEN_MGMT_IPV6_SUBNET ?=
LABGEN_MGMT_EXTERNAL_ACCESS ?= 0
CHECK_TOPOLOGY_FILE ?=
CHECK_EXPECT_PROTOCOL ?= ospf
CHECK_MIN_ROUTES ?= -1
CHECK_OUTPUT_JSON ?=
CHECK_USE_SUDO ?= 0
CHECK_MAX_WAIT_S ?= 10
CHECK_POLL_INTERVAL_S ?= 1

.PHONY: install test lint run-containerlab-exp run-ospf-convergence-exp run-routerd gen-routerd-lab check-routerd-lab clean

install:
	$(PIP) install -e .[dev]

test:
	$(PYTHON) -m pytest

lint:
	ruff check src tests scripts exps

run-containerlab-exp:
	$(MAKE) run-ospf-convergence-exp

run-ospf-convergence-exp:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/ospf_convergence_exp.py \
		--n-nodes $(EXP_N_NODES) \
		--repeats $(EXP_REPEATS) \
		--topology $(EXP_TOPOLOGY) \
		--er-p $(EXP_ER_P) \
		--ba-m $(EXP_BA_M) \
		--link-delay-ms $(EXP_LINK_DELAY_MS) \
		--node-image $(EXP_NODE_IMAGE) \
		$(if $(strip $(EXP_MGMT_NETWORK_NAME)),--mgmt-network-name $(EXP_MGMT_NETWORK_NAME),) \
		$(if $(strip $(EXP_MGMT_IPV4_SUBNET)),--mgmt-ipv4-subnet $(EXP_MGMT_IPV4_SUBNET),) \
		$(if $(strip $(EXP_MGMT_IPV6_SUBNET)),--mgmt-ipv6-subnet $(EXP_MGMT_IPV6_SUBNET),) \
		$(if $(filter 1 yes true,$(EXP_MGMT_EXTERNAL_ACCESS)),--mgmt-external-access,) \
		$(if $(filter 1 yes true,$(EXP_USE_SUDO)),--sudo,)

run-routerd:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) -m rpf.routerd \
		--config $(ROUTERD_CONFIG) \
		--log-level $(ROUTERD_LOG_LEVEL)

gen-routerd-lab:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/generate_routerd_lab.py \
		--protocol $(LABGEN_PROTOCOL) \
		--topology $(LABGEN_TOPOLOGY) \
		--n-nodes $(LABGEN_N_NODES) \
		--seed $(LABGEN_SEED) \
		$(if $(strip $(LABGEN_MGMT_NETWORK_NAME)),--mgmt-network-name $(LABGEN_MGMT_NETWORK_NAME),) \
		$(if $(strip $(LABGEN_MGMT_IPV4_SUBNET)),--mgmt-ipv4-subnet $(LABGEN_MGMT_IPV4_SUBNET),) \
		$(if $(strip $(LABGEN_MGMT_IPV6_SUBNET)),--mgmt-ipv6-subnet $(LABGEN_MGMT_IPV6_SUBNET),) \
		$(if $(filter 1 yes true,$(LABGEN_MGMT_EXTERNAL_ACCESS)),--mgmt-external-access,)

check-routerd-lab:
	@test -n "$(CHECK_TOPOLOGY_FILE)" || (echo "CHECK_TOPOLOGY_FILE is required"; exit 2)
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/check_routerd_lab.py \
		--topology-file $(CHECK_TOPOLOGY_FILE) \
		--expect-protocol $(CHECK_EXPECT_PROTOCOL) \
		--min-routes $(CHECK_MIN_ROUTES) \
		--max-wait-s $(CHECK_MAX_WAIT_S) \
		--poll-interval-s $(CHECK_POLL_INTERVAL_S) \
		$(if $(strip $(CHECK_OUTPUT_JSON)),--output-json $(CHECK_OUTPUT_JSON),) \
		$(if $(filter 1 yes true,$(CHECK_USE_SUDO)),--sudo,)

clean:
	rm -rf .pytest_cache .ruff_cache dist build *.egg-info
