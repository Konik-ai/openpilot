#include <array>
#include <cassert>
#include <fstream>
#include <map>
#include <vector>
#include <atomic>
#include <sstream>

#include "common/swaglog.h"
#include "common/util.h"
#include "third_party/raylib/include/raylib.h"

int freshClone();
int cachedFetch(const std::string &cache);
int executeGitCommand(const std::string &cmd);
std::string httpGet(const std::string &url, size_t chunk_size = 0, std::atomic<bool> *abort = nullptr);

struct Fork {
  std::string name;
  std::string url;
};

std::vector<Fork> fetchForkList();
int showForkSelection(const std::vector<Fork> &forks);
void renderForkSelection(const std::vector<Fork> &forks, int selected);

std::string get_str(std::string const s) {
  std::string::size_type pos = s.find('?');
  assert(pos != std::string::npos);
  return s.substr(0, pos);
}

// Leave some extra space for the fork installer
std::string GIT_URL = get_str("https://github.com/commaai/openpilot.git" "?                                                                ");
const std::string BRANCH_STR = get_str(BRANCH "?                                                                ");
const std::string FORK_LIST_URL = "https://gist.githubusercontent.com/ChosenCypher/6f34c27ea47ce2b52d20813fa8d1784a/raw";

#define GIT_SSH_URL "git@github.com:commaai/openpilot.git"
#define CONTINUE_PATH "/data/continue.sh"

const std::string INSTALL_PATH = "/data/openpilot";
const std::string VALID_CACHE_PATH = "/data/.openpilot_cache";

#define TMP_INSTALL_PATH "/data/tmppilot"

const int FONT_SIZE = 120;

extern const uint8_t str_continue[] asm("_binary_selfdrive_ui_installer_continue_openpilot_sh_start");
extern const uint8_t str_continue_end[] asm("_binary_selfdrive_ui_installer_continue_openpilot_sh_end");
extern const uint8_t inter_ttf[] asm("_binary_selfdrive_ui_installer_inter_ascii_ttf_start");
extern const uint8_t inter_ttf_end[] asm("_binary_selfdrive_ui_installer_inter_ascii_ttf_end");

Font font;

void run(const char* cmd) {
  int err = std::system(cmd);
  assert(err == 0);
}

// Simple HTTP GET implementation using system curl
std::string httpGet(const std::string &url, size_t chunk_size, std::atomic<bool> *abort) {
  std::string cmd = "curl -s -L \"" + url + "\"";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) return "";

  std::string result;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    if (abort && *abort) break;
    result += buffer;
  }
  pclose(pipe);
  return result;
}

// Parse JSON fork list (simple parser for the specific format)
std::vector<Fork> parseForkList(const std::string &json) {
  std::vector<Fork> forks;

  // Add default openpilot option
  forks.push_back({"openpilot (official)", "https://github.com/commaai/openpilot.git"});

  // Simple JSON parsing for the specific format: [{"name":"...","url":"..."},...]
  size_t pos = 0;
  while ((pos = json.find("\"name\":", pos)) != std::string::npos) {
    Fork fork;

    // Extract name
    size_t name_start = json.find("\"", pos + 7) + 1;
    size_t name_end = json.find("\"", name_start);
    if (name_end == std::string::npos) break;
    fork.name = json.substr(name_start, name_end - name_start);

    // Extract URL
    size_t url_pos = json.find("\"url\":", name_end);
    if (url_pos == std::string::npos) break;
    size_t url_start = json.find("\"", url_pos + 6) + 1;
    size_t url_end = json.find("\"", url_start);
    if (url_end == std::string::npos) break;

    std::string url = json.substr(url_start, url_end - url_start);
    // Convert installer URL to git URL if needed
    if (url.find("http") == 0 && url.find(".git") == std::string::npos) {
      // This is an installer URL, we need to convert it to a git URL
      // For now, we'll use a placeholder - this would need fork-specific logic
      fork.url = "https://github.com/" + fork.name + "/openpilot.git";
    } else {
      fork.url = url;
    }

    forks.push_back(fork);
    pos = url_end;
  }

  return forks;
}

std::vector<Fork> fetchForkList() {
  LOGD("Fetching fork list from %s", FORK_LIST_URL.c_str());
  std::atomic<bool> abort{false};
  std::string json = httpGet(FORK_LIST_URL, 0, &abort);

  if (json.empty()) {
    LOGW("Failed to fetch fork list, using default");
    return {{"openpilot (official)", "https://github.com/commaai/openpilot.git"}};
  }

  return parseForkList(json);
}

