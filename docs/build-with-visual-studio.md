## Building LibSass with Visual Studio

The minimum requirement to build LibSass with Visual Studio is currently
version 15.0 (Visual Studio 2017). Simply download and install [Visual
Studio from Microsoft](https://visualstudio.microsoft.com/downloads/).
The Community Edition is even free for open-source projects.

Additionally, it is recommended to have `git` installed and available
via `PATH` env-variable, in order to deduce the `libsass` version info.
If `git` is not available, the LibSass version will be set to `[NA]`.

Once installed simply load `win/libsass.sln` file into Visual Studio.
Then build (Ctrl+Shift+B) any configuration you'd like.

[1]: https://visualstudio.microsoft.com/downloads/

### Building on command prompt

In order to build with MSVC on the command line, you either need to bring
`msbuild.exe` into the common `PATH` folders or reference it via absolute path.
The easiest way is to open the `VS201X Tools Command Prompt`, which should be
a shortcuts installed alongside with Visual Studio.

It is normally located at `"%ProgramFiles(x86)%\MSBuild\15.0\Bin\MSBuild"`

* If the platform is 32-bit Windows, replace `ProgramFiles(x86)` with `ProgramFiles`.
* To build with Visual Studio 2019, replace `15.0` with `16.0` in the commands.

Once the command is available you can build with multiple config options:

```batch
MSBuild win\libsass.sln /p:Platform="x64" /p:Configuration="Release Shared" -t:Clean;Build
MSBuild win\libsass.sln /p:Platform="x86" /p:Configuration="Debug Static" -t:Clean;Build
```

The results can be found inside the `build` directory.
