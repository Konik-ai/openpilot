# Setup UI - Build Documentation

## How setup.py is compiled into an executable

This project uses Python's built-in `zipapp` module to create self-contained executables from Python scripts.

### Build Process

The compilation is handled by `/data/openpilot/release/pack.py`, which:
1. Copies relevant directories (`cereal`, `openpilot`) and specific file types (`.py`, `.png`, `.ttf`, `.capnp`)
2. Creates a zipapp archive with the specified module as the entry point
3. Produces a self-contained Python executable with a shebang (`#!/usr/bin/env python3`)

### Building the setup binary

To compile `setup.py` into an executable:

```bash
/data/openpilot/release/pack.py -o /tmp/setup openpilot.system.ui.setup
```

This creates `/tmp/setup` (or any specified output path) as a Python zipapp that:
- Contains all necessary dependencies bundled inside
- Has `openpilot.system.ui.setup:main` as its entry point
- Can be executed directly like any executable

### How it works

The zipapp creates a `__main__.py` file that serves as the entry point:

```python
# -*- coding: utf-8 -*-
import openpilot.system.ui.setup
openpilot.system.ui.setup.main()
```

When you run the executable, Python executes this `__main__.py`, which imports and calls the `main()` function from `setup.py`.

### Production deployment

The production setup executable is typically deployed to `/usr/comma/setup` on the device.
