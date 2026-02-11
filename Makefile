PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
PYTHONPATH ?= src
EXP_N_NODES ?= 50
EXP_REPEATS ?= 5
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

.PHONY: install test lint run-containerlab-exp clean

install:
	$(PIP) install -e .[dev]

test:
	$(PYTHON) -m pytest

lint:
	ruff check src tests scripts exps

run-containerlab-exp:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/ospf_coverage_containerlab_exp.py \
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

clean:
	rm -rf .pytest_cache .ruff_cache dist build *.egg-info
