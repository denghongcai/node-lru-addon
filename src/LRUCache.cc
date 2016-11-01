#include "LRUCache.h"
#include <math.h>
#include <iostream>

#ifndef __APPLE__
#include <unordered_map>
#endif

#ifdef WIN32
#include <time.h>
int gettimeofday(struct timeval *tp, void *tzp)
{
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;

    GetLocalTime(&wtm);
    tm.tm_year     = wtm.wYear - 1900;
    tm.tm_mon     = wtm.wMonth - 1;
    tm.tm_mday     = wtm.wDay;
    tm.tm_hour     = wtm.wHour;
    tm.tm_min     = wtm.wMinute;
    tm.tm_sec     = wtm.wSecond;
    tm. tm_isdst    = -1;
    clock = mktime(&tm);
    tp->tv_sec = clock;
    tp->tv_usec = wtm.wMilliseconds * 1000;

    return (0);
}
#else
#include <sys/time.h>
#endif

using namespace v8;

unsigned long getCurrentTime()
{
  timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

std::string getStringValue(Handle<Value> value)
{
  String::Utf8Value keyUtf8Value(value);
  return std::string(*keyUtf8Value);
}

NAN_MODULE_INIT(LRUCache::init)
{
  Local<String> className = Nan::New("LRUCache").ToLocalChecked();

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(className);

  Handle<ObjectTemplate> instance = tpl->InstanceTemplate();
  instance->SetInternalFieldCount(6);

  Nan::SetPrototypeMethod(tpl, "get", Get);
  Nan::SetPrototypeMethod(tpl, "set", Set);
  Nan::SetPrototypeMethod(tpl, "remove", Remove);
  Nan::SetPrototypeMethod(tpl, "clear", Clear);
  Nan::SetPrototypeMethod(tpl, "size", Size);
  Nan::SetPrototypeMethod(tpl, "stats", Stats);
  
  Nan::Set(target, className, Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(LRUCache::New)
{
  LRUCache* cache = new LRUCache();

  if (info.Length() > 0 && info[0]->IsObject())
  {
    Local<Object> config = info[0]->ToObject();

    Local<Value> maxElements = config->Get(Nan::New("maxElements").ToLocalChecked());
    if (maxElements->IsUint32())
      cache->maxElements = maxElements->Uint32Value();

    Local<Value> maxAge = config->Get(Nan::New("maxAge").ToLocalChecked());
    if (maxAge->IsUint32())
      cache->maxAge = maxAge->Uint32Value();

    Local<Value> maxLoadFactor = config->Get(Nan::New("maxLoadFactor").ToLocalChecked());
    if (maxLoadFactor->IsNumber())
      cache->data.max_load_factor(maxLoadFactor->NumberValue());

    Local<Value> size = config->Get(Nan::New("size").ToLocalChecked());
    if (size->IsUint32())
      cache->data.rehash(ceil(size->Uint32Value() / cache->data.max_load_factor()));
  }

  cache->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

LRUCache::LRUCache()
{
  this->maxElements = 0;
  this->maxAge = 0;
}

LRUCache::~LRUCache()
{
  this->disposeAll();
}

void LRUCache::disposeAll()
{
  for (HashMap::iterator itr = this->data.begin(); itr != this->data.end(); itr++)
  {
    HashEntry* entry = itr->second;
    entry->value.Reset();
    delete entry;
  }
}

void LRUCache::evict()
{
  const HashMap::iterator itr = this->data.find(this->lru.front());

  if (itr == this->data.end())
    return;

  HashEntry* entry = itr->second;

  // Dispose the V8 handle contained in the entry.
  entry->value.Reset();

  // Remove the entry from the hash and from the LRU list.
  this->data.erase(itr);
  this->lru.pop_front();

  // Free the entry itself.
  delete entry;
}

void LRUCache::remove(const HashMap::const_iterator itr)
{
  HashEntry* entry = itr->second;

  // Dispose the V8 handle contained in the entry.
  entry->value.Reset();

  // Remove the entry from the hash and from the LRU list.
  this->data.erase(itr);
  this->lru.erase(entry->pointer);

  // Free the entry itself.
  delete entry;
}

NAN_METHOD(LRUCache::Get)
{
  LRUCache* cache = Nan::ObjectWrap::Unwrap<LRUCache>(info.Holder());

  if (info.Length() != 1)
    return Nan::ThrowError(Nan::RangeError("Incorrect number of arguments for get(), expected 1"));

  std::string key = getStringValue(info[0]);
  const HashMap::const_iterator itr = cache->data.find(key);

  // If the specified entry doesn't exist, return undefined.
  if (itr == cache->data.end()) {
    info.GetReturnValue().SetUndefined();
    return;
  }

  HashEntry* entry = itr->second;

  if (cache->maxAge > 0 && getCurrentTime() - entry->timestamp > cache->maxAge)
  {
    // The entry has passed the maximum age, so we need to remove it.
    cache->remove(itr);

    // Return undefined.
    info.GetReturnValue().SetUndefined();
    return;
  }
  else
  {
    // Move the value to the end of the LRU list.
    cache->lru.splice(cache->lru.end(), cache->lru, entry->pointer);

    // Return the value.
    info.GetReturnValue().Set(Nan::New(entry->value));
    //info.GetReturnValue().Set(scope.Escape(Local<Value>::New(isolate, entry->value)));
  }
}

NAN_METHOD(LRUCache::Set)
{
  LRUCache* cache = Nan::ObjectWrap::Unwrap<LRUCache>(info.Holder());
  unsigned long now = cache->maxAge == 0 ? 0 : getCurrentTime();

  if (info.Length() != 2)
    return Nan::ThrowError(Nan::RangeError("Incorrect number of arguments for set(), expected 2"));

  std::string key = getStringValue(info[0]);
  Local<Value> value = info[1];
  const HashMap::iterator itr = cache->data.find(key);

  if (itr == cache->data.end())
  {
    // We're adding a new item. First ensure we have space.
    if (cache->maxElements > 0 && cache->data.size() == cache->maxElements)
      cache->evict();

    // Add the value to the end of the LRU list.
    KeyList::iterator pointer = cache->lru.insert(cache->lru.end(), key);

    // Add the entry to the key-value map.
    HashEntry* entry = new HashEntry(value, pointer, now);
    cache->data.insert(std::make_pair(key, entry));
  }
  else
  {
    HashEntry* entry = itr->second;

    // We're replacing an existing value, so dispose the old V8 handle to ensure it gets GC'd.
    entry->value.Reset();

    // Replace the value in the key-value map with the new one, and update the timestamp.
    entry->value.Reset(value);
    entry->timestamp = now;

    // Move the value to the end of the LRU list.
    cache->lru.splice(cache->lru.end(), cache->lru, entry->pointer);
  }

  // Return undefined.
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(LRUCache::Remove)
{
  LRUCache* cache = Nan::ObjectWrap::Unwrap<LRUCache>(info.Holder());

  if (info.Length() != 1)
    return Nan::ThrowError(Nan::RangeError("Incorrect number of arguments for remove(), expected 1"));

  std::string key = getStringValue(info[0]);
  const HashMap::iterator itr = cache->data.find(key);

  if (itr != cache->data.end())
    cache->remove(itr);
  
  // Return undefined.
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(LRUCache::Clear)
{
  LRUCache* cache = Nan::ObjectWrap::Unwrap<LRUCache>(info.Holder());

  cache->disposeAll();
  cache->data.clear();
  cache->lru.clear();

  // Return undefined.
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(LRUCache::Size)
{
  LRUCache* cache = Nan::ObjectWrap::Unwrap<LRUCache>(info.Holder());
  info.GetReturnValue().Set(Nan::New<Integer>(static_cast<uint32_t>(cache->data.size())));
}

NAN_METHOD(LRUCache::Stats)
{
  LRUCache* cache = Nan::ObjectWrap::Unwrap<LRUCache>(info.Holder());

  Local<Object> stats = Nan::New<Object>();
  Nan::Set(stats, Nan::New("size").ToLocalChecked(), Nan::New(static_cast<uint32_t>(cache->data.size())));
  Nan::Set(stats, Nan::New("buckets").ToLocalChecked(), Nan::New(static_cast<uint32_t>(cache->data.bucket_count())));
  Nan::Set(stats, Nan::New("loadFactor").ToLocalChecked(), Nan::New(static_cast<uint32_t>(cache->data.load_factor())));
  Nan::Set(stats, Nan::New("maxLoadFactor").ToLocalChecked(), Nan::New(static_cast<uint32_t>(cache->data.max_load_factor())));

  info.GetReturnValue().Set(stats);
}
