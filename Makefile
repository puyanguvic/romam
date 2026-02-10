PYTHON ?= python3
PIP ?= $(PYTHON) -m pip
PYTHONPATH ?= src
CONFIG ?= configs/experiments/failure_recovery.yaml

.PHONY: install test lint run-emu run-mininet eval clean

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

eval:
	PYTHONPATH=$(PYTHONPATH) $(PYTHON) -m rpf.eval.summarize --runs results/runs --out results/tables/summary.csv

clean:
	rm -rf .pytest_cache .ruff_cache dist build *.egg-info
