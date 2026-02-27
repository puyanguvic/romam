use std::collections::BTreeMap;
use std::fs;
use std::path::PathBuf;
use std::time::Instant;

use clap::Parser;
use irp::protocols::route_compute::{
    compute_multimetric_route_entries, compute_scalar_route_entries, LinkConstraints, LinkMetrics,
    MultiMetricGraph, MultiMetricRouteAlgorithm, MultiMetricRouteStrategyConfig,
    NextHopSelectionPolicy, ScalarRouteAlgorithm, ScalarRouteStrategyConfig, StrategyRouteEntry,
    WeightedSumCoefficients,
};
use serde::Deserialize;
use serde_json::{json, Value};

#[derive(Debug, Parser)]
#[command(name = "route_compute_bench")]
#[command(about = "Benchmark route_compute shortest-path strategies")]
struct Args {
    #[arg(long, default_value_t = 100)]
    nodes: usize,
    #[arg(long, default_value_t = 0.08)]
    density: f64,
    #[arg(long, default_value_t = 3)]
    seeds: usize,
    #[arg(long, default_value_t = 1)]
    start_seed: u64,
    #[arg(long, default_value_t = 3)]
    k_paths: usize,
    #[arg(long, default_value_t = 8)]
    pareto_max_paths: usize,
    #[arg(long, default_value_t = 8)]
    iterations: usize,
    #[arg(long)]
    topology_json: Option<PathBuf>,
    #[arg(long)]
    output_json: Option<PathBuf>,
}

#[derive(Debug, Clone, Deserialize)]
struct EdgeInput {
    from: u32,
    to: u32,
    weight: f64,
    bandwidth: Option<f64>,
    delay: Option<f64>,
    loss: Option<f64>,
    utilization: Option<f64>,
}

#[derive(Debug, Clone, Deserialize)]
struct TopologyInput {
    nodes: Option<Vec<u32>>,
    edges: Vec<EdgeInput>,
}

#[derive(Debug, Clone)]
struct LcgRng {
    state: u64,
}

impl LcgRng {
    fn new(seed: u64) -> Self {
        Self { state: seed.max(1) }
    }

    fn next_u64(&mut self) -> u64 {
        self.state = self
            .state
            .wrapping_mul(6_364_136_223_846_793_005)
            .wrapping_add(1);
        self.state
    }

    fn next_f64(&mut self) -> f64 {
        let raw = self.next_u64() >> 11;
        (raw as f64) / ((1_u64 << 53) as f64)
    }

    fn range_f64(&mut self, low: f64, high: f64) -> f64 {
        low + (high - low) * self.next_f64()
    }
}

fn percentile(sorted: &[f64], q: f64) -> f64 {
    if sorted.is_empty() {
        return f64::NAN;
    }
    let n = sorted.len();
    if n == 1 {
        return sorted[0];
    }
    let rank = (q.clamp(0.0, 1.0) * (n - 1) as f64).round() as usize;
    sorted[rank]
}

fn summarize_entries(entries: &[StrategyRouteEntry], total_destinations: usize) -> Value {
    let mut metrics: Vec<f64> = entries
        .iter()
        .map(|entry| entry.metric)
        .filter(|metric| metric.is_finite())
        .collect();
    metrics.sort_by(|a, b| a.total_cmp(b));

    let mean_metric = if metrics.is_empty() {
        f64::NAN
    } else {
        metrics.iter().sum::<f64>() / metrics.len() as f64
    };

    let mean_next_hops = if entries.is_empty() {
        0.0
    } else {
        entries
            .iter()
            .map(|entry| entry.next_hops.len() as f64)
            .sum::<f64>()
            / entries.len() as f64
    };

    json!({
        "reachable": entries.len(),
        "total_destinations": total_destinations,
        "reachable_ratio": if total_destinations == 0 { 1.0 } else { entries.len() as f64 / total_destinations as f64 },
        "mean_metric": mean_metric,
        "p95_metric": percentile(&metrics, 0.95),
        "mean_next_hops": mean_next_hops,
    })
}

