#include "base.h"

Map ConstructMap(const json11::Json &j) {
  Map ma;

  auto sites = j["sites"].array_items();

  rep (i, sites.size()) {
    auto s = sites[i];
    int id = s["id"].int_value();
    double x = s["x"].number_value();
    double y = s["y"].number_value();
    ma.sites.emplace_back(Site{id, x, y, false});
  }

  map<int, int> id_to_idx = ConstructIdToIndexMap(ma);

  auto rivers = j["rivers"].array_items();
  for (auto &&river : rivers) {
    ma.rivers.emplace_back
        (id_to_idx[river["source"].int_value()],
         id_to_idx[river["target"].int_value()]);
  }

  auto mines = j["mines"].array_items();
  for (auto &&mine : mines) {
    int id = mine.int_value();
    int idx = id_to_idx[id];
    ma.mines.emplace_back(idx);
    ma.sites[idx].is_mine = true;
  }
  return ma;
}

GameState ConstructGameState(const json11::Json &j) {  // First turn
  GameState s;
  s.rank = j["punter"].int_value();
  s.size = j["punters"].int_value();
  s.map = ConstructMap(j["map"]);
  s.turn = 0;

  s.futures = s.options = false;
  if (!j["settings"].is_null()) {
    s.futures = j["settings"]["futures"].bool_value();
    s.options = j["settings"]["options"].bool_value();
  }

  return s;
}

map<int, int> ConstructIdToIndexMap(const Map &s) {
  map<int, int> ma;
  rep (i, s.sites.size()) {
    ma[s.sites[i].id] = i;
  }
  return ma;
}

//
// IO
//
json11::Json InputJSON() {
  string head;
  for (;;) {
    char c = cin.get();
    if (isdigit(c)) head += c;
    else break;
  }

  int n_bytes = stoi(head);
  vector<char> dat(n_bytes);
  cin.read(dat.data(), n_bytes);
  string str(dat.begin(), dat.end());
  // cerr << str << endl;

  string err;
  auto j = json11::Json::parse(str, err);
  if (!err.empty()) {
    cerr << "JSON Error: " << err << endl;
    assert(err.empty());
  }

  return j;
}

void OutputJSON(const json11::Json &json) {
  string str = json.dump() + "\n";
  ostringstream os;
  cout << str.length() << ":" << str;
}

bool IsSetup(const json11::Json &json) {
  return json["state"].is_null();
}
