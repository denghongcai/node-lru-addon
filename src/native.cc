#include <node.h>
#include <nan.h>
#include "LRUCache.h"

using namespace v8;

void init(Handle<Object> exports)
{
	LRUCache::init(exports);
}

NODE_MODULE(native, init)