fn bench_scalar(
    name: &str,
    graph: &BTreeMap<u32, BTreeMap<u32, f64>>,
    src: u32,
    iterations: usize,
    config: &ScalarRouteStrategyConfig,
    total_destinations: usize,
) -> Value {
    let mut elapsed_ms = 0.0;
    let mut entries = Vec::new();

    for _ in 0..iterations.max(1) {
        let start = Instant::now();
        let current = compute_scalar_route_entries(graph, src, config);
        elapsed_ms += start.elapsed().as_secs_f64() * 1000.0;
        entries = current;
    }

    let mut out = summarize_entries(&entries, total_destinations);
    if let Some(map) = out.as_object_mut() {
        map.insert("algorithm".to_string(), json!(name));
        map.insert(
            "runtime_ms".to_string(),
            json!(elapsed_ms / iterations.max(1) as f64),
        );
    }
    out
}

fn bench_multimetric(
    name: &str,
    graph: &MultiMetricGraph,
    src: u32,
    iterations: usize,
    config: &MultiMetricRouteStrategyConfig,
    total_destinations: usize,
) -> Value {
    let mut elapsed_ms = 0.0;
    let mut entries = Vec::new();

    for _ in 0..iterations.max(1) {
        let start = Instant::now();
        let current = compute_multimetric_route_entries(graph, src, config);
        elapsed_ms += start.elapsed().as_secs_f64() * 1000.0;
        entries = current;
    }

    let mut out = summarize_entries(&entries, total_destinations);
    if let Some(map) = out.as_object_mut() {
        map.insert("algorithm".to_string(), json!(name));
        map.insert(
            "runtime_ms".to_string(),
            json!(elapsed_ms / iterations.max(1) as f64),
        );
    }
    out
}

fn generate_graph(
    seed: u64,
    nodes: usize,
    density: f64,
) -> (BTreeMap<u32, BTreeMap<u32, f64>>, MultiMetricGraph) {
    let mut rng = LcgRng::new(seed);
    let mut graph: BTreeMap<u32, BTreeMap<u32, f64>> = BTreeMap::new();

    for node in 1..=nodes as u32 {
        graph.entry(node).or_default();
    }

    if nodes >= 2 {
        for node in 1..=nodes as u32 {
            let next = if node == nodes as u32 { 1 } else { node + 1 };
            let reverse = node;
            graph
                .entry(node)
                .or_default()
                .insert(next, rng.range_f64(1.0, 20.0));
            graph
                .entry(next)
                .or_default()
                .insert(reverse, rng.range_f64(1.0, 20.0));
        }
    }

    let p = density.clamp(0.0, 1.0);
    for u in 1..=nodes as u32 {
        for v in 1..=nodes as u32 {
            if u == v {
                continue;
            }
            if graph
                .get(&u)
                .is_some_and(|neighbors| neighbors.contains_key(&v))
            {
                continue;
            }
            if rng.next_f64() < p {
                graph
                    .entry(u)
                    .or_default()
                    .insert(v, rng.range_f64(1.0, 20.0));
            }
        }
    }

    let mut mm_graph: MultiMetricGraph = BTreeMap::new();
    for (u, neighbors) in &graph {
        let mut out = BTreeMap::new();
        for (v, weight) in neighbors {
            out.insert(
                *v,
                LinkMetrics {
                    weight: *weight,
                    bandwidth: rng.range_f64(50.0, 1000.0),
                    delay: *weight * rng.range_f64(0.5, 2.0),
                    loss: rng.range_f64(0.0, 0.02),
                    utilization: rng.range_f64(0.0, 1.0),
                },
            );
        }
        mm_graph.insert(*u, out);
    }

    (graph, mm_graph)
}

