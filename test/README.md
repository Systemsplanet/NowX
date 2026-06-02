# NowX tests

Native (host) tests run under PlatformIO + Unity:

```bash
pio test -e native
```

These exercise the entire `NxProtocol` core via an in-memory
`LoopbackTransport`. No hardware required.

To run only one test file's group, edit `test_main.cpp` and comment out
the others, or use `pio test -e native --filter test_native`.
