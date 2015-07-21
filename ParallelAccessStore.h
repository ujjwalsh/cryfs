#ifndef MESSMER_PARALLELACCESSSTORE_IMPLEMENTATIONS_PARALLELACCESS_PARALLELACCESSSTORE_H_
#define MESSMER_PARALLELACCESSSTORE_IMPLEMENTATIONS_PARALLELACCESS_PARALLELACCESSSTORE_H_

#include <mutex>
#include <memory>
#include <map>
#include <unordered_map>
#include <future>
#include <cassert>
#include <type_traits>
#include <messmer/cpp-utils/macros.h>
#include "ParallelAccessBaseStore.h"


//TODO Refactor
//TODO Test cases

namespace parallelaccessstore {

template<class Resource, class ResourceRef, class Key>
class ParallelAccessStore {
public:
  explicit ParallelAccessStore(cpputils::unique_ref<ParallelAccessBaseStore<Resource, Key>> baseStore);

  class ResourceRefBase {
  public:
    //TODO Better way to initialize
    ResourceRefBase(): _cachingStore(nullptr), _key(Key::CreatePseudoRandom()) {}
    void init(ParallelAccessStore *cachingStore, const Key &key) {
      _cachingStore = cachingStore;
      _key = key;
    }
    virtual ~ResourceRefBase() {
      _cachingStore->release(_key);
    }
  private:
    ParallelAccessStore *_cachingStore;
    //TODO We're storing Key twice (here and in the base resource). Rather use getKey() on the base resource if possible somehow.
    Key _key;
  };

  cpputils::unique_ref<ResourceRef> add(const Key &key, cpputils::unique_ref<Resource> resource);
  boost::optional<cpputils::unique_ref<ResourceRef>> load(const Key &key);
  void remove(const Key &key, cpputils::unique_ref<ResourceRef> block);

private:
  class OpenResource {
  public:
	OpenResource(cpputils::unique_ref<Resource> resource): _resource(std::move(resource)), _refCount(0) {}

	Resource *getReference() {
	  ++_refCount;
	  return _resource.get();
	}

	void releaseReference() {
	  --_refCount;
	}

	bool refCountIsZero() const {
	  return 0 == _refCount;
	}

	cpputils::unique_ref<Resource> moveResourceOut() {
	  return std::move(_resource);
	}
  private:
	cpputils::unique_ref<Resource> _resource;
	uint32_t _refCount;
  };

  std::mutex _mutex;
  cpputils::unique_ref<ParallelAccessBaseStore<Resource, Key>> _baseStore;

  std::unordered_map<Key, OpenResource> _openResources;
  std::map<Key, std::promise<cpputils::unique_ref<Resource>>> _resourcesToRemove;

  cpputils::unique_ref<ResourceRef> _add(const Key &key, cpputils::unique_ref<Resource> resource);
  cpputils::unique_ref<ResourceRef> _createResourceRef(Resource *resource, const Key &key);

  void release(const Key &key);
  friend class CachedResource;

  DISALLOW_COPY_AND_ASSIGN(ParallelAccessStore);
};

template<class Resource, class ResourceRef, class Key>
ParallelAccessStore<Resource, ResourceRef, Key>::ParallelAccessStore(cpputils::unique_ref<ParallelAccessBaseStore<Resource, Key>> baseStore)
  : _mutex(),
  _baseStore(std::move(baseStore)),
  _openResources(),
  _resourcesToRemove() {
  static_assert(std::is_base_of<ResourceRefBase, ResourceRef>::value, "ResourceRef must inherit from ResourceRefBase");
}

template<class Resource, class ResourceRef, class Key>
cpputils::unique_ref<ResourceRef> ParallelAccessStore<Resource, ResourceRef, Key>::add(const Key &key, cpputils::unique_ref<Resource> resource) {
  std::lock_guard<std::mutex> lock(_mutex);
  return _add(key, std::move(resource));
}

template<class Resource, class ResourceRef, class Key>
cpputils::unique_ref<ResourceRef> ParallelAccessStore<Resource, ResourceRef, Key>::_add(const Key &key, cpputils::unique_ref<Resource> resource) {
  auto insertResult = _openResources.emplace(key, std::move(resource));
  assert(true == insertResult.second);
  return _createResourceRef(insertResult.first->second.getReference(), key);
}

template<class Resource, class ResourceRef, class Key>
cpputils::unique_ref<ResourceRef> ParallelAccessStore<Resource, ResourceRef, Key>::_createResourceRef(Resource *resource, const Key &key) {
  auto resourceRef = cpputils::make_unique_ref<ResourceRef>(resource);
  resourceRef->init(this, key);
  return std::move(resourceRef);
}

template<class Resource, class ResourceRef, class Key>
boost::optional<cpputils::unique_ref<ResourceRef>> ParallelAccessStore<Resource, ResourceRef, Key>::load(const Key &key) {
  //TODO This lock doesn't allow loading different blocks in parallel. Can we do something with futures maybe?
  std::lock_guard<std::mutex> lock(_mutex);
  auto found = _openResources.find(key);
  if (found == _openResources.end()) {
    auto resource = _baseStore->loadFromBaseStore(key);
    if (resource == boost::none) {
      return boost::none;
    }
  	return _add(key, std::move(*resource));
  } else {
    return _createResourceRef(found->second.getReference(), key);
  }
}

template<class Resource, class ResourceRef, class Key>
void ParallelAccessStore<Resource, ResourceRef, Key>::remove(const Key &key, cpputils::unique_ref<ResourceRef> resource) {
  auto insertResult = _resourcesToRemove.emplace(key, std::promise<cpputils::unique_ref<Resource>>());
  assert(true == insertResult.second);
  cpputils::destruct(std::move(resource));

  //Wait for last resource user to release it
  auto resourceToRemove = insertResult.first->second.get_future().get();
  _resourcesToRemove.erase(key); //TODO Is this erase causing a race condition?

  _baseStore->removeFromBaseStore(std::move(resourceToRemove));
}

template<class Resource, class ResourceRef, class Key>
void ParallelAccessStore<Resource, ResourceRef, Key>::release(const Key &key) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto found = _openResources.find(key);
  assert (found != _openResources.end());
  found->second.releaseReference();
  if (found->second.refCountIsZero()) {
	  auto foundToRemove = _resourcesToRemove.find(key);
	  if (foundToRemove != _resourcesToRemove.end()) {
	    foundToRemove->second.set_value(found->second.moveResourceOut());
	  }
	  _openResources.erase(found);
  }
}

}

#endif
