#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <cstring>
#include <cassert>
#include <string>
#include <cctype>

struct Rect { int x, y, w, h; };

static void fill_bgra(unsigned width, unsigned height, std::vector<uint32_t>& pixels) {
    pixels.resize(size_t(width) * height);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            uint8_t b = uint8_t((x * 13 + y * 7) & 0xFF);
            uint8_t g = uint8_t((x * 3 + y * 11) & 0xFF);
            uint8_t r = uint8_t((x * 17 + y * 5) & 0xFF);
            uint8_t a = 0xFF;
            uint32_t packed = (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
            pixels[size_t(y) * width + x] = packed;
        }
    }
}

static std::vector<Rect> make_dirty_rects(unsigned texW, unsigned texH) {
    std::vector<Rect> rects;
    unsigned rw = texW / 4;
    unsigned rh = texH / 4;
    rects.push_back({int(texW/8), int(texH/8), int(rw), int(rh)});
    rects.push_back({int(texW/2), int(texH/3), int(rw), int(rh)});
    rects.push_back({int(texW/3), int(texH/2), int(rw), int(rh)});
    return rects;
}

static void check_gl(const char* where) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        std::fprintf(stderr, "GL error 0x%X at %s\n", e, where);
        std::exit(1);
    }
}

static double now_ms() {
    using clock = std::chrono::high_resolution_clock;
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

static void sync_gpu() {
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
    const GLuint64 timeoutNS = 1000000000ull;
    GLenum res = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, timeoutNS);
    if (res == GL_TIMEOUT_EXPIRED) {
        std::fprintf(stderr, "GPU sync timeout\n");
    }
    glDeleteSync(fence);
}

static void print_gpu_info() {
    const GLubyte* vendor   = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version  = glGetString(GL_VERSION);
    const GLubyte* sl       = glGetString(GL_SHADING_LANGUAGE_VERSION);
    GLint major = 0, minor = 0; glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
    GLint maxTexSize = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    GLint numExt = 0; glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
    GLint maxCombinedTexUnits = 0; glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxCombinedTexUnits);

    std::printf("GPU Information:\n");
    std::printf("  Vendor      : %s\n", vendor   ? (const char*)vendor   : "?");
    std::printf("  Renderer    : %s\n", renderer ? (const char*)renderer : "?");
    std::printf("  GL Version  : %s (parsed core %d.%d)\n", version ? (const char*)version : "?", major, minor);
    std::printf("  GLSL        : %s\n", sl ? (const char*)sl : "?");
    std::printf("  MaxTexSize  : %d\n", maxTexSize);
    std::printf("  TexUnits    : %d (combined)\n", maxCombinedTexUnits);
    std::printf("  Extensions  : %d (count)\n", numExt);
    std::printf("\n");
}

struct BenchResult {
    unsigned w, h;
    double full_total_ms;
    double full_avg_ms;
    double sub_total_ms;
    double sub_avg_ms;
};

static BenchResult run_benchmark(unsigned texW, unsigned texH, int iterations) {
    std::vector<uint32_t> pixels;
    fill_bgra(texW, texH, pixels);
    auto dirtyRects = make_dirty_rects(texW, texH);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, texW);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.data());
    check_gl("initial glTexImage2D");
    sync_gpu();

    double t0 = now_ms();
    for (int i = 0; i < iterations; ++i) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, texW);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.data());
    }
    check_gl("loop glTexImage2D");
    sync_gpu();
    double t1 = now_ms();
    double full_ms = t1 - t0;

    t0 = now_ms();
    for (int i = 0; i < iterations; ++i) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, texW);
        for (const auto& r : dirtyRects) {
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, r.x);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, r.y);
            glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.data());
        }
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }
    check_gl("loop glTexSubImage2D");
    sync_gpu();
    t1 = now_ms();
    double sub_ms = t1 - t0;

    glDeleteTextures(1, &tex);

    BenchResult r{texW, texH, full_ms, full_ms / iterations, sub_ms, sub_ms / iterations};
    return r;
}

