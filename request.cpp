#include <atomic>
#include <codecvt>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>

#include <boost/format.hpp>

#include <windows.h>
#include <winhttp.h>

#include "modutils.h"
#include "settings.h"

HINTERNET hSession;

void InitHttp() {
  hSession = WinHttpOpen(
      L"Element Zero SimpleVote", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) throw std::runtime_error{"Failed to initialize WinHTTP"};
}

inline void CleanupHandle(HINTERNET *t) { WinHttpCloseHandle(*t); }

std::string Fetch(LPCWSTR path) {
  static std::wstring header = IIFE([] {
    auto fmt = boost::format("Identity: %s") % settings.identify;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(fmt.str());
  });
  __attribute__((cleanup(CleanupHandle))) HINTERNET hConnection =
      WinHttpConnect(hSession, L"api.pocketvote.io", INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnection) throw std::runtime_error{"Failed to connect server"};
  LPCWSTR types[2];
  types[0] = L"text/html";
  types[1] = 0;
  __attribute__((cleanup(CleanupHandle))) HINTERNET hRequest =
      WinHttpOpenRequest(hConnection, L"GET", path, nullptr, WINHTTP_NO_REFERER, types, WINHTTP_FLAG_SECURE);
  if (!hRequest) throw std::runtime_error{"Failed to open request"};
  if (!WinHttpSendRequest(hRequest, header.data(), header.size(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
    throw std::runtime_error{"Failed to send request"};
  if (!WinHttpReceiveResponse(hRequest, 0)) throw std::runtime_error{"Failed to recv response"};
  wchar_t szContentLength[32];
  DWORD cch           = 64;
  DWORD dwHeaderIndex = WINHTTP_NO_HEADER_INDEX;
  BOOL haveContentLength =
      WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, NULL, &szContentLength, &cch, &dwHeaderIndex);
  if (!haveContentLength) throw std::runtime_error{"Failed to query content size"};
  DWORD dwContentLength = _wtoi(szContentLength);
  std::string storage;
  storage.resize(dwContentLength);
  DWORD dwReceivedTotal = 0;
  while (WinHttpQueryDataAvailable(hRequest, &cch) && cch) {
    WinHttpReadData(hRequest, &storage[dwReceivedTotal], cch, &cch);
    dwReceivedTotal += cch;
  }
  return storage;
}

static std::mutex mtx;
static std::condition_variable cv;
static std::atomic_bool exited;
static std::vector<std::function<void()>> tasks;
static std::thread worker{[] {
  while (true) {
    std::unique_lock lk{mtx};
    cv.wait(lk, [] { return exited || tasks.size(); });
    if (exited) return;
    for (auto &task : tasks) task();
    tasks.clear();
  }
}};

void OnExit() {
  exited = true;
  cv.notify_one();
  worker.join();
}

void AddTask(std::function<void()> &&fn) {
  {
    std::lock_guard lg{mtx};
    tasks.emplace_back(std::move(fn));
  }
  cv.notify_one();
}