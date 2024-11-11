# QtPlayground

A Qt based application with a rendering system similar to the Nimagna application.

Up to now only for Windows.

## Usage

  1. Start the application.
  2. Select menu "Load image..." and load an image
  3. The image is shown
  4. Using the mouse, the view gets changed.

## Build instructions

### Windows

#### Requirements

- Windows 10 or 11 (not yet tested on macOS)
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

  Not yet supported/tested.