fn load_topology(
    path: &PathBuf,
) -> anyhow::Result<(BTreeMap<u32, BTreeMap<u32, f64>>, MultiMetricGraph)> {
    let raw = fs::read_to_string(path)?;
    let topology: TopologyInput = serde_json::from_str(&raw)?;

    let mut graph: BTreeMap<u32, BTreeMap<u32, f64>> = BTreeMap::new();
    let mut mm_graph: MultiMetricGraph = BTreeMap::new();

    if let Some(nodes) = topology.nodes {
        for node in nodes {
            graph.entry(node).or_default();
            mm_graph.entry(node).or_default();
        }
    }

    for edge in topology.edges {
        graph
            .entry(edge.from)
            .or_default()
            .insert(edge.to, edge.weight);
        graph.entry(edge.to).or_default();

        mm_graph.entry(edge.from).or_default().insert(
            edge.to,
            LinkMetrics {
                weight: edge.weight,
                bandwidth: edge.bandwidth.unwrap_or(100.0),
                delay: edge.delay.unwrap_or(edge.weight),
                loss: edge.loss.unwrap_or(0.0),
                utilization: edge.utilization.unwrap_or(0.0),
            },
        );
        mm_graph.entry(edge.to).or_default();
    }

    Ok((graph, mm_graph))
}

fn aggregate(seed_rows: &[Value]) -> Value {
    let mut buckets: BTreeMap<String, Vec<Value>> = BTreeMap::new();
    for row in seed_rows {
        if let Some(algos) = row.get("algorithms").and_then(Value::as_array) {
            for algo in algos {
                if let Some(name) = algo.get("algorithm").and_then(Value::as_str) {
                    buckets
                        .entry(name.to_string())
                        .or_default()
                        .push(algo.clone());
                }
            }
        }
    }

    let mut out = Vec::new();
    for (name, rows) in buckets {
        let mut runtime_ms = Vec::new();
        let mut reachable_ratio = Vec::new();
        let mut mean_metric = Vec::new();
        let mut p95_metric = Vec::new();
        let mut mean_next_hops = Vec::new();

        for row in rows {
            if let Some(v) = row.get("runtime_ms").and_then(Value::as_f64) {
                runtime_ms.push(v);
            }
            if let Some(v) = row.get("reachable_ratio").and_then(Value::as_f64) {
                reachable_ratio.push(v);
            }
            if let Some(v) = row.get("mean_metric").and_then(Value::as_f64) {
                mean_metric.push(v);
            }
            if let Some(v) = row.get("p95_metric").and_then(Value::as_f64) {
                p95_metric.push(v);
            }
            if let Some(v) = row.get("mean_next_hops").and_then(Value::as_f64) {
                mean_next_hops.push(v);
            }
        }

        let avg = |xs: &[f64]| -> f64 {
            if xs.is_empty() {
                f64::NAN
            } else {
                xs.iter().sum::<f64>() / xs.len() as f64
            }
        };

        out.push(json!({
            "algorithm": name,
            "runtime_ms": avg(&runtime_ms),
            "reachable_ratio": avg(&reachable_ratio),
            "mean_metric": avg(&mean_metric),
            "p95_metric": avg(&p95_metric),
            "mean_next_hops": avg(&mean_next_hops),
        }));
    }

    Value::Array(out)
}

fn print_summary(aggregated: &Value) {
    println!("algorithm\truntime_ms\treachable_ratio\tmean_metric\tp95_metric\tmean_next_hops");
    if let Some(rows) = aggregated.as_array() {
        for row in rows {
            let name = row.get("algorithm").and_then(Value::as_str).unwrap_or("?");
            let runtime_ms = row
                .get("runtime_ms")
                .and_then(Value::as_f64)
                .unwrap_or(f64::NAN);
            let reachable_ratio = row
                .get("reachable_ratio")
                .and_then(Value::as_f64)
                .unwrap_or(f64::NAN);
            let mean_metric = row
                .get("mean_metric")
                .and_then(Value::as_f64)
                .unwrap_or(f64::NAN);
            let p95_metric = row
                .get("p95_metric")
                .and_then(Value::as_f64)
                .unwrap_or(f64::NAN);
            let mean_next_hops = row
                .get("mean_next_hops")
                .and_then(Value::as_f64)
                .unwrap_or(f64::NAN);
            println!(
                "{}\t{:.4}\t{:.4}\t{:.4}\t{:.4}\t{:.4}",
                name, runtime_ms, reachable_ratio, mean_metric, p95_metric, mean_next_hops
            );
        }
    }
}

