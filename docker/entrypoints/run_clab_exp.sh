#!/usr/bin/env bash
set -euo pipefail

sudo_flag=()
if [[ "${EXP_USE_SUDO:-0}" == "1" ]]; then
  sudo_flag=(--sudo)
fi

mgmt_name_flag=()
if [[ -n "${EXP_MGMT_NETWORK_NAME:-}" ]]; then
  mgmt_name_flag=(--mgmt-network-name "${EXP_MGMT_NETWORK_NAME}")
fi

mgmt_v4_flag=()
if [[ -n "${EXP_MGMT_IPV4_SUBNET:-}" ]]; then
  mgmt_v4_flag=(--mgmt-ipv4-subnet "${EXP_MGMT_IPV4_SUBNET}")
fi

mgmt_v6_flag=()
if [[ -n "${EXP_MGMT_IPV6_SUBNET:-}" ]]; then
  mgmt_v6_flag=(--mgmt-ipv6-subnet "${EXP_MGMT_IPV6_SUBNET}")
fi

mgmt_external_access_flag=()
if [[ "${EXP_MGMT_EXTERNAL_ACCESS:-0}" == "1" ]]; then
  mgmt_external_access_flag=(--mgmt-external-access)
fi

python exps/ospf_coverage_containerlab_exp.py \
  --n-nodes "${EXP_N_NODES:-50}" \
  --repeats "${EXP_REPEATS:-5}" \
  --topology "${EXP_TOPOLOGY:-er}" \
  --er-p "${EXP_ER_P:-0.12}" \
  --ba-m "${EXP_BA_M:-2}" \
  --link-delay-ms "${EXP_LINK_DELAY_MS:-1.0}" \
  --node-image "${EXP_NODE_IMAGE:-ghcr.io/srl-labs/network-multitool:latest}" \
  --clab-image "${EXP_CLAB_IMAGE:-ghcr.io/srl-labs/clab:latest}" \
  "${mgmt_name_flag[@]}" \
  "${mgmt_v4_flag[@]}" \
  "${mgmt_v6_flag[@]}" \
  "${mgmt_external_access_flag[@]}" \
  "${sudo_flag[@]}"
