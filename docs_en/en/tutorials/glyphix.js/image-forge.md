---
icon: image-filter
---
# Image Management

The `glyphix.js` bundler manages all PNG image assets within the project (`src` directory). The relevant modules primarily provide the following features:
- Support for image asset configuration files and related configuration interfaces.
- Conversion of images to optimized sizes and formats for the device during bundling.

App developers only need to configure image bundling parameters as needed, while device vendors need to define specific image conversion strategies for the device.

## App Development Configuration

In app development, image bundling parameters must be configured to correctly generate the resource package. Configuring `config/image-rules.json` and properties such as `config.designWidth` in `src/manifest.json` will affect the image bundling behavior. `config/image-rules.json` is generally used to configure quality and performance parameters, while fields in `manifest.json` affect the global scaling ratio of images (used to adapt to devices with different resolutions).

::: tip
`config/image-rules.json` can be configured using the `gx config` command or other methods, but direct editing with a text editor is not recommended.
:::

When using the `gx config` command, developers will mainly focus on two parameters: `transparent` and `quality`.

### Transparent Parameter

`Transparent` indicates whether the image contains transparent pixels. If configured to false (`false`) and the source image contains transparent pixels, these pixels will be converted to opaque during generation (usually by overlaying them onto a black background). Therefore, necessary images must be marked to preserve transparent pixels; otherwise, incorrect overlay effects will be displayed. Since opaque images offer better performance on some platforms and have smaller data sizes, the `transparent` option is disabled by default.

### Quality Parameter

The `Quality` parameter represents the quality of the image after bundling, which is an integer in the range of $[0, 100]$. However, usually only three approximate quality levels are used:
- High: 100, representing the highest quality.
- Middle: 50, medium quality, the default value.
- Low: 0, low quality.

Image assets are optimized based on the quality parameter during conversion. Generally speaking, medium quality is the recommended conversion strategy as it balances display effects, rendering/loading performance, and memory footprint on the target platform. Using high quality may provide better visual quality but might lead to performance degradation. Low quality can be used for images where quality can be sacrificed to improve performance (such as photos). Specific target platforms may also ignore the `quality` parameter and use a unified strategy.

## Device and Platform Adaptation

Assuming device and platform vendors have already implemented optimized image resource formats for specific target platforms and support multiple quality and pixel formats, the following work is required to generate these image formats in glyphix.js:
- Implement a command-line tool required for **single image** conversion.
  - A command-line interface must be provided to convert PNG images to custom formats, supporting output to a specified path (including overwriting the original file).
  - It is best to provide a command-line interface to convert custom formats back to PNG images, supporting output to a specified path (including overwriting the original file). Without this feature, PC-side previewing cannot be achieved.
- Write device description files and image conversion scripts.

### Image Conversion Script

The image conversion script is a Scheme file. `glyphix.js` calls this script when an image needs to be converted, and the script can determine how to convert the image based on these variables:
- `env.image-path`: The absolute path of the image to be converted. The converted image overwrites this path.
- `env.transparent`: The transparency parameter for this image.
- `env.quality`: The quality parameter for this image.
- `env.target`: The conversion target mode, described later.
- `env.verbose`: Whether to enable verbose mode. If enabled, detailed logs can be output; otherwise, no logs should be output.
- `env.script-dir`: The absolute path where the current script file is located. If the commands required for conversion are relative to this script file and not in the `PATH` environment variable, this parameter can be used for concatenation.

`env.target` represents the **target mode** for image conversion, and its value determines which specific conversion method to apply:
- `"device"`: Executes the full conversion process for the target device, such as removing the alpha channel from opaque images and then converting them to PGF format (Glyphix image format) according to the quality parameter.
- `"emulator"`: Executes the conversion process for the emulator. Since the emulator does not support hardware-specific texture formats (such as ETC2), to ensure images display correctly in the emulator, one might only remove the alpha channel of opaque images without further converting to the target device format (or convert to a software-supported PGF format).
- `"preprocess"`: Executes only the preprocessing steps, which means removing the alpha channel of opaque images and outputting the result in PNG format.
- `"preview"`: Generates a PNG image for preview. First, the image is converted to the custom target format following the `"device"` target conversion process, and then the output image is converted back to PNG for preview use.

::: tip
If the command-line tool for image conversion does not support converting custom formats back to PNG, do not implement the `"preprocess"` and `"preview"` target modes.
:::

### image-forge Command Line Tool

`image-forge` is a command-line tool for the PGF image format provided by Glyphix, featuring the following:
- Supports converting PNG images to PGF format, and PGF back to PNG images.
- Supports common ARGB and PAL pixel formats, and distinguishes premultiplied alpha mode.
- Supports blending transparent ARGB images onto a specified solid color background to convert them into opaque images (rather than simply discarding the alpha channel).
- Supports row alignment by pixel or byte.
- Supports LZ4 compression and allows setting a minimum compression threshold (image data below the threshold will not be compressed).

For platforms using other custom image formats, `image-forge` can also be utilized to remove alpha channels.

## Image Conversion Script Example

The following example demonstrates how to use commands like `image-forge` to convert PNG to PGF images, prioritizing the use of the Palette (PAL) format.

