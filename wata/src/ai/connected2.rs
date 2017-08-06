use ::common::*;
use ::lib::unionfind::UnionFind;
use ::State;

#[derive(Serialize, Deserialize, Debug, Default)]
struct AI {
	turn: usize
}

pub fn setup(state: &mut State) {
	eprintln!("connected2");
	state.ai = ::serde_json::to_string(&AI { turn: state.my }).unwrap();
}

const R: usize = 10;

pub fn play(state: &mut State) -> usize {
	let ai: AI = ::serde_json::from_str(&state.ai).unwrap();
	state.ai = ::serde_json::to_string(&AI { turn: ai.turn + state.p }).unwrap();
	let g = state.graph.iter().map(|u| u.iter().map(|&(v, _)| v).collect()).collect();
	let dist: Vec<_> = state.mines.iter().map(|&v| ::lib::bfs(&g, v)).collect();
	let n = state.graph.len();
	let m = state.es.len();
	let mut user = vec![!0; state.es.len()];
	for (i, &mov) in state.moves.iter().enumerate() {
		if let Some(e) = mov {
			user[e] = i % state.p;
		}
	}
	let mut dists = vec![vec![vec![]; state.mines.len()]; state.p];
	for i in 0..state.p {
		use ::lib::dijkstra::Graph;
		let mut g: Graph<usize> = Graph::new(n);
		for u in 0..n {
			for &(v, e) in &state.graph[u] {
				if user[e] == i {
					g.add(u, v, 0);
				} else if user[e] == !0 {
					g.add(u, v, 1);
				}
			}
		}
		for (j, &s) in state.mines.iter().enumerate() {
			dists[i][j] = g.solve(s).1.into_iter().map(|(d, _)| d).collect();
		}
	}
	use ::rand::Rng;
	let mut score: Vec<_> = user.iter().map(|&u| if u == !0 { 0.0 } else { -1.0 }).collect();
	let mut rng = ::rand::XorShiftRng::new_unseeded();
	let mut ws = vec![0.0; m];
	'lp: for _ in 0..R {
		for q in 0..state.p {
			use ::lib::dijkstra::Graph;
			let mut g: Graph<f64> = Graph::new(n);
			for u in 0..n {
				for &(v, e) in &state.graph[u] {
					if user[e] == !0 && u < v {
						let w = -(rng.next_f64().ln());
						// let w = rng.next_f64();
						g.add(u, v, w);
						g.add(v, u, w);
					} else if user[e] == q {
						g.add(u, v, 0.0);
					}
				}
			}
			for i in 0..state.mines.len() {
				let d = ::STIME().elapsed().unwrap();
				let s = d.as_secs() as f64 + d.subsec_nanos() as f64 * 1e-9;
				if s > 0.8 {
					eprintln!("break: {}", s);
					break 'lp;
				}
				let (list, dp) = g.solve(state.mines[i]);
				let mut sum = vec![0.0; n];
				for u in list.into_iter().rev() {
					if dp[u].0 > 0.0 {
						if dists[q][i][u] <= dist[i][u] {
							sum[u] += (dist[i][u] * dist[i][u]) as f64;
							// sum[u] += (dist[i][u] * dist[i][u]) as f64 * 0.9f64.powf(dp[u].0);
						}
						// sum[u] += (dist[i][u] * dist[i][u]) as f64  / (::std::f64::consts::E + dp[u].0).ln();
						// sum[u] += (dist[i][u] * dist[i][u]) as f64  / (1.0 + dp[u].0);
						// sum[u] += (dist[i][u] * dist[i][u]) as f64 * 0.9f64.powf(dp[u].0);
						let v = dp[u].1;
						sum[v] += sum[u];
						let e = state.graph[u][state.graph[u].binary_search_by(|&(w, _)| w.cmp(&v)).unwrap()].1;
						if user[e] == !0 {
							if q == state.my {
								score[e] += sum[u] as f64 / R as f64;
							} else {
								score[e] += sum[u] as f64 / R as f64 / (state.p - 1) as f64;
							}
						}
					}
				}
			}
		}
	}
	let mut connected = vec![!0; n];
	let mut stack = vec![];
	for &s in &state.mines {
		if connected[s] != !0 { continue }
		connected[s] = s;
		stack.push(s);
		while let Some(u) = stack.pop() {
			for &(v, e) in &state.graph[u] {
				if user[e] == state.my && connected[v] == !0 {
					connected[v] = s;
					stack.push(v);
				}
			}
		}
	}
	for &u in &state.mines {
		for &(_, e) in &state.graph[u] {
			if connected[state.es[e].0] != connected[state.es[e].1] {
				score[e] *= 2.0;
			}
		}
	}
	for u in 0..n {
		for &(_, e) in &state.graph[u] {
			if connected[state.es[e].0] != connected[state.es[e].1] {
				score[e] *= 2.0;
			}
		}
	}
	let mut e = 0;
	for i in 0..m {
		if score[e] < score[i] {
			e = i;
		}
	}
	debug!(score[e]);
	e
}