fn main() -> anyhow::Result<()> {
    let args = Args::parse();
    let mut seed_rows = Vec::new();

    for idx in 0..args.seeds.max(1) {
        let seed = args.start_seed + idx as u64;
        let (graph, mm_graph) = if let Some(path) = args.topology_json.as_ref() {
            load_topology(path)?
        } else {
            generate_graph(seed, args.nodes.max(2), args.density)
        };

        let src = graph.keys().next().copied().unwrap_or(1);
        let total_destinations = graph.keys().filter(|node| **node != src).count();

        let algorithms = vec![
            bench_scalar(
                "dijkstra",
                &graph,
                src,
                args.iterations,
                &ScalarRouteStrategyConfig {
                    algorithm: ScalarRouteAlgorithm::Dijkstra,
                    selection: NextHopSelectionPolicy::Lowest,
                },
                total_destinations,
            ),
            bench_scalar(
                "ecmp",
                &graph,
                src,
                args.iterations,
                &ScalarRouteStrategyConfig {
                    algorithm: ScalarRouteAlgorithm::Ecmp,
                    selection: NextHopSelectionPolicy::Hash { seed },
                },
                total_destinations,
            ),
            bench_scalar(
                "bellman_ford",
                &graph,
                src,
                args.iterations,
                &ScalarRouteStrategyConfig {
                    algorithm: ScalarRouteAlgorithm::BellmanFord,
                    selection: NextHopSelectionPolicy::Lowest,
                },
                total_destinations,
            ),
            bench_scalar(
                "yen_ksp",
                &graph,
                src,
                args.iterations,
                &ScalarRouteStrategyConfig {
                    algorithm: ScalarRouteAlgorithm::YenKShortest {
                        k_paths: args.k_paths.max(1),
                    },
                    selection: NextHopSelectionPolicy::Hash { seed },
                },
                total_destinations,
            ),
            bench_multimetric(
                "cspf",
                &mm_graph,
                src,
                args.iterations,
                &MultiMetricRouteStrategyConfig {
                    algorithm: MultiMetricRouteAlgorithm::Cspf {
                        constraints: LinkConstraints {
                            min_bandwidth: Some(100.0),
                            max_delay: None,
                            max_loss: None,
                            max_utilization: None,
                        },
                    },
                    selection: NextHopSelectionPolicy::Lowest,
                },
                total_destinations,
            ),
            bench_multimetric(
                "weighted_sum",
                &mm_graph,
                src,
                args.iterations,
                &MultiMetricRouteStrategyConfig {
                    algorithm: MultiMetricRouteAlgorithm::WeightedSum {
                        coefficients: WeightedSumCoefficients {
                            weight: 0.5,
                            delay: 0.3,
                            loss: 10.0,
                            utilization: 1.0,
                        },
                    },
                    selection: NextHopSelectionPolicy::Lowest,
                },
                total_destinations,
            ),
            bench_multimetric(
                "pareto",
                &mm_graph,
                src,
                args.iterations,
                &MultiMetricRouteStrategyConfig {
                    algorithm: MultiMetricRouteAlgorithm::Pareto {
                        max_paths: args.pareto_max_paths.max(1),
                    },
                    selection: NextHopSelectionPolicy::Hash { seed },
                },
                total_destinations,
            ),
        ];

        seed_rows.push(json!({
            "seed": seed,
            "nodes": graph.len(),
            "source": src,
            "algorithms": algorithms,
        }));
    }

    let aggregated = aggregate(&seed_rows);
    print_summary(&aggregated);

    let payload = json!({
        "config": {
            "nodes": args.nodes,
            "density": args.density,
            "seeds": args.seeds,
            "start_seed": args.start_seed,
            "k_paths": args.k_paths,
            "pareto_max_paths": args.pareto_max_paths,
            "iterations": args.iterations,
            "topology_json": args.topology_json,
        },
        "runs": seed_rows,
        "aggregate": aggregated,
    });

    if let Some(path) = args.output_json {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::write(path, serde_json::to_vec_pretty(&payload)?)?;
    } else {
        println!("{}", serde_json::to_string_pretty(&payload)?);
    }

    Ok(())
}
