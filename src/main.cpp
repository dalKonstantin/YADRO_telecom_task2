#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

struct Product {
  int id = -1;
  int type = -1;
};

struct Machine {
  std::queue<int> queue;
  bool busy = false;
  long long busy_until = 0;
};

enum class EventType {
  Finish = 0,
  Start = 1,
  Wait = 2,
  Ready = 3,
};

struct Event {
  long long time = 0;
  EventType type = EventType::Start;
  int product_id = -1;
  int machine_id = -1;
  int queue_pos = -1;
};

struct EventCompare {
  bool operator()(const Event &lhs, const Event &rhs) const {
    if (lhs.time != rhs.time) {
      return lhs.time > rhs.time;
    }
    if (lhs.type != rhs.type) {
      return static_cast<int>(lhs.type) > static_cast<int>(rhs.type);
    }
    if (lhs.machine_id != rhs.machine_id) {
      return lhs.machine_id > rhs.machine_id;
    }
    return lhs.product_id > rhs.product_id;
  }
};

struct ParsedInput {
  int m = 0;
  int n = 0;
  std::vector<std::vector<int>> times;
  std::vector<std::vector<int>> initial_types;
  int total_products = 0;
};

enum class ParseResult {
  Ok,
  FormatError,
  OpenError,
};

bool parse_token(const std::string &token, long long &value) {
  try {
    size_t pos = 0;
    value = std::stoll(token, &pos, 10);
    return pos == token.size();
  } catch (...) {
    return false;
  }
}

bool parse_line_numbers(const std::string &line, std::vector<long long> &numbers) {
  std::istringstream iss(line);
  std::string token;
  numbers.clear();
  while (iss >> token) {
    long long value = 0;
    if (!parse_token(token, value)) {
      return false;
    }
    numbers.push_back(value);
  }
  return true;
}

