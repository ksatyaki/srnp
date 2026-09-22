# Running

## The master

```bash
srnp-master 12321
```

It prints the port it bound and then stays quiet:

```
SRNP master ready on port 12321
```

The port comes from, in order: the command-line argument, then
`SRNP_MASTER_PORT`, then 12321. Passing `0` lets the operating system pick one —
the printed line is then the only way to find out which, which is what the tests
rely on.

`Ctrl-C` shuts it down cleanly.

One master serves any number of components. Components find each other through
it, then talk directly, so the master is idle almost all of the time.

## Components

Every component needs to know where the master is:

```bash
export SRNP_MASTER_IP=127.0.0.1
export SRNP_MASTER_PORT=12321
./my_component
```

`srnp::initialize(argc, argv)` reads both and throws `srnp::InitError` if either
is missing. To skip the environment entirely, call the other overload:

```cpp
srnp::initialize("127.0.0.1", "12321");
```

`initialize` blocks for up to 10 seconds waiting for the master to hand out an
owner id, and throws if that never arrives.

### Arguments srnp understands

| Argument | Effect |
|:---------|:-------|
| `--owner-id <n>` | Ask the master for this owner id instead of a generated one. Ignored, with a warning from the master, if the id is taken. |

Anything else on the command line is left alone.

## Environment variables

| Variable | Read by | Meaning |
|:---------|:--------|:--------|
| `SRNP_MASTER_IP` | components | The master's address. Required. |
| `SRNP_MASTER_PORT` | components, master | The master's port. Required for components. |
| `SRNP_LOG_LEVEL` | `srnp-master` | How much the master prints. |

`SRNP_LOG_LEVEL` is only read by the master executable. A component sets its own
level in code:

```cpp
srnp::srnp_print_setup("debug");
```

## Log levels

`trace`, `debug`, `info`, `warning`, `error`, `fatal`, `off` — any case. Anything
below the level set is dropped before it is even formatted. `warning` and above
also print the source file and line.

The default is `info` for the master and for a component that never calls
`srnp_print_setup`.

## Ports

A component's server binds port 0, so the operating system picks one, and the
component tells the master which it got. Nothing but the master's port needs to
be fixed. Every component must be able to reach every other component's port
directly — srnp does no relaying.
