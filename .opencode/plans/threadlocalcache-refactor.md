# ThreadLocalCache Method Refactor Plan

## Goal
Encapsulate local cache operations into methods of the `ThreadLocalCache` struct for better code organization and maintainability.

## Changes Required

### 1. Modify ThreadLocalCache struct (lines 70-74)
Add member methods:
- `empty()` - returns true if cache has no nodes
- `remove()` - pops and returns head node
- `add(Node* node_)` - pushes node to head

### 2. Update call sites

#### Line 228
Change: `if(_local_cache.head == nullptr)` → `if(_local_cache.empty())`

#### Lines 233-236 (_alloc_impl success path)
Replace:
```cpp
Node *node = _local_cache.head;
_local_cache.head = node->next;
_local_cache.count--;
mem = static_cast<void*>(node);
```
With:
```cpp
mem = static_cast<void*>(_local_cache.remove());
```

#### Lines 253-255 (exception handler)
Replace:
```cpp
node->next = _local_cache.head;
_local_cache.head = node;
_local_cache.count++;
```
With:
```cpp
_local_cache.add(node);
```

#### Lines 145-147 (_transfer_from_global_to_local)
Replace direct manipulation with `_local_cache.add(node)`

#### Lines 169-171 (_transfer_from_local_to_global)
Replace direct manipulation with `_local_cache.remove()`

#### Lines 283-285 (free_size)
Replace direct manipulation with `_local_cache.add(node)`

## Benefits
- Eliminates code duplication (add pattern used 3 times)
- Single source of truth for cache operations
- Self-documenting code
- Easier to add bounds checking or assertions later