First, define the target formats for opaque and transparent cases:
``` scheme
; Define pixel format rules for opaque colors
(define (opaque-formats q)
  (cond ((<= q 50) "pal-rgb")
        (else "rgb24")))

; Define pixel format rules for transparent colors
(define (transparent-formats q)
  (cond ((<= q 50) "pal-argb-premul")
        (else "argb32-premul")))

; Calculate the target pixel format based on transparency and quality parameters
(define pixel-format
  ((if env.transparent
      transparent-formats opaque-formats)
    env.quality))

; Whether to convert the image to a palette format
(define palette (<= env.quality 50))
```

The above code uses the palette format when the quality is less than or equal to 50, and uses `pal-rgb` or `pal-argb` depending on transparency. When quality is higher than 50, it uses RGB or ARGB pixel formats with 8-bit sampling. Ultimately, the `pixel-format` variable holds the actual pixel format name used, and `palette` indicates whether to use the palette format.

Next, define the commands to be used in various situations:

``` scheme
; Whether to add the --verbose command line argument
(define if-verbose (if env.verbose "--verbose " ""))

; Call the pngquant command to reduce image colors to within 256 colors; pngquant must be installed in the system
(define color-reduction
  (string-append "pngquant --ext=.png --force " if-verbose env.image-path))

; Convert the image to PGF format
(define convert (string-append "image-forge "
  "--format=" pixel-format " " ; Specify output pixel format
  "--compress --min-compress-ratio=5 " ; Compress image data to reduce file size, minimum compression ratio is 5
  "--align=16 --pixel-align " ; Align image by 16 pixels
  if-verbose
  env.image-path))

; Remove image Alpha channel and add a background
(define remove-alpha (string-append "image-forge --bypass "
  ; On bes2500ibp watches, non-transparent images can have the alpha channel removed and blended with a black background; 
  ; this operation can improve image quality after PAL color reduction
  (if env.transparent "" "--background black ")
  if-verbose
  env.image-path))

; Command to convert PGF image back to PNG
(define decode
  (string-append "image-forge --decode " if-verbose env.image-path))
```

In the following code, `execute-try` calls the specified `f` function if the command exits with a non-zero status, and the `execute` function prints an error log and exits the script abnormally if the command exits with a non-zero status. The `run-convert` function executes the full target device image conversion process (calling the `remove-alpha` and `convert` commands).

``` scheme
; Execute a command and print the command content in verbose mode; if the command exits with a non-zero status, call function f
(define (execute-try cmd f)
  (begin
    (if env.verbose ; Print command content if in verbose mode
      (display (string-append "Run command: " cmd "\n")))
    (let ((r (system (string-append env.script-dir "/bin/" cmd))))
      (if (= r 0) 0 (f r)))
  ))

; Execute a command and print the command content in verbose mode; if the command exits abnormally, exit the program
(define (execute cmd)
  (execute-try cmd (lambda (x)
    (begin ; Print error code and exit abnormally on failure
      (display (string-append "subprocess failed (" (number->string x) "): " cmd "\n"))
      (exit-fail)
  ))))

; Convert images
(define (run-convert)
  (begin
    (execute remove-alpha) ; Remove alpha channel first
    (if palette (execute color-reduction)) ; If it is a palette format, reduce the number of pixels in the image
    (execute convert) ; Execute image conversion command
  ))
```

The `targets` macro defines processing methods for all target modes. For example, the `"device"` mode will call the `run-convert` function.

``` scheme
; Define conversion strategies for targets
(targets env.target
  ; Device mode: Final image conversion process for the target device
  ("device" (run-convert))
  ; Emulator mode: Only remove the alpha channel of non-transparent images, do not convert format
  ("emulator" (execute remove-alpha))
  ; Preprocessing mode: Remove the alpha channel of non-transparent images and add a background
  ("preprocess" (execute remove-alpha))
  ; Preview mode: Generate PNG preview images consistent with the actual device display
  ("preview" (begin
    (run-convert) ; First convert the image to PGF format
    (execute decode))) ; Then convert the image back to PNG
  )
```

### Using the Image Conversion Script

To use the image conversion script, you need to add a field to the device model description file:

``` yaml
description: default watch

screen:
  width: 454 # pixels
  height: 454 # pixels
  dpi: 326 # pixels per inch

#...
image-build: image-convert-pal.scm # Path of the image conversion script relative to this YAML file
```

### More Complex Strategies

Since the image conversion script is a full programming language rather than a configuration language like YAML or JSON, we can implement more complex custom conversion strategies without being limited by the features provided by the framework. Taking the palette format conversion above as an example: the PAL format does not perform well on images with rich colors. In such cases, the image can be converted to a format that performs better in these scenarios. The specific idea is as follows:
1. The `pngquant` command supports exiting abnormally if the quality is lower than a specified value after converting to PAL format. Therefore, configure the command parameters for this purpose.
2. Change the `color-reduction` operation executed by `execute` in the `run-convert` function to be executed by `execute-try`, and use alternative format conversion operations in the latter's exception handling function.
3. The processing method for targets like `preview` is similar, but note that when converting the output format to PNG, it is also necessary to identify the command's abnormal exit and continue trying with subsequent commands.

In summary, it is similar to the logic of a shell script, using the command's exception exit code to control the flow.
