# Hanvi
A Chinese-Vietnamese conversion tool inspired by QuickTranslator, designed to function as a drop-in replacement.

## Usage
Two primary components of the app:

* **Hanvi.exe**: The main application. The UI is designed to mimic QuickTranslator, with integrated tools for editing phrases and names.
* **HanviCLI.exe**: A command-line tool optimized for parallel batch conversions.
## Compiling

Before building, make sure Qt 6 is installed.

To build, run:
```bash
mkdir build
cd build

# Replace with your Qt installation path, if necessary.
cmake .. -DCMAKE_PREFIX_PATH=[YOUR_QT_PATH]
cmake --build . --config Release