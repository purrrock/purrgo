---
icon: package-variant-closed
---
# Glyphix.js Packaging Tool

glyphix.js is the packaging tool for Glyphix applications. It includes a command-line tool named `gx` that can be used to create, build, and run Glyphix apps. The tool also includes a graphical emulator that can simulate running Glyphix apps on a computer.

This document provides installation and usage instructions for glyphix.js. The [Quick Start](../getting-started.md) tutorial is a simpler introductory guide. Please also read [Build and Run](#build-and-run) to learn how to develop, build, and publish a Glyphix app.

## Installation

This section describes how to install the glyphix.js packaging tool. For general use, you only need to know the [npm Installation](#npm-installation) method. The [Manual Installation](#manual-installation) method is suitable for special scenarios, such as network-restricted environments or CI builds.

### npm Installation

You can use the [npm](https://nodejs.org) package manager to install the glyphix.js packaging tool. It is recommended to use the `-g` option for a global installation:
::: code-tabs
@tab npm
```bash
npm install -g glyphix-cli
```

@tab pnpm
```bash
pnpm install -g glyphix-cli
```

@tab yarn
```bash
yarn global add glyphix-cli
```
:::

::: tip
Before installing globally with pnpm, you may need to run `pnpm setup` to configure environment variables. The `pnpm install -g` command will provide instructions on how to configure them.
:::

After the installation is complete, you can run `gx --version` in the terminal to check if the installation was successful. For example:
```bash
$ npm install -g glyphix-cli
$ gx --version
gx v0.10.1 - The Glyphix applet development toolchain
commit a9337cf1 - Tue Sep 23 10:03:48 2025 +0800
```

In addition, [pngquant](#pngquant) must be installed to package application resources for certain devices.

### Manual Installation

You can also install the glyphix.js packaging tool manually from a compressed archive: add the `bin` directory from the extracted folder to your `PATH` environment variable. The following describes the installation methods on major operating systems.

::: tip
The glyphix.js tool is not just a single executable; do not omit other resource files (including all files in the `bin` and `share` directories).
:::

#### macOS / Linux

For macOS or Linux, you can use the `tar` command to install the glyphix.js packaging tool. Before that, you also need to install tools like `xz`:

::: code-tabs
@tab macOS
```bash
brew install xz
```

@tab Ubuntu / Debian
```bash
sudo apt update
sudo apt install xz-utils
```

@tab Arch Linux
```bash
sudo pacman -S xz
```
:::

After downloading the glyphix.js archive, use the following command to extract and install it:
::: code-tabs
@tab macOS
```bash
tar -xvJf glyphix-v0.7.2-darwin-arm64.tar.xz -C ~/.local
```

@tab Linux
```bash
tar -xvJf glyphix-v0.7.2-linux-x86_64.tar.xz -C ~/.local
```
:::
Please make sure to replace the `.tar.xz` filename with the actual file you downloaded that corresponds to your operating system and CPU architecture. After extraction, commands like `gx` will be located in the `~/.local/bin` directory. Please add this directory to your `PATH` environment variable, for example by updating `.bashrc` like this:
```bash
# Add ~/.local/bin to PATH if it's not already there
echo "$PATH" | grep -q "$HOME/.local/bin" || echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc # Reload bash configuration
```

::: tip
When using `Zsh`, the `.zshrc` configuration file might import `.bashrc`, so you only need to update `.bashrc`. Otherwise, please update `.zshrc` using the method above.

It is recommended to install the glyphix.js packaging tool in the user's `~/.local` directory to avoid using root privileges for installation.
:::

#### Windows

To install glyphix.js on Windows, download the corresponding Windows version archive, and then use an extraction tool that supports the `7z` format (such as [7-Zip](https://www.7-zip.org/)) to extract it to a directory, such as `C:\glyphix`. Then add `C:\glyphix\bin` to the system's [`PATH` environment variable](https://learn.microsoft.com/en-us/previous-versions/office/developer/sharepoint-2010/ee537574(v=office.14)).

You can also use the `7z` command-line tool to extract it, for example:
```shell
7z x -y glyphix-v0.7.2-windows-x64.7z -oC:/glyphix
```
This is similar to the installation method for macOS and other systems.

### Installing System Dependencies

#### pngquant

Linux and macOS users need to install `pngquant` separately. You can use `npm` to install it:
```bash
npm install -g pngquant-bin # pngquant-bin only supports installation via npm
```
The Windows version of `glyphix-cli` includes `pngquant.exe`, so no additional installation is required.

::: tip
You can also download precompiled binaries from [pngquant.org](https://pngquant.org/) or install it from your system's package manager.
:::

#### Linux System Dependencies

The Linux installation package for glyphix.js is not specific to any distribution. Currently, there is only a build package for the linux-x86_64 architecture. We have tested that it runs on Ubuntu 20.04 (or newer) and Arch Linux.

If you only use the `gx` command for packaging (which is common for CI builds), then Linux distributions without a desktop environment should be able to use it directly. Running the graphical emulator depends on the X Window System, so you may need to install xorg-related packages. Especially in a Wayland environment, you also need to install the `xwayland` package (the emulator does not yet support native Wayland).

### Uninstallation

For glyphix.js installed globally via a package manager like npm, you can use the corresponding package manager to uninstall it, for example:
::: code-tabs
@tab npm
```bash
npm uninstall -g glyphix-cli
```

@tab pnpm
```bash
pnpm uninstall -g glyphix-cli
```

@tab yarn
```bash
yarn global remove glyphix-cli
```
:::

::: tip
For non-global installations using package managers like npm, simply remove the `glyphix-cli` dependency from `package.json` and run `npm install` (or `pnpm install`, `yarn install`) to update the `node_modules` directory.
:::

For manual installations, simply delete the files from the installation archive. For example, for the `tar.xz` installation file on macOS and Linux:
```bash
tar -tf glyphix-v0.7.2-darwin-arm64.tar.xz > filelist.txt
cat filelist.txt # Check the list of files to be deleted
xargs -I {} rm -f "~/.local/{}" < filelist.txt # Execute deletion after confirming
```
The `tar -tf` command lists the files in the archive. You should replace `glyphix-xxx.tar.xz` with the actual installation file. Manual uninstallation on Windows is similar.

## Build and Run

After installing glyphix.js, use the [`gx build`](#gx-build) command in the root directory of the application source code to build the application package, or use the [`gx emu`](#gx-emu) command to run the emulator.

After building the application, please refer to the [Submitting the Application Package](#submitting-the-application-package) section to learn how to install the application on a device or submit it to an application publishing platform.

## Command Line Arguments

### General Options

#### `gx --help`

View help information. Help information can also be used in specific subcommands, for example, using `gx build --help` to view help for the `build` subcommand specifically.

#### `gx --version`

The `-V --version` option is used to view the version number of the `gx` command.

#### `gx --verbose`

`-v --verbose` enables detailed log output, which is usually not needed by application developers.

#### `gx --numeric-version`

Outputs the pure numeric version number of the `gx` command, for example `0.10.1`.

#### `gx --quiet`

`-q --quiet` enables quiet mode, suppressing most log output except for warnings and errors. This includes build progress logs when using `gx build`, a mode typically used in CI environments where a large number of application packages need to be built.

View the version number.

### `gx new`

Creates a new project. For example, `gx new myapp` will create a new project named `myapp`.

### `gx build`

Builds the project (default operation). Use the `--device` or `-d` option to specify the target device, for example:
``` bash
gx build -d default # Build for the default device
```
Use the `--dump` option to print compilation details of UX files.

glyphix.js supports incremental builds; when source code changes, only the modified parts are rebuilt.

The `-r --image-rules` parameter specifies the image packaging rules file, defaulting to `config/image-rules.json`. The value of this parameter is cached, and subsequent executions of `gx build` or `gx emu` will follow the previous configuration.

#### Command Options

- `-d --device=NAME`: Specifies the target device name, which must be an installed device configuration name. You can use the `gx list device` command to view the list of installed devices. If this option is not specified, the `default` device is used by default.
- `-f --full`: Forces a full rebuild of the project instead of an incremental build.
- `-e --emulator`: Builds the project for the emulator instead of a physical device. This option is automatically used when executing the `gx emu` command.
- `-r --image-rules=PATH`: Specifies the image packaging rules file, defaulting to `config/image-rules.json`.

#### Submitting Application Packages

After building with `gx build`, a `.glyphix-work/dist/<device-name>/<package-name>` directory will be generated in the project directory, containing the built application package file (`.pkg` file). This file can be installed on a device via a mobile debugging app or submitted to an app distribution platform.

The `-d` option should be used to build application packages separately for all supported devices. Here is an example directory structure:
```bash
.glyphix-work/dist
├─ generic-watch-368x448
│  └─ com.example.app
│     ├─ bundle.pkg
│     ├─ icon.png
│     └─ manifest.json
└─ generic-watch-466x466
   └─ com.example.app
      ├─ bundle.pkg
      ├─ icon.png
      └─ manifest.json
```
When submitting application packages, please pack and upload the **entire** `.glyphix-work/dist` directory, rather than just the `.pkg` file or any single subdirectory. The platform identifies the application based on information in the `manifest.json` file and may require `icon.png` as a preview icon.

::: tip
For Linux or macOS users, you can use a command like this to build packages for a specific category of devices:
```bash
gx list device | grep "^generic-" | xargs -n 1 gx build -d
```
This will build application packages for all devices whose names start with `generic-`.

In Windows, you can also use a similar PowerShell command for batch building:
```shell
gx list device | ? { $_ -match "^generic-" } | % { gx build -d $_ }
```
:::

### `gx emu`

See the [Emulator and Debugging](./emulator.md) documentation.

### `gx clean`

Cleans build artifacts. This command deletes the `.glyphix-work` directory in the project folder.

### `gx config`

This command starts a web interface for editing image packaging rules. Follow the command prompts to open the page in a browser. This command has two usages:
``` bash
gx config # When inside a Glyphix project, no source directory needs to be specified (currently only usable in the project root)
gx config path/to/dir # Configure a specified directory, useful for non-project image resources
```

The `-r --image-rules` parameter specifies the image packaging rules file, defaulting to `config/image-rules.json`.

### `gx image-forge`

Converts standalone image files. This command can specify any source and output paths and does not need to be executed within a Glyphix project:
``` bash
gx image-forge src -o dist
```

Option descriptions:
- `src` is the source path to convert. The `image-forge` command recursively converts all images and generates them in the target path specified by `-o, --output` (default is `dist`) according to the relative directory structure.
- `-r --image-rules` parameter specifies the image packaging rules file, defaulting to `config/image-rules.json`.
- `-d --device` specifies the target device for image conversion.

### `gx list`

Lists certain information. Currently, three operations are supported:
``` bash
gx list device # List all installed device configurations
gx list template # List all installed project templates
gx list image # List relative paths of all image resources in the current directory (similar to the find command)
```

Some information can be listed with detailed descriptions using `-d, --detailed`, for example:
```
$ gx list device -d
The following devices have been found:
  default
    Default virtual device, for debugging purposes only.

  rtt-watch
    A smartwatch from RT-Thread. With a 1.43 inch screen
    and 4 GB of storage.
```

### `gx completion`

This command is used to generate shell auto-completion scripts for the `gx` command. It currently supports [Zsh](https://www.zsh.org/) and [PowerShell 7+](https://github.com/PowerShell/PowerShell). Using `gx completion [SHELL]` outputs the auto-completion script for the specified shell (it detects the current shell if the `SHELL` parameter is not specified). To install the completion script, please use:
```bash
gx completion --install
```
After successful installation, the installation path of the command completion script will be displayed. Restart the shell session to use auto-completion, or use these commands to make it take effect immediately:
::: code-tabs
@tab Oh My Zsh
```bash
omz reload
```

@tab PowerShell
```shell
Import-Module glyphix -Force
```
:::

When using the auto-completion script, you can select `gx emu` devices, command-line options, etc., in the terminal without manual typing.

PowerShell uses cycle completion by default; it is recommended to change it to a completion menu:
```shell
Set-PSReadLineKeyHandler -Key Tab -Function MenuComplete
```
Add this command to your [`$PROFILE`](https://learn.microsoft.com/en-us/powershell/scripting/learn/shell/creating-profiles#adding-customizations-to-your-profile) configuration file to make it permanent.

::: note
If the `--install` option fails to install automatically, you can also manually install the completion script using the `gx completion` command, for example:
```shell
gx completion zsh > ~/.zsh/completion/_gx.zsh
```
:::

## Default Configuration Paths

Configurations, project templates, device information, and other data in the glyphix.js tool can be stored in the following paths:
- System-level configuration: The `share/glyphix` directory relative to the parent directory of the `gx`/`gx.exe` executable. For example, if the path of the `gx` executable is `/usr/local/glyphix`, then the resource path for system-level configuration is `/usr/local/share/glyphix`.
- User-level configuration: `~/.local/share/glyphix` on Unix-like systems, and `%APPDATA%\AppData\Roaming\glyphix` on Windows.

Configuration files can be stored in either of the above paths, with user-level configuration having higher priority. `gx.js` comes with default configuration files upon installation.

## Project Templates

Project templates are stored in the `templates` directory of the configuration path. Currently, only the `simple` template is supported, and custom templates are not supported.

## Device Configuration Files

Device configuration files are stored in the `devices` directory of the configuration path. Each device has a YAML configuration file named `<device-name>.yml`. The format of the configuration file is described as follows:

``` yaml
# file: default.yml
description:
  Device description information for developers.

screen: # Fields describing the device screen configuration; these fields are mandatory
        # (they affect UI layout and resource scaling)
  width: 454 # Number of horizontal pixels on the screen
  height: 454 # Number of vertical pixels on the screen
  dpi: 326 # Screen pixel density, in pixels per inch (PPI)

ui: # Global interface configuration; all fields are optional
  font-family: sans-serif # The system's default font family name (defaults to serif)
  font-size: 3.5 # The system's default font size, in points (pt). Note: this is not in pixels!!
  font-map: true # Whether to use a global font configuration mapping file. If true, the
                 # font-faces.css file must exist in the system resources.

# Optional path for the system's global asset package. The following configuration means
# the global asset package is stored in the default-global folder at the same level as
# default.yml. Global asset packages contain pre-installed fonts, font configuration
# mapping files, etc.
global-assets: default-global

# Optional image conversion script. The script file path is relative to the current
# device description file. If no image conversion script is specified, original PNG
# assets will be output during bundling, but resolution scaling will still be applied.
image-build: image-convert.scm

# Command to run the emulator; defaults to glyphix-emu. The emulator command's
# executable must be located in the PATH environment variable; otherwise, it cannot
# be executed.
emulator: glyphix-emu
```
