PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
PYTHONPATH ?= src
CONFIG ?= configs/experiments/failure_recovery.yaml
EXP_N_NODES ?= 50
EXP_REPEATS ?= 5
EXP_TOPOLOGY ?= er
EXP_ER_P ?= 0.12
EXP_BA_M ?= 2

.PHONY: install test lint run-emu run-mininet run-mininet-exp eval clean

install:
	$(PIP) install -e .[dev]

test:
	$(PYTHON) -m pytest

lint:
	ruff check src tests scripts

run-emu:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) -m rpf.cli.main run --config $(CONFIG)

run-mininet:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) -m rpf.cli.main run-mininet --config $(CONFIG)

run-mininet-exp:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) exps/ospf_coverage_mininet_exp.py \
		--n-nodes $(EXP_N_NODES) \
		--repeats $(EXP_REPEATS) \
		--topology $(EXP_TOPOLOGY) \
		--er-p $(EXP_ER_P) \
		--ba-m $(EXP_BA_M)

eval:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) -m rpf.eval.summarize --runs results/runs --out results/tables/summary.csv

clean:
	rm -rf .pytest_cache .ruff_cache dist build *.egg-info
