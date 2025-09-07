# GL Benchmarks

## Bench Tex
![Bench Tex](/.github/img/bench_tex.png)

### Building

#### Linux
```bash
g++ bench_tex_upload.cpp -O2 -std=c++17 -lglfw -lGLEW -lGL -o bench_tex
```

#### Windows (MSYS2/MinGW)
First install dependencies:
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-glew
```

Then build:
```bash
g++ bench_tex_upload.cpp -O2 -std=c++17 -lglfw3 -lglew32 -lopengl32 -lgdi32 -o bench_tex.exe
```

### Usage

Run with default settings (100 iterations, predefined resolutions):
```bash
./bench_tex
```

Run with custom iteration count:
```bash
./bench_tex 50  # 50 iterations
```

Run with custom resolutions:
```bash
./bench_tex 100 800x600 1280x720      # 100 iterations, custom resolutions
./bench_tex 200 1920x1080 2560x1440 3840x2160  # 200 iterations, high-res tests
```

## License

This project is licensed under the WTFPL - see the [LICENSE](LICENSE) file for details.
