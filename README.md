# RegionalDialect

This is the core runtime component for Committee of Zero's MAGES engine game patches for the Nintendo Switch, based on [LanguageBarrier](https://github.com/CommitteeOfZero/LanguageBarrier). RegionalDialect requires the Atmosphère custom firmware, allowing for asset access redirection, hooking of game engine functions to enable rendering changes, and many more features.

As we try to support all games and any possible patches with a single binary, extensive external configuration is required (in particular, the hooks here won't function without the signatures pointing to their targets). See the root patch repositories (or patched game installations) for details.

## Build Instructions
CMake and the [devkitPro toolchain](https://devkitpro.org/wiki/Getting_Started) along with the switch-dev tools and libraries are required to build RegionalDialect.
Additionally, a Dockerfile is also provided to build without needing to install the toolchains.

### Local Environment
Ensure the `DEVKITPRO` environment variable is set to the devkitpro toolchain location.
Run cmake to build
`cmake --preset Release . && cmake --build . --preset Release` 
> The `CMakePresets.json` file contains configuration for the build, parameters can be overridden with command line cache variables, or through a local `CMakeUserPresets.json`

### Docker
Build the docker image
`docker build -t regiondialect-build .`
Run the container and mount it to the project directory, you can substitute <PROJECT_DIRECTORY> with ${PWD} on PowerShell and %cd% on bash 
`docker run --rm -it -v <PROJECT_DIRECTORY>:/workspace regiondialect-build bash`
Run cmake to build
`cmake --preset Release . && cmake --build . --preset Release` 

## Post Build
Once built, copy the subsd9 file into the exefs directory corresponding to the game. A gamedef.json and main.npdm file tailored to the specific game is also necessary for the mod to function. 

## Credits

- DaveGamble - [cJSON](https://github.com/DaveGamble/cJSON)

- shadowninja108 - [Exlaunch](https://github.com/shadowninja108/exlaunch)

- craftyboss - [scarlet-exlaunch-base](https://github.com/craftyboss/scarlet-exlaunch-base)

- skyline-dev - [skyline](https://github.com/skyline-dev/skyline)

**Check [THIRDPARTY.md](THIRDPARTY.md) for further info.**


