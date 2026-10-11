#include "NotebookStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Serialization.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace {
constexpr char kDir[] = "/.crosspoint/notebooks";
constexpr char kIndex[] = "/.crosspoint/notebooks/index.bin";
constexpr char kMagic[] = "CINB";
constexpr uint8_t kVersion = 1;
struct Index { char magic[4]; uint8_t version; uint8_t count; uint32_t nextId; std::array<NotebookInfo, NOTEBOOK_MAX_NOTEBOOKS> entries{}; };
std::string pagePath(uint32_t id, uint16_t page) { return std::string(kDir) + "/" + std::to_string(id) + "_" + std::to_string(page) + ".bin"; }
bool valid(const NotebookStroke& s) { return s.pointCount >= 2 && s.pointCount <= NOTEBOOK_MAX_POINTS_PER_STROKE; }
bool recover(const std::string& path) { const std::string bak=path+".bak"; return Storage.exists(path.c_str()) || !Storage.exists(bak.c_str()) || Storage.rename(bak.c_str(),path.c_str()); }
bool atomicWrite(const std::string& path, const void* data, size_t size) {
  if (!Storage.exists(kDir) && !Storage.mkdir(kDir)) return false;
  if (!recover(path)) return false; const std::string tmp=path+".tmp", bak=path+".bak";
  if (Storage.exists(tmp.c_str())) Storage.remove(tmp.c_str()); FsFile f=Storage.open(tmp.c_str(), O_WRONLY|O_CREAT|O_TRUNC);
  if (!f) return false; const bool ok=f.write(static_cast<const uint8_t*>(data),size)==size && f.sync(); f.close();
  if (!ok) { Storage.remove(tmp.c_str()); return false; } const bool old=Storage.exists(path.c_str());
  if (Storage.exists(bak.c_str())) Storage.remove(bak.c_str()); if (old && !Storage.rename(path.c_str(),bak.c_str())) { Storage.remove(tmp.c_str()); return false; }
  if (!Storage.rename(tmp.c_str(),path.c_str())) { if(old) Storage.rename(bak.c_str(),path.c_str()); Storage.remove(tmp.c_str()); return false; }
  if (old && Storage.exists(bak.c_str())) Storage.remove(bak.c_str()); return true;
}
bool readIndex(Index& i) { i={}; if(!recover(kIndex)) return false; if(!Storage.exists(kIndex)) { memcpy(i.magic,kMagic,4);i.version=kVersion;i.nextId=1;return true;} FsFile f; if(!Storage.openFileForRead("NB",kIndex,f))return false; bool ok=serialization::tryReadPod(f,i); f.close(); return ok && memcmp(i.magic,kMagic,4)==0 && i.version==kVersion && i.count<=NOTEBOOK_MAX_NOTEBOOKS; }
bool writeIndex(const Index& i) { return atomicWrite(kIndex,&i,sizeof(i)); }
bool updatePageCount(uint32_t id,uint16_t page){Index i; if(!readIndex(i))return false;for(uint8_t n=0;n<i.count;n++)if(i.entries[n].id==id){i.entries[n].pageCount=std::max<uint8_t>(i.entries[n].pageCount,page+1);return writeIndex(i);}return false;}
}
bool NotebookStore::list(std::array<NotebookInfo, NOTEBOOK_MAX_NOTEBOOKS>& out,uint8_t& count){Index i;if(!readIndex(i))return false;out=i.entries;count=i.count;return true;}
bool NotebookStore::create(const char* title,NotebookInfo& out){Index i;if(!readIndex(i)||i.count>=NOTEBOOK_MAX_NOTEBOOKS)return false;out={};out.id=i.nextId?i.nextId:1; strncpy(out.title,title?title:"Notebook",NOTEBOOK_TITLE_MAX-1);i.nextId=out.id+1;if(!i.nextId)i.nextId=1;i.entries[i.count++]=out;return writeIndex(i);}
bool NotebookStore::remove(uint32_t id){Index i;if(!readIndex(i))return false;uint8_t at=i.count;for(uint8_t n=0;n<i.count;n++)if(i.entries[n].id==id){at=n;break;}if(at==i.count)return false;for(uint16_t p=0;p<NOTEBOOK_MAX_PAGES;p++){const std::string path=pagePath(id,p);if(Storage.exists(path.c_str()))Storage.remove(path.c_str());const std::string bak=path+".bak";if(Storage.exists(bak.c_str()))Storage.remove(bak.c_str());}for(uint8_t n=at;n+1<i.count;n++)i.entries[n]=i.entries[n+1];--i.count;return writeIndex(i);}
bool NotebookStore::loadPage(uint32_t id,uint16_t n,NotebookPage& out){out={};out.pageNumber=n;if(n>=NOTEBOOK_MAX_PAGES)return false;const std::string p=pagePath(id,n);if(!recover(p))return false;if(!Storage.exists(p.c_str()))return true;FsFile f;if(!Storage.openFileForRead("NB",p,f))return false;bool ok=serialization::tryReadPod(f,out);f.close();return ok&&out.pageNumber==n&&out.strokeCount<=NOTEBOOK_MAX_STROKES_PER_PAGE;}
bool NotebookStore::savePage(uint32_t id,const NotebookPage& p){if(p.pageNumber>=NOTEBOOK_MAX_PAGES||p.strokeCount>NOTEBOOK_MAX_STROKES_PER_PAGE)return false;for(uint8_t n=0;n<p.strokeCount;n++)if(!valid(p.strokes[n]))return false;return atomicWrite(pagePath(id,p.pageNumber),&p,sizeof(p))&&updatePageCount(id,p.pageNumber);}
bool NotebookStore::appendStroke(uint32_t id,uint16_t n,const NotebookStroke& s){NotebookPage p;if(!valid(s)||!loadPage(id,n,p)||p.strokeCount>=NOTEBOOK_MAX_STROKES_PER_PAGE)return false;NotebookStroke stored=s;uint32_t maxId=0;for(uint8_t i=0;i<p.strokeCount;i++)maxId=std::max(maxId,p.strokes[i].id);stored.id=maxId+1; if(!stored.id)stored.id=1;p.strokes[p.strokeCount++]=stored;return savePage(id,p);}
bool NotebookStore::eraseLatest(uint32_t id,uint16_t n){NotebookPage p;if(!loadPage(id,n,p)||!p.strokeCount)return true;--p.strokeCount;return savePage(id,p);}
bool NotebookStore::clearPage(uint32_t id,uint16_t n){NotebookPage p{};p.pageNumber=n;return savePage(id,p);}
