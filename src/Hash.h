#ifndef _HASH_H_
#define _HASH_H_

#include "Seq.h"

typedef int Key;

template <class T> struct KeyValue {
  Key key;
  T value;
};

template <class T> KeyValue<T> keyValue(Key key, T value) {
  KeyValue<T> kv;
  kv.key = key;
  kv.value = value;
  return kv;
}

template <class T> class Hash
{
  private:
    // Initialisation
    void init(int logBuckets)
    {
      logNumBuckets = logBuckets;
      numBuckets    = 1 << logNumBuckets;
      buckets       = new SmallSeq<KeyValue<T>> [numBuckets];
    }

    // Hash function
    int hash(Key key) {
      int h = 0;
      int bottom = (1 << logNumBuckets) - 1;
      while (key != 0) {
        h = h ^ key;
        key = key >> logNumBuckets;
      }
      return h & bottom;
    }

  public:
    int numBuckets;
    int logNumBuckets;
    Seq<KeyValue<T>>* buckets;

    // Constructors
    Hash() { init(8); }
    Hash(int logNumBuckets) { init(logNumBuckets); }

    // Copy constructor
    Hash(const Hash<T>& hash) {
      init(hash.numBuckets);
      for (int i = 0; i < hash.numBuckets; i++)
        buckets[i] = hash.buckets[i];
    }

    // Insertion
    void insert(Key key, T value) {
      int h = hash(key);
      Seq<KeyValue<T>>* bucket = &buckets[h];
      for (int i = 0; i < bucket->numElems; i++) {
        if (bucket->elems[i].key == key) {
          bucket->elems[i].value = value;
          return;
        }
      }
      bucket->append(keyValue(key, value));
    }

    // Lookup
    bool lookup(Key key, T* value) {
      int h = hash(key);
      Seq<KeyValue<T>>* bucket = &buckets[h];
      for (int i = 0; i < bucket->numElems; i++) {
        if (bucket->elems[i].key == key) {
          *value = bucket->elems[i].value;
          return true;
        }
      }
      return false;
    }

    // Membership
    bool member(Key key) {
      int tmp;
      return lookup(key, &tmp);
    }

    // Destructor
    ~Hash()
    {
      delete [] buckets;
    }
};

static inline int intLog2(int n)
{
  int res = 0;
  while (n > 1) { n = n >> 1; res++; }
  return res;
}

#endif
