#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

using namespace std;

// Обработчик SIGCHLD для очистки завершившихся фоновых процессов
static void sigchld_handler(int /*sig*/) {
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
}

// Разбивает строку на токены (команду и аргументы)
vector<string> split_tokens(const string &line) {
    vector<string> toks;
    istringstream iss(line);
    string t;
    while (iss >> t) toks.push_back(t);
    return toks;
}

int main() {
    // Установка обработчика SIGCHLD
    struct sigaction sa{};
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    string line;
    while (true) {
        cout << "mysh$ " << flush;
        if (!getline(cin, line)) break;  // EOF или Ctrl+D
        if (line.empty()) continue;

        auto toks = split_tokens(line);
        if (toks.empty()) continue;

        // --- встроенные команды ---
        if (toks[0] == "exit") {
            if (toks.size() > 1) {
                cerr << "exit: ignoring extra arguments\n";
            }
            break;  // выходим из shell
        }

        if (toks[0] == "cd") {
            const char* dir = toks.size() > 1 ? toks[1].c_str() : getenv("HOME");
            if (chdir(dir) != 0) perror("cd");
            continue;  // не запускать execvp
        }

        // --- проверка на фоновые процессы ---
        bool background = false;
        if (!toks.empty() && toks.back() == "&") {
            background = true;
            toks.pop_back();
            if (toks.empty()) continue;
        }

        // --- подготовка argv для execvp ---
        vector<char*> argv;
        for (auto &s : toks) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        // --- замер времени ---
        struct timespec tstart{}, tend{};
        clock_gettime(CLOCK_MONOTONIC, &tstart);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            // дочерний процесс
            sigaction(SIGCHLD, nullptr, nullptr); // сброс обработчика
            execvp(argv[0], argv.data());
            // если execvp вернуло — ошибка
            perror("execvp");
            _exit(127);
        } else {
            // родительский процесс
            if (background) {
                cout << "[bg] PID " << pid << "\n";
                continue; // не ждём
            } else {
                int status = 0;
                struct rusage usage{};
                pid_t w = wait4(pid, &status, 0, &usage);
                clock_gettime(CLOCK_MONOTONIC, &tend);

                double real = (tend.tv_sec - tstart.tv_sec) + (tend.tv_nsec - tstart.tv_nsec)/1e9;
                double user = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec/1e6;
                double sys  = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec/1e6;

                cout << "PID " << pid << " finished. real=" << real
                     << "s user=" << user << "s sys=" << sys << "s\n";
                cout << "voluntary ctxt s witches: " << usage.ru_nvcsw
                     << ", involuntary: " << usage.ru_nivcsw << "\n";

                if (WIFEXITED(status)) {
                    cout << "exit status: " << WEXITSTATUS(status) << "\n";
                } else if (WIFSIGNALED(status)) {
                    cout << "killed by signal: " << WTERMSIG(status) << "\n";
                }
            }
        }
    }

    cout << "\nBye\n";
    return 0;
}
