#include "field.hh"

Cell::Cell(int x, int y): x(x), y(y) {}
Cell::Cell(object &o):
  x(o["x"].get<double>()),
  y(o["y"].get<double>()) {}
Cell::Cell(const Cell &original):
  x(original.x), y(original.y) {}
Cell&Cell::operator=(const Cell &another) {
  x = another.x; y = another.y;
  return *this;
}
bool Cell::operator==(const Cell &another) {
  return another.x == x && another.y == y;
}

Agent::Agent(int x, int y, int d):
  Cell(x, y), direction(d) {}
Agent::Agent(object &o):
  Cell(o), direction(o["direction"].get<double>()) {}
Agent::Agent(const Agent &original):
  Cell(original), direction(original.direction) {}

Gold::Gold(int x, int y, int a):
  Cell(x, y), amount(a) {}
Gold::Gold(object &o):
  Cell(o), amount(o["amount"].get<double>()) {}
Gold::Gold(const Gold &original):
  Cell(original), amount(original.amount) {}

Field::Field(object &json) {
  auto &agts = json["agents"].get<value::array>();
  for (auto &a: agts) agents.push_back(Agent(a.get<object>()));
  auto &hls = json["holes"].get<value::array>();
  for (auto &h: hls) holes.push_back(Cell(h.get<object>()));
  auto &kg = json["known"].get<value::array>();
  for (auto &g: kg) known.push_back(Gold(g.get<object>()));
  auto &hg = json["hidden"].get<value::array>();
  for (auto &g: hg) hidden.push_back(Gold(g.get<object>()));
}

Field::Field(const Field &fld) {
  size = fld.size;
  for (auto a: fld.agents) agents.push_back(Agent(a));
  for (auto h: fld.holes) holes.push_back(Cell(h));
  for (auto kg: fld.known) known.push_back(Gold(kg));
  for (auto hg: fld.hidden) hidden.push_back(Gold(hg));
}

