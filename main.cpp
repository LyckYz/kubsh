#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <pwd.h>
#include <grp.h>
#include <csignal>
#include <atomic>
#include <thread>
#include <map>
#include <algorithm>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <sys/file.h>

std::atomic<bool> running{ true };
std::atomic<bool> sighup_received{ false };
std::string vfs_dir;
std::string history_path;

// Обработчик SIGHUP
void sighup_handler(int sig) {
    sighup_received = true;
}

// ================ VFS ФУНКЦИИ ================

// синхронизация файловой системы
void force_sync() {
    sync();
    usleep(50000); // 50ms
}

// Проверка существования пользователя
bool user_exists(const std::string& username) {
    struct passwd* pwd = getpwnam(username.c_str());
    if (pwd) return true;

    // Прямое чтение файла
    std::ifstream passwd_file("/etc/passwd");
    std::string line;
    while (std::getline(passwd_file, line)) {
        if (line.find(username + ":") == 0) {
            return true;
        }
    }
    return false;
}

// Создание пользователя
void create_user(const std::string& username) {
    std::cout << "\n=== CREATE_USER('" << username << "') ===" << std::endl;
    
    // 1. Проверка существования
    std::cout << "Checking if user exists..." << std::endl;
    bool exists = user_exists(username);
    std::cout << "User exists: " << (exists ? "YES" : "NO") << std::endl;
    
    if (exists) {
        std::cout << "Skipping - user already exists" << std::endl;
        return;
    }
    
    // 2. Генерация UID
    static std::atomic<int> next_uid{10000};
    int uid = next_uid++;
    std::cout << "Generated UID: " << uid << std::endl;
    
    // 3. Формирование записи
    std::string entry = username + ":x:" + std::to_string(uid) + ":" + 
                       std::to_string(uid) + "::/home/" + username + ":/bin/bash\n";
    std::cout << "Entry to write: " << entry;
    
    // 4. Проверяем права на файл
    std::cout << "Checking /etc/passwd permissions..." << std::endl;
    struct stat st;
    if (stat("/etc/passwd", &st) == 0) {
        std::cout << "File exists, mode: " << std::oct << st.st_mode << std::dec << std::endl;
        std::cout << "Can write: " << (access("/etc/passwd", W_OK) == 0 ? "YES" : "NO") << std::endl;
    }
    
    // 5. Открываем файл
    std::cout << "Opening /etc/passwd for append..." << std::endl;
    int fd = open("/etc/passwd", O_WRONLY | O_APPEND);
    if (fd == -1) {
        std::cerr << "ERROR: open() failed: " << strerror(errno) << std::endl;
        std::cout << "Trying with O_CREAT..." << std::endl;
        fd = open("/etc/passwd", O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd == -1) {
            std::cerr << "ERROR: Second open() failed: " << strerror(errno) << std::endl;
            return;
        }
    }
    
    // 6. Пишем
    std::cout << "Writing to file descriptor " << fd << "..." << std::endl;
    ssize_t written = write(fd, entry.c_str(), entry.size());
    std::cout << "write() returned: " << written << " (expected: " << entry.size() << ")" << std::endl;
    
    if (written == -1) {
        std::cerr << "ERROR: write() failed: " << strerror(errno) << std::endl;
    }
    
    // 7. Синхронизируем
    std::cout << "Syncing..." << std::endl;
    fsync(fd);
    close(fd);
    sync();
    
    // 8. Немедленная проверка
    std::cout << "Immediate verification..." << std::endl;
    int verify_fd = open("/etc/passwd", O_RDONLY);
    if (verify_fd != -1) {
        // Читаем последние 2000 байт
        off_t size = lseek(verify_fd, 0, SEEK_END);
        off_t start = (size > 2000) ? size - 2000 : 0;
        lseek(verify_fd, start, SEEK_SET);
        
        char buffer[2001];
        ssize_t bytes = read(verify_fd, buffer, 2000);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::cout << "Last " << bytes << " bytes of /etc/passwd:" << std::endl;
            std::cout << buffer << std::endl;
            
            // Ищем нашу запись
            if (strstr(buffer, entry.c_str())) {
                std::cout << "SUCCESS: Found our entry in /etc/passwd!" << std::endl;
            } else {
                std::cout << "WARNING: Our entry NOT found in /etc/passwd" << std::endl;
            }
        }
        close(verify_fd);
    }
    
    // 9. Создаем VFS файлы
    std::cout << "Creating VFS files..." << std::endl;
    std::string user_path = vfs_dir + "/" + username;
    
    if (mkdir(user_path.c_str(), 0755) == -1 && errno != EEXIST) {
        std::cerr << "ERROR: mkdir failed: " << strerror(errno) << std::endl;
    } else {
        std::cout << "Created directory: " << user_path << std::endl;
    }
    
    // Создаем файлы
    std::ofstream id_file(user_path + "/id");
    if (id_file.is_open()) {
        id_file << uid;
        std::cout << "Created id file with: " << uid << std::endl;
    }
    
    std::ofstream home_file(user_path + "/home");
    if (home_file.is_open()) {
        home_file << "/home/" + username;
        std::cout << "Created home file" << std::endl;
    }
    
    std::ofstream shell_file(user_path + "/shell");
    if (shell_file.is_open()) {
        shell_file << "/bin/bash";
        std::cout << "Created shell file" << std::endl;
    }
    
    std::cout << "=== CREATE_USER COMPLETE ===" << std::endl;
}

