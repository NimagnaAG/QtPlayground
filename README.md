# QtPlayground

A Qt based application with a rendering system similar to the Nimagna application.

Works on Windows and on macOS.

## Usage

  1. Start the application.
  2. Select menu "Load image..." and load an image
  3. The image is shown
  4. Using the mouse, the view gets changed.

## Build instructions

### Windows

#### Requirements

- Windows 10 or 11
- Install Visual Studio 2022 with "C++ Desktop Development" workload.
- Install [OpenSource Qt](https://www.qt.io/download-open-source) version **6.8.0** using default installation (must end up in default installation folder C:\Qt\6.8.0\msvc2022_64) and with the following components
  - MSVC 2022 64-bit 
  - From "Additional Libraries"
    - Qt Image Formats
    - Qt Multimedia
    - Note: Qt might automatically install additional libraries if needed
- Install Git
  - https://git-scm.com/downloads
  - Install GitHub LFS extension: https://git-lfs.com/
- Check out this repository: `git clone https://github.com/NimagnaAG/QtPlayground.git`

#### Building the solution using CMake

- Pull the latest changes from Git: 
  - `git pull --rebase`
- Open the repository root folder in Visual Studio using the "Open Folder" option
- Visual Studio will automatically 
  - detect the x64-Release and x64-Debug configurations
  - reconfigure the CMake project if a CMakeLists.txt file changes
- Select the desired **Configuration** (x64-Release, x64-Debug)
  - Release and Debug build the DLL projects and both the Test and MainApplication executables
  - Install builds the DLL projects, the MainApplication executable, and the installer projects
- Ensure that "MainApplication.exe" is selected as the startup target using the drop down next to the green play icon 
  or - in CMake Targets view - using the context menu on the MainApplication target and selecting it as Startup Item.
- Perform a **Rebuild All**
- The build output can be found at
  - Release: `x64/Release/`
  - Debug: `x64/Debug/`

  ### macOS

#### Requirements

- macOS 12.3 or newer
- Install brew package manager
  - `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
  - add `PATH=/opt/homebrew/bin/brew:$PATH`  to `~/.zshrc` (or `~/.bashrc` depending on your terminal settings)
  - Verify: `brew --version` and `which brew`
- Install CMake (>=3.24): `brew install cmake`, verify with `cmake --version`
- Install Git: `brew install git`, verify with `git --version`
- Install Git LFS: `brew install git-lfs`
  - run `git lfs install`
- Install Qt6
  - Download Qt Open Source installer form https://www.qt.io/download-qt-installer-oss
  - Install Qt version **6.8.0** with the following options
    - macos
    - Additional Libraries
      - Qt Image Formats
      - Qt Multimedia
      - Note: Qt might automatically install additional libraries if needed
- Install XCode command line tools: `xcode-select --install`
- Install XCode (tested with version 16) using App Store
- Clone this repository into `SourceDir`, e.g.  `~/development/QtApplication`

- Install XCode.
- Install [OpenSource Qt](https://www.qt.io/download-open-source) version **6.8.0** using default installation (must end up in default installation folder C:\Qt\6.8.0\msvc2022_64) and with the following components
  - MSVC 2022 64-bit 
  - From "Additional Libraries"
    - Qt Image Formats
    - Qt Multimedia
    - Note: Qt might automatically install additional libraries if needed
- Install Git
  - https://git-scm.com/downloads
  - Install GitHub LFS extension: https://git-lfs.com/
- Check out this repository: `git clone https://github.com/NimagnaAG/QtPlayground.git`

#### Building the solution using CMake

- Pull the latest changes from Git: 
  - `git pull --rebase`
- In a shell
  - cd into the repository root folder `SourceDir`
  - `mkdir build`
  - `cd build`
  - `cmake -DCMAKE_OSX_ARCHITECTURES=arm64 -G Xcode ..`
- Open `{SourceDir}/build/QtPlayground - QtPlayground.xcodeproj` in XCode
  - Compile and run
