#include <string>
#include <vector>


std::vector<std::wstring> SplitList(const std::wstring &s, wchar_t ch) {
  std::vector<std::wstring> sv;
  std::wstring l;
  for (auto c : s) {
    if (c == ch) {
      if (!l.empty()) {
        sv.push_back(std::move(l));
      }
      continue;
    }
    l.push_back(c);
  }
  if (!l.empty()) {
    sv.push_back(std::move(l));
  }
  return sv;
}

int wmain(int argc,wchar_t **argv){
    wprintf(L"Argv0: %s\n",_wpgmptr);
    auto paths=_wgetenv(L"PATH");
    auto pv=SplitList(paths,L';');
    for(const auto &p:pv){
        wprintf(L"%s\n",p.data());
    }
    auto exts=_wgetenv(L"PATHEXT");
    auto ev=SplitList(exts,L';');
    for(const auto &e:ev){
        wprintf(L"%s\n",e.data());
    }
    return 0;
}