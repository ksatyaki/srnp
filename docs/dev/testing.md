# Testing

> Describes `tests/CMakeLists.txt`, `tests/*.cpp`, `tests/tsan.supp`,
> `.github/workflows/ci.yml`.

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

GoogleTest is found if installed, and fetched at configure time if not.

## The suites

| Test | Covers |
|:-----|:-------|
| `srnp_unit_tests` | the pair space, the wire codec, meta-value parsing, the GUI's formatting |
| `srnp_integration_tests` | a real master and real nodes over loopback |

### Unit tests

`test_pair_space.cpp` exercises `PairSpace` directly: adding and finding, what an
update preserves, placeholders, what a removal keeps, and callback handle
behaviour.

`test_wire.cpp` round-trips every message and then tries to break the decoder —
wrong magic, unknown version, unknown message type, an oversized payload, a
truncated payload, trailing bytes, a string length past the end of the buffer,
an out-of-range enum. The decoder is the only code in srnp that reads bytes from
an untrusted source, so it gets the most hostile tests.

`test_meta_parsing.cpp` covers `extractStrings` and the meta-value format.

`test_gui_format.cpp` covers `gui/format`, which has no ImGui in it, so these run
whether or not the GUI was built. See [The GUI](gui.md#what-the-tests-cover).

### Integration tests

`test_integration.cpp` runs a `MasterHub` and several full nodes in one process,
each with its own `io_context` and `PairSpace`. The master binds port 0, so the
tests never collide with a real master or with each other.

They avoid fixed sleeps. `waitFor(predicate)` polls until the condition holds or
five seconds pass, which keeps them fast when things work and gives them room on
a loaded machine when they do not.

Two patterns in there are load-bearing and easy to break:

- A `Recorder` a callback writes into is declared **before** the nodes, so it
  outlives them. A late update can still fire while a node is being torn down.
- `TestNode`'s destructor closes the client and stops the server before
  destroying either, so no coroutine resumes into a dead object.

## Sanitizers

The threading and lifetime behaviour is only really verified under sanitizers.

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DSRNP_SANITIZE=address,undefined
cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DSRNP_SANITIZE=thread
cmake --build build-tsan -j
TSAN_OPTIONS=suppressions=$PWD/tests/tsan.supp ctest --test-dir build-tsan --output-on-failure
```

`tests/tsan.supp` suppresses reports from inside Boost.Asio and the standard
library that are not srnp's to fix. Nothing in srnp's own code is suppressed — if
a report names an `srnp::` frame, it is real.

## The API-reference check

`tools/check_api_reference.py` fails if a function declared in `srnp_kernel.h` or
`meta_pair_callback.hpp` has no entry in
[`docs/user/api-reference.md`](../user/api-reference.md). It runs as part of
`srnp_unit_tests`' ctest registration, guarded by `find_package(Python3)` and
skipped when python3 is absent.

A function that genuinely should not be documented goes in the script's skip list
**with a reason written next to it**, so a deliberate omission is visible rather
than silent.

Run it on its own:

```bash
python3 tools/check_api_reference.py
```

The protocol and threading pages are not checked by anything and are the ones
most likely to drift. Both name the files they describe at the top, so a reader
who suspects drift knows where to look.

## CI

`.github/workflows/ci.yml` builds and tests three ways on every push: plain,
`address,undefined`, and `thread`. `fail-fast` is off, so one configuration
failing does not hide the others.

PairView is built in the plain job only. A GL driver under the thread sanitizer
reports races that have nothing to do with srnp, so the sanitizer jobs configure
with `-DSRNP_BUILD_GUI=OFF` and do not install GLFW.

There is no documentation job. `mkdocs build --strict` is worth running by hand
before touching the docs — it is what catches a broken internal link:

```bash
pip install -r docs/requirements.txt
mkdocs build --strict
```
