#pragma once
#include "graph/graph.h"
#include "graph/graph_index_interface.h"
#include <set>

namespace agl {
template <size_t kNumBitParallelRoots = 16>
class dynamic_pruned_landmark_labeling
    : public dynamic_graph_index_interface<G>,
      public distance_query_interface<G> {
 public:
  virtual ~dynamic_pruned_landmark_labeling() {}
  virtual void construct(const G &g) override;
  virtual W query_distance(const G &g, V v_from, V v_to) override;
  virtual void add_edge(const G &g, V v_from, const E &e) override;
  virtual void remove_edge(const G &g, V v_from, V v_to) override {
    assert(false);
  }
  virtual void remove_vertices(const G &g, V old_num_vertices) override {
    num_v_ = g.num_vertices();
    resize();
  }
  virtual void add_vertices(const G &g, V old_num_vertices) override {
    num_v_ = g.num_vertices();
    resize();
    for (V v = old_num_vertices; v < num_v_; ++v) rank_[v] = v, inv_[v] = v;
  }

  std::vector<std::pair<V, W>> get_label(V v, D dir = kFwd);
  double average_label_size();

  // Test functions
  std::vector<bool> test_bit_parallel_used(const G &g);
  std::vector<V> test_get_rank();

 private:
  struct index_t {
    uint8_t bpspt_d[kNumBitParallelRoots];
    uint64_t bpspt_s[kNumBitParallelRoots][2];
    std::vector<V> spt_v;
    std::vector<uint8_t> spt_d;
    void update(V v, uint8_t d) {
      if (spt_v.empty() || spt_v.back() < v) {
        spt_v.emplace_back(v);
        spt_d.emplace_back(d);
        return;
      }
      size_t i;
      for (i = 0; i < spt_v.size(); ++i) {
        if (spt_v[i] >= v) break;
      }
      assert(i < spt_v.size());
      if (spt_v[i] == v) {
        if (spt_d[i] > d) spt_d[i] = d;
        return;
      }
      assert(spt_v[i] > v);
      spt_v.push_back(spt_v.back());
      spt_d.push_back(spt_d.back());
      for (size_t j = spt_v.size() - 1; j > i; --j) {
        spt_v[j] = spt_v[j - 1];
        spt_d[j] = spt_d[j - 1];
      }
      spt_v[i] = v;
      spt_d[i] = d;
    }
    size_t size() const { return spt_v.size(); }
  };

  const uint8_t D_INF_ = 100;
  V num_v_;

  std::vector<index_t> idx_[2];
  std::vector<V> rank_, inv_;

  void load_graph(const G &g);
  void bit_parallel_bfs(const G &g, std::vector<bool> &used);
  uint8_t distance_less(V v_from, V v_to, int direction, uint8_t upper_limit);
  void pruned_bfs(const G &g, V root, int direction,
                  const std::vector<bool> &used);
  void partial_bfs(const G &g, V v_from, V v_to, uint8_t d_ft, int direction);
  void partial_bp_bfs(const G &g, int bp_i, V v_from, int direction);
  void resize();

  // Reusable containers
  std::vector<uint8_t> bfs_dist_;
  std::vector<V> bfs_que_;
};

template <size_t kNumBitParallelRoots>
double dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::average_label_size() {
  size_t sum = 0;
  for (int i = 0; i < 2; ++i)
    for (const index_t &j : idx_[i]) sum += j.size();
  return (double)sum / num_v_;
}

template <size_t kNumBitParallelRoots>
std::vector<std::pair<V, W>> dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::get_label(V v, D dir) {
  assert(v < num_v_);
  int direction = dir == kFwd ? 0 : 1;
  std::vector<std::pair<V, W>> ret(idx_[direction][v].size());
  for (size_t i = 0; i < idx_[direction][v].size(); ++i) {
    ret[i].first = idx_[direction][v].spt_v[i];
    ret[i].second = idx_[direction][v].spt_d[i];
  }
  return ret;
}

/**
* Construct the data structure from the given graph.
* \param g graph
*/
template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::construct(const G &g) {
  // Initialize
  load_graph(g);

  std::vector<bool> used(num_v_, false);
  // Bit-Parallel Labeling
  bit_parallel_bfs(g, used);

  // Pruned labelling
  for (V root = 0; root < num_v_; ++root) {
    if (used[root]) continue;
    pruned_bfs(g, root, 0, used);
    pruned_bfs(g, root, 1, used);
    used[root] = true;
  }
}

/**
* Construct the Bit-Parallel label
* \param used vector to record the vertices used as the root of BFS
* \param num_e the number of edges of the given graph
*/
template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::bit_parallel_bfs(const G &g, std::vector<bool> &used) {
  V root = 0;
  size_t num_e = g.num_edges();
  std::vector<std::pair<uint64_t, uint64_t>> tmp_s(num_v_, {0, 0});
  std::vector<std::pair<V, V>> sibling_es(num_e);
  std::vector<std::pair<V, V>> child_es(num_e);
  V bp_roots[64];
  for (size_t bp_i = 0; bp_i < kNumBitParallelRoots; ++bp_i) {
    // Select Root
    while (root < num_v_ && used[root]) root++;
    if (root == num_v_) {
      for (V v = 0; v < num_v_; ++v) {
        idx_[0][v].bpspt_d[bp_i] = D_INF_;
        idx_[1][v].bpspt_d[bp_i] = D_INF_;
      }
      continue;
    }
    used[root] = true;

    // Select Roots
    std::vector<V> neighbors;
    {
      std::set<V> tmp;
      for (V v : g.neighbors(inv_[root]))
        if (!used[rank_[v]]) tmp.insert(rank_[v]);
      for (V v : g.neighbors(inv_[root], kBwd))
        if (tmp.count(rank_[v])) neighbors.push_back(rank_[v]);
    }
    std::sort(neighbors.begin(), neighbors.end());
    V selected_num = 0;
    for (V rank_v : neighbors) {
      used[rank_v] = true;
      bp_roots[selected_num++] = rank_v;
      if (selected_num == 64) break;
    }

    for (int direction = 0; direction < 2; ++direction) {
      int another = direction ^ 1;
      D tmp_dir = direction == 0 ? kFwd : kBwd;

      int q_head = 0, q_tail = 0;

      bfs_que_[q_tail++] = root;
      bfs_dist_[root] = 0;

      for (V i = 0; i < selected_num; ++i) {
        V v = bp_roots[i];
        bfs_que_[q_tail++] = v;
        bfs_dist_[v] = 1;
        tmp_s[v].first = 1ULL << i;
      }

      for (uint8_t d = 0; q_head < q_tail; ++d) {
        size_t num_sibling_es = 0, num_child_es = 0;
        while (q_head < q_tail && bfs_dist_[bfs_que_[q_head]] == d) {
          V v = bfs_que_[q_head++];

          for (const V &nv : g.neighbors(inv_[v], tmp_dir)) {
            V tv = rank_[nv];
            uint8_t td = d + 1;
            if (d > bfs_dist_[tv]) continue;
            if (d == bfs_dist_[tv] && v < tv) {
              sibling_es[num_sibling_es].first = v;
              sibling_es[num_sibling_es].second = tv;
              num_sibling_es++;
            }
            if (d < bfs_dist_[tv]) {
              child_es[num_child_es].first = v;
              child_es[num_child_es].second = tv;
              num_child_es++;
            }
            if (bfs_dist_[tv] == D_INF_) {
              bfs_dist_[tv] = td;
              bfs_que_[q_tail++] = tv;
            }
          }
        }

        for (size_t p = 0; p < num_sibling_es; ++p) {
          V v, w;
          std::tie(v, w) = sibling_es[p];
          tmp_s[v].second |= tmp_s[w].first;
          tmp_s[w].second |= tmp_s[v].first;
        }
        for (size_t p = 0; p < num_child_es; ++p) {
          V v, c;
          std::tie(v, c) = child_es[p];
          tmp_s[c].first |= tmp_s[v].first;
          tmp_s[c].second |= tmp_s[v].second;
        }
      }

      for (V v = 0; v < num_v_; ++v) {
        idx_[another][inv_[v]].bpspt_d[bp_i] = bfs_dist_[v];
        idx_[another][inv_[v]].bpspt_s[bp_i][0] = tmp_s[v].first;
        idx_[another][inv_[v]].bpspt_s[bp_i][1] =
            tmp_s[v].second & ~tmp_s[v].first;
        tmp_s[v].first = 0, tmp_s[v].second = 0;
      }
      for (int i = 0; i < q_tail; ++i) bfs_dist_[bfs_que_[i]] = D_INF_;
    }
  }
}

template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::resize() {
  rank_.resize(num_v_);
  inv_.resize(num_v_);
  V old_idx_size = idx_[0].size();
  idx_[0].resize(num_v_), idx_[1].resize(num_v_);
  for (V v = old_idx_size; v < num_v_; ++v) {
    for (size_t bp_i = 0; bp_i < kNumBitParallelRoots; ++bp_i) {
      idx_[0][v].bpspt_d[bp_i] = D_INF_;
      idx_[1][v].bpspt_d[bp_i] = D_INF_;
    }
  }
  bfs_dist_.assign(num_v_, D_INF_);
  bfs_que_.resize(num_v_);
}

/**
* Load and relabel the given graph
* \param g graph
*/
template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::load_graph(const G &g) {
  num_v_ = g.num_vertices();
  resize();
  {
    std::vector<V> deg(num_v_, 0);
    for (const auto &p : g.edge_list())
      if (p.first != p.second) deg[p.first]++, deg[p.second]++;

    std::vector<std::pair<double, V>> sorting_v(num_v_);
    for (V v = 0; v < num_v_; ++v) {
      double t = (double)agl::random(num_v_) / num_v_;
      sorting_v[v] = std::make_pair(deg[v] + t, v);
    }
    std::sort(sorting_v.rbegin(), sorting_v.rend());
    for (int i = 0; i < num_v_; ++i) inv_[i] = sorting_v[i].second;
    for (int i = 0; i < num_v_; ++i) rank_[inv_[i]] = i;
  }
}

/**
* Construct 2-Hop label by pruned BFS
* \param root the root vertex of pruned BFS
* \param direction the direction to do BFS
* \param used vector to record the vertices used as the root of BFS
*/
template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::pruned_bfs(const G &g, V root, int direction, const std::vector<bool> &used) {
  int another = direction ^ 1;
  D dir = direction == 0 ? kFwd : kBwd;
  int q_head = 0, q_tail = 0;
  bfs_que_[q_tail++] = root;
  bfs_dist_[root] = 0;

  while (q_head < q_tail) {
    V u = bfs_que_[q_head++];
    if (u != root &&
        distance_less(root, u, direction, bfs_dist_[u]) <= bfs_dist_[u])
      continue;
    idx_[another][inv_[u]].update(root, bfs_dist_[u]);

    for (const auto &nu : g.neighbors(inv_[u], dir)) {
      V w = rank_[nu];
      if (used[w]) continue;
      if (bfs_dist_[w] < D_INF_) continue;
      bfs_dist_[w] = bfs_dist_[u] + 1;
      bfs_que_[q_tail++] = w;
    }
  }
  for (int i = 0; i < q_tail; ++i) {
    bfs_dist_[bfs_que_[i]] = D_INF_;
  }
}

/**
* Return the distance of shortest path between two vertices.
* \param g graph
* \param v_from the starting vertex of shortest path
* \param v_to the end vertex of shortest path
*/
template <size_t kNumBitParallelRoots>
W dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::query_distance(const G &g, V v_from, V v_to) {
  assert(v_from >= 0 && v_to >= 0);
  assert(v_from < num_v_ && v_to < num_v_);
  if (v_from == v_to) return 0;
  v_from = rank_[v_from], v_to = rank_[v_to];
  return distance_less(v_from, v_to, 0, 0);
}

/**
* Return the greater either the distance of shortest path between two vertices
* or upper_limit
* \param v_from the starting vertex of shortest path
* \param v_to the end vertex of shortest path
* \param direction the direction of shortest path
* \param upper_limit When it is found out that the distance of the shortest path
* is smaller than upper_limit, this function returns the calculationg distance
* which is equal to or shorter than upper_limit.
*/
template <size_t kNumBitParallelRoots>
uint8_t dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::distance_less(V v_from, V v_to, int direction, uint8_t upper_limit) {
  int another = direction ^ 1;

  uint8_t d = D_INF_;
  const index_t &idx_from = idx_[direction][inv_[v_from]];
  const index_t &idx_to = idx_[another][inv_[v_to]];

  for (size_t bp_i = 0; bp_i < kNumBitParallelRoots; ++bp_i) {
    uint8_t td = idx_from.bpspt_d[bp_i] + idx_to.bpspt_d[bp_i];
    if (td - 2 >= d) continue;
    if (idx_from.bpspt_s[bp_i][0] & idx_to.bpspt_s[bp_i][0]) {
      td -= 2;
    } else if ((idx_from.bpspt_s[bp_i][1] & idx_to.bpspt_s[bp_i][0]) |
               (idx_from.bpspt_s[bp_i][0] & idx_to.bpspt_s[bp_i][1])) {
      td -= 1;
    }
    if (td < d) {
      d = td;
      if (d <= upper_limit) return d;
    }
  }

  for (size_t i1 = 0, i2 = 0; i1 < idx_from.size() || i2 < idx_to.size();) {
    V v1 = (i1 < idx_from.size() ? idx_from.spt_v[i1] : num_v_);
    V v2 = (i2 < idx_to.size() ? idx_to.spt_v[i2] : num_v_);
    if (v1 == v2) {
      uint8_t td = idx_from.spt_d[i1] + idx_to.spt_d[i2];
      if (td < d) {
        d = td;
        if (d <= upper_limit) return d;
      }
      i1++;
      i2++;
    } else {
      if (v1 == v_to && idx_from.spt_d[i1] <= d) return idx_from.spt_d[i1];
      if (v2 == v_from && idx_to.spt_d[i2] <= d) return idx_to.spt_d[i2];
      if (v1 < v2) i1++;
      if (v1 > v2) i2++;
    }
  }
  if (d >= D_INF_ - 2) d = D_INF_;
  return d;
}

/**
* Update 2-Hop label when the new edge is added.
* \param v_from the root vertex of partial BFS
* \param v_end one of the end vertex of new edge
* \param base_d the distance between v_from to v_end
* \param direction direction
*/
template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::partial_bfs(const G &g, V v_from, V v_end, uint8_t base_d, int direction) {
  int another = direction ^ 1;
  D dir = direction == 0 ? kFwd : kBwd;

  int q_head = 0, q_tail = 0;
  bfs_que_[q_tail++] = v_end;
  bfs_dist_[v_end] = base_d;
  while (q_head < q_tail) {
    V v = bfs_que_[q_head++];
    uint8_t d = bfs_dist_[v];
    if (distance_less(v_from, v, direction, d) <= d) continue;
    idx_[another][inv_[v]].update(v_from, d);
    for (V nv : g.neighbors(inv_[v], dir)) {
      V w = rank_[nv];
      if (bfs_dist_[w] < D_INF_) continue;
      bfs_dist_[w] = d + 1;
      bfs_que_[q_tail++] = w;
    }
  }
  for (int i = 0; i < q_tail; ++i) {
    bfs_dist_[bfs_que_[i]] = D_INF_;
  }
}

/**
* Update bit-parallel label when the new edge is added.
* \param bp_i index of bit-parallel
* \param v_from the root vertex of partial BFS
* \param direction direction
*/
template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::partial_bp_bfs(const G &g, int bp_i, V v_from, int direction) {
  V another = direction ^ 1;
  D dir = direction == 0 ? kFwd : kBwd;
  const index_t &idx_from = idx_[another][inv_[v_from]];
  const uint8_t base_d = idx_from.bpspt_d[bp_i];
  if (base_d == D_INF_) return;

  int q_head = 0, q_tail = 0;
  bfs_que_[q_tail++] = v_from;
  bfs_dist_[v_from] = base_d;
  for (uint8_t d = base_d; q_head < q_tail; ++d) {
    int old_head = q_head;
    while (q_head < q_tail && bfs_dist_[bfs_que_[q_head]] == d) {
      V v = bfs_que_[q_head++];
      const index_t &idx_v = idx_[another][inv_[v]];

      for (V nv : g.neighbors(inv_[v], dir)) {
        V tv = rank_[nv];
        index_t &idx_tv = idx_[another][inv_[tv]];
        if (d == idx_tv.bpspt_d[bp_i])
          idx_tv.bpspt_s[bp_i][1] |= idx_v.bpspt_s[bp_i][0];
        if (d + 1 < idx_tv.bpspt_d[bp_i]) {
          idx_tv.bpspt_s[bp_i][1] =
              (idx_tv.bpspt_d[bp_i] == d + 2 ? idx_tv.bpspt_s[bp_i][0] : 0);

          idx_tv.bpspt_s[bp_i][0] = 0;
          idx_tv.bpspt_d[bp_i] = d + 1;
          bfs_dist_[tv] = d + 1;
          bfs_que_[q_tail++] = tv;
        }
      }
    }

    for (int i = old_head; i < q_head; ++i) {
      V v = bfs_que_[i];
      const index_t &idx_v = idx_[another][inv_[v]];
      for (V nv : g.neighbors(inv_[v], dir)) {
        V tv = rank_[nv];
        index_t &idx_tv = idx_[another][inv_[tv]];
        if (idx_tv.bpspt_d[bp_i] == d + 1) {
          // Set propagation (2)
          idx_tv.bpspt_s[bp_i][0] |= idx_v.bpspt_s[bp_i][0];
          idx_tv.bpspt_s[bp_i][1] |= idx_v.bpspt_s[bp_i][1];
        }
      }
    }
  }
  for (int i = 0; i < q_tail; ++i) {
    bfs_dist_[bfs_que_[i]] = D_INF_;
  }
}

/**
* Add new edge and update the labels
* \param g graph
* \param v_a the vertex from which new edge starts
* \param e new edge
*/
template <size_t kNumBitParallelRoots>
void dynamic_pruned_landmark_labeling<kNumBitParallelRoots>
::add_edge(const G &g, V v_a, const E &e) {
  V v_b = to(e);
  assert(v_a >= 0 && v_b >= 0);
  assert(v_a < num_v_ && v_b < num_v_);
  v_a = rank_[v_a], v_b = rank_[v_b];

  for (size_t bp_i = 0; bp_i < kNumBitParallelRoots; ++bp_i) {
    partial_bp_bfs(g, bp_i, v_a, 0);
    partial_bp_bfs(g, bp_i, v_b, 1);
  }

  index_t &idx_a = idx_[1][inv_[v_a]];
  index_t &idx_b = idx_[0][inv_[v_b]];
  for (size_t ia = 0, ib = 0; ia < idx_a.size() || ib < idx_b.size();) {
    V r_a = ia < idx_a.size() ? idx_a.spt_v[ia] : num_v_;
    V r_b = ib < idx_b.size() ? idx_b.spt_v[ib] : num_v_;
    if (r_a == r_b) {
      partial_bfs(g, r_a, v_b, idx_a.spt_d[ia] + 1, 0);
      partial_bfs(g, r_b, v_a, idx_b.spt_d[ib] + 1, 1);
      ia++;
      ib++;
    } else if (r_a > r_b) {
      partial_bfs(g, r_b, v_a, idx_b.spt_d[ib] + 1, 1);
      ib++;
    } else {
      partial_bfs(g, r_a, v_b, idx_a.spt_d[ia] + 1, 0);
      ia++;
    }
  }
  if (idx_a.size() == 0 && idx_b.size() == 0) {
    idx_[0][inv_[v_a]].update(v_b, 1);
    idx_[1][inv_[v_b]].update(v_a, 1);
  }
}
}  // namespace agl