void renderForkSelection(const std::vector<Fork> &forks, int selected) {
  BeginDrawing();
  ClearBackground(BLACK);

  // Title
  const char *title = "Select openpilot Fork";
  int title_width = MeasureText(title, FONT_SIZE);
  DrawTextEx(font, title, (Vector2){(float)(GetScreenWidth() - title_width)/2, 100}, FONT_SIZE, 0, WHITE);

  // Fork list
  int y_start = 300;
  int item_height = 120;

  for (size_t i = 0; i < forks.size(); i++) {
    Color color = (i == selected) ? (Color){70, 91, 234, 255} : (Color){41, 41, 41, 255};
    Color text_color = (i == selected) ? WHITE : (Color){200, 200, 200, 255};

    Rectangle rect = {150, (float)(y_start + i * item_height), (float)GetScreenWidth() - 300, (float)item_height - 20};
    DrawRectangleRec(rect, color);

    // Fork name
    DrawTextEx(font, forks[i].name.c_str(), (Vector2){rect.x + 20, rect.y + 20}, 60, 0, text_color);

    // Fork URL (smaller text)
    DrawTextEx(font, forks[i].url.c_str(), (Vector2){rect.x + 20, rect.y + 70}, 35, 0, text_color);
  }

  // Instructions
  const char *instructions = "Use UP/DOWN arrows to select, ENTER to confirm, ESC to use default";
  int instr_width = MeasureText(instructions, 40);
  DrawTextEx(font, instructions, (Vector2){(float)(GetScreenWidth() - instr_width)/2, (float)GetScreenHeight() - 100}, 40, 0, WHITE);

  EndDrawing();
}

int showForkSelection(const std::vector<Fork> &forks) {
  if (forks.size() <= 1) return 0; // No selection needed

  int selected = 0;

  while (!WindowShouldClose()) {
    // Handle input
    if (IsKeyPressed(KEY_UP)) {
      selected = (selected - 1 + forks.size()) % forks.size();
    }
    if (IsKeyPressed(KEY_DOWN)) {
      selected = (selected + 1) % forks.size();
    }
    if (IsKeyPressed(KEY_ENTER)) {
      return selected;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      return 0; // Default to official openpilot
    }

    renderForkSelection(forks, selected);
  }

  return 0; // Default fallback
}

void finishInstall() {
  BeginDrawing();
    ClearBackground(BLACK);
    const char *m = "Finishing install...";
    int text_width = MeasureText(m, FONT_SIZE);
    DrawTextEx(font, m, (Vector2){(float)(GetScreenWidth() - text_width)/2 + FONT_SIZE, (float)(GetScreenHeight() - FONT_SIZE)/2}, FONT_SIZE, 0, WHITE);
  EndDrawing();
  util::sleep_for(60 * 1000);
}

void renderProgress(int progress) {
  BeginDrawing();
    ClearBackground(BLACK);
    DrawTextEx(font, "Installing...", (Vector2){150, 290}, 110, 0, WHITE);
    Rectangle bar = {150, 570, (float)GetScreenWidth() - 300, 72};
    DrawRectangleRec(bar, (Color){41, 41, 41, 255});
    progress = std::clamp(progress, 0, 100);
    bar.width *= progress / 100.0f;
    DrawRectangleRec(bar, (Color){70, 91, 234, 255});
    DrawTextEx(font, (std::to_string(progress) + "%").c_str(), (Vector2){150, 670}, 85, 0, WHITE);
  EndDrawing();
}

int doInstall() {
  // wait for valid time
  while (!util::system_time_valid()) {
    util::sleep_for(500);
    LOGD("Waiting for valid time");
  }

  // cleanup previous install attempts
  run("rm -rf " TMP_INSTALL_PATH);

  // do the install
  if (util::file_exists(INSTALL_PATH) && util::file_exists(VALID_CACHE_PATH)) {
    return cachedFetch(INSTALL_PATH);
  } else {
    return freshClone();
  }
}

int freshClone() {
  LOGD("Doing fresh clone");
  std::string cmd = util::string_format("git clone --progress %s -b %s --depth=1 --recurse-submodules %s 2>&1",
                                        GIT_URL.c_str(), BRANCH_STR.c_str(), TMP_INSTALL_PATH);
  return executeGitCommand(cmd);
}

