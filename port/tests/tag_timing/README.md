# Native Tag input scheduling

On Windows with MinGW-w64 and the SDL3 development package:

```powershell
python port/tests/tag_timing/run.py
```

Set `CC` or `SDL3_PREFIX` when the compiler or SDL3 package is elsewhere.
The fixture includes the production native client. It checks local CPU scheduling,
continuous frame numbers during lead changes, retained short taps, one-shot commands,
startup late-input feedback and transport arrival timestamps. Its transport is an
in-memory queue. It opens no network sockets, game window or audio device and needs
no extracted game assets. Internet timing requires separate multi-client tests.
