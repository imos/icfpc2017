#include "base.h"
#include "random.h"
#include "graph.h"
#include "jlog.h"
#include "agl.h"
#include "agl/cut_tree/cut_tree.h"
#include "agl/cut_tree/dinitz.h"
#include "agl/cut_tree/bi_dinitz.h"

namespace agl {
G to_directed_graph(G&& g) {
  vector<pair<V, V>> ret;
  for (auto& e : g.edge_list()) {
    if (e.first < to(e.second)) ret.emplace_back(e.first, to(e.second));
    else if (to(e.second) < e.first) ret.emplace_back(to(e.second), e.first);
  }
  sort(ret.begin(), ret.end());
  ret.erase(unique(ret.begin(), ret.end()), ret.end());
  return G(ret);
}
}  // hogeee

namespace {
pair<int, int> Greedy(const GameState &game) {
  map<pair<int, int>, int> score;

  Graph G = ConstructGraph(game), H;
  UnionFind uf;
  tie(H, uf) = ConstructContractedGraph(G, game.rank);
  int N = G.size();

  for (int mine: game.map.mines) {
    auto dst1 = SSSP(G, mine);
    vector<double> component_score(N);
    rep (v, N) component_score[uf.root[v]] += dst1[v].first * dst1[v].first;

    int root = uf.root[mine];
    for (auto &e : H[root]) {
      score[mp(min(root, e.to), max(root, e.to))] += component_score[e.to];
    }
  }

  tuple<int, int, int> bst(-1, -1, -1);
  for (auto &i : score) bst = max(bst, make_tuple(i.second, i.first.first, i.first.second));

  if (get<0>(bst) != -1) {
    return FindOriginalEdge(get<1>(bst), get<2>(bst), uf, G);
  } else {
    return RandomRemaining(game);
  }
}
}

namespace {
struct MyAIState {
  int u, v;

  template<class Archive>
  void serialize(Archive& ar, unsigned int ver) {
    ar & u & v;
  }
};

using MyState = State<MyAIState>;

GameState game;

int N;
Graph G, H;
UnionFind uf;

pair<MyAIState, vector<pair<int, int>>> Setup(const GameState &game) {
  JLOG_PUT_BENCHMARK("setup") {
    // itsumono
    ::game = game;
    G = ConstructGraph(game);

    agl::G aglg(game.map.rivers);
    aglg = agl::to_directed_graph(move(aglg));
    N = aglg.num_vertices();
    auto ct = agl::cut_tree(aglg);

    vector<tuple<int, int, int, int>> ord(N);

    rep (i, game.map.mines.size()) {
      int s = game.map.mines[i];

      vector<pair<int, int>> dst1 = SSSP(G, s);
      // rep (v, N) if (v != s) cerr << ct.query(s, v) << " ";

      rep (j, i) {
        int t = game.map.mines[j];
        ord.emplace_back(dst1[t].first, ct.query(s, t), s, t);
      }
    }

    sort(all(ord));
    reverse(all(ord));

    rep (i, 10) {
      auto o = ord[i];
      cerr << get<0>(o) << " " << get<1>(o) << " " << get<2>(o) << "-" << get<3>(o) << endl;
    }

    int max_connectivity = 0;
    const int TopK = 1;
    rep (i, TopK) max_connectivity = max(max_connectivity, get<1>(ord[i]));
    rep (i, TopK) {
      if (get<1>(ord[i]) == max_connectivity) {
        cerr << ord[i] << endl;
        int s = get<2>(ord[i]), t = get<3>(ord[i]);

        // agl::cut_tree_internal::dinitz din(aglg);
        // cerr << din.max_flow(s, t) << endl;

        return make_pair(MyAIState{s, t}, vector<pair<int, int>>{{s, t}, {t, s}});
      }
    }
  }

  assert(false);
}

pair<pair<int, int>, MyAIState> Play(const MyState &state) {
  // return make_pair(Greedy(state.game), state.ai);

  JLOG_PUT_BENCHMARK("time") {
    game = state.game;
    G = ConstructGraph(game);
    N = G.size();
    tie(H, uf) = ConstructContractedGraph(G, game.rank);

    int S = state.ai.u, T = state.ai.v;
    S = uf.root[S];
    T = uf.root[T];

    auto dst2 = SSSP(H, S);
    if (dst2[T].second == -1 || dst2[T].first == 0) {
      if (dst2[T].second == -1) cerr << "M O U D A M E P O !!!" << endl;
      else cerr << "S U C C E S S !!!" << endl;
      return make_pair(Greedy(state.game), state.ai);
    }

    vector<int> shortest_path;
    for (int v = T; v != S; v = dst2[v].second) shortest_path.emplace_back(v);
    shortest_path.emplace_back(S);
    reverse(all(shortest_path));

    tuple<int, int, int> bst(-1, -1, -1);
    for (int i = 0; i + 1 < (int)shortest_path.size(); ++i) {
      UnionFind uf2 = uf;
      uf2.Merge(shortest_path[i], shortest_path[i + 1]);

      // map<pair<int, int>, int> es;
      vector<pair<int, int>> es;
      rep (v, N) for (const auto &e: G[v]) {
        if (e.owner == -1) {
          int a = uf2.root[v];
          int b = uf2.root[e.to];
          if (a >= b) continue;
          // es[mp(min(a, b), max(a, b))] += 1;
          es.emplace_back(a, b);
        }
      }

      int s = uf2.root[S], t = uf2.root[T], f;
      if (s == t) {
        f = INT_MAX;
      } else {
        agl::bi_dinitz bdz(es, N);
        f = bdz.max_flow(uf2.root[S], uf2.root[T]);
        cerr << f << " ";
      }
      bst = max(bst, make_tuple(f, shortest_path[i], shortest_path[i + 1]));
    }
    cerr << endl;

    auto ans = FindOriginalEdge(get<1>(bst), get<2>(bst), uf, G);
    return make_pair(ans, state.ai);
  }

  assert(false);
}
}  // namespace

template<typename AIState, typename SetupFunc, typename PlayFunc>
void RunWithFutures(SetupFunc setup, PlayFunc play) {
  using MyState = State<AIState>;

  // Input
  json11::Json in_json = InputJSON(), out_json;

  if (IsSetup(in_json)) {
    // Setup
    MyState s;
    vector<pair<int, int>> futures;
    s.game = ConstructGameState(in_json);
    tie(s.ai, futures) = setup(s.game);

    vector<json11::Json> futures_json;
    for (auto &f : futures) {
      futures_json.emplace_back(
          json11::Json::object{{"source", f.first}, {"target", f.second}});
    }

    out_json = json11::Json::object{
      {"ready", s.game.rank},
      {"state", DumpState(s)},
      {"futures", futures_json},
    };

    cerr << json11::Json(futures_json).dump() << endl;
  } else {
    // Play
    MyState s = GetState<AIState>(in_json);
    pair<pair<int, int>, AIState> res = play(s);
    s.ai = res.second;

    out_json = json11::Json::object{
      {"claim", json11::Json::object{
          {"punter", s.game.rank},
          {"source", s.game.map.sites[res.first.first].id},
          {"target", s.game.map.sites[res.first.second].id}}},
      {"state", DumpState(s) }};
  }

  // Output
  OutputJSON(out_json);
}

int main() {
  srand(getpid() * time(NULL));
  RunWithFutures<MyAIState>(Setup, Play);
  return 0;
}
