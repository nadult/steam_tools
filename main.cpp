#include <stdio.h>
#include <string>
#include <vector>
#include <unistd.h>

#include "steam_friends.h"
#include "steam_client.h"
#include "steam_ugc.h"
#include "steam_utils.h"

using namespace std;

void printFriends(const steam::Client& client) {
  auto friends = client.friends();

  auto ids = friends.ids();
  for (unsigned n = 0; n < ids.size(); n++)
    printf("Friend #%d: %s [%llu]\n", n, friends.name(ids[n]).c_str(), ids[n].ConvertToUint64());
  fflush(stdout);
}

void displayImage(vector<uint8_t> data, pair<int, int> size) {
  int max_size = 16, sub_size = 2;
  CHECK(size.first == max_size * sub_size && size.second == size.first);

  for (int y = 0; y < max_size; y++) {
    for (int x = 0; x < max_size; x++) {
      int value = 0;
      for (int ix = 0; ix < sub_size; ix++)
        for (int iy = 0; iy < sub_size; iy++)
          value += data[(x * sub_size + ix + (y * sub_size + iy) * size.first) * 4 + 1];
      value /= sub_size * sub_size;
      printf("%c", value > 80 ? 'X' : ' ');
    }
    printf("\n");
  }
  fflush(stdout);
}

void printFriendAvatars(const steam::Client& client) {
  auto friends = client.friends();
  auto utils = client.utils();

  auto ids = friends.ids();
  vector<int> results(ids.size(), -1);
  vector<bool> completed(ids.size(), false);

  for (size_t n = 0; n < ids.size(); n++)
    results[n] = friends.avatar(ids[n], 0);

  unsigned num_completed = 0;
  for (unsigned r = 0; r < 100 && num_completed < ids.size(); r++) {
    for (unsigned n = 0; n < ids.size(); n++) {
      if (completed[n])
        continue;

      results[n] = friends.avatar(ids[n], 0);
      if (results[n] == 0) {
        printf("%d: no avatar\n", n);
        num_completed++;
        completed[n] = true;
      } else if (results[n] == -1) {
        continue;
      } else {
        auto size = utils.imageSize(results[n]);
        printf("%d: Getting avatar data (%dx%d)\n", n, size.first, size.second);
        auto data = utils.imageData(results[n]);
        displayImage(data, size);

        completed[n] = true;
        num_completed++;
      }
    }
    usleep(100 * 1000);
  }
  printf("Completed: %d\n", num_completed);
}

template <class T, int size> constexpr int arraySize(T (&)[size]) {
  return size;
}

string itemStateText(uint32_t bits) {
  static const char* names[] = {"subscribed",   "legacy_item", "installed",
                                "needs_update", "downloading", "download_pending"};

  if (bits == k_EItemStateNone)
    return "none";

  string out;
  for (int n = 0; n < arraySize(names); n++)
    if (bits & (1 << n)) {
      if (!out.empty())
        out += ' ';
      out += names[n];
    }
  return out;
}

void printWorkshopItems(steam::Client& client) {
  auto& ugc = client.ugc();

  auto items = ugc.subscribedItems();
  for (auto item : items) {
    auto state = ugc.state(item);
    printf("Item #%d: %s\n", (int)item, itemStateText(state).c_str());
    if (state & k_EItemStateInstalled) {
      auto info = ugc.installInfo(item);
      printf("  Installed at: %s  size: %llu  time_stamp: %u\n", info.folder.c_str(), info.size_on_disk,
             info.time_stamp);
    }
    if (state & k_EItemStateDownloading) {
      auto info = ugc.downloadInfo(item);
      printf("  Downloading: %llu / %llu bytes\n", info.bytes_downloaded, info.bytes_total);
    }
  }

  steam::QueryInfo qinfo;
  qinfo.metadata = true;
  auto qid = ugc.createQuery(qinfo, items);

  int num_retires = 20;
  for (int r = 0; r < num_retires; r++) {
    steam::runCallbacks();
    if (ugc.isCompleted(qid)) {
      auto& query = ugc.readQuery(qid);
      printf("Query completed with %d / %d results\n", query.numResults(), query.totalResults());

      for (int n = 0; n < query.numResults(); n++)
        printf("  Meta %d: %s\n", n, query.metadata(n).c_str());
      break;
    }
    usleep(100 * 1000);
  }

  if (!ugc.isCompleted(qid)) {
    printf("Query failed!\n");
  }
  ugc.finishQuery(qid);
}

void printHelp() {
  printf("Options:\n-help\n-friends\n-avatars\n-workshop\n");
}

int main(int argc, char** argv) {
  if (argc <= 1) {
    printHelp();
    return 0;
  }

  if (!steam::initAPI()) {
    printf("Steam is not running\n");
    return 0;
  }

  steam::Client client;

  for (int n = 1; n < argc; n++) {
    string option = argv[n];

    if (option == "-friends")
      printFriends(client);
    else if (option == "-avatars")
      printFriendAvatars(client);
    else if (option == "-workshop")
      printWorkshopItems(client);
    else if (option == "-help")
      printHelp();
    else {
      printf("unknown option: %s\n", argv[n]);
      return 0;
    }
  }

  return 0;
}
