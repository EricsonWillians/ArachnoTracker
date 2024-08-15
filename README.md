# ArachnoTracker

ArachnoTracker is a modern music tracker inspired by classic software like FastTracker II and MilkyTracker. It features polyphony, multi-effect processing, and Python scripting capabilities, offering a powerful tool for musicians and audio enthusiasts.

## Features

- **Polyphony Support:** Play multiple notes simultaneously within a single track.
- **Multi-Effect Processing:** Apply multiple effects to each note.
- **Python Scripting:** Use Python to script patterns and create complex compositions.
- **Cross-Platform Compatibility:** Designed to run on Linux, Windows, and macOS.

## Prerequisites

Before building ArachnoTracker, ensure you have the following dependencies installed on your system:

### Required Packages

- **CMake** (version 3.15 or higher)
- **GNU Compiler Collection (GCC)** (version 10 or higher)
- **JUCE Framework**
- **GLib**
- **GTK+ 3**
- **Pango**
- **Harfbuzz**
- **Cairo**
- **GDK-Pixbuf**
- **WebKitGTK**
- **ATK**
- **Libsoup**
- **libcurl**
- **pybind11**
- **SDL2**

### Installing Dependencies on Ubuntu

To install the required dependencies on Ubuntu, run the following commands:

```bash
sudo apt-get update

# Install CMake and GCC
sudo apt-get install cmake gcc-10 g++-10

# Install JUCE dependencies
sudo apt-get install libasound2-dev libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libcurl4-openssl-dev

# Install GLib, GTK, Pango, Harfbuzz, Cairo, GDK-Pixbuf, WebKitGTK, ATK, and Libsoup
sudo apt-get install libglib2.0-dev libgtk-3-dev libpango1.0-dev libharfbuzz-dev libcairo2-dev libgdk-pixbuf2.0-dev libwebkit2gtk-4.0-dev libatk1.0-dev libsoup2.4-dev

# Install pybind11
sudo apt-get install pybind11-dev

# Install SDL2
sudo apt-get install libsdl2-dev
```

### Installing JUCE

Download and extract the JUCE framework:

```bash
cd /path/to/your/workspace
wget https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.1.zip -O juce.zip
unzip juce.zip
mv JUCE-8.0.1 juce
```

## Building ArachnoTracker

### Step 1: Clone the Repository

If you haven't already, clone the ArachnoTracker repository to your local machine:

```bash
git clone https://github.com/yourusername/ArachnoTracker.git
cd ArachnoTracker
```

### Step 2: Configure the Build

Create a build directory and configure the project using CMake:

```bash
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=/usr/bin/gcc-10 -DCMAKE_CXX_COMPILER=/usr/bin/g++-10 ..
```

### Step 3: Build the Project

Compile the project using `make`:

```bash
make
```

If the build is successful, the executable `ArachnoTracker` will be created in the `build` directory.

## Running ArachnoTracker

Once the build is complete, you can run ArachnoTracker directly from the terminal:

```bash
./ArachnoTracker
```

A window should open, and you should hear a continuous square wave sound, verifying that the audio setup is functioning correctly.

## Troubleshooting

### Common Issues

- **No Sound:** Ensure that your audio output device is functioning correctly and that JUCE's audio settings are configured properly.
- **Missing Dependencies:** If you encounter errors related to missing libraries, ensure that all required packages are installed as per the instructions above.

### Additional Debugging

If you encounter any issues during the build process, consider running the following commands to clean the build directory and reconfigure the project:

```bash
rm -rf build
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=/usr/bin/gcc-10 -DCMAKE_CXX_COMPILER=/usr/bin/g++-10 ..
make
```

## Contributing

Contributions are welcome! If you'd like to contribute to ArachnoTracker, please fork the repository, create a new branch, and submit a pull request.

## License

ArachnoTracker is licensed under the [AGPLv3 License](https://www.gnu.org/licenses/agpl-3.0.html). Please see the `LICENSE.md` file for more details.