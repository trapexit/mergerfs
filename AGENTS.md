# mergerfs - AGENTS.md

This file contains build commands, testing procedures, and code style guidelines for agentic coding assistants working in this repository.

```bash
# Build project
make

# Build tests
make tests

# Clean build artifacts
make clean

# Build with specific options
make USE_XATTR=0      # Build without xattr support
make STATIC=1         # Build static binary
make LTO=1            # Enable link-time optimization

# Build with debugging and sanitizers
make NDEBUG= SANITIZE=1

# Install
make install
```

### Unit Tests (C++)

```bash
# Build tests
make tests

# Run all unit tests
./build/tests <test_name>
# Example: ./build/tests config_bool

# Run all tests via acutest framework
./build/tests
```

### Integration Tests (Python)

```bash
# Build the main binary first
make

# Run all integration tests
./tests/run-tests <path-to-mergerfs-binary>
# Example: ./tests/run-tests ./build/mergerfs

# Run a single integration test directly
./tests/TEST_unlink_rename <path-to-mergerfs-binary>
```

## Code Style Guidelines

### File Organization

- **Source files**: `src/*.cpp`
- **Header files**: `src/*.hpp`
- **Tests**: `tests/*.cpp` (unit), `tests/TEST_*` (integration)
- **Build**: `Makefile` (no build system generators)

### License Headers

Every source file MUST start with the full ISC license header:

```cpp
/*
  ISC License

  Copyright (c) YEAR, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
```

### Include Guards and Ordering

Use `#pragma once` for header guards. Organize includes as:

```cpp
#include "local_header.hpp"   // Local headers first
#include <system_header>      // Then system headers

#include <system_headers.h>   // C headers last, if needed
```

### Naming Conventions

- **Classes**: `PascalCase` (e.g., `Branches`, `Config`, `Policy`)
- **Functions/methods**: `snake_case` (e.g., `minfreespace()`, `from_string()`)
- **Variables**: `snake_case` (e.g., `branches_`, `fusepath_`)
- **Private members**: `_prefix` (e.g., `_impl`, `_mutex`)
- **Constants/macros**: `SCREAMING_SNAKE_CASE` (e.g., `MINFREESPACE_DEFAULT`)
- **Namespaces**: `PascalCase` for major namespaces, `lowercase` for utilities (e.g., `Policy`, `fs`, `str`)

### Indentation and Formatting

- **Indentation**: 2 spaces
- **Brace style**: K&R style
- **Line length**: No strict limit, but keep readable (~80-100 chars preferred)
- **No trailing whitespace**

```cpp
namespace Policy
{
  class ActionImpl
  {
  public:
    ActionImpl(const std::string &name_)
      : name(name_)
    {
    }

  public:
    std::string name;
    virtual int operator()(...) const = 0;
  };
}
```

### Types and Type Safety

- **Language**: C++17 (`-std=c++17`)
- **Use standard types**: `int`, `uint64_t`, `size_t`, etc.
- **Custom typedefs**: Use `typedef` for common configuration types:
  ```cpp
  typedef ToFromWrapper<bool> ConfigBOOL;
  typedef ToFromWrapper<uint64_t> ConfigUINT64;
  ```
- **Smart pointers**: Use `std::shared_ptr`, `std::unique_ptr` appropriately

### Error Handling

- **Return values**: Functions return `0` for success, negative `errno` for errors
- **System call errors**: Return negative errno (e.g., `-EACCES`, `-ENOENT`)
- **Error propagation**: Keep negative errno values through the call stack

```cpp
int
fs::some_function(const fs::path &path_)
{
  int rv = ::actual_syscall(path_);
  if(rv < 0)
    return -errno;
  return rv;
}
```

Use the `error_and_continue()` macro for policy error handling:

```cpp
#define error_and_continue(CUR,ERR) \
  {                                 \
    policy::calc_error(CUR,ERR);    \
    continue;                       \
  }
```

### Functions and Methods

- **Static helpers**: Prefix with `_` when file-local (e.g., `_helper_function()`)
- **Namespaces**: Explicitly use namespace qualifiers for organization
- **Parameters**: Pass by const reference for complex types, by value for simple types
- **Output parameters**: Use pointers or references

```cpp
static
int
_helper_function(const Branches::Ptr  &branches_,
                std::vector<Branch*> &paths_)
{
  // Implementation
  return 0;
}

namespace FUSE
{
  int
  open(const fuse_req_ctx_t *ctx,
       const char           *fusepath,
       fuse_file_info_t     *ffi);
}
```

### Testing Style

**Unit tests (C++)** use the Acutest framework:

```cpp
void
test_config_bool()
{
  ConfigBOOL v;

  TEST_CHECK(v.from_string("true") == 0);
  TEST_CHECK(v == true);
  TEST_CHECK(v.to_string() == "true");

  TEST_CHECK(v.from_string("false") == 0);
  TEST_CHECK(v == false);
  TEST_CHECK(v.to_string() == "false");

  TEST_CHECK(v.from_string("asdf") == -EINVAL);
}

TEST_LIST =
  {
   {"config_bool",test_config_bool},
   {NULL,NULL}
  };
```

**Integration tests (Python)** are scripts prefixed with `TEST_` that test specific filesystem behaviors.

### Compiler Warnings

The Makefile uses strict warnings with `-Werror` for:
- `-Wpessimizing-move` / `-Werror=pessimizing-move`
- `-Wredundant-move` / `-Werror=redundant-move`
- `-Wall`, `-Wextra`, `-Wno-unused-result`, `-Wno-unused-parameter`

Code must compile without warnings.

### Version Notes

- Only tagged releases are officially supported
- `master` and other branches are works in progress
- Always ensure code compiles and tests pass before submitting changes
