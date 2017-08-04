use ::common::*;
use ::lib::unionfind::UnionFind;
use ::State;

#[derive(Serialize, Deserialize, Debug, Default)]
pub struct AI {
	pub dist: Vec<Vec<usize>>
}

pub fn setup(state: &State) -> AI {
	eprintln!("randw");
	let g = state.graph.iter().map(|u| u.iter().map(|&(v, _)| v).collect()).collect();
	AI { dist: state.mines.iter().map(|&v| ::lib::bfs(&g, v)).collect() }
}

const R: usize = 10;

pub fn play(state: &mut State) -> usize {
	let n = state.graph.len();
	let m = state.es.len();
	let mut user = vec![!0; state.es.len()];
	for (i, &mov) in state.moves.iter().enumerate() {
		if let Some(e) = mov {
			user[e] = i % state.p;
		}
	}
	use ::rand::Rng;
	let mut score: Vec<_> = user.iter().map(|&u| if u == !0 { 0.0 } else { -1.0 }).collect();
	let mut rng = ::rand::XorShiftRng::new_unseeded();
	let mut ws = vec![0.0; m];
	for i in 0..state.mines.len() {
		for _ in 0..R {
			use ::lib::dijkstra::Graph;
			let mut g: Graph<f64> = Graph::new(n);
			for u in 0..n {
				for &(v, e) in &state.graph[u] {
					if user[e] == !0 && u < v {
						let w = rng.next_f64();
						g.add(u, v, w);
						g.add(v, u, w);
					} else if user[e] == state.my {
						g.add(u, v, 0.0);
					}
				}
			}
			let (list, dp) = g.solve(state.mines[i]);
			let mut sum = vec![0; n];
			for u in list.into_iter().rev() {
				if dp[u].0 > 0.0 {
					sum[u] += state.ai.dist[i][u] * state.ai.dist[i][u];
					let v = dp[u].1;
					sum[v] += sum[u];
					let e = state.graph[u][state.graph[u].binary_search_by(|&(w, _)| w.cmp(&v)).unwrap()].1;
					if user[e] == !0 {
						score[e] += sum[u] as f64 / R as f64;
					}
				}
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