// Удаление пользователя
void delete_user(const std::string& username) {
    // В тестовом режиме удаляем из /etc/passwd
    if (vfs_dir == "/opt/users") {

        // Читаем весь файл, пропускаем нужного пользователя
        std::ifstream passwd_in("/etc/passwd");
        std::string content;
        std::string line;

        while (std::getline(passwd_in, line)) {
            if (!line.empty() && line.find(username + ":") != 0) {
                content += line + "\n";
            }
        }
        passwd_in.close();

        // Перезаписываем файл
        std::ofstream passwd_out("/etc/passwd");
        if (passwd_out.is_open()) {
            passwd_out << content;
            passwd_out.close();
        }

        force_sync();

    }
    else {
        // Нормальный режим
        std::string cmd = "userdel -r " + username + " 2>&1";

        int result = system(cmd.c_str());
        std::cout << "Command result: " << result << std::endl;

        force_sync();
    }

    std::cout << "User deleted: " << username << std::endl;
}

void monitor_directory() {
    std::cout << "\n=== VFS MONITOR ULTRA-SIMPLE ===" << std::endl;
    std::cout << "vfs_dir = '" << vfs_dir << "'" << std::endl;
    
    // Проверяем директорию
    struct stat st;
    if (stat(vfs_dir.c_str(), &st) == -1) {
        std::cout << "ERROR: Directory doesn't exist!" << std::endl;
        return;
    }
    
    std::cout << "Directory exists, starting scans..." << std::endl;
    
    for (int i = 0; i < 10; i++) {
        std::cout << "\n--- Scan " << i + 1 << " ---" << std::endl;
        
        // Простейший способ - system()
        std::string cmd = "ls -1 " + vfs_dir + " 2>/dev/null | grep -v '^\\.'";
        std::cout << "Running: " << cmd << std::endl;
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            char buffer[128];
            int count = 0;
            
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                std::string name = buffer;
                // Убираем перевод строки
                if (!name.empty() && name.back() == '\n') {
                    name.pop_back();
                }
                
                count++;
                std::cout << "  Found via ls: '" << name << "'" << std::endl;
                
                // Проверяем, директория ли это
                std::string full_path = vfs_dir + "/" + name;
                struct stat entry_st;
                
                if (stat(full_path.c_str(), &entry_st) == 0) {
                    if (S_ISDIR(entry_st.st_mode)) {
                        std::cout << "    Is directory!" << std::endl;
                        
                        if (!user_exists(name)) {
                            std::cout << "    User doesn't exist, creating..." << std::endl;
                            create_user(name);
                        }
                    }
                }
            }
            
            pclose(pipe);
            std::cout << "Total found: " << count << std::endl;
        }
        
        if (i < 9) {
            std::cout << "Sleeping 0.5 seconds..." << std::endl;
            usleep(500000);
        }
    }
    
    std::cout << "\n=== MONITOR DONE ===" << std::endl;
}

// Инициализация VFS
void init_vfs() {
    struct stat st;
    if (stat("/opt/tests/users", &st) != -1) {
        vfs_dir = "/opt/tests/users";
    }
    else {
        const char* home = getenv("HOME");
        vfs_dir = std::string(home ? home : "/root") + "/users";
        std::cout << "Path of VFS: " << vfs_dir << std::endl;
    }

    mkdir(vfs_dir.c_str(), 0755);

    // Инициализируем пользователей
    std::ifstream passwd("/etc/passwd");
    std::string line;
    int count = 0;

    while (std::getline(passwd, line)) {
        if (line.empty()) continue;
        
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string part;

        while (std::getline(ss, part, ':')) {
            parts.push_back(part);
        }

        if (parts.size() >= 7) {
            std::string shell = parts[6];
            
            if (shell.length() >= 2 && 
                shell.substr(shell.length() - 2) == "sh") {
                
                std::string user = parts[0];
                std::string user_dir = vfs_dir + "/" + user;

                mkdir(user_dir.c_str(), 0755);

                std::ofstream(user_dir + "/id") << parts[2];
                std::ofstream(user_dir + "/home") << parts[5];
                std::ofstream(user_dir + "/shell") << parts[6];
                count++;
            }
        }
    }

    std::cout << "VFS initialized with " << count << " users" << std::endl;

    std::cout << "running flag before thread start: " << (running ? "TRUE" : "FALSE") << std::endl;


}