Field::Field(const Field &prev, const int plans[],
	     int actions[], int scores[])
  :Field(prev) {
  Cell dug[2];
  Cell targets[4];
  for (int a = 0; a != 4; a++) {
    Cell &pos = agents[a];
    int plan = plans[a];
    actions[a] = -1;		// Stop on an invalid move
    // Check if the plan is valid
    if (plan <= -1 || 24 <= plan || (a < 2 && plan%2 != 0)) {
      // Out of range or diagonal move by a samurai
      continue;
    }
    static const int dxs[] = {  0, -1, -1, -1,  0, +1, +1, +1 };
    static const int dys[] = { +1, +1,  0, -1, -1, -1,  0, +1 };
    int nx = pos.x + dxs[plan%8];
    int ny = pos.y + dys[plan%8];
    if (nx < 0 || size <= nx || ny < 0 || size <= ny) {
      // Moving to, digging, or plugging a cell out of the field
      continue;
    }
    Cell target(nx, ny);
    if (find(agents.begin(), agents.end(), target) != agents.end()) {
      // Moving to, digging, or plugging hole in a cell with another agent
      continue;
    }
    if (a >= 2 && 8 <= plan) {
      // Dogs can't dig or plug a hole
      continue;
    }
    if (plan < 16) {
      if (find(holes.begin(), holes.end(), target) != holes.end()) {
	// Moving to or digging a cell already with a hole
	continue;
      }
      if (8 <= plan) {
	dug[a] = target;
      }
    } else if (find(holes.begin(), holes.end(), target) == holes.end()) {
      // Plugging a cell without a hole
      continue;
    }
    actions[a] = plan;
    targets[a] = target;
    agents[a].direction = plan%8;
  }
  // Decide agent posisions
  for (int a = 0; a != 4; a++) {
    // Two or more agents moving to the same cell have to stop
    int action = actions[a];
    if (action < 8) {
      Cell &target = targets[a];
      if (0 <= action && action < 8) {
	for (int b = a+1; b != 4; b++) {
	  if (target == targets[b] && 0 <= actions[b] && actions[b] < 8) {
	    actions[a] = actions[b] = -1;
	    targets[a] = agents[a];
	    targets[b] = agents[b];
	    goto NOMOVE;
	  }
	}
	agents[a].x = target.x;
	agents[a].y = target.y;
      NOMOVE:;
      }
    }
  }
  for (int a = 0; a != 4; a++) {
    int action = actions[a];
    Cell target = targets[a];
    if (action < 0) {
      // Do nothing
    } else {
      agents[a].direction = action%8;
      if (action < 8) {
	// Move to the target
	if (a >= 2) {		// Dog barks on a treasure
	  auto found =
	    find_if(hidden.begin(), hidden.end(),
		    [target](auto &g) {
		      return g.x == target.x && g.y == target.y; });
	  if (found != hidden.end()) {
	    known.push_back(*found);
	    hidden.erase(found);
	  }
	}
      } else {
	for (int b = 0; b != 4; b++) {
	  if (b != a && target == agents[b]) {
	    // Cannot dig or plug a hole in a cell to which another agent moves
	    actions[a] = -1;
	    goto NODIGPLUG;
	  }
	}
	if (action < 16) {
	  // Dig a hole
	  vector <Gold> *foundIn = nullptr;
	  auto g = find(known.begin(), known.end(), target);
	  if (g != known.end()) {
	    foundIn = &known;
	  } else {
	    g = find(hidden.begin(), hidden.end(), target);
	    if (g != hidden.end()) {
	      foundIn = &hidden;
	    }
	  }
	  if (foundIn != nullptr) {
	    // Some gold is dug out
	    if (dug[0] == dug[1]) {
	      // Both teams dug out the same gold
	      scores[0] += g->amount/2;
	      scores[1] += g->amount/2;
	    } else {
	      scores[a] += g->amount;
	    }
	    if (foundIn->size() != 1) {
	      *g = *(foundIn->rbegin());
	    }
	    foundIn->pop_back();
	  }
	  auto h = find(holes.begin(), holes.end(), target);
	  if (h == holes.end()) holes.push_back(target);
	} else {
	  // Plug a hole
	  auto h = find(holes.begin(), holes.end(), target);
	  if (holes.size() != 1) *h = *holes.rbegin();
	  holes.pop_back();
	}
      NODIGPLUG:;
      }
    }
  }
}

ostream &operator<<(ostream &out, const Field &f) {
  out << "Agents:";
  for (auto a: f.agents)
    out << " @(" << a.x << ',' << a.y << ")"
	<< "↑" << a.direction;
  out << endl;
  out << "Holes:";
  for (auto h: f.holes) out << " @(" << h.x << ',' << h.y << ")";
  out << endl;
  out << "Known Golds:";
  for (auto g: f.known)
    out << " @(" << g.x << ',' << g.y << ")"
	<< "=" << g.amount;
  out << endl;
  out << "Hidden Golds:";
  for (auto g: f.hidden)
    out << " @(" << g.x << ',' << g.y << ")"
	<< "=" << g.amount;
  out << endl;
  return out;
}

object Cell::json() {
  object obj;
  obj.emplace(make_pair("x", value((double)x)));
  obj.emplace(make_pair("y", value((double)y)));
  return obj;
}

object Gold::json() {
  object obj = Cell::json();
  obj.emplace(make_pair("amount", value((double)amount)));
  return obj;
}

object Agent::json() {
  object obj = Cell::json();
  obj.emplace(make_pair("direction", value((double)direction)));
  return obj;
}

object Field::json() {
  object obj;
  obj.emplace(make_pair("size", value((double)size)));
  value::array agentsArray;
  for (Agent a: agents) agentsArray.push_back(value(a.json()));
  obj.emplace(make_pair("agents", agentsArray));
  value::array holesArray;
  for (Cell c: holes) holesArray.push_back(value(c.json()));
  obj.emplace(make_pair("holes", holesArray));
  value::array knownArray;
  for (Gold g: known) knownArray.push_back(value(g.json()));
  obj.emplace(make_pair("known", knownArray));
  value::array hiddenArray;
  for (Gold h: hidden) hiddenArray.push_back(value(h.json()));
  obj.emplace(make_pair("hidden", hiddenArray));
  return obj;
}
