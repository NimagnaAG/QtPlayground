# spdlog

## License

- Type: MIT
- URL: https://github.com/gabime/spdlog/blob/v1.x/LICENSE

## Versions available

- v1.x, commit: 93f59d0: added 18.01.2022 by Valentin Vasiliu

## Build instructions

At the current time, the library, according to the readme instuctions in the libary's repo, can be installed in 2 ways:
- header only version: copy the include folder located in spdlog/include into our application and link it
- static library version: this one seems incomplete on Windows as the Makefile is not generated and the libary cannot be built

We use the header only version because the static library approach did not turn out to work on Windows.
As a header only version, this library does not need pre-built binaries for arm64 or x64_32 architectures.

If the header only installation is to be continued follow the next steps:
1. Download the desired new version of spdlog (new commit, new branch etc.)
2. Add into this folder, e.g. as `spdlog_v1.x_93f59d0`
3. Update `FindSpdlog.cmake` and specify which folder is being used as include folder.

# About this file

- Every package must contain a readme fulfilling minimal requirements as stated in https://nimagna.sharepoint.com/SitePages/Third-Party-Components-Usage.aspx#adding-libraries-to-external
  - License information (LGPL, MIT, ...) and a link to the original license.
  - Current version available (which one are we currently using if there is multiple versions)
  - Source (where/how to get the library)
  - Detailed instructions such that anyone can update/build/recreate the library.