// Добавление в историю
void save_to_history(const std::string& command) {
    if (command.empty() || command[0] == ' ') return;

    std::ofstream history_file(history_path, std::ios::app);
    if (history_file.is_open()) {
        history_file << command << std::endl;
    }
}

// Обработка echo/debug
bool handle_echo(const std::vector<std::string>& args) {
    if (args.empty() || (args[0] != "echo" && args[0] != "debug")) return false;

    if (args[0] == "debug") {
        std::cout << std::endl; // УБРАН ЛИШНИЙ ОТСТУП
    }

    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) std::cout << " ";
        std::string arg = args[i];

        // Убираем кавычки
        if (arg.size() >= 2) {
            char first = arg[0];
            char last = arg[arg.size() - 1];
            if ((first == '\'' && last == '\'') || (first == '"' && last == '"')) {
                arg = arg.substr(1, arg.size() - 2);
            }
        }

        std::cout << arg;
    }
    std::cout << std::endl;
    return true;
}

// Обработка переменных окружения
bool handle_env(const std::vector<std::string>& args) {
    if (args.size() != 2 || args[0] != "\\e") return false;

    std::string var = args[1];

    std::cout << std::endl;

    if (var == "$PATH" || var == "PATH") {
        const char* path = getenv("PATH");
        if (path) {
            std::stringstream ss(path);
            std::string dir;
            while (std::getline(ss, dir, ':')) {
                std::cout << dir << std::endl;
            }
        }
        return true;
    }

    if (var == "$HOME" || var == "HOME") {
        const char* home = getenv("HOME");
        if (home) std::cout << home << std::endl;
        return true;
    }

    if (var[0] == '$') {
        const char* value = getenv(var.substr(1).c_str());
        if (value) std::cout << value << std::endl;
        return true;
    }

    return false;
}

// Обработка информации о разделах
bool handle_partition(const std::vector<std::string>& args) {
    if (args.size() != 2 || args[0] != "\\l") return false;

    std::string disk = args[1];
    std::cout << "Partition information for " << disk << ":\n";

    std::string cmd = "lsblk " + disk + " 2>/dev/null";
    if (system(cmd.c_str()) != 0) {
        std::cout << "Try: fdisk -l " << disk << std::endl;
    }

    return true;
}

// Выполнение внешней команды
bool execute_external(const std::vector<std::string>& args) {
    if (args.empty()) return false;

    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        std::vector<char*> exec_args;
        for (const auto& a : args) {
            exec_args.push_back(const_cast<char*>(a.c_str()));
        }
        exec_args.push_back(nullptr);

        execvp(exec_args[0], exec_args.data());
        std::cout << args[0] << ": command not found" << std::endl;
        exit(127);
    }
    else if (pid > 0) {
        // Родительский процесс
        waitpid(pid, nullptr, 0);
        return true;
    }

    return false;
}

void show_prompt() {
    if (vfs_dir != "/opt/users") {
        std::cout << "\n$:>";
        std::cout.flush();
    }
}

int main() {
    // Отключаем буферизацию
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // Инициализация истории
    const char* home = getenv("HOME");
    if (home) {
        history_path = std::string(home) + "/.kubsh_history";
    }
    else {
        history_path = "/root/.kubsh_history";
    }
    std::cout << "Path of History: " << history_path << "\n";

    // Инициализация VFS
    init_vfs();

    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    std::string input;

    // Начальное приглашение
    show_prompt();
    std::cout << "\nStarting VFS monitoring..." << std::endl;
    std::thread(monitor_directory).detach();
    while (running) {
        if (sighup_received.exchange(false)) {
            std::cout << "\nConfiguration reloaded" << std::endl;
            show_prompt();
        }

        if (!std::getline(std::cin, input)) {
            if (errno == EINTR && sighup_received.exchange(false)) {
                std::cout << "\nConfiguration reloaded" << std::endl;
                show_prompt();
                continue;
            }
            break; // Ctrl+D или ошибка
        }

        // Пропускаем пустые строки
        if (input.empty()) {
            show_prompt();
            continue;
        }

        save_to_history(input);

        std::vector<std::string> args;
        std::stringstream ss(input);
        std::string arg;
        while (ss >> arg) {
            args.push_back(arg);
        }

        if (args.empty()) {
            show_prompt();
            continue;
        }

        if (args[0] == "exit" || args[0] == "\\q") {
            std::cout << "Exiting..." << std::endl;
            break;
        }

        // Обработка команд
        bool handled = false;

        if (handle_echo(args)) {
            handled = true;
        }
        else if (handle_env(args)) {
            handled = true;
        }
        else if (handle_partition(args)) {
            handled = true;
        }

        // Внешние команды
        if (!handled) {
            if (!execute_external(args)) {
                std::cout << args[0] << ": command not found" << std::endl;
            }
        }

        // Новое приглашение после команды
        if (running) {
            show_prompt();
        }
    }

    running = false;
    usleep(200000);

    return 0;
}
