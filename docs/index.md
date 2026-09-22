# srnp

srnp is a blackboard middleware. Components publish key/value pairs, subscribe to
each other's pairs, and get callbacks when those change. There is no central data
store: every component holds its own pairs and pushes updates straight to whoever
asked for them.

It started as a hobby reimplementation of the
[PEIS Kernel](https://github.com/mbrx/peisecology) by Mathias Broxvall — the
concepts, not the code.

## Which section do you want?

**[Using srnp](user/index.md)** — build it, run the master, write a component,
and the reference for every public function. Start at the
[quickstart](user/quickstart.md).

**[Developing srnp](dev/index.md)** — how the library is put together: the
threading rules, the byte-level protocol, and what each source file does. Read
this if you are changing srnp itself rather than using it.
