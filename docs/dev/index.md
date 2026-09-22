# Developing srnp

This section describes how srnp is built, for anyone changing the library itself.
If you are writing a component that uses srnp, the [user section](../user/index.md)
is what you want.

- **[Architecture](architecture.md)** — the master, the components, and which
  file holds what.
- **[Threading](threading.md)** — the io context, strands, and the rules about
  sockets and locks that everything else depends on.
- **[Protocol](protocol.md)** — the byte-level wire format and every message
  flow.
- **[Internals](internals.md)** — the classes, and the surprising parts.
- **[Testing](testing.md)** — ctest, sanitizers, and what is and is not covered.
- **[The GUI](gui.md)** — how PairView is put together, and how to add a panel.

Each page names the source files it describes at the top. If you suspect a page
has gone stale, those files are the authority.
