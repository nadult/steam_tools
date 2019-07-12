#include "steam_internal.h"
#include "steam_ugc.h"

#define FUNC(name, ...) SteamAPI_ISteamUGC_##name

namespace steam {

UGCQuery::UGCQuery(intptr_t ugc) : m_ugc(ugc) {
}
UGCQuery::~UGCQuery() = default;

string UGCQuery::metadata(int idx) const {
  char buffer[4096];
  auto result = FUNC(GetQueryUGCMetadata)(m_ugc, m_handle, idx, buffer, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = 0;
  return buffer;
}

UGC::UGC(intptr_t ptr) : m_ptr(ptr) {
  m_query_completed.Register(this, &UGC::onQueryCompleted);
}

UGC::~UGC() {
  m_query_completed.Unregister();
}

int UGC::numSubscribedItems() const {
  return (int)FUNC(GetNumSubscribedItems)(m_ptr);
}

vector<ItemId> UGC::subscribedItems() const {
  vector<ItemId> out(numSubscribedItems());
  int result = FUNC(GetSubscribedItems)(m_ptr, out.data(), out.size());
  out.resize(result);
  return out;
}

uint32_t UGC::state(ItemId id) const {
  return FUNC(GetItemState)(m_ptr, id);
}

DownloadInfo UGC::downloadInfo(ItemId id) const {
  uint64 downloaded = 0, total = 0;
  auto result = FUNC(GetItemDownloadInfo)(m_ptr, id, &downloaded, &total);
  // TODO: handle result
  return {downloaded, total};
}

InstallInfo UGC::installInfo(ItemId id) const {
  uint64 size_on_disk;
  uint32 time_stamp;
  char buffer[4096];
  auto result = FUNC(GetItemInstallInfo)(m_ptr, id, &size_on_disk, buffer, sizeof(buffer) - 1, &time_stamp);
  buffer[sizeof(buffer) - 1] = 0;
  return {size_on_disk, buffer, time_stamp};
}

UGC::QueryId UGC::allocQuery(Query::Handle handle, const QueryInfo& info) {
  int index = -1;
  for (int n = 0; n < m_queries.size(); n++)
    if (!m_queries[n].isValid()) {
      index = n;
      break;
    }
  if (index == -1) {
    index = m_queries.size();
    m_queries.push_back(UGCQuery(m_ptr));
  }

  auto& query = m_queries[index];
  query.m_handle = handle;
  query.m_is_completed = false;
  query.m_info = info;

  return index;
}

void UGC::setupQuery(Query::Handle handle, const QueryInfo& info) {
  if (info.metadata)
    FUNC(SetReturnMetadata)(m_ptr, handle, true);
  if (info.children)
    FUNC(SetReturnChildren)(m_ptr, handle, true);
  // TODO
}

UGC::QueryId UGC::createQuery(const QueryInfo& info, vector<ItemId> items) {
  auto handle = FUNC(CreateQueryUGCDetailsRequest)(m_ptr, items.data(), items.size());
  CHECK(handle != k_UGCQueryHandleInvalid);
  // TODO: properly handle errors

  setupQuery(handle, info);

  auto result = FUNC(SendQueryUGCRequest)(m_ptr, handle);
  CHECK(result != k_uAPICallInvalid);
  printf("Send: %lld\n", (long long int)result);

  auto id = allocQuery(handle, info);
  m_queries[id].m_items = std::move(items);
  return id;
}

UGC::QueryId UGC::createQuery(const QueryInfo& info, EUGCQuery type, EUGCMatchingUGCType matching_type, unsigned app_id,
                              int page_id) {
  CHECK(page_id >= 1);
  auto handle = FUNC(CreateQueryAllUGCRequest)(m_ptr, type, matching_type, app_id, app_id, page_id);
  CHECK(handle != k_UGCQueryHandleInvalid);

  setupQuery(handle, info);

  auto result = FUNC(SendQueryUGCRequest)(m_ptr, handle);
  CHECK(result != k_uAPICallInvalid);
  printf("Send: %lld\n", (long long int)result);

  auto id = allocQuery(handle, info);
  return id;
}

UGCQuery& UGC::readQuery(QueryId id) {
  CHECK(m_queries[id].isValid() && m_queries[id].isCompleted());
  return m_queries[id];
}

bool UGC::isCompleted(QueryId id) const {
  CHECK(m_queries[id].isValid());
  return m_queries[id].isCompleted();
}

void UGC::finishQuery(QueryId id) {
  CHECK(m_queries[id].isValid());
  FUNC(ReleaseQueryUGCRequest)(m_ptr, m_queries[id].m_handle);
  m_queries[id].m_handle = k_UGCQueryHandleInvalid;
}

void UGC::onQueryCompleted(SteamUGCQueryCompleted_t* result) {
  printf("Query completed: %llu\n", (unsigned long long)result->m_handle);

  for (auto& query : m_queries)
    if (query.m_handle == result->m_handle) {
      //TODO: add member function?
      query.m_num_results = result->m_unNumResultsReturned;
      query.m_total_results = result->m_unTotalMatchingResults;
      query.m_is_completed = true;
    }
}
}
