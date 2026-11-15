#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>

namespace {
namespace fs = std::filesystem;

bool runClient(const char* command, const char* config, const char* argument,
               const char* destination = nullptr, std::string* output = nullptr) {
    int pipeFd[2];
    if (output && pipe(pipeFd) == -1) {
        std::perror("pipe");
        return false;
    }
    const pid_t pid = fork();
    if (pid == -1) {
        std::perror("fork");
        if (output) {
            close(pipeFd[0]);
            close(pipeFd[1]);
        }
        return false;
    }
    if (pid == 0) {
        if (output) {
            close(pipeFd[0]);
            if (dup2(pipeFd[1], STDOUT_FILENO) == -1) {
                _exit(126);
            }
            if (pipeFd[1] != STDOUT_FILENO) {
                close(pipeFd[1]);
            }
        }
        execlp(command, command, config, argument, destination, static_cast<char*>(nullptr));
        std::perror(command);
        _exit(127);
    }

    bool readOk = true;
    if (output) {
        close(pipeFd[1]);
        char buffer[1024];
        for (;;) {
            const ssize_t size = read(pipeFd[0], buffer, sizeof(buffer));
            if (size > 0) {
                output->append(buffer, static_cast<std::size_t>(size));
            } else if (size == 0) {
                break;
            } else if (errno != EINTR) {
                std::perror("read");
                readOk = false;
                break;
            }
        }
        close(pipeFd[0]);
    }

    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        std::perror("waitpid");
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::cerr << command << " 失败";
        if (WIFEXITED(status)) {
            std::cerr << "，退出码 " << WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            std::cerr << "，信号 " << WTERMSIG(status);
        }
        std::cerr << '\n';
        return false;
    }
    return readOk;
}

bool sameContents(const char* source, const std::string& downloaded) {
    std::ifstream original(source, std::ios::binary);
    std::ifstream copy(downloaded, std::ios::binary);
    using Reader = std::istreambuf_iterator<char>;
    return original && copy && std::equal(Reader(original), Reader(), Reader(copy), Reader());
}
}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法 " << argv[0] << " <本地测试文件>\n";
        return 2;
    }
    const char* config = std::getenv("FDFSCLIENTCONF");
    if (!config || *config == '\0' || access(config, R_OK) != 0) {
        std::cerr << "请设置 FDFSCLIENTCONF 并确认配置文件可读\n";
        return 2;
    }
    std::error_code fileError;
    if (!fs::is_regular_file(argv[1], fileError) || access(argv[1], R_OK) != 0) {
        std::cerr << "本地测试文件不存在或不可读\n";
        return 2;
    }

    const auto tempDirectory = fs::temp_directory_path(fileError);
    if (fileError) {
        std::cerr << "无法定位临时目录 " << fileError.message() << '\n';
        return EXIT_FAILURE;
    }
    std::string downloaded = (tempDirectory / "cloudharbor-fdfs-XXXXXX").string();
    const int localFile = mkstemp(downloaded.data());
    if (localFile == -1) {
        std::perror("mkstemp");
        return EXIT_FAILURE;
    }
    close(localFile);

    std::string storageId;
    const bool uploaded = runClient("fdfs_upload_file", config, argv[1], nullptr, &storageId);
    while (!storageId.empty() && (storageId.back() == '\n' || storageId.back() == '\r')) {
        storageId.pop_back();
    }

    bool matches = false;
    bool deleted = false;
    const std::regex idPattern(R"(^[A-Za-z0-9_-]+/M[0-9A-Fa-f]{2}/[0-9A-Fa-f]{2}/[0-9A-Fa-f]{2}/[A-Za-z0-9_.-]+$)");
    if (uploaded && std::regex_match(storageId, idPattern)) {
        std::cout << "FastDFS 上传成功，文件 ID " << storageId << std::endl;
        if (runClient("fdfs_download_file", config, storageId.c_str(), downloaded.c_str())) {
            matches = sameContents(argv[1], downloaded);
            std::cout << (matches ? "下载内容比对通过\n" : "下载内容与原文件不同\n");
        }
        deleted = runClient("fdfs_delete_file", config, storageId.c_str());
        if (deleted) {
            std::cout << "FastDFS 测试文件已删除\n";
        } else {
            std::cerr << "删除失败，待清理文件 ID " << storageId << '\n';
        }
    } else if (uploaded) {
        std::cerr << "上传命令返回成功，但无法识别文件 ID，请核对客户端输出\n" << storageId << '\n';
    }

    fs::remove(downloaded, fileError);
    if (fileError) {
        std::cerr << "无法删除本地临时文件 " << downloaded << '\n';
    }
    return uploaded && matches && deleted && !fileError ? EXIT_SUCCESS : EXIT_FAILURE;
}