bool line_is_blank(const std::string &line) {
  for (char c : line) {
    if (!std::isspace(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

ParseResult fail_on_line(const std::string &line) {
  std::cout << line << "\n";
  return ParseResult::FormatError;
}

ParseResult read_and_parse(const std::string &path, ParsedInput &parsed) {
  std::ifstream input(path);
  if (!input) {
    return ParseResult::OpenError;
  }

  std::vector<std::string> lines;
  std::string raw_line;
  while (std::getline(input, raw_line)) {
    if (!raw_line.empty() && raw_line.back() == '\r') {
      raw_line.pop_back();
    }
    lines.push_back(raw_line);
  }

  size_t line_index = 0;
  auto next_line = [&]() -> const std::string * {
    if (line_index >= lines.size()) {
      return nullptr;
    }
    return &lines[line_index++];
  };

  std::vector<long long> values;

  const std::string *line = next_line();
  if (line == nullptr) {
    return fail_on_line("");
  }
  if (!parse_line_numbers(*line, values) || values.size() != 2) {
    return fail_on_line(*line);
  }
  if (values[0] < 1 || values[0] > 100 || values[1] < 1 || values[1] > 100) {
    return fail_on_line(*line);
  }
  parsed.m = static_cast<int>(values[0]);
  parsed.n = static_cast<int>(values[1]);

  parsed.times.assign(parsed.m - 1, std::vector<int>(parsed.n, 0));
  for (int i = 0; i < parsed.m - 1; ++i) {
    line = next_line();
    if (line == nullptr) {
      return fail_on_line("");
    }
    if (!parse_line_numbers(*line, values) || static_cast<int>(values.size()) != parsed.n) {
      return fail_on_line(*line);
    }
    for (int j = 0; j < parsed.n; ++j) {
      if (values[j] < 0 || values[j] > 10000) {
        return fail_on_line(*line);
      }
      parsed.times[i][j] = static_cast<int>(values[j]);
    }
  }

  parsed.initial_types.assign(parsed.n, {});
  long long total_products = 0;
  for (int j = 0; j < parsed.n; ++j) {
    line = next_line();
    if (line == nullptr) {
      return fail_on_line("");
    }
    if (!parse_line_numbers(*line, values) || values.empty()) {
      return fail_on_line(*line);
    }
    if (values[0] < 0) {
      return fail_on_line(*line);
    }
    const int qj = static_cast<int>(values[0]);
    if (static_cast<int>(values.size()) != qj + 1) {
      return fail_on_line(*line);
    }
    parsed.initial_types[j].reserve(qj);
    for (int p = 0; p < qj; ++p) {
      const long long type = values[p + 1];
      if (type < 0 || type > parsed.m - 2) {
        return fail_on_line(*line);
      }
      parsed.initial_types[j].push_back(static_cast<int>(type));
    }
    total_products += qj;
    if (total_products > 100000) {
      return fail_on_line(*line);
    }
  }
  parsed.total_products = static_cast<int>(total_products);

  for (; line_index < lines.size(); ++line_index) {
    if (!line_is_blank(lines[line_index])) {
      return fail_on_line(lines[line_index]);
    }
  }

  return ParseResult::Ok;
}

int choose_machine(const std::vector<long long> &queue_wait) {
  int best_machine = 0;
  long long best_wait = queue_wait[0];
  for (size_t j = 1; j < queue_wait.size(); ++j) {
    if (queue_wait[j] < best_wait) {
      best_wait = queue_wait[j];
      best_machine = static_cast<int>(j);
    }
  }
  return best_machine;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "usage: task <path>\n";
    return 1;
  }

  ParsedInput parsed;
  const ParseResult parse_result = read_and_parse(argv[1], parsed);
  if (parse_result == ParseResult::OpenError) {
    std::cerr << "failed to open file\n";
    return 1;
  }
  if (parse_result == ParseResult::FormatError) {
    return 0;
  }

  std::vector<Machine> machines(parsed.n);
  std::vector<Product> products;
  products.reserve(parsed.total_products);
  std::vector<long long> queue_wait(parsed.n, 0);

  int next_product_id = 0;
  for (int j = 0; j < parsed.n; ++j) {
    for (int type : parsed.initial_types[j]) {
      products.push_back({next_product_id, type});
      machines[j].queue.push(next_product_id);
      queue_wait[j] += parsed.times[type][j];
      ++next_product_id;
    }
  }

  std::priority_queue<Event, std::vector<Event>, EventCompare> events;
  auto schedule_start = [&](long long time, int product_id, int machine_id) {
    machines[machine_id].busy = true;
    events.push({time, EventType::Start, product_id, machine_id, -1});
  };

  for (int j = 0; j < parsed.n; ++j) {
    if (!machines[j].queue.empty()) {
      const int product_id = machines[j].queue.front();
      machines[j].queue.pop();
      queue_wait[j] -= parsed.times[products[product_id].type][j];
      schedule_start(0, product_id, j);
    }
  }

  int finished_count = 0;
  long long stop_time = 0;

  while (!events.empty()) {
    const Event event = events.top();
    events.pop();

    Product &product = products[event.product_id];
    Machine &machine = machines[event.machine_id];

    // TODO: implement this into handle_event(event)
    if (event.type == EventType::Finish) {
      machine.busy = false;
      machine.busy_until = event.time;
      std::cout << "finish " << event.time << " " << product.id << " " << product.type << " "
                << event.machine_id << "\n";

      if (product.type == parsed.m - 2) {
        ++product.type;
        events.push({event.time, EventType::Ready, product.id, event.machine_id, -1});
      } else {
        ++product.type;
        const int next_machine = choose_machine(queue_wait);
        if (!machines[next_machine].busy) {
          schedule_start(event.time, product.id, next_machine);
        } else {
          const int queue_pos = static_cast<int>(machines[next_machine].queue.size());
          machines[next_machine].queue.push(product.id);
          queue_wait[next_machine] += parsed.times[product.type][next_machine];
          events.push(
              {event.time, EventType::Wait, product.id, next_machine, queue_pos});
        }
      }

      if (!machine.queue.empty()) {
        const int next_product = machine.queue.front();
        machine.queue.pop();
        queue_wait[event.machine_id] -=
            parsed.times[products[next_product].type][event.machine_id];
        schedule_start(event.time, next_product, event.machine_id);
      }
      continue;
    }

    if (event.type == EventType::Start) {
      const long long finish_time =
          event.time + parsed.times[product.type][event.machine_id];
      machine.busy_until = finish_time;
      std::cout << "start " << event.time << " " << product.id << " " << product.type << " "
                << event.machine_id << "\n";
      events.push({finish_time, EventType::Finish, product.id, event.machine_id, -1});
      continue;
    }

    if (event.type == EventType::Wait) {
      std::cout << "wait " << event.time << " " << product.id << " " << product.type << " "
                << event.machine_id << " " << event.queue_pos << "\n";
      continue;
    }

    std::cout << "ready " << event.time << " " << product.id << " " << event.machine_id
              << "\n";
    ++finished_count;
    if (finished_count == parsed.total_products) {
      stop_time = event.time;
    }
  }

  std::cout << "stop " << stop_time << "\n";
  return 0;
}