static bool parse_resolution(const char* s, unsigned &w, unsigned &h) {
    unsigned tw = 0, th = 0;
    if (std::sscanf(s, "%ux%u", &tw, &th) == 2 && tw > 0 && th > 0) { w = tw; h = th; return true; }
    return false;
}

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to init GLFW\n");
        return 1;
    }

    // Match Minecraft 1.21.4's minimum requirement of OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(640, 480, "bench", nullptr, nullptr);
    if (!win) {
        std::fprintf(stderr, "Failed to create window (OpenGL 3.3)\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::fprintf(stderr, "GLEW init error: %s\n", glewGetErrorString(glewErr));
        return 1;
    }

    GLint ctxMajor = 0, ctxMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &ctxMajor);
    glGetIntegerv(GL_MINOR_VERSION, &ctxMinor);
    if (ctxMajor < 3 || (ctxMajor == 3 && ctxMinor < 3)) {
        std::fprintf(stderr, "ERROR: Acquired OpenGL %d.%d but 3.3+ required.\n", ctxMajor, ctxMinor);
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    print_gpu_info();

    int iterations = 100;
    std::vector<std::pair<unsigned,unsigned>> resolutions;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        bool allDigits = true;
        for (const char* p = a; *p; ++p) if (!std::isdigit((unsigned char)*p)) { allDigits = false; break; }
        if (i == 1 && allDigits) {
            iterations = std::atoi(a);
            continue;
        }
        unsigned w = 0, h = 0;
        if (parse_resolution(a, w, h)) {
            resolutions.emplace_back(w, h);
        } else {
            std::fprintf(stderr, "Unrecognized argument '%s' (expected iterations or WxH)\n", a);
            return 1;
        }
    }

    if (resolutions.empty()) {
        unsigned defaults[][2] = {
            {128,128}, {256,256}, {320,240}, {400,300}, {512,512}, {640,480}, {800,600}, {1024,512},
            {1024,768}, {1152,864}, {1280,720}, {1280,800}, {1366,768}, {1440,900}, {1600,900}, {1680,1050},
            {1600,1200}, {1920,1080}, {1920,1200}, {2048,1152}, {2560,1080}, {2560,1440}, {3440,1440},
            {3840,2160}
        };
        for (auto &d : defaults) resolutions.emplace_back(d[0], d[1]);
    }

    std::printf("Iterations per resolution: %d\n", iterations);
    std::printf("ResolutionCount: %zu\n\n", resolutions.size());
    
    std::printf("%-12s %-12s %-12s %-10s %-10s %-10s\n", "Resolution", "FullAvg(ms)", "DirtyAvg(ms)", "Dirty%", "Speedup", "PixelsM/s");

    std::vector<BenchResult> all;
    all.reserve(resolutions.size());

    double bestSpeedup = 0.0; unsigned bestW = 0, bestH = 0;

    for (auto [w,h] : resolutions) {
        BenchResult r = run_benchmark(w, h, iterations);
        double speedup = (r.full_avg_ms > 0.0) ? (r.full_avg_ms / r.sub_avg_ms) : 0.0;
        double dirtyPercent = (r.full_avg_ms > 0.0) ? (r.sub_avg_ms / r.full_avg_ms * 100.0) : 0.0;
        double mpixPerSecFull = (r.w * r.h) / (r.full_avg_ms * 1000.0);
        double pixelsPerSec = (r.full_avg_ms > 0.0) ? ((double)r.w * r.h * 1000.0 / r.full_avg_ms) : 0.0;
        double megaPixelsPerSec = pixelsPerSec / 1.0e6;

        if (speedup > bestSpeedup) { bestSpeedup = speedup; bestW = r.w; bestH = r.h; }

        std::printf("%4ux%-6u %-12.3f %-12.3f %-10.1f %-10.2f %-10.2f\n",
                    r.w, r.h, r.full_avg_ms, r.sub_avg_ms, dirtyPercent, speedup, megaPixelsPerSec);
        all.push_back(r);
    }

    double full_total = 0.0, sub_total = 0.0;
    size_t sampleCount = 0;
    for (const auto &r : all) { full_total += r.full_total_ms; sub_total += r.sub_total_ms; ++sampleCount; }
    std::printf("\nAggregate across %zu resolutions:\n", sampleCount);
    std::printf("  Sum Full time : %.3f ms\n", full_total);
    std::printf("  Sum Dirty time: %.3f ms\n", sub_total);
    if (full_total > 0.0) {
        double overallSpeedup = full_total / sub_total;
        double percent = (sub_total / full_total) * 100.0;
        std::printf("  Dirty total is %.1f%% of Full (overall speedup %.2fx)\n", percent, overallSpeedup);
    }
    if (bestSpeedup > 0.0) {
        std::printf("\nBest per-resolution speedup: %.2fx at %ux%u\n", bestSpeedup, bestW, bestH);
    }

    std::printf("\nNote: Throughput column (PixelsM/s) is based on full uploads only and is megapixels per second.\n");

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