int cachedFetch(const std::string &cache) {
  LOGD("Fetching with cache: %s", cache.c_str());

  run(util::string_format("cp -rp %s %s", cache.c_str(), TMP_INSTALL_PATH).c_str());
  run(util::string_format("cd %s && git remote set-branches --add origin %s", TMP_INSTALL_PATH, BRANCH_STR.c_str()).c_str());

  renderProgress(10);

  return executeGitCommand(util::string_format("cd %s && git fetch --progress origin %s 2>&1", TMP_INSTALL_PATH, BRANCH_STR.c_str()));
}

int executeGitCommand(const std::string &cmd) {
  static const std::array stages = {
    // prefix, weight in percentage
    std::pair{"Receiving objects: ", 91},
    std::pair{"Resolving deltas: ", 2},
    std::pair{"Updating files: ", 7},
  };

  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) return -1;

  char buffer[512];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    std::string line(buffer);
    int base = 0;
    for (const auto &[text, weight] : stages) {
      if (line.find(text) != std::string::npos) {
        size_t percentPos = line.find("%");
        if (percentPos != std::string::npos && percentPos >= 3) {
          int percent = std::stoi(line.substr(percentPos - 3, 3));
          int progress = base + int(percent / 100. * weight);
          renderProgress(progress);
        }
        break;
      }
      base += weight;
    }
  }
  return pclose(pipe);
}

void cloneFinished(int exitCode) {
  LOGD("git finished with %d", exitCode);
  assert(exitCode == 0);

  renderProgress(100);

  // ensure correct branch is checked out
  int err = chdir(TMP_INSTALL_PATH);
  assert(err == 0);
  run(("git checkout " + BRANCH_STR).c_str());
  run(("git reset --hard origin/" + BRANCH_STR).c_str());
  run("git submodule update --init");

  // move into place
  run(("rm -f " + VALID_CACHE_PATH).c_str());
  run(("rm -rf " + INSTALL_PATH).c_str());
  run(util::string_format("mv %s %s", TMP_INSTALL_PATH, INSTALL_PATH.c_str()).c_str());

#ifdef INTERNAL
  run("mkdir -p /data/params/d/");

  // https://github.com/commaci2.keys
  const std::string ssh_keys = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIMX2kU8eBZyEWmbq0tjMPxksWWVuIV/5l64GabcYbdpI";
  std::map<std::string, std::string> params = {
    {"SshEnabled", "1"},
    {"RecordFrontLock", "1"},
    {"GithubSshKeys", ssh_keys},
  };
  for (const auto& [key, value] : params) {
    std::ofstream param;
    param.open("/data/params/d/" + key);
    param << value;
    param.close();
  }
  run(("cd " + INSTALL_PATH + " && "
      "git remote set-url origin --push " GIT_SSH_URL " && "
      "git config --replace-all remote.origin.fetch \"+refs/heads/*:refs/remotes/origin/*\"").c_str());
#endif

  // write continue.sh
  FILE *of = fopen("/data/continue.sh.new", "wb");
  assert(of != NULL);

  size_t num = str_continue_end - str_continue;
  size_t num_written = fwrite(str_continue, 1, num, of);
  assert(num == num_written);
  fclose(of);

  run("chmod +x /data/continue.sh.new");
  run("mv /data/continue.sh.new " CONTINUE_PATH);

  // wait for the installed software's UI to take over
  finishInstall();
}

int main(int argc, char *argv[]) {
  InitWindow(2160, 1080, "Installer");
  font = LoadFontFromMemory(".ttf", inter_ttf, inter_ttf_end - inter_ttf, FONT_SIZE, NULL, 0);
  SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

  if (util::file_exists(CONTINUE_PATH)) {
    finishInstall();
  } else {
    // Show fork selection screen
    std::vector<Fork> forks = fetchForkList();
    int selected_fork = showForkSelection(forks);

    // Update GIT_URL based on selection
    if (selected_fork >= 0 && selected_fork < forks.size()) {
      GIT_URL = forks[selected_fork].url;
      LOGD("Selected fork: %s (%s)", forks[selected_fork].name.c_str(), GIT_URL.c_str());
    }

    renderProgress(0);
    int result = doInstall();
    cloneFinished(result);
  }

  CloseWindow();
  UnloadFont(font);
  return 0;
}
